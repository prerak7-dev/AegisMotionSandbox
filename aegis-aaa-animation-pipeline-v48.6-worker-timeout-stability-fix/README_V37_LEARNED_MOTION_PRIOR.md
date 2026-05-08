# Aegis AAA Animation Pipeline V37 — Learned Motion Prior

V37 is the first version that treats high-quality animation generation as a **numerical motion problem**, not an LLM keyframe-writing problem.

The target architecture is:

```text
High-quality animation dataset
→ retarget all clips to UE5 Manny/Quinn skeleton
→ normalize into motion tensors
→ train motion prior
→ AI Director chooses action/style/timing
→ motion prior generates/samples pose trajectory
→ Aegis exports UE-native overlay JSON
→ plugin layers over live locomotion + Control Rig/IK polish
```

## What is included

This package adds a Python motion-prior service and training scaffold:

```text
motion-prior-service/
  aegis_motion_prior/
    dataset.py       Build tensors from Aegis JSON clips
    tensorize.py     Convert root/quaternion/contact curves to tensors
    model.py         Small Transformer denoising prior scaffold
    train.py         Train command
    infer.py         Generate command
    exporter.py      Export UE-native Aegis overlay JSON
    retrieval.py     Dataset retrieval fallback
    service.py       FastAPI inference endpoint
```

It also includes:

```text
configs/v37_motion_prior.yaml
sample-data/manifest.sample.json
sample-data/ai_soccer_kick_learned_motion_prior_seed_v37.json
scripts/*.ps1
docs/*.md
java-integration/README_V37_JAVA_INTEGRATION.md
```

## Important truth

The included V37 seed JSON is **not** a learned AAA-quality animation yet. It is a compatibility output that lets you validate the V36 live-base overlay runtime.

AAA-quality requires real training data:

- Manny/Quinn retargeted run cycles
- run-to-kick transitions
- soccer kick clips
- plant/follow-through clips
- contact labels
- clean root motion

## Recommended first real dataset

Start with 30–100 clips:

```text
10–20 run cycles
10–20 run stop / plant / turn clips
10–20 kicks
10–20 athletic weight-shift actions
10–20 follow-through / recovery clips
```

Retarget all to the same UE5 skeleton and export into Aegis JSON with:

```text
LiveBaseGeneratedOverlay-compatible curves
root loc_x/loc_y/loc_z
bone rot_qx/rot_qy/rot_qz/rot_qw
foot contact/plant curves
phase markers
```

## Quick start

From the extracted package root:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt

.\scripts\build-motion-dataset.ps1
.\scripts\train-motion-prior.ps1
.\scripts\generate-soccer-kick-v37.ps1
```

Start the service:

```powershell
.\scripts\start-motion-prior-service.ps1
```

Then call:

```text
POST http://localhost:8091/generate
```

with:

```json
{
  "action": "soccer_kick_overlay",
  "style": "powerful",
  "dominantLeg": "right",
  "durationSeconds": 1.35,
  "skeletonProfile": "UE5_Mannequin"
}
```

## Plugin compatibility

V37 outputs `LiveBaseGeneratedOverlay` JSON. Use AegisMotion plugin **V36 or later**.

The plugin should keep normal Manny/Quinn locomotion running underneath:

```text
Current source pose + generated overlay
```

not:

```text
captured frame zero + full generated animation
```
