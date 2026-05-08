from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, List

import numpy as np


def _curve_map(doc: Dict) -> Dict[str, Dict]:
    return {str(c.get("curveName", "")): c for c in doc.get("curves", []) if isinstance(c, dict)}


def _values(curves: Dict[str, Dict], name: str) -> np.ndarray:
    keys = curves[name].get("keys", [])
    return np.asarray([float(k.get("value", 0.0)) for k in keys], dtype=np.float32)


def _set_values(curves: Dict[str, Dict], name: str, values: np.ndarray) -> None:
    keys = curves[name].get("keys", [])
    for key, value in zip(keys, np.asarray(values, dtype=np.float32)):
        key["value"] = round(float(value), 7)


def _smooth(values: np.ndarray, passes: int = 1, strength: float = 0.26) -> np.ndarray:
    y = np.asarray(values, dtype=np.float32).copy()
    if len(y) < 5:
        return y
    strength = float(np.clip(strength, 0.0, 1.0))
    for _ in range(max(0, int(passes))):
        avg = y.copy()
        avg[1:-1] = (y[:-2] + 2.0 * y[1:-1] + y[2:]) * 0.25
        y = (1.0 - strength) * y + strength * avg
    return y.astype(np.float32)


def _zero_first(values: np.ndarray) -> np.ndarray:
    y = np.asarray(values, dtype=np.float32).copy()
    if len(y):
        y -= y[0]
    return y.astype(np.float32)


def _peak(values: np.ndarray) -> float:
    return float(np.max(np.abs(values))) if len(values) else 0.0


def _require(curves: Dict[str, Dict], names: List[str]) -> None:
    missing = [name for name in names if name not in curves]
    if missing:
        raise RuntimeError(f"Input overlay is missing required curves: {missing}")


