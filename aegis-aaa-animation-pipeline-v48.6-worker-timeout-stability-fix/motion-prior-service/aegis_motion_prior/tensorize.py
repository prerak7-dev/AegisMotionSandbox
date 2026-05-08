from __future__ import annotations

import json
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np

from .schema import BONES, ROOT_CHANNELS, QUAT_CHANNELS, CONTACT_CHANNELS
from .quaternion import ensure_quat_continuity, normalize_quat, quat_to_6d, sixd_to_quat, quat_relative_to_first


def _curve_lookup(doc: Dict) -> Dict[str, List[Dict]]:
    curves = doc.get("curves", [])
    if isinstance(curves, dict):
        # Offline retarget preview format: {"bone.rot_x": [{time,value}, ...]}
        return {str(k): list(v or []) for k, v in curves.items()}
    if isinstance(curves, list):
        out: Dict[str, List[Dict]] = {}
        for c in curves:
            if isinstance(c, dict) and "curveName" in c:
                out[str(c["curveName"])] = c.get("keys", []) or []
        return out
    return {}


def _sample_keys(keys: List[Dict], times: np.ndarray, default: float = 0.0) -> np.ndarray:
    if not keys:
        return np.full((len(times),), default, dtype=np.float32)
    kt = np.array([float(k.get("time", 0.0)) for k in keys], dtype=np.float32)
    kv = np.array([float(k.get("value", default)) for k in keys], dtype=np.float32)
    order = np.argsort(kt)
    kt, kv = kt[order], kv[order]
    if len(kt) == 1:
        return np.full((len(times),), float(kv[0]), dtype=np.float32)
    return np.interp(times, kt, kv).astype(np.float32)


def _resample_columns(values: np.ndarray, src_times: np.ndarray, dst_times: np.ndarray, default: float = 0.0) -> np.ndarray:
    values = np.asarray(values, dtype=np.float32)
    if values.shape[0] == 0:
        return np.full((len(dst_times), values.shape[1] if values.ndim == 2 else 1), default, dtype=np.float32)
    if len(src_times) == len(dst_times) and np.allclose(src_times, dst_times):
        return values.copy()
    out = np.empty((len(dst_times), values.shape[1]), dtype=np.float32)
    for i in range(values.shape[1]):
        out[:, i] = np.interp(dst_times, src_times, values[:, i]).astype(np.float32)
    return out


def load_aegis_json(path: str | Path) -> Dict:
    with Path(path).open("r", encoding="utf-8") as f:
        return json.load(f)


def _frame_times(doc: Dict, frames: List[Dict], fps: int, max_frames: int) -> Tuple[np.ndarray, np.ndarray, float]:
    if not frames:
        duration = float(doc.get("durationSeconds", 1.0))
        frame_count = min(max_frames, max(2, int(round(duration * fps)) + 1))
        dst = np.linspace(0.0, duration, frame_count, dtype=np.float32)
        return dst, dst, duration

    src_times = np.array([float(fr.get("time", i / max(1, fps))) for i, fr in enumerate(frames)], dtype=np.float32)
    duration = float(doc.get("durationSeconds", float(src_times[-1]) if len(src_times) else 1.0))
    frame_count = min(max_frames, max(2, int(round(duration * fps)) + 1))
    dst_times = np.linspace(0.0, duration, frame_count, dtype=np.float32)
    return src_times, dst_times, duration


def _tensor_from_training_frames(doc: Dict, fps: int, max_frames: int) -> Tuple[np.ndarray, Dict]:
    frames = doc.get("frames", []) or []
    src_times, times, duration = _frame_times(doc, frames, fps, max_frames)
    parts = []

    root_values = []
    for fr in frames:
        pelvis = (fr.get("bones") or {}).get("pelvis", {})
        t = pelvis.get("localTranslation", [0.0, 0.0, 0.0])
        root_values.append([float(t[0] if len(t) > 0 else 0.0), float(t[1] if len(t) > 1 else 0.0), float(t[2] if len(t) > 2 else 0.0)])
    root = _resample_columns(np.asarray(root_values, dtype=np.float32), src_times, times, default=0.0) if root_values else np.zeros((len(times), 3), dtype=np.float32)
    parts.append(root)

    for bone in BONES:
        q_values = []
        for fr in frames:
            b = (fr.get("bones") or {}).get(bone, {})
            q = b.get("rotationQuaternion", [1.0, 0.0, 0.0, 0.0])
            # Offline Blender export writes [w, x, y, z]; neural tensor expects [x, y, z, w].
            if len(q) >= 4:
                q_values.append([float(q[1]), float(q[2]), float(q[3]), float(q[0])])
            else:
                q_values.append([0.0, 0.0, 0.0, 1.0])
        q = _resample_columns(np.asarray(q_values, dtype=np.float32), src_times, times, default=0.0) if q_values else np.tile(np.array([[0.0, 0.0, 0.0, 1.0]], dtype=np.float32), (len(times), 1))
        # Convert absolute Manny/Quinn local rotations to additive deltas from the
        # first frame.  This is the core V47.2 fix: the overlay JSON must drive
        # animation deltas, not absolute skeleton-pose offsets.
        q = quat_relative_to_first(q)
        parts.append(quat_to_6d(q))

    contacts = doc.get("contactCurves", {}) or {}
    for name in CONTACT_CHANNELS:
        keys = contacts.get(name, []) if isinstance(contacts, dict) else []
        parts.append(_sample_keys(keys, times, 0.0)[:, None])

    tensor = np.concatenate(parts, axis=1).astype(np.float32)
    meta = {
        "sourcePath": doc.get("sourceBvh") or "offline-retarget-training-json",
        "durationSeconds": duration,
        "fps": fps,
        "frameCount": int(tensor.shape[0]),
        "tensorDim": int(tensor.shape[1]),
        "layout": "root3 + bone6d*22 + contact4",
        "sourceFormat": doc.get("schema", "aegis.offlineRetarget.trainingFrames.v1"),
        "playbackMode": "LiveBaseGeneratedOverlay",
        "retargetMethod": doc.get("retargetMethod", "offline_world_delta_to_target_rest"),
    }
    return tensor, meta


