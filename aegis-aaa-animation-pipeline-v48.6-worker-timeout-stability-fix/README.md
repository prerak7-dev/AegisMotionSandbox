# Aegis AAA Animation Pipeline — Start-to-Finish Setup and Usage Guide

This README explains how to set up, run, and operate the Aegis animation pipeline from a clean checkout. It covers both supported workflows:

1. **V48 AI Quaternion No-Retarget Generation** — the recommended production path for high-quality Aegis soccer-kick overlay JSON generation.
2. **Legacy / Traditional BVH Conversion** — the experimental/research path for BVH/Bandai-style source motion conversion.

The current production recommendation is **V48 AI Quaternion No-Retarget**. It avoids Unreal IK Retargeter, avoids editing retarget poses, and generates Manny/Quinn-native quaternion overlay JSONs directly for the Aegis custom data asset.

---

## 1. What this project does

The pipeline generates animation overlay JSON files for the AegisMotion Unreal plugin. The final JSON is imported into the Aegis custom data asset and played by the plugin as a live-base overlay over the character's current AnimGraph pose.

For V48, the target format is:

```text
LiveBaseGeneratedOverlay
UseLiveAnimGraphSourcePoseEveryFrame
UE5 Manny/Quinn skeleton profile
Quaternion rotation curves: rot_qx / rot_qy / rot_qz / rot_qw
Phase metadata for plant, pelvis-open, thigh-drive, knee-snap, ankle-whip, follow-through, counterbalance, and head focus
```

The recommended output file is:

```text
exports/aegis_v48_quaternion_soccer_kick_overlay.json
```

Its validation report is:

```text
exports/aegis_v48_quaternion_soccer_kick_overlay.json.validation.json
```

A successful validation should show something close to:

```json
{
  "valid": true,
  "sourceFormat": "AI_NATIVE_UE5_MANNEQUIN_LIVE_BASE_OVERLAY_V48_QUATERNION_NO_RETARGET",
  "quatCurveCount": 88,
  "scalarRuntimeCurveCount": 0,
  "errors": []
}
```

---

## 2. Architecture overview

### 2.1 High-level service architecture

```text
Dashboard / Browser
        |
        v
Spring Boot Orchestrator API : localhost:8088
        |
        | creates jobs, stores job state, publishes pipeline commands
        v
Kafka : localhost:9092
        |
        v
Spring Boot Worker Service : localhost:8090
        |
        | executes PowerShell + Python pipeline steps
        v
Python motion-prior / generation code
        |
        v
Generated Aegis overlay JSON
        |
        v
AegisMotion Unreal Plugin custom data asset import
```

### 2.2 Infrastructure

```text
Redis       localhost:6379   job state, logs, progress, latest export path
Kafka       localhost:9092   pipeline step commands
Kafka UI    localhost:8099   optional Kafka inspection UI
Orchestrator localhost:8088  REST API + dashboard
Worker      localhost:8090   background step executor
```

### 2.3 Backend modules

```text
backend/common
  Shared DTOs, job state, pipeline steps, Kafka command/event models.

backend/orchestrator-service
  REST API, dashboard HTML, job creation, Redis job state, Kafka command publishing.

backend/worker-service
  Kafka consumer, PowerShell step execution, progress parsing, timeout protection, export-path reporting.
```

### 2.4 Important Redis keys

```text
aegis:v46:job:{jobId}
aegis:v46:exports:latest
```

The project still uses the `v46` Redis namespace for job state because the Spring/Kafka/Redis dashboard system was introduced in V46 and retained through V48.

---

## 3. Requirements

### 3.1 Required for both V48 AI generation and dashboard usage

Install these first:

```text
Windows 10/11
PowerShell 5+ or PowerShell 7+
Git
Python 3.10+
Java JDK 17+
Maven 3.9+
Docker Desktop
```

Confirm from PowerShell:

```powershell
git --version
python --version
java -version
mvn -version
docker --version
```

### 3.2 Python dependencies

From the pipeline root:

