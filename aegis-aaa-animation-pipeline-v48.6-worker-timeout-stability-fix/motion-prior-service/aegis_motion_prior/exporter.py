from __future__ import annotations

from typing import Dict, List, Tuple

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


def _curve(name: str, joint: str, channel: str, values: np.ndarray, duration: float, unit: str = "unitless") -> Dict:
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


def _quat_xyzw_to_euler_xyz_degrees(q: np.ndarray) -> np.ndarray:
    """Legacy direct Euler conversion retained only for diagnostics.

    V47.3 no longer uses this as the default exporter path because direct Euler
    decomposition of retargeted local quaternions can cross the +/-180 boundary
    and produce 360-degree spins in the Aegis scalar curve runtime.
    """
    q = np.asarray(q, dtype=np.float32)
    norm = np.linalg.norm(q, axis=-1, keepdims=True)
    q = q / np.maximum(norm, 1e-8)
    x, y, z, w = np.moveaxis(q, -1, 0)

    t0 = 2.0 * (w * x + y * z)
    t1 = 1.0 - 2.0 * (x * x + y * y)
    roll_x = np.arctan2(t0, t1)

    t2 = 2.0 * (w * y - z * x)
    t2 = np.clip(t2, -1.0, 1.0)
    pitch_y = np.arcsin(t2)

    t3 = 2.0 * (w * z + x * y)
    t4 = 1.0 - 2.0 * (y * y + z * z)
    yaw_z = np.arctan2(t3, t4)

    return np.stack([roll_x, pitch_y, yaw_z], axis=-1).astype(np.float32) * (180.0 / np.pi)


def _quat_xyzw_to_overlay_rotvec_degrees(q: np.ndarray) -> np.ndarray:
    """Convert additive quaternions into stable scalar overlay channels.

    The plugin currently consumes scalar rot_x/rot_y/rot_z curves.  Direct Euler
    extraction is unsafe for long mocap clips because equivalent quaternions can
    decompose to +/-180-degree Euler values and scalar interpolation will spin the
    character.  For additive overlay curves we want the shortest local angular
    displacement from the live pose, so we export quaternion log-map / rotation
    vector components in degrees.  For small-to-medium animation deltas this is
    visually stable and avoids wraparound discontinuities.
    """
    q = normalize_quat(ensure_quat_continuity(q))
    # Force the shortest representation per sample.  q and -q are identical, but
    # a negative w would produce an angle > 180 degrees in the log map.
    q = np.where(q[..., 3:4] < 0.0, -q, q)
    xyz = q[..., 0:3]
    w = np.clip(q[..., 3], -1.0, 1.0)
    sin_half = np.linalg.norm(xyz, axis=-1)
    angle = 2.0 * np.arctan2(sin_half, w)
    axis = xyz / np.maximum(sin_half[..., None], 1e-8)
    rotvec = axis * angle[..., None]
    rotvec = np.where((sin_half[..., None] < 1e-6), 0.0, rotvec)
    return rotvec.astype(np.float32) * (180.0 / np.pi)


def _smooth_1d(values: np.ndarray, passes: int = 1, strength: float = 0.22) -> np.ndarray:
    y = np.asarray(values, dtype=np.float32).copy()
    if len(y) < 5:
        return y
    strength = float(np.clip(strength, 0.0, 1.0))
    for _ in range(max(0, int(passes))):
        avg = y.copy()
        avg[1:-1] = (y[:-2] + 2.0 * y[1:-1] + y[2:]) * 0.25
        y = (1.0 - strength) * y + strength * avg
    return y.astype(np.float32)


def _despike_curve(values: np.ndarray, max_step: float) -> Tuple[np.ndarray, int]:
    """Limit scalar key-to-key jumps so Unreal never interpolates a spin."""
    y = np.asarray(values, dtype=np.float32).copy()
    corrections = 0
    if len(y) < 2:
        return y, corrections
    max_step = float(max(1.0, max_step))
    for i in range(1, len(y)):
        delta = float(y[i] - y[i - 1])
        if abs(delta) > max_step:
            y[i] = y[i - 1] + np.sign(delta) * max_step
            corrections += 1
    return y.astype(np.float32), corrections


