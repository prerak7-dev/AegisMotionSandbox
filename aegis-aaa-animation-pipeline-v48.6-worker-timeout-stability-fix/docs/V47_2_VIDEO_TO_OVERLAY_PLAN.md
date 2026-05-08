# V47.2 Video-to-Overlay Research Direction

Video can be useful for Aegis, but it should not replace curated mocap as the main training source yet.

## Recommended production stance

Use video as a reference/conditioning signal, not as the primary ground-truth skeleton source.

Broadcast soccer footage is usually monocular, compressed, camera-moving, partially occluded, and has limited visibility for feet and limb twist. For an additive Unreal overlay, those are exactly the things that matter most: planted-foot timing, pelvis orientation, knee direction, ankle/ball contact, and upper-body counterbalance.

## Better hierarchy of sources

1. Retargeted mocap from known skeleton sources.
2. AMASS/CMU/KIT-style 3D motion datasets converted through the same Manny/Quinn retarget path.
3. Video-derived 3D body estimates used as weak labels, style references, or clip search/annotation signals.
4. Pure generative text/video motion only after strong validation and cleanup.

## Future V48 path

```text
soccer video
  -> player detection/tracking
  -> 2D/3D pose or SMPL estimation
  -> temporal smoothing + foot-contact estimation
  -> fit to Manny/Quinn skeleton
  -> convert to first-frame-relative additive tensor
  -> validate against foot/contact/rotation limits
  -> export Aegis curves[] overlay JSON
```

This should be a separate `VIDEO_TO_OVERLAY_WEAK_SUPERVISION` branch in the pipeline, not the default path.
