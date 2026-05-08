# AegisMotion V47.5 Plugin Patch — Live-Base Scalar Overlay Fix

This patch fixes the runtime side of the latest generated overlay test.

## Root cause

Generated V47.4/V47.5 overlays are authored as scalar additive degree curves:

```text
bone.rot_x / bone.rot_y / bone.rot_z
```

The importer correctly stores these as raw degree curves so they are not multiplied by `MaxRotationDegrees`. However, in the runtime driver, the `LiveBaseGeneratedOverlay` behavior was only enabled for generated quaternion slots.

That meant scalar generated overlays could start from the skeleton reference pose path instead of the current AnimGraph source pose path. This made strong kick curves look weaker or incorrect when applied in PIE.

## Changed file

```text
Source/AegisMotion/Private/ProceduralMotion/AnimNodes/AnimNode_AegisProceduralMotionDriver.cpp
```

## Runtime behavior after this patch

When the action asset playback mode is:

```text
LiveBaseGeneratedOverlay
```

raw scalar rotation curves now use the current source pose as the base, exactly like generated quaternion overlays:

```text
current AnimGraph pose + generated additive scalar rot_x/y/z overlay
```

This is required for the V47.5 knee-coupled generated JSON.

## Test order

1. Replace the plugin with this patched version.
2. Rebuild the Unreal project.
3. Import the V47.5 repaired JSON:

```text
exports/bandai_soccer_kick_overlay_v47_5_knee_coupled_repaired.json
```

4. Scrub around the kick window. The right leg should show clear thigh swing, knee bend/extension, and foot follow-through.
