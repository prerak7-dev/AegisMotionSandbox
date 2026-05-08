# Aegis V47.2 — Additive Neural Overlay Fix

V47.2 fixes the messy Unreal overlay output found in the failed V47 dashboard job.

## Root causes fixed

1. **Absolute local rotations were exported as additive overlay curves.**
   Offline retargeting produces absolute Manny/Quinn local rotations. Those contain skeleton/rest-pose offsets such as 90 or 180 degrees. The Aegis runtime overlay is additive on top of the live pose, so those offsets must never be exported directly.

2. **6D rotation packing was incorrect.**
   `quat_to_6d` previously flattened the first two rotation-matrix columns in row-major order. `sixd_to_quat` expected `[column0.xyz, column1.xyz]`, so reconstruction corrupted rotations before export.

3. **The manifest mislabeled all clips as `soccer_kick_overlay`.**
   Dash and run clips were eligible to be selected as the generated kick. The manifest builder now infers content/action/style from the Bandai file name and `sourceBvh`.

4. **Retrieval used padded frames.**
   `motions.npy` stores padded tensors, but retrieval ignored `mask.npy`. The generator now trims to valid source frames before time-warping.

5. **Dashboard action/style options were not passed to the generate script.**
   The worker now forwards dashboard `action`, `style`, and `dominantLeg` to `scripts/10-generate-bandai-soccer-kick-overlay.ps1`.

## Expected pipeline now

```text
Bandai BVH
  -> offline BVH-to-Manny training JSON
  -> manifest with real clip content/action/style
  -> tensor dataset using additive first-frame-relative rotations
  -> neural prior training
  -> retrieval/refinement with valid-frame trimming
  -> final curves[] overlay JSON
  -> semantic validation against additive-overlay contract
```

## Important validation rule

The final overlay is now rejected if first-frame additive rotation curves are not near zero. The failed V47 file had examples like:

```text
pelvis.rot_x = 180 deg at t=0
pelvis.rot_y = -90 deg at t=0
spine_01.rot_x = -169 deg at t=0
```

That is not a valid Aegis additive overlay. A valid overlay starts from the live/base pose and animates deltas from there.

## Running after this update

```powershell
.\scripts\v46-build-backend.ps1
.\scripts\v46-start-worker.ps1
```

From the dashboard, run the V47 flow again. The generated overlay should validate with:

```json
"valid": true,
"errors": []
```

For source-duration preservation, leave `training.exportDurationSeconds` absent or set it to `0`. To force a game-authored compressed overlay duration, set it to a positive value such as `1.35`.