```powershell
python -m pip install --upgrade pip
pip install -r requirements.txt
```

Install PyTorch separately. CPU example:

```powershell
pip install torch --index-url https://download.pytorch.org/whl/cpu
```

If you have a CUDA GPU and want faster training, install the PyTorch build that matches your CUDA version.

### 3.3 Additional requirements for legacy BVH conversion

Only needed for the traditional BVH/Bandai workflows:

```text
Blender 4.x or newer
Unreal Engine 5.x
AegisMotion plugin installed in your Unreal project
Optional: exported Manny/Quinn skeletal mesh FBX for offline retarget experiments
```

The legacy Unreal retarget path can use:

```text
UnrealEditor.exe
UnrealEditor-Cmd.exe
IK Rig / IK Retargeter assets
```

However, if editing retarget poses crashes the GPU, do **not** use the Unreal IK Retargeter path. Use the V48 no-retarget production path instead.

---

## 4. Recommended folder layout

Example:

```text
C:\UnrealProjects\aegis-aaa-animation-pipeline-v48.6-worker-timeout-stability-fix
C:\UnrealProjects\AegisMotionSandbox\AegisMotionSandbox.uproject
C:\UnrealProjects\AegisMotionSandbox\Plugins\AegisMotion
C:\Mocap\Bandai-Namco-Research-Motiondataset
C:\Mocap\AegisBandaiFbx
C:\Mocap\AegisTargets\SKM_Quinn.fbx
```

The V48 production path only needs the pipeline folder and the included gold sample.

The legacy BVH path uses the `C:\Mocap` folders if you keep the default config style.

---

## 5. Fresh setup from a clean extracted pipeline

Open PowerShell in the pipeline root:

```powershell
cd C:\UnrealProjects\aegis-aaa-animation-pipeline-v48.6-worker-timeout-stability-fix
```

If PowerShell blocks scripts, use a process-local bypass:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

Install Python dependencies:

```powershell
pip install -r requirements.txt
pip install torch --index-url https://download.pytorch.org/whl/cpu
```

Start Kafka + Redis:

```powershell
.\scripts\v46-start-infra.ps1
```

Build the backend:

```powershell
.\scripts\v46-build-backend.ps1
```

Start the orchestrator in one PowerShell window:

```powershell
.\scripts\v46-start-orchestrator.ps1
```

Start the worker in a second PowerShell window:

```powershell
.\scripts\v46-start-worker.ps1
```

Open the dashboard:

```powershell
.\scripts\v46-open-dashboard.ps1
```

Or open manually:

```text
http://localhost:8088/
```

Keep the orchestrator and worker windows open while jobs are running.

---

## 6. Dashboard usage

The dashboard is available at:

```text
http://localhost:8088/
```

It shows:

```text
all jobs
selected job details
current pipeline step
progress percentage
latest logs
options used for the job
export path
buttons to delete selected jobs or completed/failed jobs
```

### 6.1 Start a V48 AI generation job

Use these dashboard values:

```text
Pipeline mode: V48 Quaternion No-Retarget
Config path: config/aegis_v48_no_retarget.config.json
Kick style: Instep Power Shot / Inside-Foot Pass / Side-Foot Shot / Low Driven Kick / Follow-Through Heavy / Short Tap / Volley Preparation
Dominant leg: right or left
Overlay duration seconds: 1.35
Training variant count: 20-50 recommended, 35 default
Intensity: 0.0-1.5, 1.0 default
Follow-through: 0.0-1.5, 0.70 default
Plant stability: 0.0-1.0, 0.92 default
Upper-body counterbalance: 0.0-1.5, 0.78 default
Skip neural training: off for full generation, on for fast fallback/smoke test
```

Click:

```text
Run V48 job
```

### 6.2 Expected V48 dashboard steps

```text
V48_LOAD_GOLD_OVERLAY
V48_GENERATE_SYNTHETIC_VARIANTS
V48_BUILD_QUATERNION_DATASET
V48_TRAIN_QUATERNION_PRIOR
V48_GENERATE_IMPORT_JSON
V48_VALIDATE_IMPORT_JSON
COMPLETE
```

