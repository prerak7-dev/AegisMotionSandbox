from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, List, Tuple

BONES = [
    "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head",
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
    "thigh_l", "calf_l", "foot_l", "ball_l",
    "thigh_r", "calf_r", "foot_r", "ball_r",
]


def interp_keys(duration: float, fps: int, control: List[Tuple[float, float]]) -> List[Dict[str, float]]:
    n = int(round(duration * fps)) + 1
    control = sorted((max(0.0, min(1.0, float(t))), float(v)) for t, v in control)
    times = [i / fps for i in range(n)]
    out: List[Dict[str, float]] = []
    for t_abs in times:
        u = 0.0 if duration <= 0 else t_abs / duration
        if u <= control[0][0]:
            v = control[0][1]
        elif u >= control[-1][0]:
            v = control[-1][1]
        else:
            v = control[-1][1]
            for (t0, v0), (t1, v1) in zip(control, control[1:]):
                if t0 <= u <= t1:
                    a = 0.0 if t1 == t0 else (u - t0) / (t1 - t0)
                    # Smoothstep so the authored diagnostic does not pop.
                    a = a * a * (3.0 - 2.0 * a)
                    v = v0 * (1.0 - a) + v1 * a
                    break
        out.append({"time": round(t_abs, 5), "value": round(v, 4)})
    if out:
        out[-1]["time"] = round(duration, 5)
    return out


def make_curve(name: str, joint: str, channel: str, keys: List[Dict[str, float]], unit: str) -> Dict:
    return {
        "curveName": name,
        "jointName": joint,
        "channelName": channel,
        "unit": unit,
        "keys": keys,
        "originalKeyCount": len(keys),
        "compressedKeyCount": len(keys),
        "interpolation": "linear",
        "preserveKeys": True,
    }


