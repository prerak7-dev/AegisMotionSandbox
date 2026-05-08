# Aegis V47 — Neural Overlay JSON Pipeline

V47 is the first pipeline version where the default job runs from source-data acquisition to a final Aegis custom-data-asset overlay JSON without a manual Unreal retarget pause.

```text
Bandai repo
→ BVH clip discovery/selection
→ Blender offline retarget onto exported Manny/Quinn skeletal mesh FBX
→ offline retarget training JSON
→ Aegis tensor dataset
→ neural overlay-prior training
→ neural/retrieval-seeded overlay generation
→ JSON validation for the Aegis importer
→ exports/bandai_soccer_kick_overlay_v47.json
```

## What changed from V46

### 1. Default path is now automated offline retargeting

V46 still carried the legacy path:

```text
BVH → FBX → Unreal import → manual IK retarget → export AnimSequence JSON
```

V47 keeps those legacy scripts for comparison, but the default backend path is now:

```text
BVH → Blender offline target-rest retarget → Manny/Quinn training JSON
```

This removes the manual job pause and gives the ML pipeline the exact retargeted data it needs.

### 2. Final JSON is now importer-safe

The final generated overlay now has:

```json
{
  "schema": "aegis.overlay.curves.v2",
  "playbackMode": "LiveBaseGeneratedOverlay",
  "basePoseMode": "UseLiveSourcePose",
  "curves": [
    {
      "curveName": "thigh_r.rot_x",
      "jointName": "thigh_r",
      "channelName": "rot_x",
      "unit": "degrees",
      "keys": [
        { "time": 0.0, "value": 0.0 }
      ]
    }
  ]
}
```

The previous offline-preview file used a curve dictionary shape. That is useful for debugging but easy for an importer to treat as “no curves.” V47 generates a proper `curves: [...]` list.

### 3. Neural model is now a first-class pipeline stage

The old contact-aware denoiser path is replaced by:

```text
TRAIN_NEURAL_MOTION_PRIOR
```

The neural model is implemented in:

```text
motion-prior-service/aegis_motion_prior/neural_model_v47.py
motion-prior-service/aegis_motion_prior/train_neural_v47.py
motion-prior-service/aegis_motion_prior/infer_neural_v47.py
```

It uses a conditional transformer-style sequence model:

```text
input motion tensor + action/style/dominant-leg conditioning
→ temporal transformer encoder
→ residual motion refinement
→ validated overlay curves
```

The inference path is deliberately robust: it retrieves/time-warps the closest retargeted motion as a seed, then blends in the neural refinement if a checkpoint exists. Without a checkpoint, it still produces a valid overlay through the validated retrieval fallback.

### 4. Dashboard supports job deletion

The dashboard now has:

```text
Run full V47 job
Delete selected job
Delete completed/failed jobs
Pipeline step strip
Options viewer
Export path viewer
Live logs
```

Open it at:

```text
http://localhost:8088
```

## One-time setup

### 1. Install Python dependencies

```powershell
pip install -r requirements.txt
```

Install PyTorch for your CPU/CUDA setup. CPU example:

```powershell
pip install torch --index-url https://download.pytorch.org/whl/cpu
```

### 2. Export a Manny/Quinn skeletal mesh FBX from Unreal

Use the skeletal mesh asset, not the Skeleton asset.

Recommended:

```text
SKM_Quinn or SKM_Manny
```

Save it to:

```text
C:\Mocap\AegisTargets\SKM_Quinn.fbx
```

Then confirm this path in:

```text
config/offline_retarget_v46.config.json
```

```json
{
  "targetSkeletonFbx": "C:/Mocap/AegisTargets/SKM_Quinn.fbx"
}
```

### 3. Edit the main pipeline config

Copy or create:

```powershell
.\scripts\00-create-config.ps1
```

Then edit:

```text
config/aegis_bandai_v45.config.json
```

Important fields:

```json
{
  "tools": {
    "gitExe": "git",
    "blenderExe": "C:/Program Files/Blender Foundation/Blender 4.3/blender.exe"
  },
  "training": {
    "manifestPath": "sample-data/manifest.bandai.v47.training.json",
    "datasetOutput": "datasets/bandai_v47_neural_overlay",
    "checkpointOutput": "checkpoints/bandai_v47_neural_overlay",
    "exportOutput": "exports/bandai_soccer_kick_overlay_v47.json",
    "fps": 60,
    "maxFrames": 180,
    "epochs": 180
  }
}
```

## Run through backend/dashboard

### 1. Start infrastructure

```powershell
.\scripts\v46-start-infra.ps1
```

### 2. Build backend

```powershell
.\scripts\v46-build-backend.ps1
```

### 3. Start orchestrator

```powershell
.\scripts\v46-start-orchestrator.ps1
```

### 4. Start worker

```powershell
.\scripts\v46-start-worker.ps1
```

### 5. Start a full V47 job

```powershell
.\scripts\v47-run-bandai-neural-overlay-api.ps1
```

For a quick smoke pass:

```powershell
.\scripts\v47-run-bandai-neural-overlay-api.ps1 -SkipTraining -MaxClips 1
```

For a proper neural pass:

```powershell
.\scripts\v47-run-bandai-neural-overlay-api.ps1 -MaxClips 16
```

## Pipeline steps

```text
CLONE_BANDAI_REPO
EXTRACT_BANDAI_DATA
SELECT_BANDAI_CLIPS
OFFLINE_RETARGET_TO_MANNY_JSON
BUILD_TRAINING_MANIFEST
BUILD_TENSOR_DATASET
TRAIN_NEURAL_MOTION_PRIOR
GENERATE_OVERLAY_JSON
VERIFY_OVERLAY_JSON
COMPLETE
```

## Local script-only smoke path

After a selected-clip manifest and offline training JSON exist, you can run the core ML part without Kafka/Redis:

```powershell
.\scripts\07-build-training-manifest.ps1
.\scripts\08-build-bandai-training-dataset.ps1
.\scripts\09-train-bandai-motion-prior.ps1
.\scripts\10-generate-bandai-soccer-kick-overlay.ps1
.\scripts\11-verify-overlay-json.ps1
```

## Output files

```text
sample-data/manifest.bandai.v47.training.json
sample-data/training-json/offline-retargeted/*.json
datasets/bandai_v47_neural_overlay/motions.npy
datasets/bandai_v47_neural_overlay/metadata.json
checkpoints/bandai_v47_neural_overlay/neural_overlay_prior_v47.pt
exports/bandai_soccer_kick_overlay_v47.json
exports/bandai_soccer_kick_overlay_v47.json.validation.json
```

## Import into AegisMotion

Import:

```text
exports/bandai_soccer_kick_overlay_v47.json
```

Expected validation shape:

```text
schema: aegis.overlay.curves.v2
curveCount: 164
nonzeroCurveCount: > 0
errors: []
```

The exported curves include:

```text
pelvis.trans_x / trans_y / trans_z
pelvis.loc_x / loc_y / loc_z legacy aliases
bone.rot_x / rot_y / rot_z degree curves
bone.rot_qx / rot_qy / rot_qz / rot_qw reference quaternion curves
foot_l and foot_r IK/contact alpha curves
```

This is intentionally redundant so the current importer and future importer variants can bind safely.

## Dashboard deletion API

Delete one job:

```http
DELETE /api/v1/jobs/{jobId}
```

Delete terminal jobs:

```http
DELETE /api/v1/jobs?terminalOnly=true
```

Terminal means:

```text
COMPLETED or FAILED
```

Running/queued jobs are kept when `terminalOnly=true`.

## Important production notes

1. The neural model improves motion only when it has enough clips. `-MaxClips 1` is for smoke testing only.
2. The target FBX should be exported from the same Manny/Quinn skeleton family your plugin runtime uses.
3. The offline preview files in `generated/overlays/offline-retargeted-preview` are not the final import artifact. The final import artifact is in `exports/`.
4. The validator intentionally fails if `curves` is absent, empty, has no keys, or is entirely zero.
5. Legacy Unreal import/retarget scripts remain in the repo, but the V47 backend no longer pauses for manual retarget by default.
