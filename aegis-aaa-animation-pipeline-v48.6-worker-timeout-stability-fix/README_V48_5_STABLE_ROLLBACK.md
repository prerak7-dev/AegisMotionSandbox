# Aegis V48.5 Stable Rollback

This package intentionally rolls the job system back to the last stable V48.1 queue/worker flow.

## What is reverted

Removed from the active codebase by reverting to V48.1:

- worker heartbeat diagnostics
- dashboard requeue changes
- Redis command fallback queue
- Kafka publish diagnostics added after V48.1
- queue/dispatch refactors from V48.3/V48.4

## What is kept

Only one narrow fix is kept:

- after `V48_VALIDATE_IMPORT_JSON` succeeds, the worker marks the job `COMPLETED` directly.

This fixes the old final dashboard hang without changing how jobs are queued or consumed.

## Expected behavior

The job should again log lines like:

```text
[V48_LOAD_GOLD_OVERLAY] Executing scripts/v48-01-load-gold-overlay.ps1
...
[V48_VALIDATE_IMPORT_JSON] Executing scripts/v48-06-validate-import-json.ps1
Validation report: exports/aegis_v48_quaternion_soccer_kick_overlay.json.validation.json
[COMPLETE] V48 pipeline complete. Final quaternion import JSON validated successfully.
```

## Run

```powershell
.\scripts46-build-backend.ps1
.\scripts46-start-worker.ps1
```

Then start/open the dashboard using the same command you used in V48.1.