If `Skip neural training` is checked, the job skips `V48_TRAIN_QUATERNION_PRIOR` and generates from the synthetic variant/retrieval fallback.

### 6.3 Final V48 output

Import this file into the Aegis custom data asset:

```text
exports/aegis_v48_quaternion_soccer_kick_overlay.json
```

Review this validation file first:

```text
exports/aegis_v48_quaternion_soccer_kick_overlay.json.validation.json
```

The validation should show:

```text
valid: true
quatCurveCount: 88 or similar
scalarRuntimeCurveCount: 0
errors: []
```

---

## 7. V48 AI generation pipeline details

V48 is the recommended production path.

### 7.1 Input

The V48 generator starts from the known-good gold reference:

```text
sample-data/gold/ai_soccer_kick_livebase_overlay_v36.json
```

That reference already uses the correct runtime contract:

```text
LiveBaseGeneratedOverlay
UseLiveAnimGraphSourcePoseEveryFrame
UE5_Mannequin_Quinn_Manny
rot_qx / rot_qy / rot_qz / rot_qw quaternion curves
```

### 7.2 Generation contract

V48 treats the following biomechanical rules as first-class generation parameters:

```text
plant foot stays grounded
pelvis opens before thigh drive
thigh leads before calf extension
calf/knee snaps through near strike
foot follows the knee, then whips through
spine and arms counterbalance
head stays ball-focused
```

The intended phase sequence is:

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

### 7.3 Neural refinement targets

The ML step is used for refinement, not to rescue broken retargeted data. It targets:

```text
smooth timing
correct joint coupling
add realistic follow-through
predict contact timing
adjust style/intensity
remove jitter
preserve foot plant
```

### 7.4 V48 manual script sequence

You can run the V48 pipeline manually without the dashboard:

```powershell
.\scripts\v48-01-load-gold-overlay.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-02-generate-quaternion-variants.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-03-build-quaternion-dataset.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-04-train-quaternion-prior.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-05-generate-import-json.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-06-validate-import-json.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
```

For a quick smoke pass, skip the train script and run generation/validation after dataset build. The generator can use a fallback when no checkpoint exists.

### 7.5 V48 generated folders

```text
generated/v48/quaternion_variants
  Synthetic Manny/Quinn-native kick variants.

generated/v48/manifest.v48.quaternion_variants.json
  Manifest of generated training examples.

generated/v48/tensor_dataset
  Quaternion/6D tensor dataset.

generated/v48/checkpoints
  Neural refinement checkpoints.

exports/aegis_v48_quaternion_soccer_kick_overlay.json
  Final import JSON.
```

---

## 8. Traditional / legacy BVH conversion pipelines

The legacy path is retained for research and dataset experiments. It is no longer the recommended path for portfolio-quality soccer kicks because retargeting/axis issues can easily produce poor overlays.

There are two legacy variants:

1. **Legacy offline retarget path** — BVH source data is processed outside the Unreal IK Retargeter path and converted toward Manny/Quinn training JSON.
2. **Legacy Unreal manual retarget path** — BVH is converted to FBX, imported to Unreal, manually retargeted, then exported to training JSON.

Use V48 for production. Use legacy only when you specifically want to test mocap/BVH ingestion.

---

## 9. Legacy path A — Bandai/BVH offline-retarget experiment

This is the safer legacy path because it avoids manual IK Retargeter editing in Unreal.

### 9.1 Configure legacy paths

Create or update the legacy config:

```powershell
.\scripts\00-create-config.ps1
```

Edit:

```text
config/aegis_bandai_v45.config.json
```

Check these sections:

