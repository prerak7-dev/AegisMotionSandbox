from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Dict, List, Tuple

import numpy as np

from .schema import BONES, ROOT_CHANNELS, QUAT_CHANNELS, CONTACT_CHANNELS
from .quaternion import normalize_quat, ensure_quat_continuity, quat_multiply

STYLE_PRESETS: Dict[str, Dict] = {
    "instep_power_shot": {
        "duration": 1.35, "intensity": 1.0, "followThrough": 0.70, "plantStability": 0.92, "upperBodyCounterbalance": 0.78,
        "pelvis": 1.16, "spine": 1.05, "arms": 1.14, "plant": 0.62, "thigh": 1.18, "calf": 1.22, "foot": 1.14,
        "extra": {"pelvis_z": 6.0, "thigh_z": 8.0, "calf_z": -7.0, "foot_z": 5.5},
    },
    "inside_foot_pass": {
        "duration": 1.12, "intensity": 0.62, "followThrough": 0.38, "plantStability": 0.98, "upperBodyCounterbalance": 0.42,
        "pelvis": 0.72, "spine": 0.62, "arms": 0.58, "plant": 0.38, "thigh": 0.72, "calf": 0.66, "foot": 0.84,
        "extra": {"pelvis_z": 2.0, "thigh_z": 2.5, "calf_z": -2.0, "foot_y": 7.0, "foot_z": 2.0},
    },
    "side_foot_shot": {
        "duration": 1.24, "intensity": 0.80, "followThrough": 0.55, "plantStability": 0.94, "upperBodyCounterbalance": 0.60,
        "pelvis": 0.92, "spine": 0.82, "arms": 0.84, "plant": 0.52, "thigh": 0.92, "calf": 0.86, "foot": 1.04,
        "extra": {"pelvis_z": 3.5, "thigh_y": 5.0, "foot_y": 13.0, "foot_z": 3.0},
    },
    "volley_preparation": {
        "duration": 1.42, "intensity": 0.86, "followThrough": 0.62, "plantStability": 0.72, "upperBodyCounterbalance": 0.92,
        "pelvis": 1.02, "spine": 1.15, "arms": 1.22, "plant": 0.70, "thigh": 1.08, "calf": 1.10, "foot": 1.08,
        "extra": {"pelvis_z": 3.0, "pelvis_y": -4.0, "thigh_z": 10.0, "calf_z": -9.0, "foot_z": 6.0, "root_z": 3.5},
    },
    "low_driven_kick": {
        "duration": 1.18, "intensity": 0.86, "followThrough": 0.46, "plantStability": 0.96, "upperBodyCounterbalance": 0.65,
        "pelvis": 0.88, "spine": 0.86, "arms": 0.82, "plant": 0.45, "thigh": 0.96, "calf": 1.04, "foot": 0.90,
        "extra": {"pelvis_z": 2.5, "spine_x": -3.0, "thigh_z": 5.5, "calf_z": -5.5, "foot_z": 2.5, "root_z": -2.0},
    },
    "followthrough_heavy": {
        "duration": 1.55, "intensity": 0.95, "followThrough": 1.00, "plantStability": 0.88, "upperBodyCounterbalance": 0.90,
        "pelvis": 1.18, "spine": 1.16, "arms": 1.34, "plant": 0.58, "thigh": 1.08, "calf": 1.14, "foot": 1.06,
        "extra": {"pelvis_z": 7.0, "spine_z": -4.0, "thigh_z": 5.0, "calf_z": -4.0, "foot_z": 4.0},
    },
    "short_tap": {
        "duration": 0.72, "intensity": 0.36, "followThrough": 0.18, "plantStability": 1.00, "upperBodyCounterbalance": 0.20,
        "pelvis": 0.32, "spine": 0.24, "arms": 0.24, "plant": 0.22, "thigh": 0.38, "calf": 0.34, "foot": 0.48,
        "extra": {"pelvis_z": 0.8, "thigh_z": 1.2, "calf_z": -1.0, "foot_z": 2.0},
    },
}

