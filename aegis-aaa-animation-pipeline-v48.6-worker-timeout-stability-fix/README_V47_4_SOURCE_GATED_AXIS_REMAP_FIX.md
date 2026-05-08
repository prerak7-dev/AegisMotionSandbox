# Aegis V47.4 Source-Gated + Axis-Remapped Neural Overlay Fix

## Why the repaired V47.3 JSON stopped spinning but still did not look like a kick

The attached clip shows the correct symptom for the V47.3 repaired JSON: the 360-degree scalar-curve spin is gone, but the character still behaves like a stabilized shuffle/locomotion overlay rather than a soccer kick.

The debug artifacts show two remaining issues:

1. **The source motion was still not semantically a kick.**
   The packaged/stale tensor dataset still contained `dataset-1_dash_active_001.bvh`, and old metadata had mislabeled that dash clip as `soccer_kick_overlay`. Stabilizing a dash/run clip can remove spins, but it cannot invent a believable kick silhouette.

2. **The lower-body axis mapping was wrong for the current Aegis custom data asset.**
   V47.3 allowed most lower-body swing energy to land on `rot_z`. In the Aegis runtime/data asset, `rot_z` behaves like yaw/twist for Manny legs, so it creates foot shuffling or vertical counter-rotation. V47.4 treats the data asset as PRY-style channels:

   ```text
   rot_x = pitch / sagittal swing / visible kick hinge
   rot_y = roll / lateral balance
   rot_z = yaw or twist, tightly limited
   ```

## What changed

### 1. Hard source-action gate

V47.4 refuses to generate a soccer kick overlay unless the retrieved tensor source is semantically a kick. It no longer trusts only the `action` field because stale V47.3 metadata could say `action=soccer_kick_overlay` while the file path was clearly `dataset-1_dash_active_001.bvh`.

The gate checks:

- `content`
- `semanticContent`
- source BVH path
- source JSON path
- source id

If the requested action contains `kick`, any source with `dash`, `run`, or `walk` in its source identity is rejected.

### 2. Dataset verification before training

`08-build-bandai-training-dataset.ps1` now runs:

```powershell
python -m aegis_motion_prior.verify_dataset_for_action `
  --dataset "$($config.training.datasetOutput)" `
  --action "$requestedAction"
```

This stops the dashboard job before neural training if there are no valid kick clips. That prevents wasting time training on dash/run/walk clips and prevents another wrong overlay JSON from being exported.

### 3. Aegis PRY axis remap in exporter

The exporter now maps lower-body log-map motion into Aegis authoring channels:

```text
source/logmap Z swing -> Aegis rot_x
source/logmap Y lateral -> Aegis rot_y
source/logmap X twist -> Aegis rot_z, damped
```

The exported JSON metadata now records:

```json
"aegisPryConvention": "rot_x=pitch/sagittal_swing, rot_y=roll/lateral, rot_z=yaw_or_twist",
"limbSagittalSwingAxis": "rot_x",
"twistAxis": "rot_z_limited"
```

### 4. Stricter validator

The validator now fails soccer-kick JSONs when:

- source clip metadata is missing,
- `retrievedClip` is not semantically a kick,
- a repair-only JSON is being treated as a final soccer kick,
- lower-body twist/yaw curves exceed V47.4 Aegis PRY limits.

This means the V47.3 repaired JSON is now correctly rejected as **stabilized but not semantically safe**.

### 5. Diagnostic authored kick overlay

V47.4 includes a diagnostic overlay that is intentionally not ML-generated:

```text
exports/aegis_diagnostic_soccer_kick_overlay_v47_4.json
```

Use this before rerunning the full job to verify that the Aegis importer/runtime applies `rot_x` as the visible leg swing channel. If this diagnostic overlay looks like a recognizable right-leg kick, the plugin import/application path is good and the remaining issue is source data quality. If it still does not look like a kick, the problem is in the Unreal importer/runtime axis application, not the ML pipeline.

Generate it again with:

```powershell
.\scripts\13-generate-diagnostic-kick-overlay.ps1
```

## Expected dashboard behavior now

If the job only finds dash/run/walk clips, it should fail at or immediately after `BUILD_TENSOR_DATASET` with a message like:

```text
No semantically valid kick clips are present after retarget/tensor build.
The job is stopping before training/generation so it does not create another shuffle/dash overlay.
```

That failure is intentional and correct. It means the pipeline protected the plugin from importing the wrong motion.

## Run order

1. Import and test the diagnostic overlay first:

   ```text
   exports/aegis_diagnostic_soccer_kick_overlay_v47_4.json
   ```

2. If the diagnostic overlay looks kick-like, run the full dashboard job.

3. If the job fails with "No semantically valid kick clips," inspect:

   ```text
   sample-data/manifests/bandai_selected_raw_clips.json
   sample-data/manifest.bandai.v47.training.json
   ```

   You need at least one source clip whose filename/content is actually kick/shoot, not dash/run/walk.
