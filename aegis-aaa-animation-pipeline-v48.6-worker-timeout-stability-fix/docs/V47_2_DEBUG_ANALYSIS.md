# V47.2 Debug Analysis

## Files analyzed

- `JobLog.txt`
- `bandai_dash_active_dataset-1_dash_active_001_manny_overlay.json`
- `bandai_soccer_kick_overlay_v47.json`
- `ai_soccer_kick_livebase_overlay_v36.json`
- screen recording of the messy animation

## Findings

### 1. The final generated JSON had a valid `curves` array, but the curves were semantically invalid for an additive overlay.

The failed JSON had 164 curves and a proper top-level `curves: []` list, so the problem was not the earlier “no curves” import issue. The problem was that many rotation curves started with huge values at time zero.

Examples from the failed V47 output:

```text
pelvis.rot_x: 180 deg at first key
pelvis.rot_y: -90 deg at first key
pelvis.rot_z: 180 deg at first key
spine_01.rot_x: about -169 deg at first key
```

The Aegis runtime uses the live/base pose and applies the overlay on top. Therefore, first-frame overlay deltas must be near zero. Exporting absolute skeleton pose offsets causes the mesh to twist, flip, or collapse.

### 2. The source selected by retrieval was a dash clip, not a kick clip.

The job manifest labeled all retargeted clips with the default action `soccer_kick_overlay`, including dash and run clips. Retrieval selected the first/highest-scoring clip, which was:

```text
bandai_dash_active_dataset-1_dash_active_001_manny_training
```

That dash source was then compressed into the soccer-kick output path. This explains why the visible motion did not read as a kick even before considering the absolute-rotation issue.

### 3. The 6D rotation layout was broken.

The prior 6D representation packed rotation matrix columns incorrectly. The inverse function expected `[column0.xyz, column1.xyz]`, but the exporter used a row-major reshape. This corrupted the neural tensor representation and made reconstructed Euler rotations unreliable.

### 4. The generator ignored the dataset mask.

Dataset tensors are padded to a batch shape. Retrieval was warping the padded tensor instead of trimming by `mask.npy`, so zero padding could be included in the generated overlay.

## Fixes implemented

- Convert offline-retargeted absolute quaternions into first-frame-relative additive deltas during tensorization.
- Rebase the final generated tensor again before export to remove neural drift.
- Correct the 6D rotation packing layout.
- Infer Bandai content/action/style from file names and source BVH paths.
- Trim retrieved source tensors using `mask.npy` before time-warping.
- Default export duration to the selected source clip duration unless a positive override is configured.
- Extend overlay validation to reject non-zero first-frame additive rotations/translations.
- Pass dashboard action/style/dominant-leg options into the final generation script.

## Result of local smoke validation

Using the bundled sample clip, the patched exporter generated a valid curves-array overlay:

```text
schema: aegis.overlay.curves.v2
curveCount: 164
first-frame rotation max: 0 deg
first-frame translation max: 0 cm
validation errors: []
```

The bundled sample contains only a dash clip, so the smoke output is only a structural/additive-contract validation. On the full local Bandai dataset, the manifest now identifies kick clips as `soccer_kick_overlay`, so the generator should select the actual retargeted kick source for the final soccer-kick overlay.
