"""
cpptlm/visualization/dashboard_server.py — Unified Dashboard HTTP server.

URL routes:
  GET /                              → home (runs list)
  GET /?run=<id>                     → per-run view
  GET /api/runs                      → list all runs (JSON)
  GET /api/runs/<id>                 → run metadata (JSON)
  GET /api/runs/<id>/stats?offset=N  → incremental stats JSONL
  GET /api/runs/<id>/config          → config.json content
  POST /api/runs/<id>/config         → save config.json
  POST /api/runs/<id>/rerun          → rerun simulation
  GET /runs/<id>/<filename>          → static files
"""

from __future__ import annotations

import http.server
import json
import os
import subprocess
import threading
import urllib.parse
from pathlib import Path
from typing import List, Optional

from cpptlm.visualization.dashboard_ui import (
    _DASHBOARD_HTML,
    _HOME_HTML,
    make_run_view_html,
)
from cpptlm.visualization.run_context import RunContext, RunsIndex


class DashboardServer:

    def __init__(self, runs_dir: str = "runs", port: int = 8050):
        self.runs_dir = Path(runs_dir)
        self.port = port
        self._index = RunsIndex(self.runs_dir)
        self._open_run: Optional[str] = None
        self._server = None
        self._thread: Optional[threading.Thread] = None

    def set_open_run(self, run_id: str) -> None:
        self._open_run = run_id

    def start(self) -> None:
        """启动 dashboard server（非阻塞，daemon 线程）."""
        if self._server:
            return
        handler = lambda *args, **kwargs: _DashboardRequestHandler(
            *args, runs_index=self._index, open_run=self._open_run, **kwargs
        )
        self._server = http.server.HTTPServer(("0.0.0.0", self.port), handler)
        self._server._server_ref = self
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()

    def serve_forever(self) -> None:
        """启动 dashboard server（阻塞模式，供 CLI 使用）."""
        self.start()
        print(f"  [Dashboard] http://localhost:{self.port}")
        try:
            self._thread.join()
        except KeyboardInterrupt:
            self.stop()

    def stop(self) -> None:
        if self._server:
            self._server.shutdown()
            self._server = None

    @property
    def url(self) -> str:
        return f"http://localhost:{self.port}"


