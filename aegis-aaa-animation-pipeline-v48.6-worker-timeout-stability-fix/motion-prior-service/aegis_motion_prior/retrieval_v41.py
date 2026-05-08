from __future__ import annotations

import json
from pathlib import Path
from typing import Dict, Tuple

import numpy as np

from .schema import BONES
from .timewarp import resample_tensor, smooth_tensor


def _norm_text(value: object) -> str:
    import re
    return re.sub(r"[^a-z0-9]+", "_", str(value or "").lower()).strip("_")


def _clip_search_text(clip: Dict) -> str:
    parts = [
        clip.get("action"),
        clip.get("content"),
        clip.get("id"),
        clip.get("path"),
        clip.get("sourceBvh"),
        clip.get("sourcePath"),
        clip.get("notes"),
    ]
    return _norm_text(" ".join(str(x or "") for x in parts))


def _clip_semantic_content(clip: Dict) -> str:
    # Source filename/path/content is more trustworthy than the derived action.
    # A stale metadata file may say action=soccer_kick_overlay while the source is
    # clearly dataset-1_dash_active_001.bvh. In that case this must return dash.
    source_text = _norm_text(" ".join(str(clip.get(k) or "") for k in [
        "content", "semanticContent", "id", "path", "sourceBvh", "sourcePath"
    ]))
    for locomotion in ("dash", "run", "walk"):
        if locomotion in source_text:
            return locomotion
    if "kick" in source_text or "shoot" in source_text or "soccer_kick" in source_text:
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
    return str(clip.get("content") or clip.get("action") or "unknown")


def _clip_matches_requested_action(clip: Dict, condition: Dict) -> bool:
    requested = str(condition.get("action") or "").lower()
    clip_action = str(clip.get("action") or "")
    if "kick" in requested:
        return _clip_semantic_content(clip) == "kick" and (clip_action == condition.get("action") or "kick" in clip_action.lower())
    return clip_action == condition.get("action")


def load_dataset(dataset_dir: str | Path) -> Tuple[np.ndarray, np.ndarray | None, Dict]:
    dataset_dir = Path(dataset_dir)
    motions = np.load(dataset_dir / "motions.npy")
    mask_path = dataset_dir / "mask.npy"
    masks = np.load(mask_path) if mask_path.exists() else None
    with (dataset_dir / "metadata.json").open("r", encoding="utf-8") as f:
        meta = json.load(f)
    return motions, masks, meta


def _score_clip(clip: Dict, condition: Dict) -> float:
    score = 0.0
    if _clip_matches_requested_action(clip, condition):
        score += 100.0
    if clip.get("style") == condition.get("style"):
        score += 25.0
    if clip.get("dominantLeg", "unknown") == condition.get("dominantLeg", "unknown"):
        score += 10.0
    quality = clip.get("quality", "")
    score += {
        "authored": 15.0,
        "mocap_cleaned": 12.0,
        "offline_retargeted_bvh_to_manny": 14.0,
        "retargeted": 10.0,
        "generated_seed": -10.0,
    }.get(quality, 0.0)

    # Do not prefer very long raw mocap clips for a short additive action overlay.
    # They often contain setup steps, repeated takes, or full-body heading turns.
    try:
        dur = float(clip.get("durationSeconds") or 0.0)
        action = str(condition.get("action") or "")
        if "kick" in action and dur > 3.0:
            score -= min(35.0, (dur - 3.0) * 2.5)
    except Exception:
        pass
    return score


def _action_default_duration(condition: Dict) -> float:
    explicit_default = condition.get("defaultActionDurationSeconds")
    try:
        if explicit_default is not None and float(explicit_default) > 0.0:
            return float(explicit_default)
    except Exception:
        pass
    action = str(condition.get("action") or "").lower()
    if "kick" in action:
        return 1.35
    return 0.0


def _focused_motion_columns() -> np.ndarray:
    focus = {
        "pelvis", "spine_01", "spine_02", "spine_03",
        "thigh_l", "calf_l", "foot_l", "ball_l",
        "thigh_r", "calf_r", "foot_r", "ball_r",
    }
    cols = [0, 1, 2]
    offset = 3
    for bone in BONES:
        if bone in focus:
            cols.extend(range(offset, offset + 6))
        offset += 6
    return np.asarray(cols, dtype=np.int64)