```json
{
  "tools": {
    "gitExe": "git",
    "blenderExe": "C:/Program Files/Blender Foundation/Blender 5.1/blender.exe"
  },
  "bandai": {
    "repoDir": "C:/Mocap/Bandai-Namco-Research-Motiondataset",
    "rawExtractDir": "C:/Mocap/Bandai-Namco-Research-Motiondataset/extracted",
    "fbxOutputDir": "C:/Mocap/AegisBandaiFbx"
  },
  "training": {
    "manifestPath": "sample-data/manifest.bandai.v47.training.json",
    "datasetOutput": "datasets/bandai_v47_neural_overlay",
    "checkpointOutput": "checkpoints/bandai_v47_neural_overlay",
    "exportOutput": "exports/bandai_soccer_kick_overlay_v47.json",
    "exportDurationSeconds": 1.35
  }
}
```

If using offline retarget scripts, also check:

```text
config/offline_retarget_v46.config.json
```

and set the target Manny/Quinn skeletal mesh FBX path if needed.

### 9.2 Run from dashboard

In the dashboard:

```text
Pipeline mode: Legacy Bandai Experimental
Config path: config/aegis_bandai_v45.config.json
Kick style: active or desired legacy style
Dominant leg: right or left
Skip neural training: optional
```

Click:

```text
Run V48 job
```

The button label still says V48 job in the current dashboard, but choosing `Legacy Bandai Experimental` sends the legacy pipeline mode to the backend.

### 9.3 Expected legacy offline-retarget steps

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

### 9.4 Run legacy offline-retarget through API script

You can also run:

```powershell
.\scripts\v47-run-bandai-neural-overlay-api.ps1 -SkipTraining -MaxClips 1
```

For a larger pass:

```powershell
.\scripts\v47-run-bandai-neural-overlay-api.ps1 -MaxClips 16
```

Final output:

```text
exports/bandai_soccer_kick_overlay_v47.json
```

Validate/import only if the JSON passes validation and visually makes sense. The current V48 production path is still preferred.

---

## 10. Legacy path B — traditional BVH to FBX to Unreal retarget

Use this only if your Unreal IK Retargeter setup is stable. If editing retarget poses crashes the GPU, skip this path.

### 10.1 Configure Unreal paths

Edit:

```text
config/aegis_bandai_v45.config.json
```

Set:

```json
{
  "tools": {
    "unrealEditorExe": "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe",
    "unrealEditorCmdExe": "C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
  },
  "unreal": {
    "project": "C:/UnrealProjects/AegisMotionSandbox/AegisMotionSandbox.uproject",
    "pluginDir": "C:/UnrealProjects/AegisMotionSandbox/Plugins/AegisMotion",
    "importPath": "/Game/AegisMotionTraining/BandaiNamco/Imported",
    "retargetedPath": "/Game/AegisMotionTraining/BandaiNamco/RetargetedToManny",
    "trainingJsonOutput": "sample-data/training-json"
  }
}
```

### 10.2 Run the traditional script chain manually

```powershell
.\scripts\01-clone-bandai-dataset.ps1
.\scripts\02-extract-bandai-data.ps1
.\scripts\03-select-bandai-clips.ps1
.\scripts\04-convert-bandai-bvh-to-fbx.ps1
.\scripts\05-launch-unreal-batch-import.ps1
```

At this point, Unreal should have imported the FBX animations.

### 10.3 Manual Unreal retarget step

In Unreal:

1. Open the relevant IK Rig / IK Retargeter.
2. Retarget imported Bandai FBX animation sequences to Manny/Quinn.
3. Save retargeted `AnimSequence` assets to:

```text
/Game/AegisMotionTraining/BandaiNamco/RetargetedToManny
```

Then export the retargeted animation sequences:

```powershell
.\scripts\06-export-retargeted-animsequences.ps1
```

Continue data generation:

```powershell
.\scripts\07-build-training-manifest.ps1
.\scripts\08-build-bandai-training-dataset.ps1
.\scripts\09-train-bandai-motion-prior.ps1
.\scripts\10-generate-bandai-soccer-kick-overlay.ps1
.\scripts\11-verify-overlay-json.ps1
```

Final legacy output:

```text
exports/bandai_soccer_kick_overlay_v47.json
```

### 10.4 Traditional path warning

The traditional path can produce technically valid JSON that still looks bad if:

