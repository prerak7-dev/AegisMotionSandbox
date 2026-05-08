# V47 Architecture — Neural Overlay JSON Pipeline

## System intent

V47 turns the pipeline into a complete motion-data-to-Aegis-overlay factory:

```text
source repository
→ selected BVH clips
→ target-skeleton retargeted training JSON
→ tensor dataset
→ neural model checkpoint
→ final Aegis custom-data-asset overlay JSON
→ validator report
```

The final artifact is not an FBX and not a preview retarget dump. It is the JSON file the Aegis importer should ingest into the custom data asset.

## Runtime ownership

```text
Spring Boot orchestrator-service
- REST API
- dashboard
- job creation/deletion
- Redis job state reads/writes
- Kafka command publishing

Spring Boot worker-service
- Kafka command consumption
- ordered pipeline step execution
- PowerShell script launch
- live log/progress streaming into Redis
- export artifact registration

Python motion-prior-service
- offline-retargeted training JSON tensorization
- retrieval/time-warp seed selection
- neural overlay-prior training
- final overlay JSON generation
- import-shape validation

Blender
- BVH source import
- Manny/Quinn target FBX import
- target-rest-space retarget sampling
- local parent-bone-space training JSON emission

AegisMotion Unreal plugin
- imports exports/bandai_soccer_kick_overlay_v47.json
- creates/populates the custom data asset
- runtime plays curves as LiveBaseGeneratedOverlay
```

## Kafka topic

```text
aegis.pipeline.commands
```

## Redis keys

```text
aegis:v47:job:{jobId}
aegis:v47:exports:latest
```

V47 can still read/delete legacy V46 job keys for dashboard cleanup:

```text
aegis:v46:job:{jobId}
```

## Default V47 pipeline steps

```text
CLONE_BANDAI_REPO
EXTRACT_BANDAI_DATA
SELECT_BANDAI_CLIPS
OFFLINE_RETARGET_TO_MANNY_JSON
BUILD_TRAINING_MANIFEST
BUILD_TENSOR_DATASET
TRAIN_NEURAL_MOTION_PRIOR
GENERATE_OVERLAY_JSON
VERIFY_OVERLAY_JSON
COMPLETE
```

## Legacy comparison path

The following steps remain in source for debugging/comparison, but are not the default V47 path:

```text
CONVERT_BVH_TO_FBX
IMPORT_FBX_TO_UNREAL
WAIT_FOR_MANUAL_RETARGET
EXPORT_RETARGETED_ANIMSEQUENCES
```

## Data contracts

### Offline retarget training JSON

Produced by:

```text
tools/blender/offline_retarget_bandai_to_manny_json.py
```

Shape:

```json
{
  "schema": "aegis.offlineRetarget.trainingFrames.v1",
  "frames": [
    {
      "time": 0.0,
      "bones": {
        "thigh_r": {
          "rotationQuaternion": [1.0, 0.0, 0.0, 0.0],
          "rotationEulerXYZDegrees": [0.0, 0.0, 0.0],
          "localTranslation": [0.0, 0.0, 0.0]
        }
      }
    }
  ],
  "contactCurves": {}
}
```

### Tensor dataset

Produced by:

```text
motion-prior-service/aegis_motion_prior/dataset.py
```

Files:

```text
motions.npy  # [clip, frame, feature]
mask.npy     # [clip, frame]
metadata.json
```

Feature layout:

```text
root translation xyz: 3
22 mannequin bones as 6D rotations: 22 * 6
contact channels: 4
total: 139
```

### Neural checkpoint

Produced by:

```text
motion-prior-service/aegis_motion_prior/train_neural_v47.py
```

File:

```text
checkpoints/bandai_v47_neural_overlay/neural_overlay_prior_v47.pt
```

Checkpoint includes:

```text
model state
input dim
model config
condition vocabulary
training metadata
best loss metrics
```

### Final overlay JSON

Produced by:

```text
motion-prior-service/aegis_motion_prior/infer_neural_v47.py
```

File:

```text
exports/bandai_soccer_kick_overlay_v47.json
```

Shape:

```json
{
  "schema": "aegis.overlay.curves.v2",
  "playbackMode": "LiveBaseGeneratedOverlay",
  "basePoseMode": "UseLiveSourcePose",
  "curves": []
}
```

Curve channels emitted:

```text
pelvis.trans_x/trans_y/trans_z
pelvis.loc_x/loc_y/loc_z
bone.rot_x/rot_y/rot_z
bone.rot_qx/rot_qy/rot_qz/rot_qw
foot contact/IK alpha curves
```

## Validation gates

The validator fails the job if:

```text
curves is missing
curves is not a list
curves is empty
curves have no keys
key time/value fields are non-numeric
all values are zero
```

Validator:

```text
motion-prior-service/aegis_motion_prior/validate_overlay_json.py
```

Report:

```text
exports/bandai_soccer_kick_overlay_v47.json.validation.json
```

## Dashboard API additions

Delete selected job:

```http
DELETE /api/v1/jobs/{jobId}
```

Delete completed/failed jobs:

```http
DELETE /api/v1/jobs?terminalOnly=true
```

## Production robustness choices

1. Offline retargeting is now the default so the pipeline does not stop mid-job for manual Unreal operations.
2. Training JSON is bone-local and target-skeleton-aligned, so ML learns the same space the plugin consumes.
3. Inference uses retrieval/time-warp as the seed to preserve plausible contact timing.
4. Neural output is blended with the retrieved seed instead of replacing it wholesale.
5. The final export contains Euler degree curves for the current importer and quaternion reference curves for future importer revisions.
6. Validation runs as a hard gate after generation.
7. Dashboard deletion removes Redis job/log clutter without touching generated files on disk.
