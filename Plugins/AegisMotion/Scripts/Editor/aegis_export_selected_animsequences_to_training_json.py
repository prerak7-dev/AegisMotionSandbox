# Aegis V39 Training Clip Exporter — Unreal Python utility
#
# Usage in Unreal:
# 1. Select one or more Manny/Quinn AnimSequence assets in the Content Browser.
# 2. Run this script from the Python console or Editor Utility.
# 3. It writes JSON files to Saved/AegisTrainingExports.
#
# This is a practical bridge until the full C++ editor tool is integrated.

import json
import os
import unreal

BONES = [
    "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head",
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
    "thigh_l", "calf_l", "foot_l", "ball_l",
    "thigh_r", "calf_r", "foot_r", "ball_r",
]

def _keys(values, duration):
    n = len(values)
    return [{"time": round(i / max(1, n-1) * duration, 5), "value": round(float(v), 7)} for i, v in enumerate(values)]

def _curve(name, joint, channel, values, duration):
    return {
        "curveName": name,
        "jointName": joint,
        "channelName": channel,
        "keys": _keys(values, duration),
        "originalKeyCount": len(values),
        "compressedKeyCount": len(values),
        "interpolation": "linear",
        "preserveKeys": True,
    }

def export_anim_sequence(anim_seq, out_dir, action="unknown", style="authored", dominant_leg="unknown", fps=60):
    duration = float(anim_seq.get_editor_property("sequence_length"))
    frame_count = max(2, int(round(duration * fps)) + 1)

    # NOTE:
    # Unreal Python does not expose identical raw-track sampling APIs across all UE versions.
    # This utility writes the metadata shell and a place for curves.
    # For production, implement the sampling in the C++ exporter using UAnimSequence::GetBoneTransform.
    curves = []

    doc = {
        "id": anim_seq.get_name(),
        "name": anim_seq.get_name(),
        "sourceFormat": "AEGIS_TRAINING_EXPORT_V39_UNREAL_PYTHON_SHELL",
        "durationSeconds": duration,
        "frameTime": 1.0 / fps,
        "frameCount": frame_count,
        "playbackMode": "LiveBaseGeneratedOverlay",
        "basePoseMode": "UseLiveSourcePose",
        "skeletonProfile": "UE5_Mannequin",
        "generationParameters": {
            "action": action,
            "style": style,
            "dominantLeg": dominant_leg,
            "quality": "unreal_animsequence_export"
        },
        "exporterNote": "This Python utility creates the training JSON shell. Use the V39 C++ exporter implementation for actual per-bone sampled curves.",
        "curves": curves,
    }

    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, anim_seq.get_name() + ".json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)
    unreal.log("Aegis exported training JSON shell: " + path)
    return path

def main():
    selected = unreal.EditorUtilityLibrary.get_selected_assets()
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), "AegisTrainingExports")
    count = 0
    for asset in selected:
        if isinstance(asset, unreal.AnimSequence):
            export_anim_sequence(asset, out_dir, action="soccer_kick_overlay", style="powerful", dominant_leg="right")
            count += 1
    unreal.log("Aegis V39 export complete. Exported %d AnimSequence asset(s)." % count)

main()
