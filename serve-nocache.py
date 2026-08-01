#!/usr/bin/env python3
"""Serve the WASM build with no-cache headers.

Browsers cache geomTool.wasm/.js aggressively by filename; without no-cache
headers you can end up testing a stale build. Always pair this with an
incognito/private window.

  python serve-nocache.py [port]      # default 8080
  # then open http://localhost:8080/geomTool.html in an incognito window
"""
import http.server
import socketserver
import os
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build-wasm")


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=DIR, **k)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()


with socketserver.TCPServer(("", PORT), Handler) as httpd:
    print(f"serving {DIR} at http://localhost:{PORT}/geomTool.html (no-cache)")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")
