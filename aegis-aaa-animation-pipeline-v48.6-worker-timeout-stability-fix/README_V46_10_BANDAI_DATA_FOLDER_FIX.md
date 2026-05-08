# Aegis V46.10 — Bandai Current Data Folder Layout Fix

The Bandai repository currently stores motion files directly under:

```text
dataset/Bandai-Namco-Research-Motiondataset-1/data
dataset/Bandai-Namco-Research-Motiondataset-2/data
```

with `.bvh` and `.json` files.

There is no `data.zip` in the cloned repository layout you are using.

## Fixes

- `02-extract-bandai-data.ps1` no longer requires `data.zip`.
- It now validates the current `data/` folders directly.
- `select_bandai_clips.py` scans:
  - `dataset/<dataset-name>/data/*.bvh`
  - any discovered `dataset/**/data/*.bvh`
  - the old raw extract folder as fallback
- The selector writes diagnostics showing inferred content/style buckets.
- Added:

```powershell
.\scripts\v46-repair-bandai-data-folder-and-convert.ps1
```

## Recommended repair from your current state

Stop the old current job or ignore it, then run:

```powershell
.\scripts\v46-repair-bandai-data-folder-and-convert.ps1
```

Expected:

```text
Total BVH files found: > 0
Wrote bandai_selected_raw_clips.json with > 0 clips
FBX files available: > 0
```

Then start a fresh API job or continue manually from Unreal import.

## If clip selection still returns 0

Open:

```text
sample-data/manifests/bandai_selected_raw_clips.json
```

and inspect `diagnostics.inferredBucketsPreview`. Update your config filters to match the actual bucket names.