def _aegis_pry_channel_remap(bone: str, raw: np.ndarray) -> Tuple[np.ndarray, str]:
    """Map quaternion log-map axes into the Aegis data-asset PRY channels.

    V47.3 exported the log-map vector directly as rot_x/rot_y/rot_z and allowed
    large lower-body rot_z values.  In the current Aegis runtime/data asset,
    rot_z behaves like yaw/twist for the Manny legs, so that produced planted
    foot shuffling and vertical leg/torso counter-rotation instead of a forward
    kick.  V47.4 makes rot_x the primary sagittal swing channel for legs and
    treats rot_z as a tightly limited twist/yaw channel.
    """
    raw = np.asarray(raw, dtype=np.float32)
    mapped = raw.copy()
    method = "identity_logmap_to_rot_xyz"
    if bone.startswith("thigh"):
        # Thigh swing in the Bandai/retarget log-map tends to land mostly on Z,
        # while the current Aegis data asset reads visible sagittal kick swing
        # from rot_x. Keep twist/yaw as a small accent only.
        mapped = np.stack([raw[:, 2], raw[:, 1], raw[:, 0] * 0.25], axis=1).astype(np.float32)
        method = "thigh_logmap_z_to_aegis_rot_x_twist_damped"
    elif bone.startswith("calf"):
        # V47.4 accidentally used the same Z->rot_x mapping for calves. In the
        # generated kick clips the knee flexion energy is usually on source/log
        # X, so it was damped into rot_z and then clamped to about 14 degrees.
        # That is the main reason the imported result looked like a shuffle
        # instead of a kick. V47.5 maps calf X directly to rot_x.
        mapped = np.stack([raw[:, 0], raw[:, 1] * 0.35, raw[:, 2] * 0.25], axis=1).astype(np.float32)
        method = "calf_logmap_x_to_aegis_rot_x_knee_flexion"
    elif bone.startswith(("foot", "ball")):
        # Feet generally follow the thigh swing axis from Z, with some roll for
        # toe/ankle presentation. Twist remains limited.
        mapped = np.stack([raw[:, 2], raw[:, 1], raw[:, 0] * 0.25], axis=1).astype(np.float32)
        method = "foot_logmap_z_to_aegis_rot_x_twist_damped"
    elif bone.startswith(("upperarm", "lowerarm", "hand")):
        # Arms can keep broad swing, but reduce yaw/twist so they support the
        # kick instead of stealing the silhouette.
        mapped = np.stack([raw[:, 0], raw[:, 1], raw[:, 2] * 0.55], axis=1).astype(np.float32)
        method = "upper_body_twist_damped"
    elif bone.startswith("spine") or bone in {"neck_01", "head"}:
        mapped = np.stack([raw[:, 0], raw[:, 1], raw[:, 2] * 0.45], axis=1).astype(np.float32)
        method = "spine_head_yaw_damped"
    return mapped, method


def _limits_for_bone(bone: str) -> Tuple[float, float, float]:
    """Safe Aegis PRY additive limits in degrees: rot_x=pitch/swing, rot_y=roll/lateral, rot_z=yaw/twist."""
    if bone == "pelvis":
        return (24.0, 18.0, 18.0)
    if bone.startswith("spine"):
        return (26.0, 18.0, 18.0)
    if bone in {"neck_01", "head"}:
        return (26.0, 28.0, 24.0)
    if bone.startswith("clavicle"):
        return (22.0, 22.0, 18.0)
    if bone.startswith("upperarm"):
        return (70.0, 50.0, 38.0)
    if bone.startswith("lowerarm"):
        return (95.0, 28.0, 30.0)
    if bone.startswith("hand"):
        return (35.0, 25.0, 22.0)
    if bone.startswith("thigh"):
        return (95.0, 36.0, 22.0)
    if bone.startswith("calf"):
        return (125.0, 18.0, 14.0)
    if bone.startswith("foot"):
        return (75.0, 32.0, 24.0)
    if bone.startswith("ball"):
        return (45.0, 20.0, 18.0)
    return (45.0, 35.0, 25.0)