PHASE_MARKERS = [
    {"name": "blend_in_livebase", "time01": 0.0, "role": "enter overlay without popping"},
    {"name": "plant_side_stabilizes", "time01": 0.18, "role": "plant foot stays grounded"},
    {"name": "kicking_hip_loads", "time01": 0.34, "role": "kicking hip loads"},
    {"name": "pelvis_opens", "time01": 0.52, "role": "pelvis opens before thigh drive"},
    {"name": "thigh_drive", "time01": 0.66, "role": "thigh leads before calf extension"},
    {"name": "knee_snap", "time01": 0.76, "role": "calf/knee snaps through near strike"},
    {"name": "strike_contact", "time01": 0.80, "role": "ball contact"},
    {"name": "ankle_whip", "time01": 0.84, "role": "foot follows knee, then whips through"},
    {"name": "follow_through", "time01": 0.91, "role": "follow-through and recovery"},
    {"name": "blend_back_to_livebase", "time01": 1.0, "role": "return to live base"},
]


def _load_json(path: Path) -> Dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _curve_lookup(doc: Dict) -> Dict[str, List[Dict]]:
    out: Dict[str, List[Dict]] = {}
    for c in doc.get("curves", []) or []:
        if isinstance(c, dict) and c.get("curveName"):
            out[str(c["curveName"])] = c.get("keys", []) or []
    return out


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


def _smoothstep(edge0: float, edge1: float, x: np.ndarray) -> np.ndarray:
    if edge1 <= edge0:
        return (x >= edge1).astype(np.float32)
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return (t * t * (3.0 - 2.0 * t)).astype(np.float32)


def _pulse(x: np.ndarray, start: float, peak: float, end: float) -> np.ndarray:
    return np.minimum(_smoothstep(start, peak, x), 1.0 - _smoothstep(peak, end, x)).astype(np.float32)


def _quat_from_axis_angle(axis: int, deg: np.ndarray | float) -> np.ndarray:
    deg_arr = np.asarray(deg, dtype=np.float32)
    half = np.deg2rad(deg_arr) * 0.5
    q = np.zeros(deg_arr.shape + (4,), dtype=np.float32)
    q[..., axis] = np.sin(half)
    q[..., 3] = np.cos(half)
    return normalize_quat(q)


def _quat_scale(q: np.ndarray, strength: float) -> np.ndarray:
    q = normalize_quat(ensure_quat_continuity(q))
    q = np.where(q[..., 3:4] < 0.0, -q, q)
    xyz = q[..., 0:3]
    w = np.clip(q[..., 3], -1.0, 1.0)
    sin_half = np.linalg.norm(xyz, axis=-1)
    strength_arr = np.asarray(strength, dtype=np.float32)
    angle = 2.0 * np.arctan2(sin_half, w) * strength_arr
    axis = xyz / np.maximum(sin_half[..., None], 1e-8)
    out = np.zeros_like(q, dtype=np.float32)
    out[..., 0:3] = axis * np.sin(angle[..., None] * 0.5)
    out[..., 3] = np.cos(angle * 0.5)
    out = np.where((sin_half[..., None] < 1e-6), np.array([0.0, 0.0, 0.0, 1.0], dtype=np.float32), out)
    return normalize_quat(ensure_quat_continuity(out))


def _keys(values: np.ndarray, duration: float) -> List[Dict]:
    values = np.asarray(values, dtype=np.float32)
    n = len(values)
    if n <= 1:
        return [{"time": 0.0, "value": round(float(values[0]) if n else 0.0, 7)}]
    return [{"time": round(i / (n - 1) * duration, 5), "value": round(float(values[i]), 7)} for i in range(n)]


def _curve(name: str, joint: str, channel: str, values: np.ndarray, duration: float, unit: str) -> Dict:
    return {
        "curveName": name,
        "jointName": joint,
        "channelName": channel,
        "unit": unit,
        "keys": _keys(values, duration),
        "originalKeyCount": int(len(values)),
        "compressedKeyCount": int(len(values)),
        "interpolation": "linear",
        "preserveKeys": True,
    }


def _bone_layer(bone: str, kick_suffix: str, plant_suffix: str) -> str:
    if bone == "pelvis": return "pelvis"
    if bone.startswith("spine") or bone in {"neck_01", "head"}: return "spine"
    if bone.startswith(("clavicle", "upperarm", "lowerarm", "hand")): return "arms"
    if bone.endswith(plant_suffix): return "plant"
    if bone == f"thigh{kick_suffix}": return "thigh"
    if bone == f"calf{kick_suffix}": return "calf"
    if bone == f"foot{kick_suffix}" or bone == f"ball{kick_suffix}": return "foot"
    return "plant"


def _variant_times(duration: float, fps: int) -> Tuple[np.ndarray, np.ndarray]:
    frames = max(2, int(round(duration * fps)) + 1)
    t = np.linspace(0.0, duration, frames, dtype=np.float32)
    u = (t / max(duration, 1e-6)).astype(np.float32)
    return t, u


