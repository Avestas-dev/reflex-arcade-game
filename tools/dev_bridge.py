#!/usr/bin/env python3
"""Run the Reflex Arcade page from a computer, talking to the Mega over USB.

Does the same job as the ESP8266 (serves the page and relays WebSocket
messages), so you can test the game and wiring without Wi-Fi.

    pip install pyserial websockets
    python3 tools/dev_bridge.py --port /dev/cu.usbserial-2120

Then open http://localhost:8090  (add --lan to allow phones on your network).
"""
import argparse
import asyncio
import http.server
import pathlib
import re
import threading

import serial
import websockets

PAGE = pathlib.Path(__file__).resolve().parent.parent / "reflex-arcade-esp" / "web_page.h"


def load_page() -> bytes:
    text = PAGE.read_text(encoding="utf-8")
    match = re.search(r'R"rawliteral\((.*)\)rawliteral"', text, re.S)
    if not match:
        raise SystemExit(f"Could not find the page in {PAGE}")
    return match.group(1).encode("utf-8")


class PageHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = load_page()  # re-read each time so page edits show up on reload
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


async def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True, help="Mega serial port, e.g. /dev/cu.usbserial-2120")
    ap.add_argument("--http", type=int, default=8090, help="page port (WebSocket uses this + 1)")
    ap.add_argument("--lan", action="store_true", help="listen on all interfaces, not just localhost")
    ap.add_argument("-v", "--verbose", action="store_true", help="print every message")
    args = ap.parse_args()

    host = "0.0.0.0" if args.lan else "127.0.0.1"
    ser = serial.Serial(args.port, 115200, timeout=0.1)
    loop = asyncio.get_running_loop()
    lines: asyncio.Queue[str] = asyncio.Queue()
    clients = set()
    last_state = None
    last_pins = None
    write_lock = threading.Lock()

    def to_mega(line: str) -> None:
        if args.verbose:
            print(">>", line)
        with write_lock:
            ser.write((line + "\n").encode())

    def read_serial() -> None:
        buf = b""
        while True:
            buf += ser.read(512)
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.strip().decode("utf-8", "ignore")
                if line.startswith("{"):
                    loop.call_soon_threadsafe(lines.put_nowait, line)

    async def relay() -> None:
        nonlocal last_state, last_pins
        while True:
            line = await lines.get()
            if line.startswith('{"t":"state"'):
                last_state = line
            elif line.startswith('{"t":"pins"'):
                last_pins = line
            elif args.verbose:
                print("<<", line)
            websockets.broadcast(clients, line)

    async def on_client(ws) -> None:
        clients.add(ws)
        try:
            if last_pins:
                await ws.send(last_pins)
            if last_state:
                await ws.send(last_state)
            to_mega("STATE?")
            async for msg in ws:
                if msg == "ping":
                    await ws.send("pong")
                    continue
                cmd = "".join(c for c in str(msg)[:80] if 32 <= ord(c) < 127)
                if cmd:
                    to_mega(cmd)
        except websockets.ConnectionClosed:
            pass
        finally:
            clients.discard(ws)

    threading.Thread(target=read_serial, daemon=True).start()
    httpd = http.server.ThreadingHTTPServer((host, args.http), PageHandler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()

    async with websockets.serve(on_client, host, args.http + 1, ping_interval=10, ping_timeout=8):
        print(f"Reflex Arcade: http://localhost:{args.http}  (Mega on {args.port}, Ctrl+C to stop)")
        await relay()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
