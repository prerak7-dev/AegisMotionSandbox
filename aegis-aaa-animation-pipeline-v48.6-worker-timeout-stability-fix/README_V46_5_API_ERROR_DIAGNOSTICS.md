# Aegis V46.5 API Error Diagnostics

This patch improves diagnosis for:

```text
Invoke-RestMethod : The remote server returned an error: (500) Internal Server Error
```

when running:

```powershell
.\scripts\v46-run-bandai-pipeline-api.ps1
```

## What changed

- Added `GlobalExceptionHandler` to the orchestrator service so API 500 errors return JSON with:
  - exception class
  - message
  - root cause
  - hint
- Added diagnostics endpoint:

```text
GET http://localhost:8088/api/v1/diagnostics
```

- Updated `v46-run-bandai-pipeline-api.ps1` to print the server error body.
- Added:

```powershell
.\scripts\v46-diagnostics.ps1
```

## Most common cause

Kafka and/or Redis are not running or not reachable.

Run:

```powershell
.\scripts\v46-start-infra.ps1
docker ps
```

Then restart orchestrator and worker after rebuilding:

```powershell
.\scripts\v46-build-backend.ps1
.\scripts\v46-start-orchestrator.ps1
.\scripts\v46-start-worker.ps1
```

Now rerun:

```powershell
.\scripts\v46-run-bandai-pipeline-api.ps1
```
