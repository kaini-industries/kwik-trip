import base64
import contextlib
import http.client
import json
from pathlib import Path
import socket
import sys
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import oepl
import studio
from test_oepl import MAC, ap_server, jpeg, normal_route


@contextlib.contextmanager
def studio_server(**kwargs):
    with studio.StudioServer(("127.0.0.1", 0), **kwargs) as server:
        thread = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.01}, daemon=True)
        thread.start()
        try:
            yield server
        finally:
            server.shutdown()
            thread.join()


def request(server, path, data=None, *, headers=None, method=None):
    payload = json.dumps(data).encode() if isinstance(data, dict) else data
    origin = f"http://127.0.0.1:{server.server_port}"
    options = {"Origin": origin, "Content-Type": "application/json"}
    options.update(headers or {})
    options = {key: value for key, value in options.items() if value is not None}
    connection = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=2)
    try:
        connection.request(method or ("POST" if payload is not None else "GET"), path, payload, options)
        response = connection.getresponse()
        return response.status, response.read(), dict(response.getheaders())
    finally:
        connection.close()


class StudioTests(unittest.TestCase):
    def test_loopback_only(self):
        for address in ("0.0.0.0", "192.168.1.10", "example.com"):
            with self.subTest(address=address), self.assertRaises(ValueError):
                studio.StudioServer((address, 0))

    def test_serves_only_web_root_and_refuses_directory_listing_and_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            web = root / "web"
            web.mkdir()
            (web / "index.html").write_text("<p>Editor</p>")
            (root / "secret.txt").write_text("secret")
            (web / "outside").symlink_to(root / "secret.txt")
            (web / "folder").mkdir()
            with studio_server(web_root=web) as server:
                status, body, headers = request(server, "/")
                self.assertEqual(200, status)
                self.assertIn(b"Editor", body)
                self.assertEqual("no-store", headers["Cache-Control"])
                self.assertEqual("DENY", headers["X-Frame-Options"])
                self.assertNotIn("Access-Control-Allow-Origin", headers)
                for path in ("/%2e%2e/secret.txt", "/outside", "/folder/", "/api/oepl/tags", "/%00"):
                    with self.subTest(path=path):
                        status, body, _ = request(server, path)
                        self.assertIn(status, (400, 404))
                        self.assertNotIn(b"secret", body)
                self.assertEqual(200, request(server, "/", method="HEAD")[0])

    def test_host_and_origin_guard_blocks_before_client_creation(self):
        made = []
        with studio_server(client_factory=lambda ap: made.append(ap)) as server:
            for headers in ({"Host": "evil.example"}, {"Host": "localhost:1"},
                            {"Origin": "http://evil.example"}, {"Origin": "null"}, {"Origin": None},
                            {"Sec-Fetch-Site": "cross-site"}):
                with self.subTest(headers=headers):
                    status, body, _ = request(server, "/api/oepl/tags", {"ap": "http://ap.local"}, headers=headers)
                    self.assertEqual(403, status)
                    self.assertIn("error", json.loads(body))
            self.assertEqual([], made)

    def test_tags_status_upload_round_trip_with_only_loopback_mock_ap(self):
        with ap_server(normal_route) as (url, calls), studio_server() as server:
            status, body, _ = request(server, "/api/oepl/tags", {"ap": url})
            self.assertEqual(200, status)
            self.assertEqual(MAC, json.loads(body)["tags"][0]["mac"])
            status, body, _ = request(server, "/api/oepl/status", {"ap": url, "mac": MAC})
            self.assertEqual(200, status)
            self.assertEqual(MAC, json.loads(body)["tag"]["mac"])
            status, body, _ = request(server, "/api/oepl/upload", {"ap": url, "mac": MAC,
                                     "image": base64.b64encode(jpeg()).decode(), "dither": 0})
            self.assertEqual(200, status)
            self.assertFalse(json.loads(body)["displayConfirmed"])
            self.assertEqual("submitted", json.loads(body)["status"])
            self.assertEqual(1, sum(call[0] == "POST" for call in calls))

    def test_error_status_and_unknown_mac_propagate(self):
        with ap_server(lambda *_: (200, {"tags": []}, {})) as (url, _), studio_server() as server:
            status, body, _ = request(server, "/api/oepl/status", {"ap": url, "mac": MAC})
            self.assertEqual(404, status)
            self.assertIn("not registered", json.loads(body)["error"])
        with ap_server(lambda *_: (302, b"", {"Location": "http://example.invalid"})) as (url, _), studio_server() as server:
            status, body, _ = request(server, "/api/oepl/tags", {"ap": url})
            self.assertEqual(502, status)
            self.assertIn("redirect", json.loads(body)["error"])

    def test_bad_json_schema_and_mime_are_rejected_without_ap_request(self):
        made = []
        with studio_server(client_factory=lambda ap: made.append(ap)) as server:
            for payload, headers, expected in ((b"{}", {}, 400), (b"[]", {}, 400), (b"{", {}, 400),
                                              (b'{"ap":"a","ap":"b"}', {}, 400),
                                              ({"ap": "a", "extra": 1}, {}, 400),
                                              ({"ap": "a"}, {"Content-Type": "text/plain"}, 415),
                                              ({"ap": "a"}, {"Content-Encoding": "gzip"}, 415),
                                              ({"ap": "a"}, {"Transfer-Encoding": "chunked"}, 400),
                                              ({"ap": "a"}, {"Content-Length": str(studio.MAX_BODY_BYTES + 1)}, 413)):
                with self.subTest(payload=payload, headers=headers):
                    status, body, _ = request(server, "/api/oepl/tags", payload, headers=headers)
                    self.assertEqual(expected, status)
                    self.assertIn("error", json.loads(body))
            self.assertEqual([], made)

    def test_invalid_base64_partial_jpeg_and_dimensions_do_not_upload(self):
        with ap_server(normal_route) as (url, calls), studio_server() as server:
            for image in ("!notBase64!", "data:image/jpeg;base64,AAA=", "", base64.b64encode(jpeg()[:-2]).decode(),
                          base64.b64encode(jpeg(2, 1)).decode()):
                status, _, _ = request(server, "/api/oepl/upload", {"ap": url, "mac": MAC, "image": image, "dither": 0})
                self.assertEqual(400, status)
            self.assertTrue(all(call[0] == "GET" for call in calls))

    def test_unknown_endpoint_and_preflight_have_no_cross_origin_grant(self):
        with studio_server() as server:
            for path, method, expected in (("/api/oepl/reboot", "POST", 404), ("/api/oepl/tags", "OPTIONS", 405)):
                status, _, headers = request(server, path, b"{}", method=method)
                self.assertEqual(expected, status)
                self.assertNotIn("Access-Control-Allow-Origin", headers)

    def test_duplicate_host_is_rejected(self):
        with studio_server() as server:
            with socket.create_connection(("127.0.0.1", server.server_port), timeout=2) as connection:
                connection.sendall(f"GET / HTTP/1.1\r\nHost: localhost:{server.server_port}\r\nHost: evil.example\r\n\r\n".encode())
                self.assertIn(b"403", connection.recv(4096).split(b"\r\n")[0])

    def test_partial_request_body_fails_without_ap_call(self):
        made = []
        with studio_server(client_factory=lambda ap: made.append(ap)) as server:
            with socket.create_connection(("127.0.0.1", server.server_port), timeout=2) as connection:
                host = f"127.0.0.1:{server.server_port}"
                connection.sendall((f"POST /api/oepl/tags HTTP/1.1\r\nHost: {host}\r\nOrigin: http://{host}\r\n"
                                    "Content-Type: application/json\r\nContent-Length: 100\r\n\r\n{}").encode())
                connection.shutdown(socket.SHUT_WR)
                self.assertIn(b"400", connection.recv(4096).split(b"\r\n")[0])
            self.assertEqual([], made)


if __name__ == "__main__":
    unittest.main()
