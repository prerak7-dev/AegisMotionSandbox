# V47.5 Debug Analysis

The latest import result had no 360 spin, but the leg still did not read as a kick. The uploaded JSON showed that the problem had moved from scalar safety to animation semantics.

## Observed from the generated overlay

```text
durationSeconds: 1.35
source: dataset-1_kick_normal_001.bvh
schema: aegis.overlay.curves.v2
curveCount: 76
```

The JSON was valid and source-gated to a kick, but the knee channel was almost empty:

```text
thigh_r.rot_x peak ~= 93.68°
calf_r.rot_x peak  ~= 5.47°
foot_r.rot_x peak  ~= 73.54°
```

A soccer kick needs a strong opposite calf/knee curve. The diagnostic kick that looked recognizable had roughly this relationship:

```text
thigh_r.rot_x: -42° to +82°
calf_r.rot_x:  +62° to -105°
foot_r.rot_x:  -28° to +52°
```

So the pipeline output had thigh and foot energy, but not knee flexion/extension. This makes the leg look like a stiff shuffle.

## Plugin issue found at the same time

The plugin imported generated scalar curves as raw-degree curves. That is correct. However, the runtime only enabled `LiveBaseGeneratedOverlay` base-pose behavior for generated quaternion slots. Scalar slots were still being started from the reference pose path.

The paired plugin patch fixes this by enabling live-base pose for any raw scalar rotation slot when the action asset playback mode is `LiveBaseGeneratedOverlay`.

## Next test order

1. Rebuild the patched plugin.
2. Import `exports/bandai_soccer_kick_overlay_v47_5_knee_coupled_repaired.json`.
3. Test in debug scrub around 0.65 to 0.95 normalized seconds.
4. If the repaired JSON now reads as a proper kick, rerun the full V47.5 pipeline.