def _sanitize_overlay_rotation_channels(bone: str, raw: np.ndarray) -> Tuple[np.ndarray, Dict]:
    raw = np.asarray(raw, dtype=np.float32)
    remapped, remap_method = _aegis_pry_channel_remap(bone, raw)
    out = np.zeros_like(raw, dtype=np.float32)
    limits = _limits_for_bone(bone)
    max_steps = tuple(max(10.0, limit * 0.35) for limit in limits)
    corrections = 0
    clipped = 0
    for axis in range(3):
        values = remapped[:, axis]
        before_clip = values.copy()
        values = np.clip(values, -limits[axis], limits[axis])
        clipped += int(np.sum(np.abs(before_clip - values) > 1e-4))
        values, c = _despike_curve(values, max_steps[axis])
        corrections += c
        values = _smooth_1d(values, passes=2, strength=0.16)
        # Preserve additive contract exactly.
        if len(values):
            values = values - values[0]
            values = np.clip(values, -limits[axis], limits[axis])
        out[:, axis] = values
    stats = {
        "bone": bone,
        "limits": {"rot_x": limits[0], "rot_y": limits[1], "rot_z": limits[2]},
        "remapMethod": remap_method,
        "rawMaxAbsDegrees": float(np.max(np.abs(raw))) if raw.size else 0.0,
        "remappedMaxAbsDegrees": float(np.max(np.abs(remapped))) if remapped.size else 0.0,
        "sanitizedMaxAbsDegrees": float(np.max(np.abs(out))) if out.size else 0.0,
        "clippedKeyCount": clipped,
        "jumpCorrectionCount": corrections,
    }
    return out.astype(np.float32), stats


def _rms(values: np.ndarray) -> float:
    values = np.asarray(values, dtype=np.float32)
    return float(np.sqrt(np.mean(values * values))) if values.size else 0.0


def _zero_first(values: np.ndarray) -> np.ndarray:
    y = np.asarray(values, dtype=np.float32).copy()
    if len(y):
        y = y - y[0]
    return y.astype(np.float32)


def _peak_abs(values: np.ndarray) -> float:
    values = np.asarray(values, dtype=np.float32)
    return float(np.max(np.abs(values))) if values.size else 0.0


