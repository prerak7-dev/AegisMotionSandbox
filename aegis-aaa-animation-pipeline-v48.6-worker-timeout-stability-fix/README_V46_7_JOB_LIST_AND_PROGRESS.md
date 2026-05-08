# Aegis V46.7 — Job List + Worker Progress Diagnostics

Fixes/confusion addressed:

## 1. `/api/v1/jobs` now works

Previously:

```text
http://localhost:8088/api/v1/jobs
```

returned:

```text
No static resource api/v1/jobs
```

because only `/api/v1/jobs/{jobId}` existed.

V46.7 adds:

```text
GET /api/v1/jobs
```

and:

```powershell
.\scripts\v46-list-jobs.ps1
```

## 2. Jobs staying QUEUED means the worker is not progressing

If a job remains:

```text
status: QUEUED
currentStep: CLONE_BANDAI_REPO
```

then either:

- `v46-start-worker.ps1` is not running,
- the worker crashed,
- Kafka is not delivering messages,
- the worker is failing before it can update Redis.

Keep the worker PowerShell window open and check its console.

## 3. Empty AegisBandaiFbx folder

The FBX folder remains empty until the worker reaches:

```text
CONVERT_BVH_TO_FBX
```

To check the local pipeline stage:

```powershell
.\scripts\v46-check-bandai-stage.ps1
```

This reports:

- whether clips were selected
- how many FBX files exist
- how many exported training JSON files exist

## Required after installing V46.7

Rebuild and restart:

```powershell
.\scripts\v46-build-backend.ps1
.\scripts\v46-start-orchestrator.ps1
.\scripts\v46-start-worker.ps1
```

Then list jobs:

```powershell
.\scripts\v46-list-jobs.ps1
```