```text
source clip semantics are wrong
retarget pose is wrong
axis mapping is wrong
Euler/scalar curves are used as final runtime rotations
leg/knee/foot coupling is lost
```

For high-quality soccer kicks, use the V48 quaternion no-retarget path.

---

## 11. API reference

The dashboard uses the same REST API.

### 11.1 Health check

```http
GET http://localhost:8088/api/v1/health
```

### 11.2 Start a V48 AI job

```http
POST http://localhost:8088/api/v1/pipelines/bandai/run
Content-Type: application/json
```

Body:

```json
{
  "configPath": "config/aegis_v48_no_retarget.config.json",
  "pipelineMode": "V48_QUATERNION_NO_RETARGET",
  "skipTraining": false,
  "action": "soccer_kick_overlay",
  "kickStyle": "instep_power_shot",
  "dominantLeg": "right",
  "variantCount": 35,
  "durationSeconds": 1.35,
  "intensity": 1.0,
  "followThrough": 0.7,
  "plantStability": 0.92,
  "upperBodyCounterbalance": 0.78
}
```

### 11.3 Start a legacy Bandai/BVH job

```json
{
  "configPath": "config/aegis_bandai_v45.config.json",
  "pipelineMode": "LEGACY_BANDAI_EXPERIMENTAL",
  "skipClone": false,
  "skipTraining": true,
  "useOfflineRetarget": true,
  "pauseForManualRetarget": false,
  "action": "soccer_kick_overlay",
  "style": "active",
  "dominantLeg": "right",
  "maxClips": 1
}
```

### 11.4 Job endpoints

```http
GET    /api/v1/jobs
GET    /api/v1/jobs/{jobId}
GET    /api/v1/jobs/{jobId}/logs
GET    /api/v1/jobs/{jobId}/logs/tail?limit=140
DELETE /api/v1/jobs/{jobId}
DELETE /api/v1/jobs?terminalOnly=true
GET    /api/v1/exports/latest
```

### 11.5 Continue after manual retarget

Only for the traditional Unreal manual-retarget path:

```http
POST /api/v1/jobs/{jobId}/continue-after-retarget
```

---

## 12. Importing the generated JSON into AegisMotion

Use the AegisMotion plugin importer/custom data asset workflow.

Recommended import target for V48:

```text
exports/aegis_v48_quaternion_soccer_kick_overlay.json
```

The plugin/runtime should use quaternion curves first:

```text
rot_qx
rot_qy
rot_qz
rot_qw
```

Scalar `rot_x / rot_y / rot_z` curves should be treated only as debug/fallback for production generated soccer kicks.

Recommended first test settings:

```text
Playback Mode: LiveBaseGeneratedOverlay
Base Pose Mode: UseLiveAnimGraphSourcePoseEveryFrame
Foot Lock / generated IK: OFF for first quality test
Global alpha: 1.0
Relevant bone weights: 1.0
```

Trigger the overlay while Manny/Quinn locomotion or the intended base pose is already playing. This is an overlay, not a full standalone locomotion clip.

---

## 13. Cleaning, restarting, and avoiding hung jobs

### 13.1 Normal restart

Close old orchestrator and worker windows.

Then:

```powershell
.\scripts\v46-start-infra.ps1
.\scripts\v46-start-orchestrator.ps1
.\scripts\v46-start-worker.ps1
.\scripts\v46-open-dashboard.ps1
```

### 13.2 Full infrastructure stop

```powershell
.\scripts\v46-stop-infra.ps1
```

Then start again:

```powershell
.\scripts\v46-start-infra.ps1
```

### 13.3 Delete old job states from dashboard

Use:

```text
Delete selected
Delete completed/failed
```

Do not delete a running job unless you have already stopped the worker/orchestrator and know the process is no longer running.

### 13.4 If a job is stuck at 0%

Usually this means the worker is not running or not consuming commands.

Check:

```text
PowerShell window running v46-start-worker.ps1 is open
Kafka is running on localhost:9092
Redis is running on localhost:6379
Orchestrator is reachable at http://localhost:8088/api/v1/health
```

