"""
cpptlm/visualization/app.py — FastAPI-based Dashboard HTTP server.

URL routes:
  GET /                              → home (runs list)
  GET /?run=<id>                     → per-run view
  GET /new                           → new run wizard page
  GET /editor                        → topology editor
  GET /api/runs                      → list all runs (JSON)
  GET /api/runs/<id>                 → run metadata (JSON)
  GET /api/runs/<id>/stats?offset=N  → incremental stats JSONL
  GET /api/runs/<id>/stream          → SSE real-time stream (NEW)
  GET /api/runs/<id>/config          → config.json content
  POST /api/runs/<id>/config         → save config.json
  POST /api/runs/<id>/rerun          → rerun simulation
  GET /runs/<id>/<filename>          → static files for a run
  GET /editor/assets/...             → editor assets
"""

from __future__ import annotations

import json
import os
import subprocess
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

from fastapi import FastAPI, HTTPException, Query, Response
from fastapi.responses import HTMLResponse, PlainTextResponse
from pydantic import BaseModel
from starlette.responses import FileResponse

from cpptlm.visualization.run_context import RunContext, RunsIndex
from cpptlm.visualization.simulation_runner import SimulationRunner


# =============================================================================
# Pydantic Models
# =============================================================================

class ConfigSaveRequest(BaseModel):
    config: str


class RerunRequest(BaseModel):
    cycles: Optional[int] = None


class CreateRunRequest(BaseModel):
    binary_path: str
    config_path: str
    cycles: int = 50000
    seed: int = 0
    interval: int = 1000


class RunMetadataResponse(BaseModel):
    run_id: str
    is_active: bool
    created_at: str
    params: Dict[str, Any]
    has_topology: bool
    has_report: bool


class RunListItemResponse(BaseModel):
    run_id: str
    is_active: bool
    created_at: str
    params: Dict[str, Any]
    has_topology: bool
    has_report: bool


class StatsResponse(BaseModel):
    records: List[Dict[str, Any]]
    offset: int


class ConfigResponse(BaseModel):
    status: str = "saved"


class RerunResponse(BaseModel):
    status: str
    run_id: str
    pid: int


class CreateRunResponse(BaseModel):
    run_id: str
    status: str


# =============================================================================
# FastAPI App
# =============================================================================

app = FastAPI(title="CppTLM Dashboard", version="1.0")

# Module-level state (shared across requests)
_runs_dir: Optional[Path] = None
_index: Optional[RunsIndex] = None


def get_index() -> RunsIndex:
    global _index
    if _index is None:
        raise HTTPException(500, "RunsIndex not initialized")
    return _index


def init_app(runs_dir: str = "runs", port: int = 8001) -> None:
    """Initialize the app with runs directory and port."""
    global _runs_dir, _index
    _runs_dir = Path(runs_dir)
    _index = RunsIndex(_runs_dir)


# Initialize on module load
init_app()


# =============================================================================
# Helper Functions
# =============================================================================

def _safe_read_json(path: Path) -> Dict[str, Any]:
    """Read JSON file safely, return empty dict if not found."""
    if path.exists():
        return json.loads(path.read_text(encoding="utf-8"))
    return {}


def _resolve_run_file(run: RunContext, filename: str) -> Path:
    """Resolve a file path within run directory with security checks."""
    file_path = run.root / filename
    # resolve() and security check
    resolved = file_path.resolve()
    run_root_resolved = run.root.resolve()
    if not str(resolved).startswith(str(run_root_resolved)):
        raise HTTPException(403, "Path traversal detected")
    return file_path


# =============================================================================
# HTML Pages
# =============================================================================

@app.get("/", response_class=HTMLResponse)
async def home(run: Optional[str] = Query(None)) -> str:
    """Home page - runs list or per-run view."""
    from cpptlm.visualization.dashboard_ui import _HOME_HTML

    if run:
        idx = get_index()
        run_ctx = idx.get_run(run)
        if run_ctx is None:
            raise HTTPException(404, f"Run not found: {run}")
        from cpptlm.visualization.dashboard_ui import make_run_view_html
        return make_run_view_html(run, run_ctx.is_active())
    return _HOME_HTML


@app.get("/new", response_class=HTMLResponse)
async def new_run_page() -> str:
    """New run wizard page."""
    static_path = Path(__file__).parent / "static" / "new_run.html"
    if static_path.exists():
        return static_path.read_text(encoding="utf-8")
    raise HTTPException(404, "new_run.html not found")


@app.get("/editor", response_class=HTMLResponse)
async def editor_page() -> str:
    """Topology editor page."""
    editor_path = Path(__file__).parent / "static" / "editor" / "index.html"
    if editor_path.exists():
        return editor_path.read_text(encoding="utf-8")
    raise HTTPException(404, "editor/index.html not found")


