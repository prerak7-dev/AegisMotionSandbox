# Aegis V47.5 — Knee-Coupled Overlay Fix

This patch fixes the case where the V47.4 overlay passed validation and stopped spinning, but still looked like a shuffle instead of a kick.

## Root cause

The generated JSON was structurally valid and came from a real `kick_normal` source, but the lower-leg motion was exported incorrectly:

- `thigh_r.rot_x` had strong kick swing: about `93.7°`.
- `calf_r.rot_x` had only about `5.5°`.
- Most calf energy was lost during lower-body axis remapping because V47.4 used the same log-map `Z -> rot_x` mapping for thighs and calves.
- In the Bandai retargeted kick, calf/knee flexion energy often sits on log-map `X`, so V47.4 damped it into `rot_z` and clamped it as twist.

The validator now warns about this readability failure even when the overlay is technically safe.

## Pipeline changes

Changed files:

```text
motion-prior-service/aegis_motion_prior/exporter.py
motion-prior-service/aegis_motion_prior/repair_scalar_overlay_kick_v47_5.py
motion-prior-service/aegis_motion_prior/validate_overlay_json.py
scripts/12-repair-existing-overlay-v47.ps1
```

The exporter now uses bone-specific lower-body mapping:

```text
thigh: source/logmap Z -> Aegis rot_x
calf:  source/logmap X -> Aegis rot_x
foot:  source/logmap Z -> Aegis rot_x
```

Then, for soccer kick overlays only, it applies an anatomical readability pass:

```text
thigh.rot_x = primary swing
calf.rot_x  = opposite-sign knee flexion/extension when calf motion is too weak
foot.rot_x  = smaller toe/ankle presentation
plant leg   = damped so it stabilizes instead of looking like a second kick
rot_y/z     = damped on the kick leg to avoid shuffle/twist
```

## Test repair included

A repair of the uploaded V47.4 JSON is included here:

```text
exports/bandai_soccer_kick_overlay_v47_5_knee_coupled_repaired.json
```

Validation report:

```text
exports/bandai_soccer_kick_overlay_v47_5_knee_coupled_repaired.json.validation.json
```

Expected important values:

```text
thigh_r.rot_x peak: about 89.5°
calf_r.rot_x peak:  about 108.0°
foot_r.rot_x peak:  about 56.8°
```

## Repair an existing V47.4 overlay before rerunning the full job

```powershell
.\scripts\12-repair-existing-overlay-v47.ps1 `
  -Input exports\bandai_soccer_kick_overlay_v47.json `
  -Out exports\bandai_soccer_kick_overlay_v47_5_knee_coupled_repaired.json `
  -DominantLeg right
```

Use `-StabilizeOnly` only for the old V47.4 spin-sanitization repair path. For the soccer kick issue, do not use `-StabilizeOnly`.

## Important paired plugin fix

This pipeline patch should be used with the V47.5 plugin runtime patch. The plugin patch makes scalar `LiveBaseGeneratedOverlay` curves start from the current AnimGraph source pose instead of the skeleton reference pose.
