from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict

from .exporter import export_aegis_overlay_json
from .model import build_model, require_torch
from .retrieval_v41 import retrieve_and_warp
from .timewarp import smooth_tensor

def generate_v43(condition: Dict, dataset_dir: str, checkpoint: str | None = None) -> Dict:
    tensor, info = retrieve_and_warp(dataset_dir, condition)
    mode = "retrieval_timewarp"

    if checkpoint and Path(checkpoint).exists():
        torch = require_torch()
        ckpt = torch.load(checkpoint, map_location="cpu")
        model = build_model(input_dim=int(ckpt["inputDim"]))
        model.load_state_dict(ckpt["model"])
        model.eval()
        with torch.no_grad():
            pred = model(torch.from_numpy(tensor[None].astype("float32"))).numpy()[0]
        # Blend denoised output with retrieval to preserve impact/contact.
        tensor = 0.65 * tensor + 0.35 * pred
        mode = "retrieval_timewarp_contact_denoise"

    tensor = smooth_tensor(tensor, passes=1, strength=0.06)
    doc = export_aegis_overlay_json(tensor, condition, {
        "model": "AegisV43RetrievalWarpContactDenoiser",
        "dataset": dataset_dir,
        "checkpoint": checkpoint,
        "generationMode": mode,
        **info
    })
    doc["headFocus"] = {
        "enabled": True,
        "mode": "runtime_dynamic_look_at_target",
        "target": condition.get("lookAtTarget", "KickTarget"),
        "neckWeight": 0.35,
        "headWeight": 0.65,
        "maxYawDegrees": 55,
        "maxPitchDegrees": 35,
        "smoothingHalfLife": 0.08
    }
    doc["lateIK"] = {
        "enabled": True,
        "mode": "post_overlay_plant_foot_only",
        "applyAfterHeadLookAt": True,
        "rootCorrection": False,
        "maxCorrectionCm": 18
    }
    return doc

def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--dataset", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--checkpoint", default=None)
    p.add_argument("--action", default="soccer_kick_overlay")
    p.add_argument("--style", default="powerful")
    p.add_argument("--dominant-leg", default="right")
    p.add_argument("--duration", type=float, default=1.35)
    p.add_argument("--fps", type=int, default=60)
    p.add_argument("--look-at-target", default="KickTarget")
    a = p.parse_args()

    condition = {
        "id": "aegis-v43-generated-overlay",
        "name": "Aegis V43 Generated Overlay",
        "action": a.action,
        "style": a.style,
        "dominantLeg": a.dominant_leg,
        "durationSeconds": a.duration,
        "fps": a.fps,
        "skeletonProfile": "UE5_Mannequin",
        "lookAtTarget": a.look_at_target,
    }
    doc = generate_v43(condition, a.dataset, a.checkpoint)
    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(f"Wrote {out}")

if __name__ == "__main__":
    main()