# =============================================================================
# API Endpoints - Runs
# =============================================================================

@app.get("/api/runs", response_model=List[RunListItemResponse])
async def list_runs() -> List[Dict[str, Any]]:
    """List all runs."""
    idx = get_index()
    runs = idx.list_runs()
    return [
        {
            "run_id": r.run_id,
            "is_active": r.is_active(),
            "created_at": r.meta().get("created_at", ""),
            "params": r.meta().get("params", {}),
            "has_topology": r.topology_png() is not None,
            "has_report": r.report() is not None,
        }
        for r in runs
    ]


@app.get("/api/runs/{run_id}", response_model=RunMetadataResponse)
async def get_run(run_id: str) -> Dict[str, Any]:
    """Get run metadata."""
    idx = get_index()
    run_ctx = idx.get_run(run_id)
    if run_ctx is None:
        raise HTTPException(404, f"Run not found: {run_id}")
    return {
        "run_id": run_ctx.run_id,
        "is_active": run_ctx.is_active(),
        "created_at": run_ctx.meta().get("created_at", ""),
        "params": run_ctx.meta().get("params", {}),
        "has_topology": run_ctx.topology_png() is not None,
        "has_report": run_ctx.report() is not None,
    }


@app.get("/api/runs/{run_id}/stats", response_model=StatsResponse)
async def get_run_stats(run_id: str, offset: int = Query(0)) -> Dict[str, Any]:
    """Get incremental stats from stats.jsonl."""
    idx = get_index()
    run_ctx = idx.get_run(run_id)
    if run_ctx is None:
        raise HTTPException(404, f"Run not found: {run_id}")
    records, new_offset = run_ctx.stats(offset)
    return {"records": records, "offset": new_offset}


@app.get("/api/runs/{run_id}/config", response_class=PlainTextResponse)
async def get_run_config(run_id: str) -> str:
    """Get config.json content."""
    idx = get_index()
    run_ctx = idx.get_run(run_id)
    if run_ctx is None:
        raise HTTPException(404, f"Run not found: {run_id}")
    config_content = run_ctx.config()
    return json.dumps(config_content, indent=2)


@app.post("/api/runs/{run_id}/config", response_model=ConfigResponse)
async def save_run_config(run_id: str, body: ConfigSaveRequest) -> Dict[str, str]:
    """Save config.json."""
    idx = get_index()
    run_ctx = idx.get_run(run_id)
    if run_ctx is None:
        raise HTTPException(404, f"Run not found: {run_id}")
    # Validate JSON
    try:
        json.loads(body.config)
    except json.JSONDecodeError:
        raise HTTPException(400, "Invalid JSON in config")
    config_file = run_ctx.root / "config.json"
    config_file.write_text(body.config, encoding="utf-8")
    return {"status": "saved"}


@app.post("/api/runs/{run_id}/rerun", response_model=RerunResponse)
async def rerun_simulation(run_id: str, body: Optional[RerunRequest] = None) -> Dict[str, Any]:
    """Rerun simulation."""
    idx = get_index()
    run_ctx = idx.get_run(run_id)
    if run_ctx is None:
        raise HTTPException(404, f"Run not found: {run_id}")

    if run_ctx.is_active():
        raise HTTPException(409, "Simulation already running")

    meta = run_ctx.meta()
    params = meta.get("params", {})
    cycles = body.cycles if body and body.cycles else params.get("cycles", 50000)
    binary_path = params.get("binary_path", "")

    if not binary_path:
        raise HTTPException(400, "No binary_path in run metadata")

    binary = Path(binary_path)
    if not binary.exists() or not os.access(binary, os.X_OK):
        raise HTTPException(400, f"binary_path is not a valid executable: {binary_path}")

    stats_file = run_ctx.root / "stats.jsonl"
    if stats_file.exists():
        stats_file.write_bytes(b"")

    # Use SimulationRunner to build command and launch
    runner = SimulationRunner(binary, run_ctx.root)
    stream_path = run_ctx.root / "stats.jsonl"
    proc = runner.launch(
        config_path=run_ctx.root / "config.json",
        cycles=cycles,
        seed=params.get("seed", 0),
        interval=params.get("interval", 1000),
        stream_path=stream_path,
    )

    dot_path = run_ctx.root / "topology.dot"
    runner.generate_topology_dot(
        config_path=run_ctx.root / "config.json",
        output_path=dot_path
    )

    meta["rerun_count"] = meta.get("rerun_count", 0) + 1
    meta["last_run"] = __import__("datetime").datetime.now().isoformat()
    (run_ctx.root / "meta.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")

    return {
        "status": "started",
        "run_id": run_id,
        "pid": proc.pid,
    }


# =============================================================================
# SSE Endpoint (NEW)
# =============================================================================

