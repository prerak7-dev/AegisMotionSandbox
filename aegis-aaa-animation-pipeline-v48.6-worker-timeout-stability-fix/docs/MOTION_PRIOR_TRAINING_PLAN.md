# V37 Motion Prior Training Plan

## Goal

Generate `LiveBaseGeneratedOverlay` action clips that layer over Manny/Quinn locomotion and look like authored game animation.

## Dataset requirements

Each training clip should be retargeted to the same skeleton profile:

```text
UE5_Mannequin / Manny / Quinn
```

Each clip should export:

```text
pelvis.loc_x/y/z
bone.rot_qx/qy/qz/qw
foot_l/foot_r contact curves
phase markers
action/style metadata
```

## Representation

V37 trains on:

```text
root location: 3 values
per-bone rotation: 6D rotation representation
contacts: 4 values
```

The exporter converts 6D rotations back to quaternions for Aegis JSON.

## Training losses to add next

The included training script starts with denoising reconstruction. For serious quality, add:

```text
L_rot: 6D rotation reconstruction
L_vel: first-derivative smoothness
L_acc: second-derivative smoothness
L_fk: forward-kinematic joint position loss
L_contact: foot contact classification
L_footslide: planted foot velocity penalty
L_root: root trajectory/velocity loss
```

## Motion retrieval before full generative model

The fastest route to visible quality is retrieval + warping:

```text
retrieve closest kick clip
slice plant/strike/follow-through
time-warp to requested action duration
export as live-base overlay
```

Then use the neural model as a denoiser/blender.

## Future model

A stronger V38/V39 model should be:

```text
conditional diffusion transformer
conditioned on:
- action type
- style
- desired trajectory
- target point
- dominant limb
- contact schedule
```