def repair_soccer_kick_scalar_overlay(doc: Dict, dominant_leg: str = "right") -> Dict:
    curves = _curve_map(doc)
    side = "r" if not str(dominant_leg).lower().startswith("l") else "l"
    plant = "l" if side == "r" else "r"
    required = [f"thigh_{side}.rot_x", f"calf_{side}.rot_x", f"foot_{side}.rot_x"]
    _require(curves, required)

    thigh = _zero_first(_smooth(np.clip(_values(curves, f"thigh_{side}.rot_x"), -90.0, 90.0), passes=1))
    calf_before = _values(curves, f"calf_{side}.rot_x")
    foot_before = _values(curves, f"foot_{side}.rot_x")

    calf_synth = _zero_first(_smooth(np.clip(-1.22 * thigh, -118.0, 118.0), passes=2, strength=0.28))
    foot_synth = _zero_first(_smooth(np.clip(0.52 * thigh - 0.10 * calf_synth, -62.0, 62.0), passes=2, strength=0.26))

    # Prefer the physically coherent diagnostic-kick relationship when the imported calf is weak.
    calf = calf_synth if _peak(calf_before) < max(34.0, 0.52 * _peak(thigh)) else _zero_first(_smooth(0.72 * calf_before + 0.28 * calf_synth, passes=1))
    foot = foot_synth if _peak(foot_before) < 22.0 or _peak(foot_before) > 68.0 else _zero_first(_smooth(0.55 * foot_before + 0.45 * foot_synth, passes=1))

    _set_values(curves, f"thigh_{side}.rot_x", thigh)
    _set_values(curves, f"calf_{side}.rot_x", calf)
    _set_values(curves, f"foot_{side}.rot_x", foot)

    for name, scale, limit in [
        (f"thigh_{side}.rot_y", 0.45, 18.0), (f"thigh_{side}.rot_z", 0.35, 10.0),
        (f"calf_{side}.rot_y", 0.25, 8.0), (f"calf_{side}.rot_z", 0.18, 5.0),
        (f"foot_{side}.rot_y", 0.45, 16.0), (f"foot_{side}.rot_z", 0.35, 12.0),
    ]:
        if name in curves:
            _set_values(curves, name, _zero_first(_smooth(np.clip(_values(curves, name) * scale, -limit, limit), passes=1)))

    for bone, sx, syz, lx, ly, lz in [
        (f"thigh_{plant}", 0.32, 0.35, 30.0, 14.0, 10.0),
        (f"calf_{plant}", 0.25, 0.25, 20.0, 8.0, 6.0),
        (f"foot_{plant}", 0.32, 0.35, 22.0, 12.0, 8.0),
    ]:
        for ax, scale, limit in [("x", sx, lx), ("y", syz, ly), ("z", syz, lz)]:
            name = f"{bone}.rot_{ax}"
            if name in curves:
                _set_values(curves, name, _zero_first(_smooth(np.clip(_values(curves, name) * scale, -limit, limit), passes=1)))

    for bone in ["spine_01", "spine_02", "spine_03", "neck_01", "head"]:
        for ax in "xyz":
            name = f"{bone}.rot_{ax}"
            if name in curves:
                _set_values(curves, name, _zero_first(_smooth(_values(curves, name) * 0.75, passes=1)))

    # Preserve the additive contract exactly.
    for c in doc.get("curves", []):
        name = str(c.get("curveName", ""))
        if ".rot_" in name or ".trans_" in name or ".loc_" in name:
            keys = c.get("keys", [])
            if keys:
                offset = float(keys[0].get("value", 0.0))
                for key in keys:
                    key["value"] = round(float(key.get("value", 0.0)) - offset, 7)

    rot_values = []
    for c in doc.get("curves", []):
        if ".rot_" in str(c.get("curveName", "")):
            rot_values.extend(float(k.get("value", 0.0)) for k in c.get("keys", []))
    arr = np.asarray(rot_values, dtype=np.float32)

    doc["id"] = "aegis-v47-5-knee-coupled-repaired-overlay"
    doc["name"] = "Aegis V47.5 Knee-Coupled Repaired Soccer Kick Overlay"
    doc["sourceFormat"] = "AI_NATIVE_UE5_MANNEQUIN_NEURAL_OVERLAY_PRIOR_V47_5_KNEE_COUPLED"
    doc.setdefault("motionPrior", {})["version"] = "V47.5-repair"
    doc.setdefault("motionPrior", {})["generationMode"] = "scalar_overlay_knee_coupled_repair_v47_5"
    doc.setdefault("generationParameters", {})["generator"] = "neural_overlay_prior_v47_5_knee_coupled_repair"
    doc.setdefault("qualityReport", {})["anatomicalFocus"] = {
        "applied": True,
        "version": "V47.5-repair",
        "dominantLeg": "right" if side == "r" else "left",
        "thighPeak": round(_peak(thigh), 4),
        "calfPeakBefore": round(_peak(calf_before), 4),
        "calfPeakAfter": round(_peak(calf), 4),
        "footPeakBefore": round(_peak(foot_before), 4),
        "footPeakAfter": round(_peak(foot), 4),
        "plantLegDampingApplied": True,
        "twistDampingApplied": True,
    }
    doc.setdefault("qualityReport", {})["maxAbsRotationDegrees"] = round(float(np.max(np.abs(arr))) if arr.size else 0.0, 5)
    doc.setdefault("qualityReport", {})["rotationRmsDegrees"] = round(float(np.sqrt(np.mean(arr * arr))) if arr.size else 0.0, 5)
    return doc


def main() -> None:
    parser = argparse.ArgumentParser(description="Repair a V47.4 scalar soccer kick overlay using V47.5 knee coupling and plant-leg damping.")
    parser.add_argument("--input", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--dominant-leg", default="right")
    args = parser.parse_args()
    doc = json.loads(Path(args.input).read_text(encoding="utf-8"))
    doc = repair_soccer_kick_scalar_overlay(doc, args.dominant_leg)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(f"Wrote V47.5 knee-coupled repaired overlay: {out}")
    q = doc.get("qualityReport", {})
    print(f"Max abs rotation degrees: {q.get('maxAbsRotationDegrees')}")
    print(f"Anatomical focus: {q.get('anatomicalFocus')}")


if __name__ == "__main__":
    main()
