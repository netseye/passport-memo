#!/usr/bin/env python3
"""Serve the real portal with synthetic, read-only fixtures on localhost."""

from http.server import BaseHTTPRequestHandler, HTTPServer
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = {
    "online": False,
    "clock_ready": False,
    "configured": True,
    "connection_message": "配置热点已开启，退出后连接路由器",
    "ssid": "Demo Wi-Fi",
    "resource": "volc.bigasr.sauc.duration",
    "ee04_host": "",
    "app_id": "",
    "brightness": 75,
    "sounds": True,
    "has_api_key": False,
    "has_access_key": True,
    "portal": True,
}
NOTES = [
    {
        "id": 1,
        "text": "今天的灵感：做一个不用打开手机，也能随时记录想法的小伙伴。",
        "created": 1789088400,
        "done": False,
        "partial": False,
    }
]


class Preview(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            data = (ROOT / "main/memo_portal.html").read_bytes()
            kind = "text/html; charset=utf-8"
        elif self.path in ("/api/state", "/api/notes"):
            if self.headers.get("X-Memo-Key") != "12345678":
                self.send_error(401, "Use the demonstration code 12345678")
                return
            value = STATE if self.path == "/api/state" else NOTES
            data = json.dumps(value, ensure_ascii=False).encode("utf-8")
            kind = "application/json; charset=utf-8"
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", kind)
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        self.send_error(405, "Documentation preview is read-only")

    def log_message(self, *_args):
        pass


if __name__ == "__main__":
    print("Demo portal: http://127.0.0.1:8766 — code 12345678", flush=True)
    HTTPServer(("127.0.0.1", 8766), Preview).serve_forever()
