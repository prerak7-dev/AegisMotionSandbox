# Aegis V46 — Spring Boot + Kafka + Redis Orchestrated Animation Pipeline

V46 wraps the V45 Bandai automation pipeline in a production-style backend:

```text
Spring Boot Orchestrator API
→ Kafka pipeline commands
→ Spring Boot Worker service
→ V45 PowerShell/Python/Blender/Unreal scripts
→ Redis job status/progress/logs
→ Python motion-prior service/output JSON
→ AegisMotion plugin import
```

## Services

```text
backend/common
backend/orchestrator-service
backend/worker-service
```

## Infrastructure

```text
Kafka: localhost:9092
Redis: localhost:6379
Kafka UI: http://localhost:8099
Orchestrator API: http://localhost:8088
Worker service: localhost:8090
```

## End-to-end run

### 1. Create/edit the Bandai config

```powershell
.\scripts\00-create-config.ps1
```

Edit:

```text
config\aegis_bandai_v45.config.json
```

### 2. Start Kafka + Redis

```powershell
.\scripts\v46-start-infra.ps1
```

### 3. Build backend

```powershell
.\scripts\v46-build-backend.ps1
```

### 4. Start orchestrator

Open PowerShell window 1:

```powershell
.\scripts\v46-start-orchestrator.ps1
```

### 5. Start worker

Open PowerShell window 2:

```powershell
.\scripts\v46-start-worker.ps1
```

### 6. Start pipeline through Spring Boot API

Open PowerShell window 3:

```powershell
.\scripts\v46-run-bandai-pipeline-api.ps1 -SkipTraining
```

This returns a job object with a `jobId`.

The pipeline will run:

```text
clone Bandai repo
extract data.zip
select BVH clips
convert BVH to FBX
launch Unreal import
```

Then it pauses at:

```text
WAITING_FOR_RETARGET
```

### 7. Manual retarget step

In Unreal, retarget imported Bandai animations to Manny/Quinn and save them to:

```text
/Game/AegisMotionTraining/BandaiNamco/RetargetedToManny
```

### 8. Continue the job

```powershell
.\scripts\v46-continue-after-retarget.ps1 -JobId "<jobId>"
```

The worker continues:

```text
export retargeted AnimSequences through V45 commandlet
build manifest
build tensors
train or retrieval/time-warp fallback
generate LiveBaseGeneratedOverlay JSON
```

### 9. Check job state/logs

```powershell
.\scripts\v46-get-job.ps1 -JobId "<jobId>"
.\scripts\v46-get-job-logs.ps1 -JobId "<jobId>"
```

### 10. Final output

```text
exports/bandai_soccer_kick_overlay_v45.json
```

Import that into AegisMotion.

## API

Start Bandai pipeline:

```http
POST /api/v1/pipelines/bandai/run
```

Body:

```json
{
  "configPath": "C:/.../config/aegis_bandai_v45.config.json",
  "skipTraining": true,
  "pauseForManualRetarget": true,
  "action": "soccer_kick_overlay",
  "style": "active",
  "dominantLeg": "right"
}
```

Continue after manual retarget:

```http
POST /api/v1/jobs/{jobId}/continue-after-retarget
```

Get job:

```http
GET /api/v1/jobs/{jobId}
```

Get logs:

```http
GET /api/v1/jobs/{jobId}/logs
```

## Why V46 matters

This turns Aegis from a local script chain into a real game-tech data pipeline:

```text
Spring Boot = orchestration and REST API
Kafka = asynchronous pipeline events
Redis = job status/progress/logs/cache
Python = ML/tensor/training/generation
Blender = source motion conversion
Unreal C++ plugin = training export + runtime import/playback
```
