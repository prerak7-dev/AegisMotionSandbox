# Aegis V46.15 — Import Report Path + Fallback

This patch handles the case where Unreal imports assets successfully, but
`Saved/AegisV46ImportReport.json` is not written.

## Why it happens

In some command-line/editor contexts, Unreal's Python `unreal.Paths.project_saved_dir()`
can resolve unexpectedly, or the script can import assets but fail/exit before writing the report.

## Fixes

- `05-launch-unreal-batch-import.ps1` now passes an explicit absolute report path to Unreal via:

```text
AEGIS_IMPORT_REPORT_PATH
```

- The Unreal Python import script writes to that exact path.
- If the report is still missing but `.uasset` files exist under:

```text
<Project>/Content/AegisMotionTraining/BandaiNamco/Imported
```

the PowerShell script creates a fallback report and allows the pipeline to continue.

## What to run now

Since you already see imported assets, run:

```powershell
.\scripts\05-launch-unreal-batch-import.ps1
.\scripts\v46-check-unreal-import-report.ps1
```

If assets already exist, the import step may replace/reuse them and then generate a valid report.

Once the report has imported object paths, continue to retargeting.
