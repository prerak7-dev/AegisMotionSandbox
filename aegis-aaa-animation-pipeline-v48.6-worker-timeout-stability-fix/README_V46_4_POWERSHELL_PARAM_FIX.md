# Aegis V46.4 PowerShell Param Fix

Fixes this PowerShell error:

```text
param : The term 'param' is not recognized as the name of a cmdlet...
```

## Root cause

In PowerShell, a script's `param(...)` block must be the first executable statement in the file.
Some V46 scripts had:

```powershell
$ErrorActionPreference = "Stop"
param(...)
```

That makes PowerShell treat `param` as a command instead of a script parameter block.

## Fix

Moved all `param(...)` blocks above `$ErrorActionPreference = "Stop"` in scripts that need parameters.

Fixed scripts include:

- `scripts/01-clone-bandai-dataset.ps1`
- `scripts/02-extract-bandai-data.ps1`
- `scripts/03-select-bandai-clips.ps1`
- `scripts/04-convert-bandai-bvh-to-fbx.ps1`
- `scripts/05-launch-unreal-batch-import.ps1`
- `scripts/06-export-retargeted-animsequences.ps1`
- `scripts/07-build-training-manifest.ps1`
- `scripts/08-build-bandai-training-dataset.ps1`
- `scripts/09-train-bandai-motion-prior.ps1`
- `scripts/10-generate-bandai-soccer-kick-overlay.ps1`
- `scripts/run-v45-end-to-end.ps1`
- `scripts/v45_common.ps1`
- `scripts/v46-continue-after-retarget.ps1`
- `scripts/v46-get-job-logs.ps1`
- `scripts/v46-get-job.ps1`
- `scripts/v46-run-bandai-pipeline-api.ps1`

## Retry

Run:

```powershell
.\scripts\v46-run-bandai-pipeline-api.ps1
```

To run with training enabled, do not pass `-SkipTraining`.

To test faster with retrieval fallback:

```powershell
.\scripts\v46-run-bandai-pipeline-api.ps1 -SkipTraining
```
