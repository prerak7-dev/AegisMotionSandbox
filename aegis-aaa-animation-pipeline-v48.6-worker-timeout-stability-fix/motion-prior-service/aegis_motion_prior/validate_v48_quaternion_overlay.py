from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, List

import numpy as np

from .schema import BONES, QUAT_CHANNELS

REQUIRED_PHASES = [
    "plant_side_stabilizes", "kicking_hip_loads", "pelvis_opens", "thigh_drive",
    "knee_snap", "strike_contact", "ankle_whip", "follow_through",
]


def _curve_lookup(doc: Dict) -> Dict[str, List[Dict]]:
    return {str(c.get("curveName")): c.get("keys", []) or [] for c in doc.get("curves", []) or [] if isinstance(c, dict) and c.get("curveName")}


def _values(keys: List[Dict]) -> np.ndarray:
    return np.asarray([float(k.get("value", 0.0)) for k in keys], dtype=np.float32)


def validate(path: str | Path, out: str | Path | None = None) -> Dict:
    path = Path(path)
    doc = json.loads(path.read_text(encoding="utf-8"))
    curves = _curve_lookup(doc)
    errors: List[str] = []
    warnings: List[str] = []

    if doc.get("playbackMode") != "LiveBaseGeneratedOverlay":
        errors.append("playbackMode must be LiveBaseGeneratedOverlay")
    if doc.get("basePoseMode") != "UseLiveAnimGraphSourcePoseEveryFrame":
        errors.append("basePoseMode must be UseLiveAnimGraphSourcePoseEveryFrame")
    if "V48_QUATERNION_NO_RETARGET" not in str(doc.get("sourceFormat", "")):
        errors.append("sourceFormat must identify the V48 quaternion no-retarget path")

    phase_names = {str(p.get("name")) for p in doc.get("phaseMarkers", []) or []}
    for phase in REQUIRED_PHASES:
        if phase not in phase_names:
            errors.append(f"Missing required phase marker: {phase}")

    quat_curve_count = 0
    bad_quat_norm_count = 0
    non_identity_first_frame = 0
    for bone in BONES:
        q_cols = []
        for ch in QUAT_CHANNELS:
            name = f"{bone}.{ch}"
            if name not in curves:
                errors.append(f"Missing quaternion curve: {name}")
                q_cols = []
                break
            q_cols.append(_values(curves[name])[:, None])
            quat_curve_count += 1
        if not q_cols:
            continue
        q = np.concatenate(q_cols, axis=1)
        norms = np.linalg.norm(q, axis=1)
        bad_quat_norm_count += int(np.sum(np.abs(norms - 1.0) > 0.035))
        if len(q):
            first = q[0]
            if np.linalg.norm(first - np.array([0.0, 0.0, 0.0, 1.0], dtype=np.float32)) > 0.05:
                non_identity_first_frame += 1
    if bad_quat_norm_count:
        errors.append(f"Quaternion curves contain {bad_quat_norm_count} keys with invalid norm")
    if non_identity_first_frame:
        errors.append(f"{non_identity_first_frame} bones do not start at identity additive quaternion")

    scalar_runtime = [name for name in curves if name.endswith(".rot_x") or name.endswith(".rot_y") or name.endswith(".rot_z")]
    if scalar_runtime:
        errors.append("Production V48 overlay must not include scalar runtime rot_x/rot_y/rot_z curves; use rot_q* curves plus debug_* only")

    gp = doc.get("generationParameters", {}) or {}
    contract = gp.get("phaseGenerationContract", {}) or {}
    for key in ["plantFootStaysGrounded", "pelvisOpensBeforeThighDrive", "thighLeadsBeforeCalfExtension", "kneeSnapsThroughNearStrike", "footFollowsKneeThenWhipsThrough", "spineAndArmsCounterbalance", "headStaysBallFocused"]:
        if contract.get(key) is not True:
            errors.append(f"Missing or false phase generation parameter: {key}")
    ml = gp.get("mlRefinementContract", {}) or {}
    for key in ["smoothTiming", "correctJointCoupling", "addRealisticFollowThrough", "predictContactTiming", "adjustStyleAndIntensity", "removeJitter", "preserveFootPlant"]:
        if ml.get(key) is not True:
            errors.append(f"Missing or false ML refinement parameter: {key}")

    report = {
        "valid": not errors,
        "path": str(path),
        "schema": doc.get("schema"),
        "sourceFormat": doc.get("sourceFormat"),
        "durationSeconds": doc.get("durationSeconds"),
        "frameCount": doc.get("frameCount"),
        "curveCount": len(doc.get("curves", []) or []),
        "quatCurveCount": quat_curve_count,
        "phaseCount": len(doc.get("phaseMarkers", []) or []),
        "badQuatNormCount": bad_quat_norm_count,
        "nonIdentityFirstFrameBoneCount": non_identity_first_frame,
        "scalarRuntimeCurveCount": len(scalar_runtime),
        "warnings": warnings,
        "errors": errors,
    }
    if out:
        Path(out).parent.mkdir(parents=True, exist_ok=True)
        Path(out).write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--out", default=None)
    args = p.parse_args()
    report = validate(args.input, args.out)
    print(json.dumps(report, indent=2))
    if not report["valid"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
