from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict

import numpy as np

from .exporter import export_aegis_overlay_json
from .model import build_model, require_torch
from .retrieval import retrieve_clip

def generate(condition: Dict, dataset_dir: str, checkpoint: str | None = None) -> Dict:
    if checkpoint and Path(checkpoint).exists():
        torch = require_torch()
        ckpt = torch.load(checkpoint, map_location="cpu")
        input_dim = int(ckpt["inputDim"])
        model = build_model(input_dim=input_dim)
        model.load_state_dict(ckpt["model"])
        model.eval()

        seed, info = retrieve_clip(dataset_dir, condition)
        with torch.no_grad():
            x = torch.from_numpy(seed[None].astype("float32"))
            pred = model(x).numpy()[0]
        return export_aegis_overlay_json(pred, condition, {
            "model": "MotionPriorTransformer",
            "checkpoint": checkpoint,
            "dataset": dataset_dir,
            "generationMode": "transformer_denoise_seed",
            **info,
        })

    # Retrieval baseline. This is the practical first production path.
    seed, info = retrieve_clip(dataset_dir, condition)
    return export_aegis_overlay_json(seed, condition, {
        "model": "MotionRetrievalBaseline",
        "dataset": dataset_dir,
        "generationMode": "retrieval_fallback",
        **info,
    })

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--checkpoint", default=None)
    parser.add_argument("--action", default="soccer_kick_overlay")
    parser.add_argument("--style", default="powerful")
    parser.add_argument("--dominant-leg", default="right")
    parser.add_argument("--duration", type=float, default=1.35)
    parser.add_argument("--fps", type=int, default=60)
    args = parser.parse_args()

    condition = {
        "id": "motion-prior-generated-v37",
        "name": "Motion Prior Generated V37",
        "action": args.action,
        "style": args.style,
        "dominantLeg": args.dominant_leg,
        "durationSeconds": args.duration,
        "fps": args.fps,
        "skeletonProfile": "UE5_Mannequin",
    }
    doc = generate(condition, args.dataset, args.checkpoint)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(f"Wrote {out}")

if __name__ == "__main__":
    main()
