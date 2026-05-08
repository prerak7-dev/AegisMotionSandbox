# Aegis V46.12 — Unreal Import Verification

If the backend reaches `WAITING_FOR_RETARGET` but you do not see:

```text
/Game/AegisMotionTraining/BandaiNamco/Imported
```

then the Unreal import step probably imported 0 assets while still exiting successfully.

## Fixes

V46.12 changes the import step to:

- prefer the pipeline-local fixed Python import script,
- create the destination Content Browser folder,
- import FBX as skeletal/animation assets,
- save the destination directory,
- write an import report:

```text
<Project>/Saved/AegisV46ImportReport.json
```

- fail the step if Unreal imports 0 assets.

## Check import status

```powershell
.\scripts\v46-check-unreal-import-report.ps1
```

Expected Content Browser path:

```text
/Game/AegisMotionTraining/BandaiNamco/Imported
```

Expected disk path:

```text
<Project>/Content/AegisMotionTraining/BandaiNamco/Imported
```

## Manual re-run import after FBX conversion

```powershell
.\scripts\05-launch-unreal-batch-import.ps1
.\scripts\v46-check-unreal-import-report.ps1
```

If imported object paths are still 0, open the report JSON and Unreal Output Log to see the FBX import error.
