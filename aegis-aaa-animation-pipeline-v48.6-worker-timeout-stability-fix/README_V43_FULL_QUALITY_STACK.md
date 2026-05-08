# Aegis AAA Animation Pipeline V43 — Full Quality Stack

V43 bundles the roadmap from V39 to V43.

## V39 — Training Clip Exporter bridge

Unreal editor exporter: retargeted Manny/Quinn AnimSequence → Aegis training JSON.

## V40 — Batch manifest builder

Open:

```text
http://localhost:8092/manifest-builder
```

Build a manifest from retargeted Aegis JSON clips.

## V41 — Retrieval + time-warp

Before neural generation is good, retrieval is the best quality path:

```text
find best real clip
→ time-warp to requested duration
→ smooth lightly
→ export LiveBaseGeneratedOverlay JSON
```

## V42 — Contact-aware denoising prior

Adds losses for:

- rotation
- root motion
- velocity
- acceleration
- contacts

## V43 — Runtime polish metadata

Generated JSON includes:

- dynamic head look-at metadata
- late plant-foot-only IK metadata
- no root/chest/head correction from IK

## Recommended command flow

```powershell
.\scripts\start-v43-dashboard.ps1
```

Then open:

```text
http://localhost:8092/v43
```

After you have retargeted/exported real Aegis JSON clips and created `datasets/v37_real`:

```powershell
.\scripts\train-contact-prior-v42.ps1
.\scripts\generate-overlay-v43.ps1
```

Output:

```text
exports/aegis_v43_soccer_kick_overlay.json
```

Import that into AegisMotion V43 plugin or V36+ live-base overlay plugin.
