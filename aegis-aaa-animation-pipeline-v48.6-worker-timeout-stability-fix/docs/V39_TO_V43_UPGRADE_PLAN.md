# V39 → V43 Upgrade Plan

## V39: Unreal Training Clip Exporter
Export any selected Manny/Quinn AnimSequence into Aegis JSON training format.

## V40: Batch retarget/export workflow
Use the manifest builder to track exported clips and metadata.

## V41: Retrieval + time-warp
This is the first route to visibly high quality because it reuses real animation motion.

## V42: Contact-aware denoising model
Use a model to clean/adjust retrieved clips, not hallucinate everything from scratch.

## V43: Dynamic head look-at and late IK polish
The plugin should apply:
1. live base locomotion
2. generated overlay
3. dynamic head/neck look-at
4. plant-foot-only IK as a final post-process

No root/chest correction from foot IK.
