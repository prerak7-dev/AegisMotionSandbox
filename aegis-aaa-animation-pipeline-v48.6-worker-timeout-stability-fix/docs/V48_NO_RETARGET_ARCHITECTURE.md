# V48 No-Retarget Architecture

## Why V48 exists

The previous Bandai/BVH/retarget path produced valid JSON files but not reliably good soccer kicks. GPU crashes while editing Unreal retarget poses made the path operationally unsafe. V48 therefore removes retargeting from the production pipeline and generates directly in the Aegis runtime contract.

## Runtime contract

V48 outputs quaternion live-base overlays:

```text
playbackMode = LiveBaseGeneratedOverlay
basePoseMode = UseLiveAnimGraphSourcePoseEveryFrame
rotation channels = bone.rot_qx, bone.rot_qy, bone.rot_qz, bone.rot_qw
```

Scalar `rot_x/rot_y/rot_z` curves are not production runtime channels in V48.

## Variant generation

The V48 generator treats the V36 kick as a gold template. It does not hallucinate unbounded animation. It applies controlled quaternion edits by semantic layer:

```text
root/pelvis
spine/head
arms
plant leg
kicking thigh
kicking calf/knee
kicking foot/ankle
```

The generator applies style-specific phase and strength parameters for:

```text
inside_foot_pass
instep_power_shot
side_foot_shot
volley_preparation
low_driven_kick
followthrough_heavy
short_tap
```

## Phase model

The generated variants share a phase model:

```text
blend_in_livebase
plant_side_stabilizes
kicking_hip_loads
pelvis_opens
thigh_drive
knee_snap
strike_contact
ankle_whip
follow_through
blend_back_to_livebase
```

This directly encodes the requested football biomechanics: plant stability, pelvis before thigh, thigh before calf, knee snap near strike, ankle whip, upper-body counterbalance, and head focus.

## Neural refinement

The V48 training step uses a lightweight neural denoising/refinement prior over the generated quaternion tensor dataset. The deterministic phase generator remains authoritative. The model is intended to refine:

```text
timing smoothness
joint coupling
follow-through realism
contact timing
style/intensity interpolation
jitter reduction
foot-plant preservation
```

## Job steps

```text
V48_LOAD_GOLD_OVERLAY
V48_GENERATE_SYNTHETIC_VARIANTS
V48_BUILD_QUATERNION_DATASET
V48_TRAIN_QUATERNION_PRIOR
V48_GENERATE_IMPORT_JSON
V48_VALIDATE_IMPORT_JSON
COMPLETE
```

## Legacy branch

Legacy Bandai scripts are retained for research and comparison only. They should not be used for production soccer-kick generation until retargeting and visual validation are solved.
