from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict

import numpy as np

from .model import require_torch
from .neural_model_v47 import V47ModelConfig, build_neural_overlay_model, condition_ids_from_request
from .retrieval_v41 import retrieve_and_warp
from .timewarp import smooth_tensor
from .tensorize import rebase_tensor_to_additive_first_frame, tensor_to_components
from .quaternion import normalize_quat, ensure_quat_continuity, quat_to_6d
from .schema import BONES
from .v48_quaternion_exporter import export_v48_quaternion_overlay_json, default_phase_markers


def _load_checkpoint(path: str | Path):
    torch = require_torch()
    return torch.load(path, map_location="cpu")


def _renormalize_tensor_quats(tensor: np.ndarray) -> np.ndarray:
    x = np.asarray(tensor, dtype=np.float32).copy()
    comps = tensor_to_components(x)
    offset = 3
    for bone in BONES:
        q = normalize_quat(ensure_quat_continuity(comps["quats"][bone]))
        x[:, offset:offset + 6] = quat_to_6d(q)
        offset += 6
    return x.astype(np.float32)


def _read_dataset_variant_count(dataset_dir: str | Path) -> int:
    try:
        meta = json.loads((Path(dataset_dir) / "metadata.json").read_text(encoding="utf-8"))
        return len(meta.get("clips", []) or [])
    except Exception:
        return 0


def _one_hot(index: int, size: int) -> np.ndarray:
    out = np.zeros((1, size), dtype=np.float32)
    out[0, max(0, min(int(index), size - 1))] = 1.0
    return out

def _condition_vector(condition: Dict, vocab: Dict) -> np.ndarray:
    action_vocab = vocab.get("action", {"unknown": 0})
    style_vocab = vocab.get("style", {"unknown": 0})
    leg_vocab = vocab.get("dominantLeg", {"unknown": 0})
    a = action_vocab.get(str(condition.get("action", "unknown")), action_vocab.get("unknown", 0))
    s = style_vocab.get(str(condition.get("style", "unknown")), style_vocab.get("unknown", 0))
    l = leg_vocab.get(str(condition.get("dominantLeg", "unknown")), leg_vocab.get("unknown", 0))
    return np.concatenate([_one_hot(a, len(action_vocab)), _one_hot(s, len(style_vocab)), _one_hot(l, len(leg_vocab))], axis=1).astype(np.float32)

def _apply_v48_frame_denoiser(torch, ckpt: Dict, tensor: np.ndarray, condition: Dict) -> np.ndarray:
    nn = torch.nn
    input_dim = int(ckpt.get("inputDim", tensor.shape[-1]))
    cond_dim = int(ckpt.get("condDim", 1))
    hidden_dim = int(ckpt.get("hiddenDim", 256))
    class V48FrameDenoiser(nn.Module):
        def __init__(self):
            super().__init__()
            self.net = nn.Sequential(
                nn.Linear(input_dim + cond_dim, hidden_dim), nn.SiLU(), nn.LayerNorm(hidden_dim),
                nn.Linear(hidden_dim, hidden_dim), nn.SiLU(), nn.LayerNorm(hidden_dim),
                nn.Linear(hidden_dim, input_dim),
            )
            self.delta_scale = nn.Parameter(torch.tensor(0.20))
        def forward(self, y, c):
            return y + self.net(torch.cat([y, c], dim=-1)) * self.delta_scale.tanh()
    model = V48FrameDenoiser()
    model.load_state_dict(ckpt["model"])
    model.eval()
    cond = _condition_vector(condition, ckpt.get("conditionVocab", {}))
    cond = np.repeat(cond, tensor.shape[0], axis=0)
    with torch.no_grad():
        pred = model(torch.from_numpy(tensor.astype("float32")), torch.from_numpy(cond.astype("float32"))).numpy()
    return pred.astype(np.float32)


