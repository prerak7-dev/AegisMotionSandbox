from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict

import numpy as np

from .exporter import export_aegis_overlay_json
from .model import require_torch
from .neural_model_v47 import V47ModelConfig, build_neural_overlay_model, condition_ids_from_request
from .retrieval_v41 import retrieve_and_warp
from .timewarp import smooth_tensor
from .tensorize import rebase_tensor_to_additive_first_frame


def _load_checkpoint(path: str | Path):
    torch = require_torch()
    return torch.load(path, map_location="cpu")


def generate_v47(condition: Dict, dataset_dir: str, checkpoint: str | None = None, neural_blend: float = 0.45) -> Dict:
    tensor, info = retrieve_and_warp(dataset_dir, condition)
    tensor = smooth_tensor(tensor, passes=1, strength=0.04)
    mode = "retrieval_timewarp_validated_fallback"
    ckpt_path = Path(checkpoint) if checkpoint else None

    if ckpt_path and ckpt_path.exists():
        torch = require_torch()
        ckpt = _load_checkpoint(ckpt_path)
        cfg = dict(ckpt.get("config", {}))
        cfg["input_dim"] = int(ckpt.get("inputDim", tensor.shape[-1]))
        model = build_neural_overlay_model(V47ModelConfig(**cfg), ckpt.get("conditionVocab", {}))
        model.load_state_dict(ckpt["model"])
        model.eval()
        cond_ids = condition_ids_from_request(condition, ckpt.get("conditionVocab", {}))
        with torch.no_grad():
            pred = model(torch.from_numpy(tensor[None].astype("float32")), cond_ids).numpy()[0]
        blend = float(np.clip(neural_blend, 0.0, 1.0))
        # Keep contact/impact timing from retrieval while letting the network clean and style motion.
        tensor = (1.0 - blend) * tensor + blend * pred
        mode = "neural_retrieval_seed_refine_v47"

    tensor = smooth_tensor(tensor, passes=1, strength=0.035)
    tensor = rebase_tensor_to_additive_first_frame(tensor)
    if float(condition.get("durationSeconds") or 0.0) <= 0.0:
        condition = dict(condition)
        condition["durationSeconds"] = float(info.get("durationSeconds", 1.35))
    doc = export_aegis_overlay_json(tensor, condition, {
        "model": "AegisNeuralOverlayPriorV47",
        "dataset": dataset_dir,
        "checkpoint": str(ckpt_path) if ckpt_path else None,
        "generationMode": mode,
        **info,
    })
    doc["headFocus"] = {
        "enabled": True,
        "mode": "runtime_dynamic_look_at_target",
        "target": condition.get("lookAtTarget", "KickTarget"),
        "neckWeight": 0.35,
        "headWeight": 0.65,
        "maxYawDegrees": 55,
        "maxPitchDegrees": 35,
        "smoothingHalfLife": 0.08,
    }
    doc["lateIK"] = {
        "enabled": True,
        "mode": "post_overlay_plant_foot_only",
        "applyAfterHeadLookAt": True,
        "rootCorrection": False,
        "maxCorrectionCm": 18,
    }
    return doc


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--dataset", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--checkpoint", default=None)
    p.add_argument("--action", default="soccer_kick_overlay")
    p.add_argument("--style", default="active")
    p.add_argument("--dominant-leg", default="right")
    p.add_argument("--duration", type=float, default=0.0, help="Seconds. Use 0 or omit for source-clip duration.")
    p.add_argument("--fps", type=int, default=60)
    p.add_argument("--look-at-target", default="KickTarget")
    p.add_argument("--neural-blend", type=float, default=0.45)
    p.add_argument("--default-action-duration", type=float, default=1.35, help="Used when --duration is 0 for short additive actions such as kicks.")
    p.add_argument("--include-quaternion-reference", action="store_true", help="Also emit rot_q* reference curves. Off by default so the Aegis importer only receives scalar overlay curves.")
    a = p.parse_args()

    condition = {
        "id": "aegis-v47-neural-generated-overlay",
        "name": "Aegis V47 Neural Generated Overlay",
        "action": a.action,
        "style": a.style,
        "dominantLeg": a.dominant_leg,
        "durationSeconds": a.duration,
        "fps": a.fps,
        "skeletonProfile": "UE5_Mannequin",
        "lookAtTarget": a.look_at_target,
        "includeQuaternionReference": bool(a.include_quaternion_reference),
        "defaultActionDurationSeconds": a.default_action_duration,
    }
    doc = generate_v47(condition, a.dataset, a.checkpoint, a.neural_blend)
    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(f"Wrote {out}")
    print(f"Curve count: {len(doc.get('curves', []))}")


if __name__ == "__main__":
    main()
