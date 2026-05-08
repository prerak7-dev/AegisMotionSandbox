from __future__ import annotations

from pathlib import Path
from typing import Any, Dict

from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from .source_fetcher import fetch_source
from .source_registry import get_source_list

ROOT = Path(__file__).resolve().parents[2]

app = FastAPI(title="Aegis V38 Motion Data Harvester")

static_dir = Path(__file__).resolve().parents[1] / "static"
app.mount("/static", StaticFiles(directory=str(static_dir)), name="static")

class FetchRequest(BaseModel):
    source: str
    params: Dict[str, Any] = {}

@app.get("/")
def index():
    return FileResponse(static_dir / "dataset-harvester.html")

@app.get("/api/sources")
def sources():
    return {"sources": get_source_list()}

@app.post("/api/fetch")
def fetch(req: FetchRequest):
    payload = {"source": req.source, **req.params}
    result = fetch_source(payload, ROOT)
    return {
        "source": result.source,
        "files": result.files,
        "manifestPath": result.manifest_path,
        "notes": result.notes,
    }

@app.get("/api/health")
def health():
    return {"service": "aegis-v38-motion-data-harvester", "status": "ok"}


@app.get("/v43")
def v43_dashboard():
    return FileResponse(static_dir / "v43-quality-dashboard.html")

@app.get("/manifest-builder")
def manifest_builder():
    return FileResponse(static_dir / "training-manifest-builder.html")
