import contextlib
import http.client
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import sys
import threading
import time
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import oepl

MAC = "00000123456789AB"


def jpeg(width=1, height=1):
    """Tiny original baseline grayscale JPEG: one constant-gray MCU, no fixture dependency."""
    def segment(marker, value):
        return b"\xff" + bytes([marker]) + (len(value) + 2).to_bytes(2, "big") + value
    frame = b"\x08" + height.to_bytes(2, "big") + width.to_bytes(2, "big") + b"\x01\x01\x11\x00"
    # One length-1 code: zero DC category, then zero AC end-of-block.
    huffman = b"\x00\x01" + b"\x00" * 15 + b"\x00" + b"\x10\x01" + b"\x00" * 15 + b"\x00"
    return (b"\xff\xd8" + segment(0xDB, b"\x00" + b"\x01" * 64) + segment(0xC0, frame) +
            segment(0xC4, huffman) + segment(0xDA, b"\x01\x01\x00\x00\x3f\x00") + b"\x3f\xff\xd9")


@contextlib.contextmanager
def ap_server(route):
    calls = []

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            self.respond()

        def do_POST(self):
            self.respond()

        def respond(self):
            body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
            calls.append((self.command, self.path, dict(self.headers), body))
            status, data, headers = route(self.command, self.path, body)
            if isinstance(data, dict):
                data = json.dumps(data).encode()
            self.send_response(status)
            for key, value in headers.items():
                self.send_header(key, value)
            if "Content-Length" not in headers and "Transfer-Encoding" not in headers:
                self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def log_message(self, *_):
            pass

    with ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
        thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.01}, daemon=True)
        thread.start()
        try:
            yield f"http://127.0.0.1:{server.server_port}", calls
        finally:
            server.shutdown()
            thread.join()


def normal_route(method, path, body):
    if path.startswith("/get_db"):
        return 200, {"tags": [{"mac": MAC, "alias": "Bench", "hwType": 10,
                               "RSSI": -47, "batteryMv": 3000, "pending": 2, "lastseen": 0}]}, {}
    if path == "/tagtypes/0A.json":
        return 200, {"width": 1, "height": 1, "bpp": 2, "rotatebuffer": 1}, {}
    if method == "POST" and path == "/imgupload":
        return 200, b"Ok, saved", {}
    return 404, b"missing", {}


class AddressTests(unittest.TestCase):
    def test_canonical_mac(self):
        self.assertEqual(MAC, oepl.canonical_mac("0123456789ab"))
        self.assertEqual(MAC, oepl.canonical_mac(MAC))
        for value in (None, 12, "01:23:45:67:89:ab", "0123456789AZ", "0123456789AB\n"):
            with self.subTest(value=value), self.assertRaises(oepl.OeplError):
                oepl.canonical_mac(value)

    def test_ap_origin(self):
        self.assertEqual("http://ap.local:8080", oepl.ap_url("http://AP.local:8080/"))
        self.assertEqual("https://[::1]", oepl.ap_url("https://[::1]/"))
        for value in (None, "ap.local", "ftp://ap.local", "http://user:secret@ap.local", "http://ap.local/api",
                      "http://ap.local/?", "http://ap.local/#", "http://ap.local:", "http://ap.local:0",
                      "http://ap.local:65536", "http://ap.local\n", "http://ap.local\\@elsewhere", "http://[::1%lo0]"):
            with self.subTest(value=value), self.assertRaises(oepl.OeplError):
                oepl.ap_url(value)


class JpegTests(unittest.TestCase):
    def test_baseline_dimensions_and_truncation(self):
        self.assertEqual((1, 1), oepl.jpeg_dimensions(jpeg()))
        self.assertEqual((296, 128), oepl.jpeg_dimensions(jpeg(296, 128)))
        for image in (b"", b"not jpeg", jpeg()[:-1], jpeg()[:-5], jpeg() + b"trailing",
                      b"\xff\xd8\xff\xd9", jpeg().replace(b"\xff\xc0", b"\xff\xc2"),
                      jpeg(0, 1), jpeg(2049, 1), jpeg().replace(b"\x3f\xff\xd9", b"\xff\xd9")):
            with self.subTest(image=image[:20]), self.assertRaises(oepl.OeplError):
                oepl.jpeg_dimensions(image)

    def test_scan_and_component_headers(self):
        image = jpeg()
        sof = image.index(b"\xff\xc0")
        bad = bytearray(image)
        bad[sof + 4] = 12
        with self.assertRaisesRegex(oepl.OeplError, "8-bit"):
            oepl.jpeg_dimensions(bytes(bad))
        bad = bytearray(image)
        scan = image.index(b"\xff\xda")
        bad[scan + 5] = 9
        with self.assertRaisesRegex(oepl.OeplError, "scan"):
            oepl.jpeg_dimensions(bytes(bad))

    def test_bad_tables_and_oversized_images_are_rejected(self):
        image = jpeg()
        for marker in (b"\xff\xdb", b"\xff\xc4"):
            bad = bytearray(image)
            offset = image.index(marker)
            bad[offset + 4] = 0xFF
            with self.subTest(marker=marker), self.assertRaisesRegex(oepl.OeplError, "table"):
                oepl.jpeg_dimensions(bytes(bad))
        with self.assertRaisesRegex(oepl.OeplError, "2 MiB"):
            oepl.jpeg_dimensions(b"\xff\xd8" + b"x" * oepl.MAX_IMAGE_BYTES)


