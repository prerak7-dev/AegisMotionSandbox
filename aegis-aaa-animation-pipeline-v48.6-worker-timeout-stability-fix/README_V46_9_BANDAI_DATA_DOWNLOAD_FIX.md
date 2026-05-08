# Aegis V46.9 — Bandai data.zip Download / Zero-Clip Fix

This patch fixes the issue seen in the job logs:

```text
WARNING: No data.zip files found under C:/Mocap/Bandai-Namco-Research-Motiondataset.
Wrote ... bandai_selected_raw_clips.json with 0 clips
```

## Root cause

The repository clone completed, but the expected `data.zip` files were not present locally. The Bandai dataset READMEs state that the BVH motions and JSON annotations are inside `data.zip`.

V46.9 now:

- tries `git lfs pull` after clone/pull,
- if no `data.zip` is found, downloads dataset-1 and dataset-2 `data.zip` directly from GitHub raw/media URLs,
- verifies that BVH files exist after extraction,
- fails early if zero clips are selected instead of continuing into Unreal import with no FBX files,
- adds a repair script:

```powershell
.\scripts\v46-repair-bandai-data-and-convert.ps1
```

## Quick repair for your current state

After installing V46.9, run:

```powershell
.\scripts\v46-repair-bandai-data-and-convert.ps1
```

Expected result:

```text
selected clips > 0
FBX files > 0
```

Then either start a fresh API job, or continue manually from Unreal import:

```powershell
.\scripts\05-launch-unreal-batch-import.ps1
```

## Recommended clean test

Restart orchestrator/worker after rebuilding, then start a new job. New jobs will now fail early if data is missing instead of silently continuing with zero FBX files.