class _DashboardRequestHandler(http.server.BaseHTTPRequestHandler):

    def __init__(self, *args, runs_index: RunsIndex, open_run: Optional[str] = None, **kwargs):
        self._index = runs_index
        self._open_run = open_run
        super().__init__(*args, **kwargs)

    def log_message(self, fmt, *args):
        pass

    def _send_json(self, data: dict, status: int = 200) -> None:
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def _send_html(self, html: str, status: int = 200) -> None:
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(html.encode())

    def _send_file(self, file_path: Path) -> None:
        if not file_path.exists():
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"File not found")
            return
        mime_types = {
            ".png": "image/png",
            ".html": "text/html",
            ".json": "application/json",
            ".txt": "text/plain",
        }
        ext = file_path.suffix.lower()
        mime = mime_types.get(ext, "application/octet-stream")
        self.send_response(200)
        self.send_header("Content-Type", mime)
        self.end_headers()
        self.wfile.write(file_path.read_bytes())

    def do_GET(self):
        path = urllib.parse.unquote(self.path)
        parsed = urllib.parse.urlparse(path)
        qs = urllib.parse.parse_qs(parsed.query)

        if path == "/" or path.startswith("/?"):
            run_id = qs.get("run", [None])[0]
            if run_id:
                run = self._index.get_run(run_id)
                if run is None:
                    self._send_json({"error": f"Run not found: {run_id}"}, 404)
                    return
                is_active = run.is_active()
                self._send_html(make_run_view_html(run_id, is_active))
            else:
                self._send_html(_HOME_HTML)

        elif path.startswith("/api/runs"):
            self._handle_api_runs(path)

        elif path == "/api/schema":
            schema_path = Path(__file__).parent.parent.parent / "cpptlm_config/schema.json"
            if schema_path.exists():
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(schema_path.read_bytes())
            else:
                self.send_response(404)
                self.end_headers()

        elif path.startswith("/runs/"):
            self._handle_static_file(path)

        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        path = urllib.parse.unquote(self.path)
        if path.startswith("/api/runs/"):
            self._handle_post_runs(path)
        else:
            self.send_response(404)
            self.end_headers()

    def _handle_api_runs(self, path: str) -> None:
        parts = path.split("/")
        if len(parts) == 3 and parts[1] == "api" and parts[2] == "runs":
            runs = self._index.list_runs()
            self._send_json([
                {
                    "run_id": r.run_id,
                    "is_active": r.is_active(),
                    "created_at": r.meta().get("created_at", ""),
                    "params": r.meta().get("params", {}),
                    "has_topology": r.topology_png() is not None,
                    "has_report": r.report() is not None,
                }
                for r in runs
            ])
            return

        if len(parts) >= 4 and parts[1] == "api" and parts[2] == "runs":
            run_id = parts[3]
            run = self._index.get_run(run_id)
            if run is None:
                self._send_json({"error": f"Run not found: {run_id}"}, 404)
                return

            if len(parts) == 4:
                self._send_json({
                    "run_id": run.run_id,
                    "is_active": run.is_active(),
                    "created_at": run.meta().get("created_at", ""),
                    "params": run.meta().get("params", {}),
                    "has_topology": run.topology_png() is not None,
                    "has_report": run.report() is not None,
                })
                return

            if len(parts) == 5:
                sub = parts[4]
                if sub == "stats":
                    offset = int(urllib.parse.parse_qs(urllib.parse.urlparse(path).query).get("offset", ["0"])[0])
                    records, new_offset = run.stats(offset)
                    self._send_json({"records": records, "offset": new_offset})
                elif sub == "config":
                    config_content = run.config()
                    self.send_response(200)
                    self.send_header("Content-Type", "text/plain; charset=utf-8")
                    self.end_headers()
                    self.wfile.write(json.dumps(config_content, indent=2).encode())
                else:
                    self.send_response(404)
                    self.end_headers()
            else:
                self.send_response(404)
                self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()

    def _handle_post_runs(self, path: str) -> None:
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length) if content_length > 0 else b""
        try:
            payload = json.loads(body.decode()) if body else {}
        except json.JSONDecodeError:
            payload = {}

        parts = path.split("/")
        if len(parts) < 5:
            self._send_json({"error": "Invalid path"}, 400)
            return

        run_id = parts[3]
        action = parts[4] if len(parts) > 4 else ""
        run = self._index.get_run(run_id)
        if run is None:
            self._send_json({"error": f"Run not found: {run_id}"}, 404)
            return

        if action == "config":
            new_config = payload.get("config", "")
            try:
                json.loads(new_config)
            except json.JSONDecodeError:
                self._send_json({"error": "Invalid JSON in config"}, 400)
                return
            config_file = run.root / "config.json"
            config_file.write_text(new_config, encoding="utf-8")
            self._send_json({"status": "saved"})
            return

        if action == "rerun":
            if run.is_active():
                self._send_json({"error": "Simulation already running"}, 409)
                return

            meta = run.meta()
            params = meta.get("params", {})
            cycles = payload.get("cycles", params.get("cycles", 50000))
            binary_path = params.get("binary_path", "")

            if not binary_path:
                self._send_json({"error": "No binary_path in run metadata"}, 400)
                return

            binary = Path(binary_path)
            if not binary.exists() or not os.access(binary, os.X_OK):
                self._send_json({"error": f"binary_path is not a valid executable: {binary_path}"}, 400)
                return

            stats_file = run.root / "stats.jsonl"
            if stats_file.exists():
                stats_file.write_bytes(b"")

            pid_file = run.root / "pid"
            cmd = [str(binary), "--cycles", str(cycles)]
            proc = subprocess.Popen(cmd, cwd=str(run.root))
            pid_file.write_text(str(proc.pid), encoding="utf-8")

            meta["rerun_count"] = meta.get("rerun_count", 0) + 1
            meta["last_run"] = __import__("datetime").datetime.now().isoformat()
            (run.root / "meta.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")

            self._send_json({
                "status": "started",
                "run_id": run_id,
                "pid": proc.pid,
            })
            return

        self._send_json({"error": "Unknown action"}, 404)

    def _handle_static_file(self, path: str) -> None:
        parts = path.split("/")
        if len(parts) < 4:
            self.send_response(404)
            self.end_headers()
            return
        run_id = parts[2]
        filename = "/".join(parts[3:])
        # 安全检查：禁止路径遍历
        if ".." in filename:
            self.send_response(403)
            self.end_headers()
            return
        run = self._index.get_run(run_id)
        if run is None:
            self.send_response(404)
            self.end_headers()
            return
        file_path = run.root / filename
        # resolve() 后必须在 run.root 下，防止 .. 逃逸
        try:
            resolved = file_path.resolve()
            if not str(resolved).startswith(str(run.root.resolve())):
                self.send_response(403)
                self.end_headers()
                return
        except (OSError, RuntimeError):
            self.send_response(400)
            self.end_headers()
            return
        self._send_file(file_path)

    def handle_one_request(self):
        try:
            super().handle_one_request()
        except (ConnectionError, BrokenPipeError, OSError):
            pass