def _style_extra_quat(bone: str, u: np.ndarray, preset: Dict, kick_suffix: str) -> np.ndarray:
    extra = preset.get("extra", {}) or {}
    q = np.tile(np.array([[0.0, 0.0, 0.0, 1.0]], dtype=np.float32), (len(u), 1))
    load = _pulse(u, 0.28, 0.48, 0.72)
    strike = _pulse(u, 0.58, 0.80, 0.98)
    follow = _pulse(u, 0.80, 0.94, 1.0)
    if bone == "pelvis":
        if extra.get("pelvis_x", 0): q = quat_multiply(_quat_from_axis_angle(0, extra["pelvis_x"] * load), q)
        if extra.get("pelvis_y", 0): q = quat_multiply(_quat_from_axis_angle(1, extra["pelvis_y"] * load), q)
        if extra.get("pelvis_z", 0): q = quat_multiply(_quat_from_axis_angle(2, extra["pelvis_z"] * strike), q)
    elif bone.startswith("spine"):
        if extra.get("spine_x", 0): q = quat_multiply(_quat_from_axis_angle(0, extra["spine_x"] * strike), q)
        if extra.get("spine_z", 0): q = quat_multiply(_quat_from_axis_angle(2, extra["spine_z"] * strike), q)
    elif bone == f"thigh{kick_suffix}":
        if extra.get("thigh_y", 0): q = quat_multiply(_quat_from_axis_angle(1, extra["thigh_y"] * strike), q)
        if extra.get("thigh_z", 0): q = quat_multiply(_quat_from_axis_angle(2, extra["thigh_z"] * strike), q)
    elif bone == f"calf{kick_suffix}":
        if extra.get("calf_z", 0): q = quat_multiply(_quat_from_axis_angle(2, extra["calf_z"] * strike), q)
    elif bone == f"foot{kick_suffix}" or bone == f"ball{kick_suffix}":
        if extra.get("foot_y", 0): q = quat_multiply(_quat_from_axis_angle(1, extra["foot_y"] * strike), q)
        if extra.get("foot_z", 0): q = quat_multiply(_quat_from_axis_angle(2, extra["foot_z"] * (0.65 * strike + 0.35 * follow)), q)
    return normalize_quat(ensure_quat_continuity(q))


