# Aegis V46.18 — Progress Dashboard

V46.18 adds a lightweight web dashboard so you do not need to manually refresh:

```text
http://localhost:8088/api/v1/jobs/<jobId>
```

or scroll through huge logs.

## Dashboard

Open:

```text
http://localhost:8088/
```

or run:

```powershell
.\scripts\v46-open-dashboard.ps1
```

The dashboard shows:

- all jobs
- selected job status
- progress bar
- current pipeline step
- latest logs only
- auto-refresh every ~1.5 seconds

## Conversion progress

The BVH → FBX script now emits progress messages like:

```text
AEGIS_PROGRESS|step=CONVERT_BVH_TO_FBX|progress=31|message=Converting 55/99 : clip_name
```

The worker reads those lines and updates Redis job state while the step is still running.

## What to do for a fresh run

After manually deleting old FBX files and old Unreal imports:

```powershell
.\scripts\v46-build-backend.ps1
```

Restart services:

```powershell
.\scripts\v46-start-orchestrator.ps1
.\scripts\v46-start-worker.ps1
```

Open:

```text
http://localhost:8088/
```

Then start the job:

```powershell
.\scripts\v46-run-bandai-pipeline-api.ps1
```

Watch the dashboard instead of refreshing JSON endpoints.
