"""Huobz AI Coder — minimal working entrypoint."""
from __future__ import annotations

import json
from http.server import BaseHTTPRequestHandler, HTTPServer


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):  # noqa: N802
        body = {
            "ok": True,
            "service": "huobz-ai-coder",
            "title": "Huobz AI Coder",
            "description": "Multi-model coding agent CLI that routes tasks across AI providers — developer tool, not Sophyane cloud OS.",
            "health": "pass",
        }
        data = json.dumps(body, indent=2).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt, *args):  # quiet
        return


def main() -> None:
    port = 8765
    print(f"Huobz AI Coder listening on http://127.0.0.1:{port}")
    HTTPServer(("127.0.0.1", port), Handler).serve_forever()


if __name__ == "__main__":
    main()