def _apply_soccer_kick_anatomical_focus(rotations: Dict[str, np.ndarray], condition: Dict) -> Dict:
    """Make generated scalar overlays read as a soccer kick, not a generic mocap shuffle.

    This is intentionally conservative and runs after quaternion-logmap sanitizing.
    It does not invent a kick from a non-kick source; source gating still handles
    that. It fixes the common retarget/export failure where the thigh has a clear
    swing curve but the knee flexion got lost during axis remapping or sanitizer
    clamping. The diagnostic overlay proved the Aegis runtime reads a kick best as:
      thigh.rot_x = windup/back-swing -> forward strike
      calf.rot_x  = opposite-sign knee bend/extension
      foot.rot_x  = smaller toe/ankle presentation
    """
    action = str(condition.get("action", "")).lower()
    if "kick" not in action:
        return {"applied": False, "reason": "not_a_kick_action"}

    dominant = str(condition.get("dominantLeg", "right")).lower()
    kick_suffix = "_l" if dominant.startswith("l") else "_r"
    plant_suffix = "_r" if kick_suffix == "_l" else "_l"

    thigh_name = f"thigh{kick_suffix}"
    calf_name = f"calf{kick_suffix}"
    foot_name = f"foot{kick_suffix}"
    if thigh_name not in rotations or calf_name not in rotations or foot_name not in rotations:
        return {"applied": False, "reason": "dominant_leg_tracks_missing"}

    thigh = rotations[thigh_name][:, 0].astype(np.float32)
    calf = rotations[calf_name][:, 0].astype(np.float32)
    foot = rotations[foot_name][:, 0].astype(np.float32)

    thigh_peak = _peak_abs(thigh)
    calf_peak_before = _peak_abs(calf)
    foot_peak_before = _peak_abs(foot)

    report = {
        "applied": True,
        "version": "V47.5",
        "dominantLeg": "left" if kick_suffix == "_l" else "right",
        "thighPeakBefore": round(thigh_peak, 4),
        "calfPeakBefore": round(calf_peak_before, 4),
        "footPeakBefore": round(foot_peak_before, 4),
        "kneeCouplingApplied": False,
        "footCouplingApplied": False,
        "plantLegDampingApplied": True,
        "twistDampingApplied": True,
    }

    if thigh_peak > 35.0:
        # If the calf is too weak relative to the thigh, synthesize anatomical
        # knee counter-rotation from the thigh curve. This mirrors the diagnostic
        # kick overlay that looked correct in Unreal: thigh +82, calf about -105.
        calf_synth = _zero_first(_smooth_1d(np.clip(-1.22 * thigh, -118.0, 118.0), passes=2, strength=0.28))
        if calf_peak_before < max(34.0, 0.52 * thigh_peak):
            calf = calf_synth
            report["kneeCouplingApplied"] = True
        else:
            calf = _zero_first(_smooth_1d(np.clip(0.72 * calf + 0.28 * calf_synth, -118.0, 118.0), passes=1, strength=0.2))

        foot_synth = _zero_first(_smooth_1d(np.clip(0.52 * thigh - 0.10 * calf, -62.0, 62.0), passes=2, strength=0.26))
        if foot_peak_before < 22.0 or foot_peak_before > 68.0:
            foot = foot_synth
            report["footCouplingApplied"] = True
        else:
            foot = _zero_first(_smooth_1d(np.clip(0.55 * foot + 0.45 * foot_synth, -68.0, 68.0), passes=1, strength=0.2))

        rotations[calf_name][:, 0] = calf
        rotations[foot_name][:, 0] = foot

    # Make the dominant leg read as a sagittal hinge instead of a twist/shuffle.
    for bone, y_scale, z_scale, y_limit, z_limit in [
        (thigh_name, 0.45, 0.35, 18.0, 10.0),
        (calf_name, 0.25, 0.18, 8.0, 5.0),
        (foot_name, 0.45, 0.35, 16.0, 12.0),
    ]:
        if bone in rotations:
            rotations[bone][:, 1] = _zero_first(_smooth_1d(np.clip(rotations[bone][:, 1] * y_scale, -y_limit, y_limit), passes=1, strength=0.22))
            rotations[bone][:, 2] = _zero_first(_smooth_1d(np.clip(rotations[bone][:, 2] * z_scale, -z_limit, z_limit), passes=1, strength=0.22))

    # The plant leg should stabilize the pose. If it swings as much as the kick
    # leg, the result reads as foot shuffling or a march instead of a kick.
    for bone, x_scale, yz_scale, x_limit, y_limit, z_limit in [
        (f"thigh{plant_suffix}", 0.32, 0.35, 30.0, 14.0, 10.0),
        (f"calf{plant_suffix}", 0.25, 0.25, 20.0, 8.0, 6.0),
        (f"foot{plant_suffix}", 0.32, 0.35, 22.0, 12.0, 8.0),
    ]:
        if bone in rotations:
            rotations[bone][:, 0] = _zero_first(_smooth_1d(np.clip(rotations[bone][:, 0] * x_scale, -x_limit, x_limit), passes=1, strength=0.22))
            rotations[bone][:, 1] = _zero_first(_smooth_1d(np.clip(rotations[bone][:, 1] * yz_scale, -y_limit, y_limit), passes=1, strength=0.22))
            rotations[bone][:, 2] = _zero_first(_smooth_1d(np.clip(rotations[bone][:, 2] * yz_scale, -z_limit, z_limit), passes=1, strength=0.22))

    report["calfPeakAfter"] = round(_peak_abs(rotations[calf_name][:, 0]), 4)
    report["footPeakAfter"] = round(_peak_abs(rotations[foot_name][:, 0]), 4)
    report["plantThighPeakAfter"] = round(_peak_abs(rotations.get(f"thigh{plant_suffix}", np.zeros((0, 3), dtype=np.float32))[:, 0]), 4) if f"thigh{plant_suffix}" in rotations else 0.0
    return report


