from __future__ import annotations

import argparse
import json
import re
import shutil
from pathlib import Path
from typing import Dict, List

import numpy as np

from .tensorize import aegis_json_to_tensor


def _norm_text(value: object) -> str:
    return re.sub(r"[^a-z0-9]+", "_", str(value or "").lower()).strip("_")


def _infer_semantic_content(clip: Dict, meta: Dict | None = None) -> str:
    meta = meta or {}
    source_text = _norm_text(" ".join(str(x or "") for x in [
        clip.get("content"), clip.get("semanticContent"), clip.get("id"), clip.get("path"),
        clip.get("sourceBvh"), meta.get("sourceBvh"), meta.get("sourcePath"), meta.get("id"),
    ]))
    for locomotion in ("dash", "run", "walk"):
        if locomotion in source_text:
            return locomotion
    if "kick" in source_text or "shoot" in source_text:
        return "kick"
    action_text = _norm_text(clip.get("action"))
    if "kick" in action_text:
        return "kick"
    if "dash" in action_text:
        return "dash"
    if "run" in action_text:
        return "run"
    if "walk" in action_text:
        return "walk"
    return str(clip.get("content") or "unknown")


def _validate_clip_label(clip: Dict, meta: Dict) -> None:
    action = str(clip.get("action") or "").lower()
    semantic = _infer_semantic_content(clip, meta)
    # Hard fail stale/corrupt metadata: a dash/run/walk file must never be allowed
    # to train or generate as a soccer kick overlay.
    if "kick" in action and semantic != "kick":
        raise RuntimeError(
            "Training manifest labels a non-kick source as a soccer kick overlay. "
            f"clipId={clip.get('id')} semanticContent={semantic} source={clip.get('sourceBvh') or meta.get('sourceBvh') or meta.get('sourcePath') or clip.get('path')}"
        )
    clip["semanticContent"] = semantic

def build_dataset(manifest_path: str, out_dir: str, fps: int = 60, max_frames: int = 180) -> None:
    manifest_path = Path(manifest_path)
    out_dir = Path(out_dir)
    # The dataset output is fully derived from the manifest. Remove any stale
    # metadata/mask/motions from previous V47.x runs so an old dash clip cannot
    # survive after the manifest has been corrected.
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    with manifest_path.open("r", encoding="utf-8") as f:
        manifest = json.load(f)

    tensors: List[np.ndarray] = []
    metas: List[Dict] = []
    base_dir = manifest_path.parent

    for clip in manifest.get("clips", []):
        clip_path = base_dir / clip["path"]
        tensor, meta = aegis_json_to_tensor(clip_path, fps=fps, max_frames=max_frames)
        _validate_clip_label(clip, meta)
        meta.update(clip)
        tensors.append(tensor)
        metas.append(meta)

    if not tensors:
        raise RuntimeError("Manifest contains no clips.")

    # Pad to the largest T in this build.
    max_t = max(t.shape[0] for t in tensors)
    dim = tensors[0].shape[1]
    batch = np.zeros((len(tensors), max_t, dim), dtype=np.float32)
    mask = np.zeros((len(tensors), max_t), dtype=np.float32)
    for i, t in enumerate(tensors):
        batch[i, :t.shape[0], :] = t
        mask[i, :t.shape[0]] = 1.0

    np.save(out_dir / "motions.npy", batch)
    np.save(out_dir / "mask.npy", mask)
    with (out_dir / "metadata.json").open("w", encoding="utf-8") as f:
        json.dump({
            "manifest": str(manifest_path),
            "fps": fps,
            "maxFrames": max_frames,
            "clips": metas,
            "shape": list(batch.shape),
        }, f, indent=2)

    print(f"Built dataset: {out_dir}")
    print(f"motions.npy shape: {batch.shape}")

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--max-frames", type=int, default=180)
    args = parser.parse_args()
    build_dataset(args.manifest, args.out, args.fps, args.max_frames)

if __name__ == "__main__":
    main()
