# Aegis V46.11 — Blender BVH→FBX Argument Fix

Fixes this conversion-step error:

```text
usage: blender.exe [-h] --src SRC --dst DST ...
blender.exe: error: the following arguments are required: --src, --dst
Blender did not create expected FBX
```

## Root cause

The conversion script expected custom args after Blender's `--` separator:

```powershell
blender --background --python convert_bvh_to_fbx.py -- --src in.bvh --dst out.fbx
```

On the tested Blender/PowerShell combination, those arguments were not reaching the Python script reliably.

## Fix

V46.11 passes source/destination through environment variables instead:

```text
AEGIS_BVH_SRC
AEGIS_FBX_DST
```

The Blender Python script now supports both:

- `--src` / `--dst`
- env var fallback

## Quick test

```powershell
.\scripts\v46-test-one-bvh-conversion.ps1
```

Then run the full conversion step again:

```powershell
.\scripts\04-convert-bandai-bvh-to-fbx.ps1
```

or the repair chain:

```powershell
.\scripts\v46-repair-bandai-data-folder-and-convert.ps1
```