def export_aegis_overlay_json(tensor: np.ndarray, condition: Dict, metadata: Dict | None = None) -> Dict:
    metadata = metadata or {}
    duration = float(condition.get("durationSeconds", metadata.get("durationSeconds", 1.35)))
    fps = int(condition.get("fps", metadata.get("fps", 60)))
    include_quats = bool(condition.get("includeQuaternionReference", False))
    comps = tensor_to_components(tensor)
    curves: List[Dict] = []

    root = comps["root"]
    # Emit both trans_* and legacy loc_* root channels so existing importer variants can bind.
    for i, ch in enumerate(["trans_x", "trans_y", "trans_z"]):
        curves.append(_curve(f"pelvis.{ch}", "pelvis", ch, root[:, i], duration, unit="cm"))
    for i, ch in enumerate(ROOT_CHANNELS):
        curves.append(_curve(f"pelvis.{ch}", "pelvis", ch, root[:, i], duration, unit="cm"))

    sanitizer_reports: List[Dict] = []
    all_rot_values: List[np.ndarray] = []
    sanitized_rotations: Dict[str, np.ndarray] = {}
    quaternion_reference: Dict[str, np.ndarray] = {}
    for bone in BONES:
        q = comps["quats"][bone]
        raw = _quat_xyzw_to_overlay_rotvec_degrees(q)
        e, report = _sanitize_overlay_rotation_channels(bone, raw)
        sanitizer_reports.append(report)
        sanitized_rotations[bone] = e
        quaternion_reference[bone] = q

    anatomical_focus_report = _apply_soccer_kick_anatomical_focus(sanitized_rotations, condition)

    for bone in BONES:
        e = sanitized_rotations[bone]
        all_rot_values.append(e.reshape(-1))
        for i, ch in enumerate(["rot_x", "rot_y", "rot_z"]):
            curves.append(_curve(f"{bone}.{ch}", bone, ch, e[:, i], duration, unit="degrees"))
        if include_quats:
            q = quaternion_reference[bone]
            for i, ch in enumerate(QUAT_CHANNELS):
                curves.append(_curve(f"{bone}.{ch}", bone, ch, q[:, i], duration, unit="quaternion_reference"))

    for name, values in comps["contacts"].items():
        joint, channel = name.split(".", 1)
        curves.append(_curve(name, joint, channel, values, duration, unit="alpha"))

    frame_count = int(tensor.shape[0])
    nonzero_curve_count = sum(1 for c in curves if any(abs(float(k.get("value", 0.0))) > 1e-5 for k in c.get("keys", [])))
    rot_concat = np.concatenate(all_rot_values) if all_rot_values else np.zeros((0,), dtype=np.float32)
    rotation_rms = _rms(rot_concat)
    max_abs_rotation = float(np.max(np.abs(rot_concat))) if rot_concat.size else 0.0
    clipped_total = sum(int(r["clippedKeyCount"]) for r in sanitizer_reports)
    jump_total = sum(int(r["jumpCorrectionCount"]) for r in sanitizer_reports)

    return {
        "id": condition.get("id", "aegis-v47-neural-overlay"),
        "name": condition.get("name", "Aegis V47 Neural Generated Overlay"),
        "schema": "aegis.overlay.curves.v2",
        "sourceFormat": "AI_NATIVE_UE5_MANNEQUIN_NEURAL_OVERLAY_PRIOR_V47_5_KNEE_COUPLED",
        "durationSeconds": duration,
        "frameTime": 1.0 / fps,
        "fps": fps,
        "frameCount": frame_count,
        "playbackMode": "LiveBaseGeneratedOverlay",
        "basePoseMode": "UseLiveSourcePose",
        "skeletonProfile": condition.get("skeletonProfile", "UE5_Mannequin"),
        "coordinateSystem": {
            "pluginForwardAxis": "pelvis.trans_y",
            "pluginLateralAxis": "pelvis.trans_x",
            "pluginUpAxis": "pelvis.trans_z",
            "rotationSpace": "target_parent_bone_local_additive_logmap",
            "rotationUnits": "degrees",
            "quaternionReferenceCurvesIncluded": include_quats,
            "aegisPryConvention": "rot_x=pitch/sagittal_swing, rot_y=roll/lateral, rot_z=yaw_or_twist",
            "limbSagittalSwingAxis": "rot_x",
            "twistAxis": "rot_z_limited",
            "lateralAxis": "rot_y",
            "axisRemap": "lower_body_logmap_z_to_aegis_rot_x",
        },
        "motionPrior": {
            "version": "V47.5",
            "model": metadata.get("model", "AegisNeuralOverlayPriorV47"),
            "checkpoint": metadata.get("checkpoint", None),
            "trainingDataset": metadata.get("dataset", None),
            "generationMode": metadata.get("generationMode", "neural_retrieval_seed_refine"),
            "retrievedClip": metadata.get("retrievedClip"),
            "retrievalScore": metadata.get("score"),
            "timeWarpedFrames": metadata.get("timeWarpedFrames"),
            "validSourceFrames": metadata.get("validSourceFrames"),
            "sourceClipDurationSeconds": metadata.get("sourceClipDurationSeconds"),
            "trimmedSourceFrames": metadata.get("trimmedSourceFrames"),
            "trimStartFrame": metadata.get("trimStartFrame"),
            "trimEndFrame": metadata.get("trimEndFrame"),
            "autoDuration": metadata.get("autoDuration"),
            "autoDurationMode": metadata.get("autoDurationMode"),
            "exactActionMatch": metadata.get("exactActionMatch"),
            "sourceKind": metadata.get("sourceKind"),
        },
        "generationParameters": {
            "action": condition.get("action", "soccer_kick_overlay"),
            "style": condition.get("style", "active"),
            "dominantLeg": condition.get("dominantLeg", "right"),
            "intensity": condition.get("intensity", 1.0),
            "generator": "neural_overlay_prior_v47_5_knee_coupled",
            "sourceRetarget": "offline_bvh_to_manny_json",
        },
        "qualityReport": {
            "curveCount": len(curves),
            "nonzeroCurveCount": nonzero_curve_count,
            "rotationRmsDegrees": round(rotation_rms, 5),
            "maxAbsRotationDegrees": round(max_abs_rotation, 5),
            "sanitizerClippedKeyCount": clipped_total,
            "sanitizerJumpCorrectionCount": jump_total,
            "anatomicalFocus": anatomical_focus_report,
            "hasCurvesArrayForAegisImporter": True,
            "quaternionReferenceCurvesIncluded": include_quats,
        },
        "rotationSanitizer": {
            "version": "V47.5",
            "method": "quaternion_logmap_shortest_path_plus_aegis_pry_axis_remap_plus_joint_limits_plus_knee_coupling",
            "reports": sanitizer_reports,
        },
        "curves": curves,
    }