def aegis_json_to_tensor(path: str | Path, fps: int = 60, max_frames: int = 180) -> Tuple[np.ndarray, Dict]:
    """Return tensor shape [T, D].

    Supported sources:
    - Final Aegis overlay JSON: curves as a list of curve objects.
    - Offline retarget training JSON: frames[*].bones[*].rotationQuaternion and localTranslation.
    """
    doc = load_aegis_json(path)
    if doc.get("frames"):
        return _tensor_from_training_frames(doc, fps=fps, max_frames=max_frames)

    duration = float(doc.get("durationSeconds", 1.0))
    frame_count = min(max_frames, max(2, int(round(duration * fps)) + 1))
    times = np.linspace(0.0, duration, frame_count, dtype=np.float32)
    curves = _curve_lookup(doc)

    parts = []

    # Root location. Accept both v37 loc_* and v47 trans_* aliases.
    root_cols = []
    for ch in ROOT_CHANNELS:
        alias = ch.replace("loc_", "trans_")
        root_cols.append(_sample_keys(curves.get(f"pelvis.{ch}", curves.get(f"pelvis.{alias}", [])), times, 0.0)[:, None])
    parts.append(np.concatenate(root_cols, axis=1))

    # Bone rotations. Prefer quaternion channels; fall back to identity if a preview Euler file is passed in.
    for bone in BONES:
        q_cols = []
        for ch in QUAT_CHANNELS:
            default = 1.0 if ch == "rot_qw" else 0.0
            q_cols.append(_sample_keys(curves.get(f"{bone}.{ch}", []), times, default)[:, None])
        q = np.concatenate(q_cols, axis=1)
        q = normalize_quat(ensure_quat_continuity(q))
        parts.append(quat_to_6d(q))

    for name in CONTACT_CHANNELS:
        parts.append(_sample_keys(curves.get(name, []), times, 0.0)[:, None])

    tensor = np.concatenate(parts, axis=1).astype(np.float32)
    meta = {
        "sourcePath": str(path),
        "durationSeconds": duration,
        "fps": fps,
        "frameCount": frame_count,
        "tensorDim": int(tensor.shape[1]),
        "layout": "root3 + bone6d*22 + contact4",
        "sourceFormat": doc.get("sourceFormat", doc.get("schema", "unknown")),
        "playbackMode": doc.get("playbackMode", "LiveBaseGeneratedOverlay"),
    }
    return tensor, meta



def rebase_tensor_to_additive_first_frame(tensor: np.ndarray) -> np.ndarray:
    """Force a generated tensor to satisfy the Aegis additive-overlay contract.

    Root translation starts at zero and every bone rotation starts at identity. This
    protects the final export from neural drift and from any retrieved legacy clips
    that still contain absolute local-rotation offsets.
    """
    x = np.asarray(tensor, dtype=np.float32).copy()
    if x.shape[0] == 0:
        return x
    x[:, 0:3] -= x[0:1, 0:3]
    offset = 3
    for _bone in BONES:
        q = sixd_to_quat(x[:, offset:offset + 6])
        q = quat_relative_to_first(q)
        x[:, offset:offset + 6] = quat_to_6d(q)
        offset += 6
    return x.astype(np.float32)

def tensor_to_components(tensor: np.ndarray) -> Dict:
    """Inverse of aegis_json_to_tensor layout."""
    x = np.asarray(tensor, dtype=np.float32)
    root = x[:, 0:3]
    offset = 3
    quats = {}
    for bone in BONES:
        six = x[:, offset:offset + 6]
        offset += 6
        quats[bone] = sixd_to_quat(six)
    contacts = {}
    for i, name in enumerate(CONTACT_CHANNELS):
        contacts[name] = np.clip(x[:, offset + i], 0.0, 1.0)
    return {"root": root, "quats": quats, "contacts": contacts}
