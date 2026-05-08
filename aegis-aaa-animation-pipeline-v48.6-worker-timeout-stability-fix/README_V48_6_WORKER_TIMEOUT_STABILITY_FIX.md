# V48.6 Worker Timeout + Job Stability Fix

This patch returns to the stable V48.1/V48.5 job flow and fixes the real remaining reliability issue: a hung PowerShell/Python validation process could occupy the single worker thread forever. When that happened, the current job stayed at `V48_VALIDATE_IMPORT_JSON` / 98%, and new jobs stayed at 0% because the worker could not consume the next command.

## What changed

- `ProcessRunner` now applies hard per-step timeouts and kills the whole PowerShell/Python process tree on timeout.
- V48 validation has a short timeout because it should only load/check/write JSON.
- Training keeps a longer timeout.
- The worker now logs `[WORKER_RECEIVED]` when it consumes a command.
- The worker now queues the next step in Redis before publishing the next Kafka command, which makes stale/duplicate commands safe to ignore.
- The orchestrator reconciles stale V48 validation jobs:
  - if a valid `.validation.json` exists, the job is marked `COMPLETED`;
  - if no valid report exists after the stale window, the job is marked `FAILED` instead of staying stuck forever.
- `scripts/v48-06-validate-import-json.ps1` now logs the exact input/report paths and verifies that the validation report was actually created.

## Environment timeout overrides

Optional:

```powershell
$env:AEGIS_VALIDATION_TIMEOUT_SECONDS = "180"
$env:AEGIS_V48_STEP_TIMEOUT_SECONDS = "600"
$env:AEGIS_TRAINING_TIMEOUT_SECONDS = "1800"
```

## Required restart after upgrading

Close old orchestrator/worker windows first. If an old worker is currently hung in Python/PowerShell, the new code cannot kill a process that was launched by the old worker. Then rebuild and restart:

```powershell
.\scripts\v46-build-backend.ps1
.\scripts\v46-start-worker.ps1
```

Open the dashboard and create a new V48 job. The job should now progress from 0% to complete, or fail cleanly with a timeout instead of blocking later jobs.
