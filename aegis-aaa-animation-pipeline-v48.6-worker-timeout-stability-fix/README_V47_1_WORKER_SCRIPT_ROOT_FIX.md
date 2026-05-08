# V47.1 Worker Script Root Fix

This patch fixes dashboard-launched worker jobs resolving PowerShell scripts from:

```text
backend/worker-service/scripts/*.ps1
```

instead of the pipeline root:

```text
<pipeline-root>/scripts/*.ps1
```

## Cause

`aegis.worker.pipeline-root` defaults to `.`. When the Spring Boot worker is launched from `backend/worker-service`, `.` points at the worker module, so the worker attempted to run:

```text
backend/worker-service/scripts/02-extract-bandai-data.ps1
```

That folder does not exist because scripts live at the pipeline root.

## Fix

The worker now resolves the real pipeline root by walking upward from the configured root and current working directory until it finds the canonical layout:

```text
<pipeline-root>/scripts/02-extract-bandai-data.ps1
<pipeline-root>/backend/worker-service
```

Changed files:

```text
backend/worker-service/src/main/java/com/aegis/worker/config/WorkerProperties.java
backend/worker-service/src/main/java/com/aegis/worker/service/ProcessRunner.java
backend/worker-service/src/main/java/com/aegis/worker/service/StepExecutor.java
```

## Expected dashboard log

A job log should now show something like:

```text
Pipeline root: C:\UnrealProjects\aegis-aaa-animation-pipeline-v47-neural-overlay
Running: powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\UnrealProjects\aegis-aaa-animation-pipeline-v47-neural-overlay\scripts\02-extract-bandai-data.ps1 ...
```

## Recommended local launch

The launcher still sets `AEGIS_PIPELINE_ROOT` explicitly:

```powershell
.\scripts\v46-start-worker.ps1
```

But this patch makes the worker robust even when launched directly from `backend/worker-service` or from an IDE.
