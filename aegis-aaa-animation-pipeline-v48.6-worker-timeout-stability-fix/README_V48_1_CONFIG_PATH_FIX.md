# V48.1 - Config Path / Gold Overlay Load Fix

This patch fixes the failure at `V48_LOAD_GOLD_OVERLAY` where the worker logged:

```text
Get-Content : Could not find a part of the path
'C:\\UnrealProjects\\aegis-aaa-animation-pipeline-v48-quaternion-no-retarget\\'.
```

## Root cause

`scripts/v45_common.ps1` had `Get-AegisV45ConfigPath` without a formal `param(...)` block. When V48 scripts called:

```powershell
$config = Get-AegisV45Config $ConfigPath
```

the supplied `config/aegis_v48_no_retarget.config.json` argument was ignored. The helper silently loaded the legacy Bandai config instead. Since the legacy config has no `v48.goldOverlayPath`, the gold path resolved to the pipeline root directory, causing `Get-Content` to read a folder instead of the V36 JSON.

## Fix

- `Get-AegisV45ConfigPath` now accepts `param([string]$ConfigPath = "")`.
- Relative config paths are resolved from the pipeline root.
- Added `Resolve-AegisV45Path` for root-relative artifact paths.
- V48 scripts now resolve gold/input/output paths using `Resolve-AegisV45Path`.

## Files changed

```text
scripts/v45_common.ps1
scripts/v48-01-load-gold-overlay.ps1
scripts/v48-02-generate-quaternion-variants.ps1
scripts/v48-03-build-quaternion-dataset.ps1
scripts/v48-04-train-quaternion-prior.ps1
scripts/v48-05-generate-import-json.ps1
scripts/v48-06-validate-import-json.ps1
```

## Expected dashboard log after patch

```text
[V48_LOAD_GOLD_OVERLAY] Executing scripts/v48-01-load-gold-overlay.ps1
Pipeline root: C:\UnrealProjects\aegis-aaa-animation-pipeline-v48-quaternion-no-retarget
Running: powershell.exe ... -File ...\scripts\v48-01-load-gold-overlay.ps1 -ConfigPath config/aegis_v48_no_retarget.config.json
Loading V36 gold quaternion live-base overlay
Gold overlay OK: C:\UnrealProjects\aegis-aaa-animation-pipeline-v48-quaternion-no-retarget\sample-data\gold\ai_soccer_kick_livebase_overlay_v36.json
```

## Verification performed

The Python V48 stages were smoke-tested directly:

```text
variant generation: passed
tensor dataset build: passed
1-epoch quaternion prior training: passed
final import JSON generation: passed
V48 quaternion validation: passed
```

Validation reported:

```json
{
  "valid": true,
  "quatCurveCount": 88,
  "scalarRuntimeCurveCount": 0,
  "errors": []
}
```
