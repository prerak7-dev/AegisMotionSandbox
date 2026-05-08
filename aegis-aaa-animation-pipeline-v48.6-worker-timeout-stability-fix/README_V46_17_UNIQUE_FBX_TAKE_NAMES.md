# Aegis V46.17 — Unique FBX Take Names

If V46.16 imports only 2 AnimSequences from 99 FBX files, the likely cause is FBX animation stack/take name collision.

## Root cause

Unreal often names imported AnimSequences from the FBX animation take/action name, not only from the file name.
If every Blender-exported FBX uses a default action name, Unreal can repeatedly overwrite/reuse the same 1–2 AnimSequence assets.

## Fixes

- Blender converter now gives every FBX a unique action/take name:

```text
ANIM_<fbx_filename_stem>
```

- FBX export uses only the active action:

```text
bake_anim_use_all_actions=False
```

- Unreal import now imports each clip into its own subfolder under:

```text
/Game/AegisMotionTraining/BandaiNamco/Imported/Animations/<clip_name>
```

to avoid any remaining name collision.

## Required repair

You must rebuild the FBX files because the old files have non-unique/default take names:

```powershell
.\scripts\v46-rebuild-fbx-with-unreal-proxy-mesh.ps1
.\scripts\v46-inspect-one-fbx-for-unreal.ps1
.\scripts\v46-clean-bandai-unreal-import-folder.ps1
.\scripts\05-launch-unreal-batch-import.ps1
.\scripts\v46-check-unreal-import-report.ps1
```

Expected:

```text
Report version: V46.17
Report imported AnimSequences: close to 99
```

Retarget from:

```text
/Game/AegisMotionTraining/BandaiNamco/Imported/Animations
```

to:

```text
/Game/AegisMotionTraining/BandaiNamco/RetargetedToManny
```
