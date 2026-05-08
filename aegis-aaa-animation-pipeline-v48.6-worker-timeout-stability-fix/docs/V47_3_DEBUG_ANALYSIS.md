# V47.3 Debug Analysis

## Observed in the supplied failed import

The generated JSON was not empty and was accepted by the V47.2 validator. The issue was semantic: unsafe scalar rotation curves.

The supplied validation report showed:

- `durationSeconds: 11.9`
- `curveCount: 164`
- `maxAbsRotationDegrees: 179.2464`
- warning only: `spine_01.rot_z`, `thigh_l.rot_z`, `thigh_r.rot_z` were near 180 degrees

Direct inspection of the curves found adjacent-key discontinuities such as:

- `spine_01.rot_z`: about `350.97` degrees
- `thigh_l.rot_z`: about `353.15` degrees
- `thigh_r.rot_z`: about `350.73` degrees

A scalar curve runtime will not know those are equivalent quaternion orientations. It will interpolate the float values and rotate through the long path.

## Stage diagnosis

### Retargeting

Retargeting exported plausible target-skeleton local quaternions, but the selected mocap source was a long full kick take rather than a short overlay segment.

### Tensor generation

The additive rebase was working. First-frame rotation was near zero, so this was not the original V47 absolute-rest-pose bug.

### Neural inference

The model blended a representation that was valid in quaternion/6D space, but the final scalar decomposition was not safe for Aegis runtime curves.

### JSON export

This was the main corruption point. Direct Euler decomposition of additive quaternions produced near-180 scalar channels and wrap discontinuities.

### Unreal import/runtime

The Unreal side was most likely applying the scalar curves it was given. Aegis cannot infer quaternion equivalence from independent `rot_x`, `rot_y`, `rot_z` float curves.

## Fix

V47.3 makes the exporter runtime-aware:

1. Trim long kick takes to a short action window.
2. Convert additive quaternions to shortest-path rotation-vector scalar channels.
3. Clamp to conservative per-bone additive limits.
4. Despike adjacent scalar keys.
5. Disable quaternion reference curves by default.
6. Fail validation when dangerous scalar curves are detected.
