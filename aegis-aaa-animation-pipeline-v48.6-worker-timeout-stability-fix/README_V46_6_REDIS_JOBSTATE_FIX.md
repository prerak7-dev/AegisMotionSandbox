# Aegis V46.6 Redis Job-State Fix

Fixes the case where:

```powershell
.\scripts\v46-run-bandai-pipeline-api.ps1
```

creates a job successfully, but:

```powershell
.\scripts\v46-get-job.ps1 -JobId "<jobId>"
```

returns HTTP 500.

## Root cause

The job is created and saved to Redis, but the `/jobs/{jobId}` endpoint has to deserialize the
Redis JSON back into `JobState`. If the timestamp/module serialization format differs between
the orchestrator/worker, the direct `readValue(..., JobState.class)` path can fail.

## Fixes

- Added a resilient fallback parser in both:
  - `orchestrator-service/JobStore`
  - `worker-service/WorkerJobStore`
- Jackson config now uses:
  - `JavaTimeModule`
  - `findAndRegisterModules()`
  - `FAIL_ON_UNKNOWN_PROPERTIES` disabled
- `v46-get-job.ps1` and `v46-get-job-logs.ps1` now print the backend error body if anything still fails.

## Required after installing this patch

Rebuild and restart both services:

```powershell
.\scripts\v46-build-backend.ps1
```

Restart orchestrator and worker windows:

```powershell
.\scripts\v46-start-orchestrator.ps1
.\scripts\v46-start-worker.ps1
```

Then retry:

```powershell
.\scripts\v46-get-job.ps1 -JobId "<jobId>"
```

If the old job was serialized in a broken format, starting a new job after V46.6 is the cleanest test.
