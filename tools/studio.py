#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Serve the etag editor and an explicit, loopback-only OpenEPaperLink bridge."""

from __future__ import annotations

import argparse
import base64
import binascii
import json
import mimetypes
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import threading
import time
from urllib.parse import unquote, urlsplit

from oepl import Client, MAX_IMAGE_BYTES, OeplError, strict_json


WEB_ROOT = Path(__file__).resolve().parents[1] / "web"
MAX_BODY_BYTES = 3 * 1024 * 1024
MAX_STATIC_BYTES = 16 * 1024 * 1024
READ_TIMEOUT = 8.0


class StudioServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address=("127.0.0.1", 8000), *, web_root=WEB_ROOT, client_factory=Client):
        if address[0] not in ("127.0.0.1", "localhost"):
            raise ValueError("Studio binds only to 127.0.0.1; remote browser access is unsupported.")
        self.web_root = Path(web_root).resolve()
        self.client_factory = client_factory
        self._slots = threading.BoundedSemaphore(8)
        super().__init__(("127.0.0.1", address[1]), StudioHandler)

    def process_request(self, request, client_address):
        if not self._slots.acquire(blocking=False):
            request.close()
            return
        try:
            super().process_request(request, client_address)
        except Exception:
            self._slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self._slots.release()


class StudioHandler(BaseHTTPRequestHandler):
    server: StudioServer
    server_version = "etag-studio"

    def setup(self):
        super().setup()
        self.connection.settimeout(READ_TIMEOUT)

    def log_message(self, _format, *args):
        # Do not log local unit identifiers, AP addresses, or image payloads.
        pass

    def _reply(self, status: int, data: bytes, content_type: str, *, head=False):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Frame-Options", "DENY")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True
        if not head:
            self.wfile.write(data)

    def _json(self, status: int, result: dict):
        self._reply(status, json.dumps(result, ensure_ascii=False, allow_nan=False).encode("utf-8"), "application/json; charset=utf-8")

    def _trusted_request(self, *, require_origin=False):
        hosts = self.headers.get_all("Host", [])
        port = self.server.server_port
        if len(hosts) != 1 or hosts[0].lower() not in (f"127.0.0.1:{port}", f"localhost:{port}"):
            raise OeplError("Studio requires its local Host address and port.", 403)
        origins = self.headers.get_all("Origin", [])
        if len(origins) > 1 or (require_origin and not origins):
            raise OeplError("Studio API requests require the editor's same-origin Origin header.", 403)
        if origins and origins[0] != "http://" + hosts[0].lower():
            raise OeplError("Cross-origin Studio requests are not allowed.", 403)
        site = self.headers.get("Sec-Fetch-Site")
        if site is not None and site not in ("same-origin", "none"):
            raise OeplError("Cross-site Studio requests are not allowed.", 403)

    def do_GET(self):
        self._static()

    def do_HEAD(self):
        self._static(head=True)

    def _static(self, *, head=False):
        try:
            self._trusted_request()
            url = urlsplit(self.path)
            if url.scheme or url.netloc or url.path.startswith("/api/"):
                raise OeplError("No such resource.", 404)
            decoded = unquote(url.path, errors="strict")
            if "\x00" in decoded or "\\" in decoded:
                raise OeplError("Invalid resource path.", 400)
            relative = decoded.lstrip("/") or "index.html"
            path = (self.server.web_root / relative).resolve()
            if not path.is_relative_to(self.server.web_root) or not path.is_file():
                raise OeplError("No such resource.", 404)
            with path.open("rb") as source:
                data = source.read(MAX_STATIC_BYTES + 1)
            if len(data) > MAX_STATIC_BYTES:
                raise OeplError("Static resource exceeds the size limit.", 413)
            mime = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
            self._reply(200, data, mime, head=head)
        except (OeplError, OSError, UnicodeError, ValueError) as exc:
            self._json(exc.status if isinstance(exc, OeplError) else 400, {"error": str(exc) if isinstance(exc, OeplError) else "Cannot read that resource."})

    def _body(self) -> dict:
        if self.headers.get("Transfer-Encoding") is not None:
            raise OeplError("Chunked Studio requests are unsupported.", 400)
        lengths = self.headers.get_all("Content-Length", [])
        if len(lengths) != 1 or not lengths[0].isascii() or not lengths[0].isdigit():
            raise OeplError("A single Content-Length is required.", 411)
        if len(lengths[0]) > 8:
            raise OeplError("Studio request exceeds the size limit.", 413)
        length = int(lengths[0])
        if not 0 < length <= MAX_BODY_BYTES:
            raise OeplError("Studio JSON request must be nonempty and at most 3 MiB.", 413)
        types = self.headers.get_all("Content-Type", [])
        if len(types) != 1 or types[0].split(";", 1)[0].strip().lower() != "application/json":
            raise OeplError("Studio API requires application/json.", 415)
        if self.headers.get("Content-Encoding") is not None:
            raise OeplError("Compressed Studio requests are unsupported.", 415)
        deadline = time.monotonic() + READ_TIMEOUT
        raw = bytearray()
        while len(raw) < length:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError()
            self.connection.settimeout(remaining)
            chunk = self.rfile.read1(min(65536, length - len(raw)))
            if not chunk:
                break
            raw.extend(chunk)
        if len(raw) != length:
            raise OeplError("Studio request body is incomplete.", 400)
        try:
            return strict_json(raw)
        except OeplError as exc:
            raise OeplError("Studio request must be one valid JSON object with unique fields.", 400) from exc

    def do_POST(self):
        try:
            self._trusted_request(require_origin=True)
            schemas = {"/api/oepl/tags": {"ap"}, "/api/oepl/status": {"ap", "mac"},
                       "/api/oepl/upload": {"ap", "mac", "image", "dither"}}
            if self.path not in schemas:
                raise OeplError("No such API endpoint.", 404)
            data = self._body()
            if set(data) != schemas[self.path]:
                raise OeplError("Studio request fields do not match this endpoint.", 400)
            client = self.server.client_factory(data["ap"])
            if self.path.endswith("/tags"):
                result = client.tags()
            elif self.path.endswith("/status"):
                result = client.status(data["mac"])
            else:
                encoded = data["image"]
                if not isinstance(encoded, str) or len(encoded) > 4 * ((MAX_IMAGE_BYTES + 2) // 3):
                    raise OeplError("Image must be base64 JPEG data of at most 2 MiB.", 400)
                try:
                    image = base64.b64decode(encoded, validate=True)
                except (binascii.Error, ValueError) as exc:
                    raise OeplError("Image is not valid base64 JPEG data.", 400) from exc
                result = client.upload(data["mac"], image, data["dither"])
            self._json(200, result)
        except OeplError as exc:
            self._json(exc.status, {"error": str(exc)})
        except TimeoutError:
            self._json(408, {"error": "Studio request body timed out."})
        except (OSError, ValueError):
            self._json(400, {"error": "Cannot complete that Studio request."})

    def do_OPTIONS(self):
        self._json(405, {"error": "Cross-origin access is unsupported; open the local editor."})


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args(argv)
    if not 1 <= args.port <= 65535:
        parser.error("port must be within 1..65535")
    try:
        with StudioServer(("127.0.0.1", args.port)) as server:
            print(f"etag Studio: http://localhost:{server.server_port} (loopback only)", flush=True)
            server.serve_forever()
    except KeyboardInterrupt:
        return 0
    except OSError as exc:
        parser.exit(1, f"error: cannot start Studio: {exc}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
