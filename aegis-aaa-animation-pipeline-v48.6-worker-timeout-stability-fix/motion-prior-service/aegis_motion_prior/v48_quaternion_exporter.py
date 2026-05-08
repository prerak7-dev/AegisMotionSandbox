from __future__ import annotations

from typing import Dict, List
import numpy as np

from .schema import BONES, ROOT_CHANNELS, QUAT_CHANNELS
from .tensorize import tensor_to_components
from .quaternion import normalize_quat, ensure_quat_continuity


def _keys(values: np.ndarray, duration: float) -> List[Dict]:
    values = np.asarray(values, dtype=np.float32)
    n = len(values)
    if n <= 1:
        return [{"time": 0.0, "value": round(float(values[0]) if n else 0.0, 7)}]
    return [
        {"time": round(i / (n - 1) * duration, 5), "value": round(float(values[i]), 7)}
        for i in range(n)
    ]


def _curve(name: str, joint: str, channel: str, values: np.ndarray, duration: float, unit: str) -> Dict:
    values = np.asarray(values, dtype=np.float32)
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


def _quat_to_rotvec_degrees(q: np.ndarray) -> np.ndarray:
    q = normalize_quat(ensure_quat_continuity(q))
    q = np.where(q[..., 3:4] < 0.0, -q, q)
    xyz = q[..., 0:3]
    w = np.clip(q[..., 3], -1.0, 1.0)
    sin_half = np.linalg.norm(xyz, axis=-1)
    angle = 2.0 * np.arctan2(sin_half, w)
    axis = xyz / np.maximum(sin_half[..., None], 1e-8)
    rotvec = axis * angle[..., None]
    rotvec = np.where((sin_half[..., None] < 1e-6), 0.0, rotvec)
    return rotvec.astype(np.float32) * (180.0 / np.pi)


def default_phase_markers() -> List[Dict]:
    return [
        {"name": "blend_in_livebase", "time01": 0.0, "role": "enter overlay without popping"},
        {"name": "plant_side_stabilizes", "time01": 0.18, "role": "plant foot stays grounded and weight settles"},
        {"name": "kicking_hip_loads", "time01": 0.34, "role": "backswing/load before acceleration"},
        {"name": "pelvis_opens", "time01": 0.52, "role": "pelvis opens before thigh drive"},
        {"name": "thigh_drive", "time01": 0.66, "role": "thigh leads before calf extension"},
        {"name": "knee_snap", "time01": 0.76, "role": "calf/knee snaps through near strike"},
        {"name": "strike_contact", "time01": 0.80, "role": "ball contact / peak foot velocity"},
        {"name": "ankle_whip", "time01": 0.84, "role": "foot follows knee, then whips through"},
        {"name": "follow_through", "time01": 0.91, "role": "realistic follow-through and recovery"},
        {"name": "blend_back_to_livebase", "time01": 1.0, "role": "return to locomotion/source pose"},
    ]


def phase_generation_contract() -> Dict:
    return {
        "plantFootStaysGrounded": True,
        "pelvisOpensBeforeThighDrive": True,
        "thighLeadsBeforeCalfExtension": True,
        "kneeSnapsThroughNearStrike": True,
        "footFollowsKneeThenWhipsThrough": True,
        "spineAndArmsCounterbalance": True,
        "headStaysBallFocused": True,
        "biomechanics": [
            "pelvis opens",
            "plant side stabilizes",
            "kicking hip loads",
            "thigh accelerates",
            "knee snaps",
            "ankle whips",
            "torso counters",
            "arms balance",
            "head remains focused",
        ],
    }


def ml_refinement_contract() -> Dict:
    return {
        "smoothTiming": True,
        "correctJointCoupling": True,
        "addRealisticFollowThrough": True,
        "predictContactTiming": True,
        "adjustStyleAndIntensity": True,
        "removeJitter": True,
        "preserveFootPlant": True,
        "notes": "The neural prior refines clean Manny/Quinn-native quaternion variants; it is not asked to rescue retargeted scalar Euler motion.",
    }