def _generate_variant_doc(gold: Dict, style: str, idx: int, variant_count_for_style: int, fps: int, dominant_leg: str, rng: np.random.Generator, overrides: Dict) -> Dict:
    preset = dict(STYLE_PRESETS[style])
    preset.update({k: v for k, v in overrides.items() if k in {"intensity", "followThrough", "plantStability", "upperBodyCounterbalance"}})
    duration = float(overrides.get("durationSeconds") or preset["duration"])
    intensity = float(overrides.get("intensity", preset["intensity"]))
    follow = float(overrides.get("followThrough", preset["followThrough"]))
    plant_stability = float(overrides.get("plantStability", preset["plantStability"]))
    upper_counter = float(overrides.get("upperBodyCounterbalance", preset["upperBodyCounterbalance"]))

    # Controlled but non-identical variants.  Keep variation small because the gold sample is trusted.
    spread = 0.06 if variant_count_for_style > 1 else 0.0
    layer_jitter = {k: float(np.clip(1.0 + rng.normal(0.0, spread), 0.82, 1.18)) for k in ["pelvis", "spine", "arms", "plant", "thigh", "calf", "foot"]}
    phase_jitter = float(np.clip(rng.normal(0.0, 0.018), -0.035, 0.035))

    curves = _curve_lookup(gold)
    gold_duration = float(gold.get("durationSeconds", 1.35))
    t, u = _variant_times(duration, fps)
    # Slight phase warp: preserve the animation structure while making strike timing style-controllable.
    src_u = np.clip(u + phase_jitter * _pulse(u, 0.30, 0.70, 0.96), 0.0, 1.0)
    src_times = src_u * gold_duration

    kick_suffix = "_l" if dominant_leg.lower().startswith("l") else "_r"
    plant_suffix = "_r" if kick_suffix == "_l" else "_l"
    out_curves: List[Dict] = []

    # Root / pelvis translation: plant-stable styles reduce lateral/root motion; volley adds lift.
    root_scale = float(np.clip(0.85 + 0.25 * intensity - 0.18 * plant_stability, 0.42, 1.22))
    for ch in ROOT_CHANNELS:
        vals = _sample_keys(curves.get(f"pelvis.{ch}", []), src_times, 0.0) * root_scale
        if ch == "loc_z" and preset.get("extra", {}).get("root_z"):
            vals = vals + preset["extra"]["root_z"] * _pulse(u, 0.32, 0.58, 0.92)
        vals = vals - vals[0]
        out_curves.append(_curve(f"pelvis.{ch}", "pelvis", ch, vals, duration, "cm"))
        out_curves.append(_curve(f"pelvis.{ch.replace('loc_', 'trans_')}", "pelvis", ch.replace("loc_", "trans_"), vals, duration, "cm"))

    for bone in BONES:
        q_cols = []
        for ch in QUAT_CHANNELS:
            default = 1.0 if ch == "rot_qw" else 0.0
            q_cols.append(_sample_keys(curves.get(f"{bone}.{ch}", []), src_times, default)[:, None])
        q = normalize_quat(ensure_quat_continuity(np.concatenate(q_cols, axis=1)))
        layer = _bone_layer(bone, kick_suffix, plant_suffix)
        strength = float(preset.get(layer, 1.0)) * layer_jitter[layer]
        if layer in {"thigh", "calf", "foot"}:
            strength *= float(np.clip(0.64 + 0.55 * intensity, 0.25, 1.35))
        elif layer == "plant":
            strength *= float(np.clip(1.10 - 0.62 * plant_stability, 0.22, 1.0))
        elif layer in {"spine", "arms"}:
            strength *= float(np.clip(0.45 + 0.75 * upper_counter, 0.20, 1.35))
        elif layer == "pelvis":
            strength *= float(np.clip(0.50 + 0.55 * intensity, 0.25, 1.30))
        # Follow-through heavy styles keep more post-strike motion instead of damping quickly.
        follow_weight = 1.0 + (follow - 0.55) * 0.35 * _pulse(u, 0.78, 0.93, 1.0)
        q = _quat_scale(q, strength)
        q_extra = _style_extra_quat(bone, u, preset, kick_suffix)
        q = quat_multiply(q_extra, q)
        # Apply follow-through scale with a second gentle pass for the kick chain only.
        if layer in {"thigh", "calf", "foot", "pelvis", "spine", "arms"}:
            q = _quat_scale(q, follow_weight)
        q = normalize_quat(ensure_quat_continuity(q))
        for i, ch in enumerate(QUAT_CHANNELS):
            out_curves.append(_curve(f"{bone}.{ch}", bone, ch, q[:, i], duration, "quaternion_xyzw"))

    # Contact/plant curves. Plant foot locks from plant through strike; kick foot is unlocked for the swing.
    plant_foot = "foot_l" if kick_suffix == "_r" else "foot_r"
    kick_foot = "foot_r" if kick_suffix == "_r" else "foot_l"
    plant_lock = np.clip(_smoothstep(0.12, 0.22, u) * (1.0 - _smoothstep(0.88, 1.0, u)), 0.0, 1.0) * plant_stability
    kick_contact = np.clip(_pulse(u, 0.72, 0.80, 0.88), 0.0, 1.0)
    contact_values = {
        f"{plant_foot}.ik_lock_alpha": plant_lock,
        f"{plant_foot}.plant_lock_alpha": plant_lock,
        f"{kick_foot}.ik_lock_alpha": np.zeros_like(u),
        f"{kick_foot}.plant_lock_alpha": kick_contact * 0.18,
    }
    for name in CONTACT_CHANNELS:
        vals = contact_values.get(name, np.zeros_like(u))
        joint, channel = name.split(".", 1)
        out_curves.append(_curve(name, joint, channel, vals, duration, "alpha"))

    variant_id = f"v48_{style}_{dominant_leg}_{idx:03d}"
    return {
        "id": variant_id,
        "name": f"V48 {style.replace('_', ' ').title()} Variant {idx:03d}",
        "schema": "aegis.overlay.curves.v2",
        "sourceFormat": "AI_NATIVE_UE5_MANNEQUIN_LIVE_BASE_OVERLAY_V48_SYNTHETIC_VARIANT",
        "durationSeconds": duration,
        "frameTime": 1.0 / fps,
        "fps": fps,
        "frameCount": len(t),
        "playbackMode": "LiveBaseGeneratedOverlay",
        "basePoseMode": "UseLiveAnimGraphSourcePoseEveryFrame",
        "skeletonProfile": "UE5_Mannequin_Quinn_Manny",
        "generationParameters": {
            "action": "soccer_kick_overlay",
            "style": style,
            "dominantLeg": dominant_leg,
            "intensity": intensity,
            "followThrough": follow,
            "plantStability": plant_stability,
            "upperBodyCounterbalance": upper_counter,
            "goldReference": gold.get("id", "ai-soccer-kick-livebase-overlay-v36"),
            "variantIndex": idx,
            "phaseGenerationContract": {
                "plantFootStaysGrounded": True,
                "pelvisOpensBeforeThighDrive": True,
                "thighLeadsBeforeCalfExtension": True,
                "kneeSnapsThroughNearStrike": True,
                "footFollowsKneeThenWhipsThrough": True,
                "spineAndArmsCounterbalance": True,
                "headStaysBallFocused": True,
            },
            "mlRefinementTargets": [
                "smooth timing", "correct joint coupling", "add realistic follow-through", "predict contact timing",
                "adjust style/intensity", "remove jitter", "preserve foot plant",
            ],
        },
        "phaseMarkers": PHASE_MARKERS,
        "curves": out_curves,
    }


