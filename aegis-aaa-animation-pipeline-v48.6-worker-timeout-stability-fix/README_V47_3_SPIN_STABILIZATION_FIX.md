# Aegis V47.3 — 360 Spin Stabilization Fix

## Problem fixed

The V47.2 output was structurally valid JSON, but the scalar rotation curves were still unsafe for the current Aegis custom data asset runtime.

The debug overlay had these concrete failures:

- `durationSeconds: 11.9` for a kick overlay, so the runtime played a long raw mocap take instead of a short authored action overlay.
- `spine_01.rot_z`, `thigh_l.rot_z`, and `thigh_r.rot_z` reached about `+/-180` degrees.
- Several scalar curves jumped by roughly `350` degrees between adjacent keys.
- The validator passed the file because V47.2 only checked the first additive frame and basic curve shape.

That combination explains the in-engine result: initial foot shuffling, then torso/legs rotating against each other, then repeated 360-degree spins.

## Root cause

V47.2 correctly rebased absolute Manny local rotations to frame-zero-relative additive rotations, but it still exported the quaternion output by direct Euler decomposition into scalar `rot_x`, `rot_y`, `rot_z` curves.

For an Unreal runtime that consumes independent scalar rotation curves, direct Euler values near `+/-180` are dangerous. A quaternion can be valid and continuous while the Euler representation wraps from `-176` to `+173`. Scalar interpolation then takes the long path and visually spins the character.

The selected Bandai kick clip was also a long full mocap take, not a 1.35s overlay segment. V47.2 preserved that source duration by default.

## V47.3 changes

### 1. Short action-window trimming

The retrieval stage now trims long kick takes down to a compact high-energy lower-body action window before time-warping.

Default kick overlay duration is now `1.35s` unless overridden.

### 2. Scalar-safe rotation export

The exporter no longer uses direct Euler decomposition as the default scalar output. It converts additive quaternions to shortest-path quaternion log-map / rotation-vector degrees, then applies conservative per-bone additive joint limits.

This avoids `+/-180` Euler wraparound and prevents 360-degree scalar interpolation.

### 3. Quaternion curves are off by default

The final Aegis importer JSON now emits only the curves the current custom data asset should consume:

- `pelvis.trans_*`
- legacy `pelvis.loc_*`
- per-bone `rot_x`, `rot_y`, `rot_z`
- foot contact alpha curves

The old `rot_q*` reference curves are disabled by default to avoid accidental import/runtime interpretation.

### 4. Validator now fails dangerous overlays

The validator now rejects:

- kick overlays longer than 3 seconds
- near-180-degree scalar rotation curves
- per-key scalar discontinuities that can create spins
- per-bone rotations exceeding V47.3 additive joint limits

The V47.2 debug file now correctly fails validation.

## Run the fixed pipeline

From the pipeline root:

```powershell
.\scripts\v46-build-backend.ps1
.\scripts\v46-start-worker.ps1
```

Open the dashboard and run the full V47.3 job. The dashboard now includes an **Overlay duration seconds** field, defaulting to `1.35`.

## Repair an already-generated bad V47 overlay

If you want to quickly test a repaired version of the current bad JSON without rerunning the whole job:

```powershell
.\scripts\12-repair-existing-overlay-v47.ps1 `
  -Input exports\bandai_soccer_kick_overlay_v47.json `
  -Out exports\bandai_soccer_kick_overlay_v47_3_repaired.json `
  -Duration 1.35
```

Then import:

```text
exports/bandai_soccer_kick_overlay_v47_3_repaired.json
```

## Expected validation after the fix

A good V47.3 overlay should look like this:

```json
{
  "valid": true,
  "schema": "aegis.overlay.curves.v2",
  "curveCount": 76,
  "maxFirstFrameRotationDegrees": 0.0,
  "dangerousNear180CurveCount": 0,
  "outOfLimitCurveCount": 0,
  "discontinuousRotationCurveCount": 0,
  "errors": []
}
```
