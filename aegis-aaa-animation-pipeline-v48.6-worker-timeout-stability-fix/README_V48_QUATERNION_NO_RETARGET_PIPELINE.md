# Aegis V48 Quaternion No-Retarget Soccer Kick Pipeline

V48 removes the Bandai/BVH/FBX/Unreal IK Retargeter path from the production workflow. The production path now starts from the known-good `ai_soccer_kick_livebase_overlay_v36.json` and generates Manny/Quinn-native quaternion overlays directly.

## Production flow

```text
V36 gold quaternion live-base overlay
→ controlled V48 synthetic kick variants
→ quaternion tensor dataset
→ V48 neural refinement prior
→ final Aegis import JSON
→ V48 quaternion validation
```

No Bandai clone. No BVH. No FBX. No Unreal IK Retargeter. No retarget-pose editing.

## Main config

```text
config/aegis_v48_no_retarget.config.json
```

The gold reference is stored at:

```text
sample-data/gold/ai_soccer_kick_livebase_overlay_v36.json
```

The final output is:

```text
exports/aegis_v48_quaternion_soccer_kick_overlay.json
```

## Phase generation contract

Every generated import JSON carries these phase rules as first-class metadata and uses them to drive the variant generator:

```text
plant foot stays grounded
pelvis opens before thigh drive
thigh leads before calf extension
calf/knee snaps through near strike
foot follows the knee, then whips through
spine and arms counterbalance
head stays ball-focused
```

The same biomechanical intent is also encoded as:

```text
pelvis opens
plant side stabilizes
kicking hip loads
thigh accelerates
knee snaps
ankle whips
torso counters
arms balance
head remains focused
```

## ML refinement targets

The neural prior is trained only after clean synthetic quaternion variants are generated. Its job is refinement, not retarget rescue:

```text
smooth timing
correct joint coupling
add realistic follow-through
predict contact timing
adjust style/intensity
remove jitter
preserve foot plant
```

## Dashboard controls

The dashboard now exposes:

```text
Kick Style
Dominant Leg
Overlay Duration
Training Variant Count
Intensity
Follow-through
Plant Stability
Upper-body Counterbalance
```

The old Bandai path remains as `LEGACY_BANDAI_EXPERIMENTAL`, but the default dashboard mode is `V48_QUATERNION_NO_RETARGET`.

## Scripts

```powershell
# 1. Verify the V36 gold reference
.\scripts\v48-01-load-gold-overlay.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json

# 2. Generate controlled Manny/Quinn-native quaternion variants
.\scripts\v48-02-generate-quaternion-variants.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json

# 3. Build tensor dataset
.\scripts\v48-03-build-quaternion-dataset.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json

# 4. Train neural refinement prior
.\scripts\v48-04-train-quaternion-prior.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json

# 5. Generate final import JSON
.\scripts\v48-05-generate-import-json.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json

# 6. Validate final import JSON
.\scripts\v48-06-validate-import-json.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
```

Or run the job from the dashboard after starting backend services:

```powershell
.\scripts\v46-build-backend.ps1
.\scripts\v46-start-worker.ps1
.\scripts\v46-open-dashboard.ps1
```

## Validation expectations

A valid V48 production import JSON must have:

```text
sourceFormat: AI_NATIVE_UE5_MANNEQUIN_LIVE_BASE_OVERLAY_V48_QUATERNION_NO_RETARGET
playbackMode: LiveBaseGeneratedOverlay
basePoseMode: UseLiveAnimGraphSourcePoseEveryFrame
rot_qx / rot_qy / rot_qz / rot_qw curves for all bones
phase markers for plant, pelvis open, thigh drive, knee snap, strike, ankle whip, follow-through
no scalar runtime rot_x / rot_y / rot_z curves
```

Debug scalar curves are allowed only as `debug_*` curves.

## Included smoke output

This package includes a smoke-generated fallback output created from V48 variants without a trained checkpoint:

```text
exports/aegis_v48_quaternion_soccer_kick_overlay.json
exports/aegis_v48_quaternion_soccer_kick_overlay.json.validation.json
```

The validation report should show:

```text
valid: true
quatCurveCount: 88
scalarRuntimeCurveCount: 0
errors: []
```