def main() -> None:
    ap = argparse.ArgumentParser(description="Generate an authored Aegis V47.4 right-leg soccer-kick diagnostic overlay JSON.")
    ap.add_argument("--out", required=True)
    ap.add_argument("--duration", type=float, default=1.35)
    ap.add_argument("--fps", type=int, default=60)
    ap.add_argument("--dominant-leg", default="right", choices=["right", "left"])
    args = ap.parse_args()

    duration = float(args.duration)
    fps = int(args.fps)
    curves: List[Dict] = []
    zero = interp_keys(duration, fps, [(0, 0), (1, 0)])

    # Root translation is intentionally tiny for an overlay; locomotion remains owned by the CharacterMovement/base pose.
    root_controls = {
        "trans_x": [(0, 0), (0.35, -1.5), (0.62, 1.0), (1.0, 0.0)],
        "trans_y": [(0, 0), (0.40, -4.0), (0.64, 5.0), (1.0, 0.0)],
        "trans_z": [(0, 0), (0.45, 1.8), (0.62, 0.6), (1.0, 0.0)],
    }
    for ch in ["trans_x", "trans_y", "trans_z"]:
        curves.append(make_curve(f"pelvis.{ch}", "pelvis", ch, interp_keys(duration, fps, root_controls[ch]), "cm"))
    for src, ch in [("trans_x", "loc_x"), ("trans_y", "loc_y"), ("trans_z", "loc_z")]:
        curves.append(make_curve(f"pelvis.{ch}", "pelvis", ch, interp_keys(duration, fps, root_controls[src]), "cm"))

    pose: Dict[str, Dict[str, List[Tuple[float, float]]]] = {b: {"rot_x": [(0, 0), (1, 0)], "rot_y": [(0, 0), (1, 0)], "rot_z": [(0, 0), (1, 0)]} for b in BONES}

    # V47.4 convention: rot_x is the visible pitch/sagittal swing channel.
    # Right-leg kick: windup -> strike -> follow-through -> settle.
    pose["pelvis"]["rot_x"] = [(0, 0), (0.38, -8), (0.58, 12), (0.78, 7), (1, 0)]
    pose["pelvis"]["rot_y"] = [(0, 0), (0.38, -5), (0.60, 8), (0.82, 4), (1, 0)]
    pose["pelvis"]["rot_z"] = [(0, 0), (0.40, -6), (0.64, 7), (1, 0)]

    pose["spine_01"]["rot_x"] = [(0, 0), (0.36, 8), (0.60, -12), (0.82, -5), (1, 0)]
    pose["spine_01"]["rot_y"] = [(0, 0), (0.36, 4), (0.62, -8), (1, 0)]
    pose["spine_01"]["rot_z"] = [(0, 0), (0.45, 6), (0.66, -8), (1, 0)]
    pose["spine_02"]["rot_x"] = [(0, 0), (0.36, 5), (0.60, -8), (1, 0)]
    pose["spine_03"]["rot_x"] = [(0, 0), (0.36, 4), (0.60, -6), (1, 0)]
    pose["neck_01"]["rot_x"] = [(0, 0), (0.60, -4), (1, 0)]
    pose["head"]["rot_x"] = [(0, 0), (0.60, -5), (1, 0)]

    # Arms counterbalance: opposite arm rises during strike.
    pose["upperarm_l"]["rot_x"] = [(0, 0), (0.35, -18), (0.58, 32), (0.78, 18), (1, 0)]
    pose["upperarm_l"]["rot_y"] = [(0, 0), (0.58, 16), (1, 0)]
    pose["lowerarm_l"]["rot_x"] = [(0, 0), (0.58, 36), (1, 0)]
    pose["upperarm_r"]["rot_x"] = [(0, 0), (0.35, 20), (0.58, -28), (0.78, -15), (1, 0)]
    pose["upperarm_r"]["rot_y"] = [(0, 0), (0.58, -14), (1, 0)]
    pose["lowerarm_r"]["rot_x"] = [(0, 0), (0.58, -30), (1, 0)]

    # Plant/support leg stabilizes; kick leg gets strong rot_x pitch.
    pose["thigh_l"]["rot_x"] = [(0, 0), (0.38, -10), (0.62, 14), (0.82, 6), (1, 0)]
    pose["thigh_l"]["rot_y"] = [(0, 0), (0.55, 6), (1, 0)]
    pose["calf_l"]["rot_x"] = [(0, 0), (0.42, 8), (0.65, -10), (1, 0)]
    pose["foot_l"]["rot_x"] = [(0, 0), (0.42, -6), (0.65, 8), (1, 0)]

    pose["thigh_r"]["rot_x"] = [(0, 0), (0.30, -42), (0.55, 82), (0.72, 48), (1, 0)]
    pose["thigh_r"]["rot_y"] = [(0, 0), (0.33, 8), (0.55, -14), (0.76, -6), (1, 0)]
    pose["thigh_r"]["rot_z"] = [(0, 0), (0.55, 8), (1, 0)]
    pose["calf_r"]["rot_x"] = [(0, 0), (0.30, 62), (0.55, -105), (0.72, -68), (1, 0)]
    pose["calf_r"]["rot_y"] = [(0, 0), (0.55, 5), (1, 0)]
    pose["foot_r"]["rot_x"] = [(0, 0), (0.30, -28), (0.55, 52), (0.72, 26), (1, 0)]
    pose["foot_r"]["rot_y"] = [(0, 0), (0.55, -8), (1, 0)]
    pose["ball_r"]["rot_x"] = [(0, 0), (0.55, 22), (0.72, 10), (1, 0)]

    for bone in BONES:
        for ch in ["rot_x", "rot_y", "rot_z"]:
            curves.append(make_curve(f"{bone}.{ch}", bone, ch, interp_keys(duration, fps, pose[bone][ch]), "degrees"))

    contact = {
        "foot_l.ik_lock_alpha": [(0, 1), (0.88, 1), (1, 0.35)],
        "foot_r.ik_lock_alpha": [(0, 0.0), (0.22, 0.0), (0.55, 0.0), (0.84, 0.15), (1, 0.35)],
        "foot_l.plant_lock_alpha": [(0, 1), (0.86, 1), (1, 0.25)],
        "foot_r.plant_lock_alpha": [(0, 0.0), (0.24, 0.0), (0.60, 0.0), (0.86, 0.2), (1, 0.3)],
    }
    for name, ctl in contact.items():
        joint, channel = name.split(".", 1)
        curves.append(make_curve(name, joint, channel, interp_keys(duration, fps, ctl), "alpha"))

    doc = {
        "id": "aegis-v47-4-diagnostic-right-kick-overlay",
        "name": "Aegis V47.4 Diagnostic Right-Leg Kick Overlay",
        "schema": "aegis.overlay.curves.v2",
        "sourceFormat": "AUTHOR_DIAGNOSTIC_AEGIS_PRY_V47_4",
        "durationSeconds": duration,
        "frameTime": 1.0 / fps,
        "fps": fps,
        "frameCount": int(round(duration * fps)) + 1,
        "playbackMode": "LiveBaseGeneratedOverlay",
        "basePoseMode": "UseLiveSourcePose",
        "skeletonProfile": "UE5_Mannequin",
        "coordinateSystem": {
            "aegisPryConvention": "rot_x=pitch/sagittal_swing, rot_y=roll/lateral, rot_z=yaw_or_twist",
            "limbSagittalSwingAxis": "rot_x",
            "twistAxis": "rot_z_limited",
            "rotationUnits": "degrees",
        },
        "motionPrior": {
            "version": "V47.4",
            "generationMode": "authored_axis_diagnostic_kick_v47_4",
            "sourceKind": "authored_diagnostic",
            "retrievedClip": None,
            "exactActionMatch": True,
        },
        "generationParameters": {
            "action": "soccer_kick_overlay",
            "style": "diagnostic",
            "dominantLeg": args.dominant_leg,
            "generator": "authored_aegis_pry_diagnostic_v47_4",
            "sourceRetarget": "none_diagnostic_axis_probe",
        },
        "qualityReport": {
            "curveCount": len(curves),
            "nonzeroCurveCount": sum(1 for c in curves if any(abs(k["value"]) > 1e-5 for k in c["keys"])),
            "semanticWarning": "Diagnostic only: use this to verify Aegis importer/runtime axis mapping before running the ML job.",
        },
        "curves": curves,
    }
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(f"Wrote {out} with {len(curves)} curves")


if __name__ == "__main__":
    main()
