# V46 Architecture

## Topic

```text
aegis.pipeline.commands
```

## Redis keys

```text
aegis:v46:job:{jobId}
aegis:v46:exports:latest
```

## Pipeline steps

```text
CLONE_BANDAI_REPO
EXTRACT_BANDAI_DATA
SELECT_BANDAI_CLIPS
CONVERT_BVH_TO_FBX
IMPORT_FBX_TO_UNREAL
WAIT_FOR_MANUAL_RETARGET
EXPORT_RETARGETED_ANIMSEQUENCES
BUILD_TRAINING_MANIFEST
BUILD_TENSOR_DATASET
TRAIN_CONTACT_PRIOR
GENERATE_OVERLAY_JSON
COMPLETE
```

## Runtime ownership

Spring Boot does not do ML. It coordinates work.

```text
orchestrator-service:
- REST API
- creates jobs
- publishes Kafka commands
- stores initial Redis state

worker-service:
- consumes Kafka commands
- executes V45 scripts
- streams logs to Redis
- publishes next command

Python:
- data selection
- tensor build
- retrieval/time-warp
- contact-aware training
- final JSON generation

Unreal plugin:
- batch imports
- commandlet exports retargeted AnimSequences
- imports generated LiveBaseGeneratedOverlay JSON
```
