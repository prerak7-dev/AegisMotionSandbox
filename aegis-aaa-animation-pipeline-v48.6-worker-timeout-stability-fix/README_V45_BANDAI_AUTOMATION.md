# Aegis V45 — Bandai Namco Automated Dataset Builder

V45 automates the Bandai Namco → Aegis learned-motion-prior path as much as possible.

## What V45 automates

```text
clone Bandai Namco repository
→ extract data.zip
→ select useful BVH clips by content/style
→ convert BVH to FBX using Blender
→ launch Unreal batch FBX import
→ run Aegis V45 commandlet to export retargeted AnimSequences
→ build training manifest
→ build tensors
→ train contact-aware prior or use retrieval/time-warp fallback
→ generate LiveBaseGeneratedOverlay JSON
```

## What still needs one-time manual setup

Unreal retargeting still needs a valid IK Rig / IK Retargeter setup because source skeletons and UE versions vary.

One-time manual step:

```text
Imported Bandai FBX AnimSequences
→ Retarget to Manny/Quinn
→ Save to /Game/AegisMotionTraining/BandaiNamco/RetargetedToManny
```

After that, V45 commandlet export is automated.

## Setup

1. Install V45 plugin into your Unreal project:

```text
<Project>/Plugins/AegisMotion
```

2. Extract this V45 pipeline.

3. Create config:

```powershell
.\scripts\00-create-config.ps1
```

4. Edit:

```text
config/aegis_bandai_v45.config.json
```

Set:

```text
Blender path
UnrealEditor path
UnrealEditor-Cmd path
Unreal project path
Aegis plugin path
Bandai repo path
FBX output path
```

## Run full workflow

```powershell
.\scripts\run-v45-end-to-end.ps1
```

The script pauses after FBX import so you can retarget to Manny/Quinn. Then it continues:

```text
export training JSON
build manifest
build tensors
train/generate
```

## Output

```text
exports/bandai_soccer_kick_overlay_v45.json
```

Import that JSON into AegisMotion using the V36+ live-base overlay importer.

## Fast test without training

If you do not have enough clips yet, skip training:

```powershell
.\scripts\run-v45-end-to-end.ps1 -SkipTraining
```

This uses retrieval + time-warp fallback, which is often better than hand-authored procedural curves once the source clips are real.
