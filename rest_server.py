#!/usr/bin/env python3
"""Simple local REST server with a live value dashboard.

This module starts an HTTP server on port 8000 and exposes a minimal
JSON API for reading and updating a fixed set of values. A lightweight
Tkinter GUI runs alongside the server and displays the current values
in real time.
"""

import json
import random
import re
import threading
import time
import queue
import tkinter as tk
from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn

# Server port and value constraints
PORT = 8000
PARAM_COUNT = 4
VALUE_MIN = 0
VALUE_MAX = 100
PATH_PATTERN = re.compile(r"^/parameters/([0-3])$")

# Request queue and delay simulation
REQUEST_QUEUE_MAX = 10
DELAY_MEAN_MS = 200
DELAY_STD_MS = 200
DELAY_MIN_MS = 0
DELAY_MAX_MS = 5000


def generate_delay_seconds():
    """Generate a simulated network delay in seconds."""
    delay_ms = random.gauss(DELAY_MEAN_MS, DELAY_STD_MS)
    delay_ms = max(DELAY_MIN_MS, min(DELAY_MAX_MS, delay_ms))
    return delay_ms / 1000.0


class PendingRequest:
    """Container for a queued request waiting to be processed."""

    def __init__(self, handler, method, index, value=None):
        self.handler = handler
        self.method = method
        self.index = index
        self.value = value
        self.response_data = None
        self.status = 200
        self.event = threading.Event()

class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    """HTTP server that handles requests in separate threads."""
    daemon_threads = True

class ParameterRequestHandler(BaseHTTPRequestHandler):
    """Request handler for the local JSON API.

    The handler supports GET requests to retrieve a value and PUT requests
    to update a value. All responses are sent as JSON.
    """
    server_version = "RestServer/1.0"
    sys_version = ""

    def _send_json(self, data, status=200):
        """Write a JSON response with the given HTTP status."""
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _parse_index(self):
        """Extract the parameter index from the request path."""
        match = PATH_PATTERN.match(self.path)
        if not match:
            return None
        try:
            index = int(match.group(1))
        except ValueError:
            return None
        if 0 <= index < PARAM_COUNT:
            return index
        return None

    def do_GET(self):
        """Handle a GET request by enqueueing it and waiting for processing."""
        index = self._parse_index()
        if index is None:
            self.send_error(404, "Not Found")
            return

        request = PendingRequest(self, "GET", index)
        try:
            self.server.request_queue.put_nowait(request)
        except queue.Full:
            # Queue is full; discard the request and do not respond.
            threading.Event().wait()
            return

        request.event.wait()
        self._send_json(request.response_data, status=request.status)

    def do_PUT(self):
        """Handle a PUT request by enqueueing it and waiting for processing."""
        index = self._parse_index()
        if index is None:
            self.send_error(404, "Not Found")
            return

        content_length = self.headers.get("Content-Length")
        if content_length is None:
            self.send_error(411, "Content-Length Required")
            return

        try:
            raw_body = self.rfile.read(int(content_length))
            payload = json.loads(raw_body.decode("utf-8"))
        except (ValueError, json.JSONDecodeError):
            self.send_error(400, "Malformed JSON")
            return

        if not isinstance(payload, dict):
            self.send_error(400, "JSON body must be an object")
            return

        value = payload.get("value")
        if not isinstance(value, int):
            self.send_error(400, "Value must be an integer")
            return

        if value < VALUE_MIN or value > VALUE_MAX:
            self.send_error(400, f"Value must be between {VALUE_MIN} and {VALUE_MAX}")
            return

        request = PendingRequest(self, "PUT", index, value=value)
        try:
            self.server.request_queue.put_nowait(request)
        except queue.Full:
            # Queue is full; discard the request and do not respond.
            threading.Event().wait()
            return

        request.event.wait()
        self._send_json(request.response_data, status=request.status)

    def log_message(self, format, *args):
        """Suppress default request logging to keep output minimal."""
        return


def request_worker(server):
    """Process queued requests one at a time, applying simulated network delay."""
    while True:
        request = server.request_queue.get()
        try:
            delay = generate_delay_seconds()
            time.sleep(delay)

            if request.method == "GET":
                with server.state_lock:
                    value = server.state[request.index]
                request.response_data = {"index": request.index, "value": value}
            else:
                with server.state_lock:
                    server.state[request.index] = request.value
                request.response_data = {"index": request.index, "value": request.value}

            request.status = 200
            request.event.set()
        finally:
            server.request_queue.task_done()


def build_gui(root, state, state_lock):
    """Create and configure the Tkinter dashboard window.

    The dashboard displays the current values in a simple horizontal layout
    and refreshes them periodically from the shared server state.
    """
    root.title(f"REST Parameter Server ({PORT})")
    root.geometry("640x180")
    root.resizable(False, False)

    title_frame = tk.Frame(root)
    title_frame.pack(fill="x", padx=12, pady=(12, 4))

    labels = []
    for i in range(PARAM_COUNT):
        label = tk.Label(title_frame, text=f"Parameter {i}", font=("Arial", 14, "bold"))
        label.grid(row=0, column=i, padx=14, sticky="n")
        labels.append(label)

    value_frame = tk.Frame(root)
    value_frame.pack(fill="x", padx=12, pady=(4, 12))

    value_labels = []
    for i in range(PARAM_COUNT):
        value_label = tk.Label(value_frame, text="0", font=("Arial", 36), width=4, anchor="center")
        value_label.grid(row=0, column=i, padx=14)
        value_labels.append(value_label)

    def refresh():
        with state_lock:
            values = list(state)
        for i, value_label in enumerate(value_labels):
            value_label.config(text=str(values[i]))
        root.after(100, refresh)

    refresh()
    return root


def run_server(server):
    """Run the HTTP server loop until shutdown is requested."""
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


def main():
    """Initialize server state, start the API server thread, and launch the GUI."""
    initial_state = [random.randint(VALUE_MIN, VALUE_MAX) for _ in range(PARAM_COUNT)]
    state_lock = threading.Lock()

    server = ThreadingHTTPServer(("", PORT), ParameterRequestHandler)
    server.state = initial_state
    server.state_lock = state_lock
    server.request_queue = queue.Queue(maxsize=REQUEST_QUEUE_MAX)

    worker_thread = threading.Thread(target=request_worker, args=(server,), daemon=True)
    worker_thread.start()

    server_thread = threading.Thread(target=run_server, args=(server,), daemon=True)
    server_thread.start()

    root = tk.Tk()
    build_gui(root, server.state, state_lock)

    def on_close():
        server.shutdown()
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()

if __name__ == "__main__":
    main()
