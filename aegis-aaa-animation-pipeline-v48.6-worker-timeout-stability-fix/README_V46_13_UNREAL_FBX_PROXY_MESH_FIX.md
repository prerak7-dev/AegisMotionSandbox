# Aegis V46.13 — Unreal FBX Proxy Mesh Fix

Your import report showed:

```json
"fbxCount": 99,
"importedObjectPaths": []
```

That means Unreal saw the FBX files but imported 0 assets.

## Root cause

The Blender BVH → FBX conversion exported armature/animation data only. Unreal often imports 0 assets from an FBX that has animation/armature but no skinned mesh, unless an existing compatible Skeleton is supplied.

## Fix

V46.13 adds a tiny skinned proxy mesh during Blender export:

```text
Aegis_Unreal_Import_Proxy_Mesh
```

This lets Unreal import each FBX as a SkeletalMesh/Skeleton/AnimSequence package. The proxy mesh is only a technical import bridge; the animation can then be retargeted to Manny/Quinn.

## Required repair

Delete/rebuild existing FBX files because they were generated without the proxy mesh:

```powershell
.\scripts\v46-rebuild-fbx-with-unreal-proxy-mesh.ps1
```

Then import again:

```powershell
.\scripts\05-launch-unreal-batch-import.ps1
.\scripts\v46-check-unreal-import-report.ps1
```

Expected Content Browser path:

```text
/Game/AegisMotionTraining/BandaiNamco/Imported
```

Expected report:

```text
importedObjectPaths > 0
```

Then retarget those imported source animations to Manny/Quinn and continue the backend job.
