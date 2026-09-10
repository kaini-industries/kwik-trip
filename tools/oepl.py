#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Independent, bounded HTTP client for an existing OpenEPaperLink access point.

This implements API interoperability, not the OEPL radio protocol or tag flashing.
No upstream source is included. See docs/reference/openepaperlink.md for provenance.
"""

from __future__ import annotations

import argparse
import http.client
import ipaddress
import json
import re
import secrets
import socket
import sys
import time
from pathlib import Path
from urllib.parse import urlsplit


MAX_IMAGE_BYTES = 2 * 1024 * 1024
MAX_RESPONSE_BYTES = 256 * 1024
MAX_DIMENSION = 2048
MAX_PIXELS = 2048 * 2048
MAX_TAGS = 256
REQUEST_TIMEOUT = 8.0
OPERATION_TIMEOUT = 30.0


class OeplError(Exception):
    """Expected input, transport, or AP contract failure with an HTTP-facing status."""

    def __init__(self, message: str, status: int = 502):
        super().__init__(message)
        self.status = status


def canonical_mac(value: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"(?:[0-9a-fA-F]{12}|[0-9a-fA-F]{16})", value):
        raise OeplError("MAC must contain exactly 12 or 16 hexadecimal digits, without separators.", 400)
    return value.upper().zfill(16)


def ap_url(value: str) -> str:
    """Accept an explicit HTTP origin only; never silently rewrite a destination."""
    if not isinstance(value, str) or len(value) > 512 or re.search(r"[\s\x00-\x1f\x7f\\]", value):
        raise OeplError("AP address must be an explicit http:// or https:// origin.", 400)
    try:
        parts = urlsplit(value)
        port = parts.port
        host = parts.hostname
        if (parts.scheme not in ("http", "https") or not host or
                parts.username is not None or parts.password is not None or
                parts.path not in ("", "/") or "?" in value or "#" in value or
                (port is not None and not 1 <= port <= 65535)):
            raise ValueError()
        if ":" in host:
            ipaddress.IPv6Address(host)  # No zone identifiers or arbitrary URL syntax.
            if "%" in host:
                raise ValueError()
            formatted_host = f"[{host.lower()}]"
        else:
            if len(host) > 253 or any(not re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?", label)
                                      for label in host.split(".")):
                raise ValueError()
            formatted_host = host.lower()
        # A trailing colon is not a valid explicit port.
        if parts.netloc.endswith(":"):
            raise ValueError()
        suffix = f":{port}" if port is not None else ""
        return f"{parts.scheme}://{formatted_host}{suffix}"
    except ValueError as exc:
        raise OeplError("AP address must be http(s)://host[:port], without credentials, path, query, or fragment.", 400) from exc


def strict_json(data: bytes) -> dict:
    def pairs(values):
        result = {}
        for key, value in values:
            if key in result:
                raise ValueError("duplicate field")
            result[key] = value
        return result

    def invalid_constant(_):
        raise ValueError("non-finite number")

    try:
        result = json.loads(data.decode("utf-8"), object_pairs_hook=pairs, parse_constant=invalid_constant)
    except (UnicodeError, ValueError, RecursionError) as exc:
        raise OeplError("AP returned malformed JSON.") from exc
    if not isinstance(result, dict):
        raise OeplError("AP response must be a JSON object.")
    if "error" in result:
        raise OeplError("AP reported an error in its JSON response.")
    return result


def _integer(value, name: str, low: int, high: int):
    if value is None:
        return None
    if type(value) is not int or not low <= value <= high:
        raise OeplError(f"AP returned invalid {name}.")
    return value


def jpeg_dimensions(data: bytes) -> tuple[int, int]:
    """Check bounded baseline JPEG framing and dimensions, without decoding pixels.

    All segments and scan boundaries must be complete. Entropy coefficients are
    not decoded; the AP remains responsible for successful JPEG decompression.
    """
    if not isinstance(data, bytes) or not 4 <= len(data) <= MAX_IMAGE_BYTES:
        raise OeplError("JPEG must be nonempty and at most 2 MiB.", 400)
    if not data.startswith(b"\xff\xd8"):
        raise OeplError("Image must be a baseline JPEG.", 400)
    offset = 2
    dimensions = None
    components = {}
    scanned = set()
    saw_scan = False
    quantization = set()
    huffman = set()
    while offset < len(data):
        if data[offset] != 0xFF:
            raise OeplError("Malformed JPEG marker framing.", 400)
        while offset < len(data) and data[offset] == 0xFF:
            offset += 1
        if offset >= len(data):
            break
        marker = data[offset]
        offset += 1
        if marker == 0xD9:
            if (offset != len(data) or not saw_scan or dimensions is None or
                    scanned != set(components) or not quantization or not huffman):
                raise OeplError("JPEG is incomplete or has trailing data.", 400)
            return dimensions
        if marker in (0, 0xD8, 0x01) or 0xD0 <= marker <= 0xD7:
            raise OeplError("Unexpected JPEG marker.", 400)
        if offset + 2 > len(data):
            break
        length = int.from_bytes(data[offset:offset + 2], "big")
        if length < 2 or offset + length > len(data):
            raise OeplError("JPEG contains a truncated segment.", 400)
        segment = data[offset + 2:offset + length]
        offset += length
        if marker in (0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF):
            raise OeplError("Only baseline JPEG is supported; export without progressive encoding.", 400)
        if marker == 0xC0:
            if dimensions is not None or len(segment) < 6:
                raise OeplError("Invalid JPEG frame header.", 400)
            depth, height, width, count = segment[0], int.from_bytes(segment[1:3], "big"), int.from_bytes(segment[3:5], "big"), segment[5]
            if depth != 8 or count not in (1, 3) or len(segment) != 6 + 3 * count:
                raise OeplError("JPEG must have 8-bit grayscale or three-component color.", 400)
            if not 1 <= width <= MAX_DIMENSION or not 1 <= height <= MAX_DIMENSION or width * height > MAX_PIXELS:
                raise OeplError("JPEG dimensions exceed 2048 × 2048.", 400)
            components = {segment[i]: segment[i + 2] for i in range(6, len(segment), 3)}
            if len(components) != count:
                raise OeplError("Invalid JPEG component identifiers.", 400)
            for i in range(6, len(segment), 3):
                sampling = segment[i + 1]
                if not 1 <= sampling >> 4 <= 4 or not 1 <= sampling & 15 <= 4 or segment[i + 2] > 3:
                    raise OeplError("Invalid JPEG sampling or quantization selector.", 400)
            dimensions = (width, height)
        elif marker == 0xDB:
            if not segment:
                raise OeplError("Empty JPEG quantization table.", 400)
            cursor = 0
            while cursor < len(segment):
                selector = segment[cursor]
                if selector > 3 or cursor + 65 > len(segment) or 0 in segment[cursor + 1:cursor + 65]:
                    raise OeplError("Invalid baseline JPEG quantization table.", 400)
                quantization.add(selector)
                cursor += 65
        elif marker == 0xC4:
            if not segment:
                raise OeplError("Empty JPEG Huffman table.", 400)
            cursor = 0
            while cursor < len(segment):
                if cursor + 17 > len(segment):
                    raise OeplError("Truncated JPEG Huffman table.", 400)
                selector = segment[cursor]
                count = sum(segment[cursor + 1:cursor + 17])
                if selector >> 4 > 1 or selector & 15 > 3 or not 1 <= count <= 256 or cursor + 17 + count > len(segment):
                    raise OeplError("Invalid JPEG Huffman table.", 400)
                huffman.add(selector)
                cursor += 17 + count
        elif marker == 0xDA:
            if not components or len(segment) < 6:
                raise OeplError("Invalid JPEG scan header.", 400)
            count = segment[0]
            selected = set(segment[1:1 + 2 * count:2])
            if (not 1 <= count <= 3 or len(segment) != 4 + 2 * count or
                    len(selected) != count or not selected <= set(components) or selected & scanned or
                    segment[-3:] != b"\x00\x3f\x00"):
                raise OeplError("Unsupported or malformed JPEG scan.", 400)
            for i in range(1, 1 + 2 * count, 2):
                selector = segment[i + 1]
                if (components[segment[i]] not in quantization or selector >> 4 not in huffman or
                        (0x10 | (selector & 15)) not in huffman):
                    raise OeplError("JPEG scan references missing coding tables.", 400)
            scanned |= selected
            start = offset
            while offset < len(data):
                if data[offset] != 0xFF:
                    offset += 1
                    continue
                if offset + 1 >= len(data):
                    break
                next_marker = data[offset + 1]
                if next_marker == 0 or 0xD0 <= next_marker <= 0xD7:
                    offset += 2
                else:
                    break
            if offset == start:
                raise OeplError("JPEG scan has no image data.", 400)
            saw_scan = True
        elif marker == 0xDD:
            if len(segment) != 2:
                raise OeplError("Invalid JPEG restart interval.", 400)
        elif not (0xE0 <= marker <= 0xEF or marker == 0xFE):
            raise OeplError("Unsupported JPEG marker.", 400)
    raise OeplError("JPEG is truncated or missing its end marker.", 400)


class Client:
    def __init__(self, ap: str, timeout: float = REQUEST_TIMEOUT):
        self.ap = ap_url(ap)
        if not 0 < timeout <= OPERATION_TIMEOUT:
            raise OeplError("Timeout must be between zero and 30 seconds.", 400)
        self.timeout = timeout

    def _request(self, path: str, deadline: float, body: bytes | None = None,
                 content_type: str | None = None) -> bytes:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise OeplError("AP operation timed out.")
        parts = urlsplit(self.ap)
        cls = http.client.HTTPSConnection if parts.scheme == "https" else http.client.HTTPConnection
        connection = cls(parts.hostname, parts.port, timeout=min(self.timeout, remaining))
        headers = {"Accept": "application/json" if body is None else "text/plain", "Accept-Encoding": "identity", "Connection": "close"}
        if content_type:
            headers["Content-Type"] = content_type
        try:
            connection.request("GET" if body is None else "POST", path, body=body, headers=headers)
            response_socket = connection.sock
            response = connection.getresponse()
            if 300 <= response.status < 400:
                raise OeplError("AP redirected the request; redirects are disabled. Use its direct address.")
            if response.status != 200:
                raise OeplError(f"AP returned HTTP {response.status}.")
            if response.getheader("Content-Encoding", "identity").lower() != "identity":
                raise OeplError("AP returned unsupported compressed HTTP content.")
            advertised = response.getheader("Content-Length")
            transfer = response.getheader("Transfer-Encoding")
            if transfer is not None and (transfer.strip().lower() != "chunked" or advertised is not None):
                raise OeplError("AP returned unsupported or ambiguous HTTP transfer framing.")
            if advertised is not None:
                if not advertised.isascii() or not advertised.isdigit() or len(advertised) > 8 or int(advertised) > MAX_RESPONSE_BYTES:
                    raise OeplError("AP response exceeds the size limit or has invalid length.")
            chunks = bytearray()
            while True:
                if response.isclosed():
                    break
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise OeplError("AP operation timed out.")
                if response_socket is not None:
                    response_socket.settimeout(min(self.timeout, remaining))
                chunk = response.read1(min(16384, MAX_RESPONSE_BYTES + 1 - len(chunks)))
                if not chunk:
                    break
                chunks.extend(chunk)
                if len(chunks) > MAX_RESPONSE_BYTES:
                    raise OeplError("AP response exceeds the size limit.")
            if advertised is not None and len(chunks) != int(advertised):
                raise OeplError("AP returned an incomplete response.")
            return bytes(chunks)
        except (TimeoutError, socket.timeout) as exc:
            raise OeplError("AP request timed out; image submission, if attempted, has an unknown outcome.") from exc
        except (OSError, http.client.HTTPException) as exc:
            raise OeplError("Cannot complete the AP request; check its address and network connection. A started upload may have an unknown outcome.") from exc
        finally:
            connection.close()

    def _json(self, path: str, deadline: float) -> dict:
        return strict_json(self._request(path, deadline))

    def _tag_rows(self, page: dict) -> list:
        rows = page.get("tags")
        if not isinstance(rows, list) or len(rows) > MAX_TAGS:
            raise OeplError("AP response must contain a bounded tags array.")
        for row in rows:
            if not isinstance(row, dict):
                raise OeplError("AP returned an invalid tag record.")
        return rows

    def _normalize(self, rows: list, deadline: float) -> tuple[list, list]:
        metadata = {}
        errors = []
        tags = []
        seen = set()
        for row in rows:
            try:
                mac = canonical_mac(row.get("mac"))
            except OeplError as exc:
                raise OeplError("AP returned an invalid tag MAC.") from exc
            if mac in seen:
                raise OeplError("AP returned duplicate tags; its database may have changed during pagination. Retry listing.")
            seen.add(mac)
            kind = _integer(row.get("hwType"), "hwType", 0, 255)
            alias = row.get("alias")
            if alias is not None and (not isinstance(alias, str) or len(alias) > 512):
                raise OeplError("AP returned an invalid alias.")
            if kind is not None and kind not in metadata:
                try:
                    meta = self._json(f"/tagtypes/{kind:02X}.json", deadline)
                    width = _integer(meta.get("width"), "display width", 1, MAX_DIMENSION)
                    height = _integer(meta.get("height"), "display height", 1, MAX_DIMENSION)
                    bpp = _integer(meta.get("bpp"), "display bpp", 1, 8)
                    if width is None or height is None or bpp is None:
                        raise OeplError("AP display metadata lacks width, height, or bpp.")
                    metadata[kind] = (width, height, bpp)
                except OeplError as exc:
                    metadata[kind] = (None, None, None)
                    errors.append(f"Hardware type {kind:02X}: {exc}")
            width, height, bpp = metadata.get(kind, (None, None, None))
            tags.append({"mac": mac, "alias": alias, "hwType": kind,
                         "width": width, "height": height, "bpp": bpp,
                         "batteryMv": _integer(row.get("batteryMv"), "batteryMv", 0, 65535),
                         "rssi": _integer(row.get("RSSI"), "RSSI", -128, 127),
                         "lastseen": _integer(row.get("lastseen"), "lastseen", 0, 0xFFFFFFFF),
                         "pending": _integer(row.get("pending"), "pending", 0, 65535)})
        return tags, errors

    def tags(self) -> dict:
        deadline = time.monotonic() + OPERATION_TIMEOUT
        position = 0
        rows = []
        while True:
            page = self._json("/get_db" + (f"?pos={position}" if position else ""), deadline)
            batch = self._tag_rows(page)
            rows.extend(batch)
            if len(rows) > MAX_TAGS:
                raise OeplError("AP tag list exceeds the supported 256-record bound.")
            if "continu" not in page:
                break
            continuation = page["continu"]
            # The pinned AP parses pos as uint8_t. Never wrap or silently truncate.
            if type(continuation) is not int or not position < continuation <= 255 or not batch:
                raise OeplError("AP returned invalid or looping pagination (continu must advance within 1..255).")
            position = continuation
        tags, errors = self._normalize(rows, deadline)
        result = {"ap": self.ap, "tags": tags}
        if errors:
            result["metadataError"] = "; ".join(errors)
        return result

    def _status(self, mac: str, deadline: float) -> dict:
        page = self._json(f"/get_db?mac={mac}", deadline)
        rows = self._tag_rows(page)
        if not rows:
            raise OeplError("Selected MAC is not registered on this access point.", 404)
        if len(rows) != 1 or "continu" in page:
            raise OeplError("AP returned an ambiguous selected-tag response.")
        tags, errors = self._normalize(rows, deadline)
        if tags[0]["mac"] != mac:
            raise OeplError("AP returned a different MAC than the selected tag.")
        result = {"ap": self.ap, "tag": tags[0]}
        if errors:
            result["metadataError"] = "; ".join(errors)
        return result

    def status(self, mac: str) -> dict:
        return self._status(canonical_mac(mac), time.monotonic() + OPERATION_TIMEOUT)

    def upload(self, mac: str, image: bytes, dither: int = 0) -> dict:
        mac = canonical_mac(mac)
        if type(dither) is not int or dither not in (0, 1):
            raise OeplError("Dither must be 0 or 1.", 400)
        dimensions = jpeg_dimensions(image)
        deadline = time.monotonic() + OPERATION_TIMEOUT
        tag = self._status(mac, deadline)["tag"]
        if tag["width"] is None or tag["height"] is None:
            raise OeplError("AP display dimensions are unavailable; image submission was not attempted.", 409)
        expected = (tag["width"], tag["height"])
        if dimensions != expected:
            raise OeplError(f"JPEG is {dimensions[0]} × {dimensions[1]}; selected AP tag expects {expected[0]} × {expected[1]}. Submission was not attempted.", 400)
        boundary = "etag-" + secrets.token_hex(24)
        # Scalar fields precede the file: the AP reads them when upload begins.
        fields = b"".join(f'--{boundary}\r\nContent-Disposition: form-data; name="{key}"\r\n\r\n{value}\r\n'.encode("ascii")
                          for key, value in (("mac", mac), ("dither", dither)))
        body = (fields + f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="image.jpg"\r\nContent-Type: image/jpeg\r\n\r\n'.encode("ascii")
                + image + f"\r\n--{boundary}--\r\n".encode("ascii"))
        response = self._request("/imgupload", deadline, body, f"multipart/form-data; boundary={boundary}")
        if response.strip() != b"Ok, saved":
            raise OeplError("AP did not return its expected upload acknowledgement; submission outcome is unknown. Check the AP before retrying.")
        return {"ap": self.ap, "mac": mac, "status": "submitted", "displayConfirmed": False,
                "message": "Image submitted to the AP. Its response is not display confirmation; check the tag after its next check-in."}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("tags", "status", "upload"):
        command = sub.add_parser(name)
        command.add_argument("--ap", required=True, help="Explicit AP origin, e.g. http://192.168.1.50")
        if name != "tags":
            command.add_argument("--mac", required=True, help="12 or 16 hex digits")
        if name == "upload":
            command.add_argument("image", type=Path, help="Baseline JPEG at the AP-reported tag dimensions")
            command.add_argument("--dither", type=int, choices=(0, 1), default=0)
    args = parser.parse_args(argv)
    try:
        client = Client(args.ap)
        if args.command == "tags":
            result = client.tags()
        elif args.command == "status":
            result = client.status(args.mac)
        else:
            with args.image.open("rb") as source:
                data = source.read(MAX_IMAGE_BYTES + 1)
            result = client.upload(args.mac, data, args.dither)
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return 0
    except (OeplError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