Restart cleanly:

```powershell
# close old orchestrator/worker windows first
.\scripts\v46-start-orchestrator.ps1
.\scripts\v46-start-worker.ps1
```

### 13.5 If a job is stuck at validation

V48.6 adds step timeouts and process-tree cleanup. Validation should normally finish in seconds.

Check whether this file exists:

```powershell
Test-Path .\exports\aegis_v48_quaternion_soccer_kick_overlay.json.validation.json
```

Open it:

```powershell
Get-Content .\exports\aegis_v48_quaternion_soccer_kick_overlay.json.validation.json
```

If it says `"valid": true`, the JSON is usable even if the dashboard was behind.

You can run validation manually:

```powershell
.\scripts\v48-06-validate-import-json.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
```

### 13.6 Timeout environment variables

Optional overrides:

```powershell
$env:AEGIS_VALIDATION_TIMEOUT_SECONDS = "180"
$env:AEGIS_V48_STEP_TIMEOUT_SECONDS = "600"
$env:AEGIS_TRAINING_TIMEOUT_SECONDS = "1800"
```

---

## 14. Development notes and best practices

### 14.1 Recommended production workflow

Use this for the main portfolio-quality soccer kick:

```text
V48 Quaternion No-Retarget
→ generate 20-50 clean synthetic variants from V36 gold reference
→ train/refine quaternion prior
→ export final quaternion import JSON
→ validate
→ import into Aegis data asset
→ record Unreal preview
→ iterate style/intensity/follow-through/plant stability
```

### 14.2 When to skip training

Use `skipTraining` for:

```text
quick dashboard smoke tests
checking service health
checking config path correctness
checking import format
```

Do not use `skipTraining` for final quality review unless the fallback already looks good.

### 14.3 When to use legacy BVH

Use legacy BVH only for:

```text
researching external mocap data
building future datasets
comparing source motion against V48 synthetic results
experimenting with offline retargeting
```

Do not use legacy BVH as the main production path while Unreal retarget-pose editing is unstable.

### 14.4 Job concurrency

The current worker is effectively single-lane because outputs use shared paths:

```text
exports/aegis_v48_quaternion_soccer_kick_overlay.json
generated/v48/
datasets/bandai_v47_neural_overlay/
```

Run one generation job at a time. If you need parallel jobs later, add per-job output directories first.

---

## 15. Quick command cheat sheet

### First-time setup

```powershell
cd C:\UnrealProjects\aegis-aaa-animation-pipeline-v48.6-worker-timeout-stability-fix
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
pip install -r requirements.txt
pip install torch --index-url https://download.pytorch.org/whl/cpu
.\scripts\v46-start-infra.ps1
.\scripts\v46-build-backend.ps1
```

### Start services

```powershell
# Window 1
.\scripts\v46-start-orchestrator.ps1

# Window 2
.\scripts\v46-start-worker.ps1

# Window 3 or browser
.\scripts\v46-open-dashboard.ps1
```

### Manual V48 run

```powershell
.\scripts\v48-01-load-gold-overlay.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-02-generate-quaternion-variants.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-03-build-quaternion-dataset.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-04-train-quaternion-prior.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-05-generate-import-json.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
.\scripts\v48-06-validate-import-json.ps1 -ConfigPath config\aegis_v48_no_retarget.config.json
```

### Legacy Bandai smoke job through API

```powershell
.\scripts\v47-run-bandai-neural-overlay-api.ps1 -SkipTraining -MaxClips 1
```

### Open latest V48 output

```powershell
notepad .\exports\aegis_v48_quaternion_soccer_kick_overlay.json.validation.json
```

---

## 16. Final recommendation

For the AegisMotion soccer-kick portfolio piece, use:

```text
V48 Quaternion No-Retarget
```

Treat the legacy Bandai/BVH path as research. The professional-quality route is Manny/Quinn-native quaternion overlay generation from a trusted gold reference, refined by ML, validated as an Aegis import JSON, and reviewed in Unreal through the actual plugin runtime.
