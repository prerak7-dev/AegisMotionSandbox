from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, List, Optional

BONES: List[str] = [
    "pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "head",
    "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l",
    "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
    "thigh_l", "calf_l", "foot_l", "ball_l",
    "thigh_r", "calf_r", "foot_r", "ball_r",
]

ROOT_CHANNELS = ["loc_x", "loc_y", "loc_z"]
QUAT_CHANNELS = ["rot_qx", "rot_qy", "rot_qz", "rot_qw"]
CONTACT_CHANNELS = [
    "foot_l.ik_lock_alpha",
    "foot_r.ik_lock_alpha",
    "foot_l.plant_lock_alpha",
    "foot_r.plant_lock_alpha",
]

@dataclass
class MotionCondition:
    action: str
    style: str = "neutral"
    dominant_leg: str = "right"
    duration_seconds: float = 1.35
    skeleton_profile: str = "UE5_Mannequin"
    intensity: float = 1.0

@dataclass
class TensorSpec:
    fps: int = 60
    max_frames: int = 180
    skeleton_profile: str = "UE5_Mannequin"

@dataclass
class GeneratedMotion:
    tensor: Any
    condition: MotionCondition
    metadata: Dict[str, Any]