@app.get("/api/runs/{run_id}/stream")
async def stream_run_events(run_id: str):
    """SSE real-time stream for stats.jsonl updates.

    Streams new stats records as they are written to stats.jsonl.
    Uses text/event-stream format with JSON data payloads.
    """
    idx = get_index()
    run_ctx = idx.get_run(run_id)
    if run_ctx is None:
        raise HTTPException(404, f"Run not found: {run_id}")

    async def event_generator():
        last_offset = 0
        stats_file = run_ctx.root / "stats.jsonl"

        while True:
            if not stats_file.exists():
                # Wait for stats file to be created
                await _sleep(0.5)
                continue

            # Get current file size
            current_size = stats_file.stat().st_size

            if last_offset < current_size:
                # Read new data
                records, new_offset = run_ctx.stats(last_offset)
                last_offset = new_offset

                for record in records:
                    yield f"data: {json.dumps(record)}\n\n"

                if records:
                    # Yield a heartbeat after processing records
                    yield f"data: {json.dumps({'__heartbeat': True, 'offset': last_offset})}\n\n"

            # Check if simulation is still active
            is_active = run_ctx.is_active()

            # Wait before next poll
            await _sleep(0.2 if is_active else 1.0)

            # If not active and we've caught up, send final event
            if not is_active and last_offset >= current_size:
                yield f"data: {json.dumps({'__end': True})}\n\n"
                break

    from starlette.responses import StreamingResponse
    return StreamingResponse(
        event_generator(),
        media_type="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "X-Accel-Buffering": "no",
        }
    )


async def _sleep(seconds: float) -> None:
    """Async sleep helper."""
    import asyncio
    await asyncio.sleep(seconds)


# =============================================================================
# Create Run
# =============================================================================

@app.post("/api/runs", response_model=CreateRunResponse, status_code=201)
async def create_run(body: CreateRunRequest) -> Dict[str, str]:
    """Create a new run."""
    idx = get_index()

    if not body.binary_path or not body.config_path:
        raise HTTPException(400, "binary_path and config_path required")

    try:
        config_content = Path(body.config_path).read_text()
    except Exception as e:
        raise HTTPException(400, f"Cannot read config: {e}")

    params = {
        "binary_path": body.binary_path,
        "config_path": body.config_path,
        "cycles": body.cycles,
        "seed": body.seed,
        "interval": body.interval,
    }

    run_ctx = idx.create_run(config_content, params)
    return {"run_id": run_ctx.run_id, "status": "created"}


# =============================================================================
# Static Files
# =============================================================================

@app.get("/runs/{run_id}/{path:path}")
async def get_run_file(run_id: str, path: str) -> FileResponse:
    """Get static file from a run directory."""
    # Security: prevent path traversal
    if ".." in path:
        raise HTTPException(403, "Path traversal detected")

    idx = get_index()
    run_ctx = idx.get_run(run_id)
    if run_ctx is None:
        raise HTTPException(404, f"Run not found: {run_id}")

    file_path = run_ctx.root / path
    resolved = file_path.resolve()
    run_root_resolved = run_ctx.root.resolve()

    if not str(resolved).startswith(str(run_root_resolved)):
        raise HTTPException(403, "Path traversal detected")

    if not file_path.exists():
        raise HTTPException(404, "File not found")

    # Determine media type
    ext = file_path.suffix.lower()
    mime_types = {
        ".png": "image/png",
        ".html": "text/html",
        ".json": "application/json",
        ".txt": "text/plain",
        ".js": "application/javascript",
        ".css": "text/css",
    }
    media_type = mime_types.get(ext, "application/octet-stream")

    return FileResponse(
        file_path,
        media_type=media_type,
        headers={"Access-Control-Allow-Origin": "*"}
    )


@app.get("/editor/assets/{path:path}")
async def get_editor_asset(path: str) -> FileResponse:
    """Get editor asset file."""
    # Security: prevent path traversal
    if ".." in path:
        raise HTTPException(403, "Path traversal detected")

    asset_path = Path(__file__).parent / "static" / "editor" / "assets" / path
    if not asset_path.exists() or not asset_path.is_file():
        raise HTTPException(404, "Asset not found")

    return FileResponse(asset_path)


@app.get("/api/schema")
async def get_schema():
    """Get JSON schema for validation."""
    schema_path = Path(__file__).parent.parent.parent / "cpptlm_config" / "schema.json"
    if schema_path.exists():
        return FileResponse(schema_path, media_type="application/json")
    raise HTTPException(404, "Schema not found")


# =============================================================================
# Entry Point
# =============================================================================

def run_server(runs_dir: str = "runs", port: int = 8001) -> None:
    """Run the FastAPI server with uvicorn."""
    import uvicorn

    init_app(runs_dir)

    uvicorn.run(
        app,
        host="0.0.0.0",
        port=port,
        log_level="info",
    )


if __name__ == "__main__":
    run_server()