def generate_variants(gold_path: str | Path, out_dir: str | Path, manifest_out: str | Path, count: int = 35, fps: int = 120, dominant_leg: str = "right", seed: int = 48, overrides: Dict | None = None) -> None:
    gold_path = Path(gold_path)
    out_dir = Path(out_dir)
    manifest_out = Path(manifest_out)
    out_dir.mkdir(parents=True, exist_ok=True)
    gold = _load_json(gold_path)
    overrides = overrides or {}
    styles = list(STYLE_PRESETS.keys())
    rng = np.random.default_rng(seed)
    count = max(7, int(count))
    base_each = count // len(styles)
    remainder = count % len(styles)
    clips = []
    idx_global = 0
    for si, style in enumerate(styles):
        n = base_each + (1 if si < remainder else 0)
        for j in range(1, n + 1):
            idx_global += 1
            doc = _generate_variant_doc(gold, style, idx_global, n, fps, dominant_leg, rng, overrides if style == str(overrides.get("style", style)) else {})
            path = out_dir / f"{doc['id']}.json"
            path.write_text(json.dumps(doc, indent=2), encoding="utf-8")
            rel = path.relative_to(manifest_out.parent).as_posix() if path.is_relative_to(manifest_out.parent) else path.as_posix()
            clips.append({
                "id": doc["id"],
                "path": rel,
                "action": "soccer_kick_overlay",
                "content": "kick",
                "style": style,
                "dominantLeg": dominant_leg,
                "quality": "v48_synthetic_quaternion_gold_reference_variant",
                "sourceFormat": doc["sourceFormat"],
                "durationSeconds": doc["durationSeconds"],
                "frameCount": doc["frameCount"],
                "license": "PROJECT_INTERNAL_SYNTHETIC_DERIVED_FROM_USER_GOLD_REFERENCE",
                "notes": "Manny/Quinn-native quaternion live-base overlay. No Bandai, BVH, FBX, or Unreal IK Retargeter used.",
            })
    manifest = {
        "datasetName": "aegis_v48_quaternion_kick_variants",
        "version": "V48",
        "skeletonProfile": "UE5_Mannequin_Quinn_Manny",
        "goldReference": gold_path.as_posix(),
        "variantCount": len(clips),
        "productionPath": "no_retarget_quaternion_livebase_overlay",
        "clips": clips,
    }
    manifest_out.parent.mkdir(parents=True, exist_ok=True)
    manifest_out.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Generated {len(clips)} V48 quaternion variants in {out_dir}")
    print(f"Wrote manifest: {manifest_out}")


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--gold", required=True)
    p.add_argument("--out-dir", required=True)
    p.add_argument("--manifest", required=True)
    p.add_argument("--count", type=int, default=35)
    p.add_argument("--fps", type=int, default=120)
    p.add_argument("--dominant-leg", default="right")
    p.add_argument("--style", default="instep_power_shot")
    p.add_argument("--intensity", type=float, default=1.0)
    p.add_argument("--follow-through", type=float, default=0.70)
    p.add_argument("--plant-stability", type=float, default=0.92)
    p.add_argument("--upper-body-counterbalance", type=float, default=0.78)
    p.add_argument("--seed", type=int, default=48)
    args = p.parse_args()
    generate_variants(args.gold, args.out_dir, args.manifest, args.count, args.fps, args.dominant_leg, args.seed, {
        "style": args.style,
        "intensity": args.intensity,
        "followThrough": args.follow_through,
        "plantStability": args.plant_stability,
        "upperBodyCounterbalance": args.upper_body_counterbalance,
    })


if __name__ == "__main__":
    main()
