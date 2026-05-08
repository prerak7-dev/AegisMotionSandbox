# V47.4 Debug Analysis: Repaired JSON Still Not a Kick

## Observed clip

The repaired V47.3 import stops the repeated 360-degree spin, but the animation still reads as foot shuffling / locomotion stabilization instead of a kick.

## JSON findings

The V47.3 repaired JSON had valid Aegis importer structure:

- `schema = aegis.overlay.curves.v2`
- `curves[]` exists
- `durationSeconds = 1.35`
- first-frame additive values are near zero

But the motion was still wrong.

### Source problem

The packaged/stale generated output was derived from:

```text
dataset-1_dash_active_001.bvh
```

Old metadata mislabeled that clip as:

```text
action = soccer_kick_overlay
```

That is why retrieval could pass while still selecting a dash. The repaired output was therefore a stabilized dash/shuffle, not a kick.

### Axis problem

The V47.3 repaired JSON carried large lower-body energy on `rot_z`, for example:

```text
thigh_l.rot_z around 90 degrees
thigh_r.rot_z around -81 degrees
foot_l.rot_z around -47 degrees
foot_r.rot_z around -41 degrees
```

In the current Aegis custom data asset/runtime, this channel behaves like yaw/twist for the legs. It does not produce a clean forward kick silhouette. V47.4 therefore remaps lower-body sagittal swing to `rot_x` and tightly limits `rot_z`.

## Conclusion

The problem is no longer "JSON has no curves" and no longer the raw 360-degree Euler wrap. The remaining problem is:

```text
wrong source motion + wrong lower-body exported axis convention
```

V47.4 fixes both by source-gating kick generation and remapping exported channels to the Aegis PRY convention.
