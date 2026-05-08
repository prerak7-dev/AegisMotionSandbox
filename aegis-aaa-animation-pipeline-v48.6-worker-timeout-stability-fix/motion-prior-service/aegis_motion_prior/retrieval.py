from __future__ import annotations

import json
from pathlib import Path
from typing import Dict, Tuple

import numpy as np

def load_dataset(dataset_dir: str | Path) -> Tuple[np.ndarray, Dict]:
    dataset_dir = Path(dataset_dir)
    motions = np.load(dataset_dir / "motions.npy")
    with (dataset_dir / "metadata.json").open("r", encoding="utf-8") as f:
        meta = json.load(f)
    return motions, meta

def retrieve_clip(dataset_dir: str | Path, condition: Dict) -> Tuple[np.ndarray, Dict]:
    """Simple action/style retrieval baseline.

    This is intentionally simple. It gives you a working high-quality path as soon as you add
    real animation clips, before any neural model is trained.
    """
    motions, meta = load_dataset(dataset_dir)
    clips = meta.get("clips", [])
    action = condition.get("action", "")
    style = condition.get("style", "")

    best_i = 0
    best_score = -1
    for i, clip in enumerate(clips):
        score = 0
        if clip.get("action") == action:
            score += 10
        if style and clip.get("style") == style:
            score += 3
        if clip.get("dominantLeg") == condition.get("dominantLeg"):
            score += 1
        if score > best_score:
            best_score = score
            best_i = i

    return motions[best_i], {"retrievedClip": clips[best_i] if clips else None, "score": best_score}