def _trim_to_action_window(tensor: np.ndarray, source_duration: float, target_duration: float, action: str) -> Tuple[np.ndarray, Dict]:
    """Extract the most relevant short action window from a long mocap take.

    Bandai kick clips can be long full takes rather than authored 1-second overlay
    clips. Feeding the whole take into Aegis creates shuffling and repeated spins.
    This trims a compact high-energy lower-body window before time-warping.
    """
    x = np.asarray(tensor, dtype=np.float32)
    n = int(x.shape[0])
    if n <= 8 or target_duration <= 0.0 or source_duration <= 0.0 or "kick" not in str(action).lower():
        return x, {"trimmed": False, "trimStartFrame": 0, "trimEndFrame": max(0, n - 1), "trimmedSourceFrames": n}

    effective_rate = max(1.0, (n - 1) / max(1e-6, source_duration))
    # Leave context before/after the peak so windup and follow-through survive.
    window = int(round(target_duration * effective_rate * 1.35))
    window = int(np.clip(window, 12, n))
    if window >= n:
        return x, {"trimmed": False, "trimStartFrame": 0, "trimEndFrame": n - 1, "trimmedSourceFrames": n}

    cols = _focused_motion_columns()
    cols = cols[cols < x.shape[1]]
    diff = np.diff(x[:, cols], axis=0)
    energy = np.linalg.norm(diff, axis=1)
    if len(energy) >= 5:
        kernel = np.ones(5, dtype=np.float32) / 5.0
        energy = np.convolve(energy, kernel, mode="same")
    if len(energy) == 0 or not np.isfinite(energy).all():
        start = max(0, (n - window) // 2)
    else:
        # Score every possible window by motion energy, with a mild center prior so
        # the selector avoids idle setup at the very beginning/end of a take.
        padded = np.pad(energy, (1, 0), mode="constant")
        csum = np.cumsum(padded)
        scores = []
        max_start = n - window
        center = max_start * 0.5
        sigma = max(1.0, max_start * 0.35)
        for s in range(max_start + 1):
            e = csum[min(len(csum) - 1, s + window - 1)] - csum[s]
            center_prior = np.exp(-0.5 * ((s - center) / sigma) ** 2)
            scores.append(float(e) * (0.8 + 0.2 * float(center_prior)))
        start = int(np.argmax(scores))
    end = min(n, start + window)
    return x[start:end], {"trimmed": True, "trimStartFrame": start, "trimEndFrame": end - 1, "trimmedSourceFrames": end - start}


def retrieve_and_warp(dataset_dir: str | Path, condition: Dict) -> Tuple[np.ndarray, Dict]:
    motions, masks, meta = load_dataset(dataset_dir)
    clips = meta.get("clips", [])
    if not clips:
        return motions[0], {"retrievedClip": None, "score": 0.0}

    requested_action = condition.get("action")
    exact_indices = [i for i, c in enumerate(clips) if _clip_matches_requested_action(c, condition)]
    if "kick" in str(requested_action or "").lower() and not exact_indices:
        available = []
        for c in clips[:50]:
            available.append({
                "id": c.get("id"),
                "action": c.get("action"),
                "content": c.get("content"),
                "semanticContent": _clip_semantic_content(c),
                "source": c.get("sourceBvh") or c.get("sourcePath") or c.get("path"),
            })
        raise RuntimeError(
            "No semantically valid soccer kick training clip was found in the tensor dataset. "
            "Refusing to generate a kick overlay from dash/run/walk data. "
            "This protects the Aegis custom data asset from importing a stabilized-but-wrong locomotion overlay. "
            f"Available clip preview: {available}"
        )
    candidate_indices = exact_indices or list(range(len(clips)))
    best_i = max(candidate_indices, key=lambda i: _score_clip(clips[i], condition))
    tensor = motions[best_i]
    valid_frames = None
    if masks is not None:
        valid_frames = int(np.sum(masks[best_i] > 0.5))
    if not valid_frames or valid_frames <= 0:
        valid_frames = int(clips[best_i].get("frameCount") or tensor.shape[0])
    valid_frames = max(2, min(valid_frames, tensor.shape[0]))
    tensor = tensor[:valid_frames]

    fps = int(condition.get("fps", meta.get("fps", 60)))
    requested_duration = condition.get("durationSeconds", None)
    try:
        duration = float(requested_duration) if requested_duration is not None else 0.0
    except Exception:
        duration = 0.0
    auto_duration = duration <= 0.0
    auto_duration_mode = "explicit"

    source_duration = float(clips[best_i].get("durationSeconds") or (valid_frames - 1) / max(1, fps))
    if auto_duration:
        default_duration = _action_default_duration(condition)
        if default_duration > 0.0:
            duration = default_duration
            auto_duration_mode = "action_default_short_overlay"
        else:
            duration = source_duration
            auto_duration_mode = "source_clip_duration"

    tensor, trim_info = _trim_to_action_window(
        tensor,
        source_duration=source_duration,
        target_duration=duration,
        action=str(condition.get("action") or ""),
    )

    out_frames = max(2, int(round(duration * fps)) + 1)
    warped = resample_tensor(tensor, out_frames)
    warped = smooth_tensor(warped, passes=1, strength=0.10)
    exact_action_match = any(_clip_matches_requested_action(c, condition) for c in clips)
    return warped, {
        "retrievedClip": clips[best_i],
        "score": _score_clip(clips[best_i], condition),
        "timeWarpedFrames": out_frames,
        "durationSeconds": duration,
        "fps": fps,
        "validSourceFrames": valid_frames,
        "sourceClipDurationSeconds": source_duration,
        "autoDuration": auto_duration,
        "autoDurationMode": auto_duration_mode,
        "exactActionMatch": exact_action_match,
        **trim_info,
    }