class ClientTests(unittest.TestCase):
    def test_list_and_selected_status_use_ap_metadata(self):
        with ap_server(normal_route) as (url, calls):
            client = oepl.Client(url)
            result = client.tags()
            self.assertEqual({"mac": MAC, "alias": "Bench", "hwType": 10, "width": 1,
                              "height": 1, "bpp": 2, "batteryMv": 3000, "rssi": -47,
                              "lastseen": 0, "pending": 2}, result["tags"][0])
            self.assertEqual(MAC, client.status("0123456789ab")["tag"]["mac"])
            self.assertIn(("GET", f"/get_db?mac={MAC}"), [(c[0], c[1]) for c in calls])
            self.assertEqual(["/get_db", "/tagtypes/0A.json"], [c[1] for c in calls[:2]])

    def test_pagination_fetches_every_page_and_caches_metadata(self):
        def route(method, path, body):
            if path == "/get_db":
                return 200, {"tags": [{"mac": MAC, "hwType": 10}], "continu": 1}, {}
            if path == "/get_db?pos=1":
                return 200, {"tags": [{"mac": "00000123456789AC", "hwType": 10}]}, {}
            return normal_route(method, path, body)
        with ap_server(route) as (url, calls):
            tags = oepl.Client(url).tags()["tags"]
            self.assertEqual(2, len(tags))
            self.assertIsNone(tags[0]["batteryMv"])
            self.assertEqual(1, sum(c[1] == "/tagtypes/0A.json" for c in calls))

    def test_invalid_or_looping_pagination_is_not_silent_truncation(self):
        for continuation in (0, -1, 256, True, "1"):
            with self.subTest(continuation=continuation):
                with ap_server(lambda *_: (200, {"tags": [{"mac": MAC}], "continu": continuation}, {})) as (url, _):
                    with self.assertRaisesRegex(oepl.OeplError, "pagination"):
                        oepl.Client(url).tags()
        with ap_server(lambda *_: (200, {"tags": [{"mac": MAC}], "continu": 1}, {})) as (url, calls):
            with self.assertRaisesRegex(oepl.OeplError, "pagination"):
                oepl.Client(url).tags()
            self.assertEqual(2, len(calls))

    def test_malformed_upstream_and_error_json_are_rejected(self):
        for data in (b"<html>login</html>", b"[]", b'{"tags": [], "tags": []}', b'{"tags":NaN}',
                     {"error": "invalid"}, {"tags": "bad"}, {"tags": [True]}, {"tags": [{"mac": "no"}]},
                     {"tags": [{"mac": MAC, "hwType": True}]}, {"tags": [{"mac": MAC, "RSSI": "-40"}]}):
            with self.subTest(data=data), ap_server(lambda *_, value=data: (200, value, {})) as (url, _):
                with self.assertRaises(oepl.OeplError):
                    oepl.Client(url).tags()

    def test_unknown_mac_does_not_submit(self):
        with ap_server(lambda *_: (200, {"tags": []}, {})) as (url, calls):
            with self.assertRaisesRegex(oepl.OeplError, "not registered"):
                oepl.Client(url).upload(MAC, jpeg())
            self.assertTrue(all(call[0] == "GET" for call in calls))

    def test_mismatched_or_ambiguous_selected_mac_does_not_submit(self):
        for rows in ([{"mac": "00000123456789AC"}], [{"mac": MAC}, {"mac": MAC}]):
            with ap_server(lambda *_, value=rows: (200, {"tags": value}, {})) as (url, calls):
                with self.assertRaises(oepl.OeplError):
                    oepl.Client(url).upload(MAC, jpeg())
                self.assertTrue(all(call[0] == "GET" for call in calls))

    def test_missing_metadata_lists_unknown_but_blocks_upload(self):
        def route(method, path, body):
            return (404, b"missing", {}) if path.startswith("/tagtypes") else normal_route(method, path, body)
        with ap_server(route) as (url, calls):
            client = oepl.Client(url)
            result = client.tags()
            self.assertIn("metadataError", result)
            self.assertIsNone(result["tags"][0]["width"])
            with self.assertRaisesRegex(oepl.OeplError, "dimensions are unavailable"):
                client.upload(MAC, jpeg())
            self.assertTrue(all(call[0] == "GET" for call in calls))

    def test_invalid_inputs_and_wrong_dimensions_never_post(self):
        with ap_server(normal_route) as (url, calls):
            client = oepl.Client(url)
            for data, dither in ((jpeg(2, 1), 0), (jpeg()[:-4], 0), (jpeg(), True), (jpeg(), 3)):
                with self.subTest(dither=dither), self.assertRaises(oepl.OeplError):
                    client.upload(MAC, data, dither)
            self.assertTrue(all(call[0] == "GET" for call in calls))

    def test_upload_sends_scalars_before_file_and_never_claims_display(self):
        with ap_server(normal_route) as (url, calls):
            result = oepl.Client(url).upload("0123456789ab", jpeg(), 1)
            self.assertEqual("submitted", result["status"])
            self.assertFalse(result["displayConfirmed"])
            posts = [call for call in calls if call[0] == "POST"]
            self.assertEqual(1, len(posts))
            _, path, headers, body = posts[0]
            self.assertEqual("/imgupload", path)
            self.assertTrue(headers["Content-Type"].startswith("multipart/form-data; boundary="))
            self.assertLess(body.index(b'name="mac"'), body.index(b'name="file"'))
            self.assertLess(body.index(b'name="dither"'), body.index(b'name="file"'))
            self.assertIn(MAC.encode(), body)
            self.assertIn(jpeg(), body)
            self.assertNotIn(b'name="rotate"', body)

    def test_http_200_without_upload_ack_is_unknown_and_not_retried(self):
        for reply in (b"", b"Error while saving", b"OK", b"<html>Portal</html>"):
            def route(method, path, body):
                return (200, reply, {}) if method == "POST" else normal_route(method, path, body)
            with self.subTest(reply=reply), ap_server(route) as (url, calls):
                with self.assertRaisesRegex(oepl.OeplError, "outcome is unknown"):
                    oepl.Client(url).upload(MAC, jpeg())
                self.assertEqual(1, sum(c[0] == "POST" for c in calls))

    def test_redirect_errors_response_limits_and_partial_read(self):
        responses = ((302, b"", {"Location": "http://example.invalid/"}),
                     (409, b"busy", {}), (200, b"{}", {"Content-Length": str(oepl.MAX_RESPONSE_BYTES + 1)}),
                     (200, b"{}", {"Content-Length": "20"}), (200, b"x" * (oepl.MAX_RESPONSE_BYTES + 1), {}))
        for response in responses:
            with self.subTest(response=response[:1]), ap_server(lambda *_, value=response: value) as (url, calls):
                with self.assertRaises(oepl.OeplError):
                    oepl.Client(url).tags()
                self.assertEqual(1, len(calls))

    def test_ambiguous_and_unsupported_transfer_framing_is_rejected(self):
        for headers in ({"Transfer-Encoding": "chunked", "Content-Length": "2"},
                        {"Transfer-Encoding": "gzip"}):
            with self.subTest(headers=headers), ap_server(lambda *_, value=headers: (200, b"{}", value)) as (url, _):
                with self.assertRaisesRegex(oepl.OeplError, "transfer framing"):
                    oepl.Client(url).tags()

    def test_valid_chunked_response_is_supported(self):
        payload = b'{"tags": []}'
        framed = f"{len(payload):x}\r\n".encode() + payload + b"\r\n0\r\n\r\n"
        with ap_server(lambda *_: (200, framed, {"Transfer-Encoding": "chunked"})) as (url, _):
            self.assertEqual([], oepl.Client(url).tags()["tags"])

    def test_timeout_is_bounded_and_no_retry(self):
        def slow(*_):
            time.sleep(0.06)
            return 200, {"tags": []}, {}
        with ap_server(slow) as (url, calls):
            with self.assertRaisesRegex(oepl.OeplError, "timed out"):
                oepl.Client(url, timeout=0.01).tags()
            self.assertEqual(1, len(calls))

    def test_timed_out_upload_is_never_automatically_retried(self):
        def route(method, path, body):
            if method == "POST":
                time.sleep(0.06)
            return normal_route(method, path, body)
        with ap_server(route) as (url, calls):
            with self.assertRaisesRegex(oepl.OeplError, "unknown outcome"):
                oepl.Client(url, timeout=0.01).upload(MAC, jpeg())
            self.assertEqual(1, sum(call[0] == "POST" for call in calls))

    def test_cli_errors_return_nonzero(self):
        with mock.patch("sys.stderr") as stderr:
            self.assertEqual(1, oepl.main(["tags", "--ap", "not-an-origin"]))
            self.assertTrue(stderr.write.called)


if __name__ == "__main__":
    unittest.main()
