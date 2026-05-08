# Aegis V46.16 — Animation-Only Import Fix

If Unreal imports many SkeletalMesh assets but only 1–2 AnimSequences, that is not enough for this pipeline.

## Root cause

The previous import path imported every FBX as a skeletal mesh. That creates many source meshes/skeleton packages, but does not reliably create one AnimSequence per FBX.

## Fix

V46.16 uses a two-pass import:

1. Import the first FBX as a source SkeletalMesh/Skeleton:

```text
/Game/AegisMotionTraining/BandaiNamco/Imported/SkeletonSource
```

2. Import every FBX as an animation-only asset using that source Skeleton:

```text
/Game/AegisMotionTraining/BandaiNamco/Imported/Animations
```

The AnimSequences you retarget are now under:

```text
/Game/AegisMotionTraining/BandaiNamco/Imported/Animations
```

## Recommended repair

Optional cleanup:

```powershell
.\scripts\v46-clean-bandai-unreal-import-folder.ps1
```

Then run:

```powershell
.\scripts\05-launch-unreal-batch-import.ps1
.\scripts\v46-check-unreal-import-report.ps1
```

Expected:

```text
Report imported AnimSequences: close to FBX count
```

Then retarget the AnimSequences from:

```text
/Game/AegisMotionTraining/BandaiNamco/Imported/Animations
```

to:

```text
/Game/AegisMotionTraining/BandaiNamco/RetargetedToManny
```
