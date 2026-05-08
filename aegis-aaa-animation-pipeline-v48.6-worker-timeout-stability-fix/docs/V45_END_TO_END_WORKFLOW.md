# V45 End-to-End Workflow

## 0. Install prerequisites

Required:

- Git
- Python 3.10+
- Blender 4.x
- Unreal Engine 5.x
- AegisMotion V45 plugin

Optional but recommended:

- Visual Studio C++ workload
- CUDA PyTorch for training speed

## 1. Install plugin

Copy the V45 plugin to:

```text
<Project>/Plugins/AegisMotion
```

Rebuild the Unreal project.

Confirm this menu appears:

```text
Tools → Aegis Motion → Export Selected AnimSequences to Training JSON
```

And the commandlet works:

```text
-run=AegisExportAnimSequences
```

## 2. Configure pipeline

```powershell
.\scripts\00-create-config.ps1
```

Edit:

```text
config/aegis_bandai_v45.config.json
```

## 3. Clone/extract/select/convert

```powershell
.\scripts\01-clone-bandai-dataset.ps1
.\scripts\02-extract-bandai-data.ps1
.\scripts\03-select-bandai-clips.ps1
.\scripts\04-convert-bandai-bvh-to-fbx.ps1
```

## 4. Import to Unreal

```powershell
.\scripts\05-launch-unreal-batch-import.ps1
```

## 5. Manual retarget step

In Unreal:

1. Open IK Retargeter.
2. Retarget imported Bandai FBX animations to Manny/Quinn.
3. Save retargeted `AnimSequence` assets to:

```text
/Game/AegisMotionTraining/BandaiNamco/RetargetedToManny
```

## 6. Export training JSON automatically

```powershell
.\scripts\06-export-retargeted-animsequences.ps1
```

Output:

```text
sample-data/training-json/*.json
```

## 7. Build manifest and tensors

```powershell
.\scripts\07-build-training-manifest.ps1
.\scripts\08-build-bandai-training-dataset.ps1
```

## 8. Generate with retrieval/time-warp first

```powershell
.\scripts\10-generate-bandai-soccer-kick-overlay.ps1
```

If no checkpoint exists, it automatically uses retrieval + time-warp.

## 9. Train and regenerate

```powershell
.\scripts\09-train-bandai-motion-prior.ps1
.\scripts\10-generate-bandai-soccer-kick-overlay.ps1
```

## 10. Import final JSON

Import:

```text
exports/bandai_soccer_kick_overlay_v45.json
```

into AegisMotion.

Recommended first test:

```text
Playback Mode: LiveBaseGeneratedOverlay
Foot Lock: OFF
Two-Bone IK: OFF
```

Then test while Quinn/Manny is moving through normal locomotion.
