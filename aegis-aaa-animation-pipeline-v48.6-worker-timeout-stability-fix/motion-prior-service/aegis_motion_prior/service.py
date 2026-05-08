from __future__ import annotations

import os
from typing import Optional

from fastapi import FastAPI
from pydantic import BaseModel

from .infer import generate

app = FastAPI(title="Aegis Motion Prior Service V37")

class GenerateRequest(BaseModel):
    action: str = "soccer_kick_overlay"
    style: str = "powerful"
    dominantLeg: str = "right"
    durationSeconds: float = 1.35
    fps: int = 60
    skeletonProfile: str = "UE5_Mannequin"
    checkpoint: Optional[str] = None
    datasetDir: Optional[str] = None

@app.get("/health")
def health():
    return {"service": "aegis-motion-prior-service", "version": "V37", "status": "ok"}

@app.post("/generate")
def generate_endpoint(req: GenerateRequest):
    dataset = req.datasetDir or os.environ.get("AEGIS_MOTION_PRIOR_DATASET", "datasets/v37")
    checkpoint = req.checkpoint or os.environ.get("AEGIS_MOTION_PRIOR_CHECKPOINT")
    condition = req.model_dump()
    condition["id"] = f"motion-prior-{req.action}-v37"
    condition["name"] = f"Motion Prior {req.action} V37"
    return generate(condition, dataset, checkpoint)
