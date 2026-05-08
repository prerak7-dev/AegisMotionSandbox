from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, List


def _is_number(v: Any) -> bool:
    try:
        float(v)
        return True
    except Exception:
        return False


def _norm_text(value: Any) -> str:
    import re
    return re.sub(r"[^a-z0-9]+", "_", str(value or "").lower()).strip("_")


def _clip_semantic_content(clip: Dict[str, Any] | None) -> str:
    clip = clip or {}
    source_text = _norm_text(" ".join(str(x or "") for x in [
        clip.get("content"), clip.get("semanticContent"), clip.get("id"),
        clip.get("path"), clip.get("sourceBvh"), clip.get("sourcePath"),
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
    return "unknown"



def _rotation_limit_for_curve(name: str) -> float:
    bone = name.split('.', 1)[0] if '.' in name else name
    axis = name.rsplit('_', 1)[-1] if '_' in name else ''
    if bone == "pelvis":
        vals = {"x": 24.0, "y": 18.0, "z": 18.0}
    elif bone.startswith("spine"):
        vals = {"x": 26.0, "y": 18.0, "z": 18.0}
    elif bone in {"neck_01", "head"}:
        vals = {"x": 26.0, "y": 28.0, "z": 24.0}
    elif bone.startswith("thigh"):
        vals = {"x": 95.0, "y": 36.0, "z": 22.0}
    elif bone.startswith("calf"):
        vals = {"x": 125.0, "y": 18.0, "z": 14.0}
    elif bone.startswith("foot"):
        vals = {"x": 75.0, "y": 32.0, "z": 24.0}
    elif bone.startswith("ball"):
        vals = {"x": 45.0, "y": 20.0, "z": 18.0}
    elif bone.startswith("upperarm"):
        vals = {"x": 70.0, "y": 50.0, "z": 38.0}
    elif bone.startswith("lowerarm"):
        vals = {"x": 95.0, "y": 28.0, "z": 30.0}
    else:
        vals = {"x": 45.0, "y": 35.0, "z": 25.0}
    return vals.get(axis, 45.0)

def validate_overlay(path: str | Path) -> Dict[str, Any]:
    path = Path(path)
    doc = json.loads(path.read_text(encoding="utf-8"))
    errors: List[str] = []
    warnings: List[str] = []

    curves = doc.get("curves")
    if not isinstance(curves, list):
        errors.append("Top-level 'curves' must be a non-empty list of curve objects for the Aegis importer.")
        curves = []
    if len(curves) == 0:
        errors.append("No curves were found. The plugin importer would create an empty data asset.")

    required_top = ["durationSeconds", "frameTime", "frameCount", "playbackMode", "basePoseMode", "skeletonProfile"]
    for key in required_top:
        if key not in doc:
            warnings.append(f"Missing top-level metadata field: {key}")

    total_keys = 0
    nonzero_curves = 0
    bad_curve_count = 0
    max_abs_rotation = 0.0
    max_first_frame_rotation = 0.0
    max_first_frame_translation = 0.0
    large_first_frame_curves: List[str] = []
    very_large_rotation_curves: List[str] = []
    out_of_limit_curves: List[str] = []
    discontinuous_rotation_curves: List[str] = []
    names = set()
    for i, c in enumerate(curves):
        if not isinstance(c, dict):
            bad_curve_count += 1
            continue
        name = c.get("curveName")
        if not name:
            errors.append(f"Curve index {i} is missing curveName.")
        else:
            if name in names:
                warnings.append(f"Duplicate curveName: {name}")
            names.add(name)
        keys = c.get("keys")
        if not isinstance(keys, list) or not keys:
            errors.append(f"Curve {name or i} has no keys.")
            continue
        total_keys += len(keys)
        prev_time = None
        has_nonzero = False
        numeric_values = []
        for ki, k in enumerate(keys):
            if not isinstance(k, dict) or not _is_number(k.get("time")) or not _is_number(k.get("value")):
                errors.append(f"Curve {name or i} key {ki} must contain numeric time and value.")
                break
            t = float(k["time"])
            v = float(k["value"])
            numeric_values.append(v)
            if prev_time is not None and t < prev_time:
                errors.append(f"Curve {name or i} has descending key times.")
                break
            prev_time = t
            if abs(v) > 1e-5:
                has_nonzero = True
        if has_nonzero:
            nonzero_curves += 1

        # V47.2 semantic contract: this JSON is an additive overlay. The first
        # rotation/translation key should be near zero, otherwise absolute
        # skeleton rest-pose offsets will be added on top of the live pose in Unreal.
        if name and numeric_values:
            first_abs = abs(float(numeric_values[0]))
            max_abs = max(abs(float(v)) for v in numeric_values)
            if ".rot_" in name and not any(q in name for q in ["rot_qx", "rot_qy", "rot_qz", "rot_qw"]):
                max_abs_rotation = max(max_abs_rotation, max_abs)
                max_first_frame_rotation = max(max_first_frame_rotation, first_abs)
                if first_abs > 2.5:
                    large_first_frame_curves.append(f"{name}={first_abs:.2f}deg")
                limit = _rotation_limit_for_curve(name)
                if max_abs > limit + 1.0:
                    out_of_limit_curves.append(f"{name}={max_abs:.2f}deg limit={limit:.2f}")
                if len(numeric_values) >= 2:
                    jumps = [abs(float(numeric_values[j]) - float(numeric_values[j - 1])) for j in range(1, len(numeric_values))]
                    max_jump = max(jumps) if jumps else 0.0
                    if max_jump > max(45.0, limit * 0.75):
                        discontinuous_rotation_curves.append(f"{name}=jump {max_jump:.2f}deg")
                if max_abs > 135.0:
                    very_large_rotation_curves.append(f"{name}={max_abs:.2f}deg")
            if any(name.endswith(suffix) for suffix in [".trans_x", ".trans_y", ".trans_z", ".loc_x", ".loc_y", ".loc_z"]):
                max_first_frame_translation = max(max_first_frame_translation, first_abs)

    curve_values_by_name: Dict[str, List[float]] = {}
    for c in curves:
        if isinstance(c, dict) and c.get("curveName") and isinstance(c.get("keys"), list):
            vals = []
            for k in c.get("keys", []):
                if isinstance(k, dict) and _is_number(k.get("value")):
                    vals.append(float(k.get("value", 0.0)))
            curve_values_by_name[str(c.get("curveName"))] = vals

    important = ["pelvis.trans_x", "pelvis.trans_y", "pelvis.trans_z", "thigh_r.rot_x", "calf_r.rot_x", "foot_r.rot_x"]
    missing_important = [n for n in important if n not in names]
    if missing_important:
        warnings.append("Missing common gameplay overlay curves: " + ", ".join(missing_important))

    # V47.5 kick readability check: a soccer kick needs knee counter-rotation.
    # The V47.4 file was valid by scalar/spin rules but calf_r.rot_x had only
    # about five degrees while thigh_r.rot_x had almost ninety, so it imported as
    # a weak shuffle instead of a kick.
    for side in ("r", "l"):
        thigh_vals = curve_values_by_name.get(f"thigh_{side}.rot_x", [])
        calf_vals = curve_values_by_name.get(f"calf_{side}.rot_x", [])
        if thigh_vals and calf_vals:
            thigh_peak = max(abs(v) for v in thigh_vals)
            calf_peak = max(abs(v) for v in calf_vals)
            if thigh_peak > 45.0 and calf_peak < max(28.0, 0.35 * thigh_peak):
                warnings.append(
                    f"Weak knee flexion for possible kick leg {side}: thigh_{side}.rot_x peak={thigh_peak:.2f}deg, "
                    f"calf_{side}.rot_x peak={calf_peak:.2f}deg. V47.5 knee coupling should repair this before import."
                )

    if total_keys == 0:
        errors.append("Curves list exists, but there are zero keys.")
    if nonzero_curves == 0:
        errors.append("All curve values are zero. This would import but produce no visible motion.")
    if large_first_frame_curves:
        errors.append("Additive overlay violation: first rotation key is not near zero on " + "; ".join(large_first_frame_curves[:12]))
    if max_first_frame_translation > 10.0:
        errors.append(f"Additive overlay violation: first root translation key is {max_first_frame_translation:.2f}cm, expected near 0cm.")
    if out_of_limit_curves:
        errors.append("Rotation curves exceed V47.4 Aegis PRY additive overlay joint limits: " + "; ".join(out_of_limit_curves[:12]))
    if discontinuous_rotation_curves:
        errors.append("Rotation curves contain scalar discontinuities that can create 360-degree spins: " + "; ".join(discontinuous_rotation_curves[:12]))
    if very_large_rotation_curves:
        errors.append("Dangerous near-180-degree rotation curves detected; these are not valid for scalar Aegis overlay import: " + "; ".join(very_large_rotation_curves[:12]))

    try:
        duration = float(doc.get("durationSeconds", 0.0))
        action = str((doc.get("generationParameters") or {}).get("action") or "").lower()
        if "kick" in action and duration > 3.0:
            errors.append(f"Soccer kick overlay duration is {duration:.2f}s. Expected a short additive action window, normally 1.0-1.8s.")
        if "kick" in action:
            prior = doc.get("motionPrior") or {}
            mode = str(prior.get("generationMode") or "").lower()
            retrieved = prior.get("retrievedClip") if isinstance(prior.get("retrievedClip"), dict) else None
            source_kind = str(prior.get("sourceKind") or "").lower()
            if source_kind == "authored_diagnostic":
                warnings.append("This is an authored diagnostic kick overlay, not ML-generated from mocap.")
            elif mode.startswith("v47_3_repair") or mode.startswith("v47_4_repair"):
                errors.append("Repair-only JSON is not semantically safe as a soccer kick source. Regenerate from a real kick training clip instead of importing the repaired locomotion overlay.")
            elif not retrieved:
                errors.append("Soccer kick overlay has no retrievedClip metadata. The validator cannot prove the source motion was a kick.")
            else:
                semantic = _clip_semantic_content(retrieved)
                exact = prior.get("exactActionMatch")
                if semantic != "kick" or exact is not True:
                    errors.append(
                        "Soccer kick overlay was not generated from a semantically valid kick source. "
                        f"semanticContent={semantic}, exactActionMatch={exact}, source={retrieved.get('sourceBvh') or retrieved.get('sourcePath') or retrieved.get('path')}"
                    )
    except Exception as exc:
        warnings.append(f"Could not run soccer-kick source validation: {exc}")

    report = {
        "path": str(path),
        "valid": len(errors) == 0,
        "schema": doc.get("schema"),
        "curveCount": len(curves),
        "totalKeyCount": total_keys,
        "nonzeroCurveCount": nonzero_curves,
        "badCurveCount": bad_curve_count,
        "maxAbsRotationDegrees": round(max_abs_rotation, 4),
        "maxFirstFrameRotationDegrees": round(max_first_frame_rotation, 4),
        "maxFirstFrameTranslationCm": round(max_first_frame_translation, 4),
        "dangerousNear180CurveCount": len(very_large_rotation_curves),
        "outOfLimitCurveCount": len(out_of_limit_curves),
        "discontinuousRotationCurveCount": len(discontinuous_rotation_curves),
        "kneeReadabilityWarnings": len([w for w in warnings if w.startswith("Weak knee flexion")]),
        "errors": errors,
        "warnings": warnings,
    }
    return report


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--overlay", required=True)
    p.add_argument("--report", default=None)
    a = p.parse_args()
    report = validate_overlay(a.overlay)
    text = json.dumps(report, indent=2)
    if a.report:
        out = Path(a.report)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text, encoding="utf-8")
        print(f"Wrote validation report: {out}")
    print(text)
    if not report["valid"]:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