def export_v48_quaternion_overlay_json(tensor: np.ndarray, condition: Dict, metadata: Dict | None = None) -> Dict:
    metadata = metadata or {}
    duration = float(condition.get("durationSeconds", metadata.get("durationSeconds", 1.35)) or 1.35)
    fps = int(condition.get("fps", metadata.get("fps", 120)) or 120)
    include_debug = bool(condition.get("includeDebugScalarCurves", True))
    comps = tensor_to_components(tensor)
    curves: List[Dict] = []

    root = comps["root"]
    for i, ch in enumerate(ROOT_CHANNELS):
        curves.append(_curve(f"pelvis.{ch}", "pelvis", ch, root[:, i], duration, unit="cm"))
    for i, ch in enumerate(["trans_x", "trans_y", "trans_z"]):
        curves.append(_curve(f"pelvis.{ch}", "pelvis", ch, root[:, i], duration, unit="cm"))

    max_abs_rotvec = 0.0
    quat_curve_count = 0
    for bone in BONES:
        q = normalize_quat(ensure_quat_continuity(comps["quats"][bone]))
        for i, ch in enumerate(QUAT_CHANNELS):
            curves.append(_curve(f"{bone}.{ch}", bone, ch, q[:, i], duration, unit="quaternion_xyzw"))
            quat_curve_count += 1
        if include_debug:
            rv = _quat_to_rotvec_degrees(q)
            max_abs_rotvec = max(max_abs_rotvec, float(np.max(np.abs(rv))) if rv.size else 0.0)
            for i, ch in enumerate(["debug_twist_deg", "debug_lateral_deg", "debug_sagittal_deg"]):
                curves.append(_curve(f"{bone}.{ch}", bone, ch, rv[:, i], duration, unit="degrees_debug_only"))

    for name, values in comps["contacts"].items():
        joint, channel = name.split(".", 1)
        curves.append(_curve(name, joint, channel, values, duration, unit="alpha"))

    style = str(condition.get("style", "instep_power_shot"))
    dominant_leg = str(condition.get("dominantLeg", "right"))
    return {
        "id": condition.get("id", f"aegis-v48-{style}-{dominant_leg}-quaternion-overlay"),
        "name": condition.get("name", f"Aegis V48 {style.replace('_', ' ').title()} Quaternion Live-Base Overlay"),
        "schema": "aegis.overlay.curves.v2",
        "sourceFormat": "AI_NATIVE_UE5_MANNEQUIN_LIVE_BASE_OVERLAY_V48_QUATERNION_NO_RETARGET",
        "durationSeconds": duration,
        "frameTime": 1.0 / fps,
        "fps": fps,
        "frameCount": int(tensor.shape[0]),
        "playbackMode": "LiveBaseGeneratedOverlay",
        "basePoseMode": "UseLiveAnimGraphSourcePoseEveryFrame",
        "skeletonProfile": condition.get("skeletonProfile", "UE5_Mannequin_Quinn_Manny"),
        "coordinateSystem": {
            "pluginForwardAxis": "pelvis.loc_y",
            "pluginLateralAxis": "pelvis.loc_x",
            "pluginUpAxis": "pelvis.loc_z",
            "rotationRuntimeContract": "quaternion_curves_are_authoritative",
            "rotationCurveOrder": "rot_qx, rot_qy, rot_qz, rot_qw",
            "limbSagittalSwingAxis": "local_z",
            "twistAxis": "local_x",
            "lateralAxis": "local_y",
            "retargeting": "none; generated directly in Manny/Quinn additive live-base space",
        },
        "generationParameters": {
            "action": condition.get("action", "soccer_kick_overlay"),
            "style": style,
            "dominantLeg": dominant_leg,
            "intensity": float(condition.get("intensity", 1.0)),
            "followThrough": float(condition.get("followThrough", 0.65)),
            "plantStability": float(condition.get("plantStability", 0.9)),
            "upperBodyCounterbalance": float(condition.get("upperBodyCounterbalance", 0.7)),
            "generator": "v48_manny_quinn_native_quaternion_phase_generator_plus_neural_prior",
            "criticalRuntimeRequirement": "This JSON is an overlay, not a standalone full-body run-up. Trigger it over live Manny/Quinn locomotion or an appropriate base pose.",
            "layers": {
                "baseLocomotion": "provided by AnimBP state machine/live source pose",
                "pelvisSpineArms": "phase-authored counterbalance",
                "plantLeg": "stabilizer/foot-lock overlay",
                "kickLeg": "dominant kick-chain quaternion overlay",
                "head": "target-focus metadata and small counter-rotation",
            },
            "phaseGenerationContract": phase_generation_contract(),
            "mlRefinementContract": ml_refinement_contract(),
        },
        "phaseMarkers": condition.get("phaseMarkers") or default_phase_markers(),
        "headFocus": {
            "enabled": True,
            "mode": "runtime_dynamic_look_at_target",
            "target": condition.get("lookAtTarget", "KickTarget"),
            "neckWeight": 0.35,
            "headWeight": 0.65,
            "maxYawDegrees": 55,
            "maxPitchDegrees": 35,
            "smoothingHalfLife": 0.08,
            "note": "Head remains ball-focused; runtime look-at should use these settings after overlay application.",
        },
        "footPlant": {
            "enabled": True,
            "dominantPlantFoot": "left" if dominant_leg.lower().startswith("r") else "right",
            "contactTimingSource": "phase_generator_plus_neural_refinement",
            "preserveFootPlant": True,
            "recommendedRuntime": "Apply generated plant_lock_alpha before/with late IK; do not let IK override kick foot during strike.",
        },
        "motionPrior": {
            "version": "V48",
            "model": metadata.get("model", "AegisQuaternionKickPriorV48"),
            "checkpoint": metadata.get("checkpoint"),
            "trainingDataset": metadata.get("dataset"),
            "generationMode": metadata.get("generationMode", "phase_variant_seed_neural_refine"),
            "goldReference": metadata.get("goldReference"),
            "variantSet": metadata.get("variantSet"),
            "trainingVariantCount": metadata.get("trainingVariantCount"),
            "neuralBlend": metadata.get("neuralBlend"),
        },
        "validationHints": {
            "requiresQuaternionCurves": True,
            "scalarRotXYZRuntime": "disabled_for_production; debug curves only",
            "quatCurveCount": quat_curve_count,
            "maxDebugRotvecDegrees": round(max_abs_rotvec, 4),
        },
        "curves": curves,
    }
