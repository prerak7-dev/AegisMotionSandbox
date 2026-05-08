# Aegis V46.14 — Import Script Version Guard

Your latest report still contains the old V46.12 error text, and it has no `version` field. That means Unreal is still running the old import script, or you are reading an old report.

V46.14 makes that impossible to miss.

## Fixes

- `05-launch-unreal-batch-import.ps1` now forces the pipeline-local import script.
- It refuses to fall back to an older plugin script.
- It deletes the old import report before importing.
- It verifies the new report has `version: V46.13` or `version: V46.14`.
- Added:

```powershell
.\scripts\v46-inspect-one-fbx-for-unreal.ps1
```

This opens one FBX in Blender and verifies it contains both mesh and armature data.

## Correct repair sequence

```powershell
.\scripts\v46-rebuild-fbx-with-unreal-proxy-mesh.ps1
.\scripts\v46-inspect-one-fbx-for-unreal.ps1
.\scripts\05-launch-unreal-batch-import.ps1
.\scripts\v46-check-unreal-import-report.ps1
```

The new report must show:

```json
"version": "V46.14"
```

If the report has no `version` field, the old script/report is still being used.