def generate_v48(condition: Dict, dataset_dir: str, checkpoint: str | None = None, neural_blend: float = 0.35) -> Dict:
    tensor, info = retrieve_and_warp(dataset_dir, condition)
    tensor = smooth_tensor(tensor, passes=1, strength=0.025)
    mode = "v48_phase_variant_retrieval_fallback"
    ckpt_path = Path(checkpoint) if checkpoint else None

    if ckpt_path and ckpt_path.exists():
        torch = require_torch()
        ckpt = _load_checkpoint(ckpt_path)
        if str(ckpt.get("format", "")).startswith("aegis.quaternion_kick_prior.v48"):
            pred = _apply_v48_frame_denoiser(torch, ckpt, tensor, condition)
        else:
            cfg = dict(ckpt.get("config", {}))
            cfg["input_dim"] = int(ckpt.get("inputDim", tensor.shape[-1]))
            model = build_neural_overlay_model(V47ModelConfig(**cfg), ckpt.get("conditionVocab", {}))
            model.load_state_dict(ckpt["model"])
            model.eval()
            cond_ids = condition_ids_from_request(condition, ckpt.get("conditionVocab", {}))
            with torch.no_grad():
                pred = model(torch.from_numpy(tensor[None].astype("float32")), cond_ids).numpy()[0]
        blend = float(np.clip(neural_blend, 0.0, 0.70))
        # Modest blend: ML polishes timing/jitter/coupling while the deterministic
        # biomechanical phase generator remains the source of truth.
        tensor = (1.0 - blend) * tensor + blend * pred
        mode = "v48_phase_variant_seed_neural_refine"

    tensor = smooth_tensor(tensor, passes=1, strength=0.018)
    tensor = _renormalize_tensor_quats(tensor)
    tensor = rebase_tensor_to_additive_first_frame(tensor)
    tensor = _renormalize_tensor_quats(tensor)

    if float(condition.get("durationSeconds") or 0.0) <= 0.0:
        condition = dict(condition)
        condition["durationSeconds"] = float(info.get("durationSeconds", 1.35))
    condition.setdefault("phaseMarkers", default_phase_markers())

    doc = export_v48_quaternion_overlay_json(tensor, condition, {
        "model": "AegisQuaternionKickPriorV48",
        "dataset": dataset_dir,
        "checkpoint": str(ckpt_path) if ckpt_path else None,
        "generationMode": mode,
        "trainingVariantCount": _read_dataset_variant_count(dataset_dir),
        "variantSet": "v48_synthetic_quaternion_gold_reference_variants",
        "goldReference": condition.get("goldReference", "sample-data/gold/ai_soccer_kick_livebase_overlay_v36.json"),
        "neuralBlend": neural_blend if ckpt_path and ckpt_path.exists() else 0.0,
        **info,
    })
    return doc


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--dataset", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--checkpoint", default=None)
    p.add_argument("--action", default="soccer_kick_overlay")
    p.add_argument("--style", default="instep_power_shot")
    p.add_argument("--dominant-leg", default="right")
    p.add_argument("--duration", type=float, default=1.35)
    p.add_argument("--fps", type=int, default=120)
    p.add_argument("--intensity", type=float, default=1.0)
    p.add_argument("--follow-through", type=float, default=0.70)
    p.add_argument("--plant-stability", type=float, default=0.92)
    p.add_argument("--upper-body-counterbalance", type=float, default=0.78)
    p.add_argument("--look-at-target", default="KickTarget")
    p.add_argument("--neural-blend", type=float, default=0.35)
    p.add_argument("--include-debug-scalar-curves", action="store_true", default=True)
    args = p.parse_args()

    condition = {
        "id": "aegis-v48-quaternion-generated-soccer-kick-overlay",
        "name": "Aegis V48 Quaternion Generated Soccer Kick Overlay",
        "action": args.action,
        "style": args.style,
        "dominantLeg": args.dominant_leg,
        "durationSeconds": args.duration,
        "fps": args.fps,
        "skeletonProfile": "UE5_Mannequin_Quinn_Manny",
        "lookAtTarget": args.look_at_target,
        "intensity": args.intensity,
        "followThrough": args.follow_through,
        "plantStability": args.plant_stability,
        "upperBodyCounterbalance": args.upper_body_counterbalance,
        "includeDebugScalarCurves": args.include_debug_scalar_curves,
    }
    doc = generate_v48(condition, args.dataset, args.checkpoint, args.neural_blend)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(f"Wrote {out}")
    print(f"Curve count: {len(doc.get('curves', []))}")
    print(f"Quaternion runtime curves: {doc.get('validationHints', {}).get('quatCurveCount')}")


if __name__ == "__main__":
    main()
