# Aegis AAA Animation Pipeline

Aegis AAA Animation Pipeline is a Java/Spring Boot service pipeline for converting external animation data into validated Aegis overlay JSON that can be imported into the Aegis Motion Unreal Engine plugin.

The pipeline is designed as a professional backend-style animation-processing system: source animation files are uploaded as jobs, normalized through asynchronous services, cached and tracked through Redis, processed through Kafka-backed stages, validated against a stable JSON contract, and exported as Unreal-friendly procedural animation data.

This README documents the tools, architecture, workflow, local setup, and the full start-to-finish process for creating an overlay JSON file.

---

## 1. What this pipeline does

The pipeline converts animation source data into a structured JSON overlay that the Unreal plugin can import into an `Aegis Procedural Action Asset`.

```text
BVH / FBX / JSON animation source
        |
        v
Ingestion Service
        |
        v
Kafka job event
        |
        v
Processing Service
        |
        v
Skeleton mapping + transform normalization + curve generation
        |
        v
Redis job state + generated artifact metadata
        |
        v
Validated Aegis Overlay JSON
        |
        v
Unreal Editor importer
        |
        v
Aegis Procedural Action Asset
```

The output JSON is not an FBX and not a baked Unreal animation sequence. It is a procedural overlay data file containing curves, bindings, duration, metadata, and skeleton mapping information. The Aegis Unreal plugin imports that JSON into editable curves that can drive procedural motion in the custom animation driver.

---

## 2. Core goals

- Convert external mocap or authored animation data into Aegis-compatible overlay JSON.
- Keep animation processing repeatable, inspectable, and deterministic.
- Separate heavy data processing from Unreal Editor runtime logic.
- Use backend engineering patterns relevant to online services: job orchestration, asynchronous processing, caching, validation, and observability.
- Produce clean JSON artifacts that can be versioned, reviewed, and imported by tools.
- Let designers and technical animators tune the final result in Unreal after import.

---

## 3. Tools and technologies

### Backend pipeline

| Area | Tooling |
|---|---|
| Language | Java 17+ |
| Framework | Spring Boot 3.x |
| Build system | Maven |
| Messaging | Apache Kafka |
| Cache / job state | Redis |
| Serialization | Jackson JSON |
| Validation | Bean Validation / custom schema validation |
| Local services | Docker / Docker Compose |
| Testing | JUnit, Spring Boot Test |
| API testing | Postman, curl, or PowerShell `Invoke-RestMethod` |

### Animation and game-dev side

| Area | Tooling |
|---|---|
| Engine | Unreal Engine 5.x |
| Runtime plugin | Aegis Motion Unreal plugin |
| Authoring target | `UAegisProceduralActionAsset` |
| Import target | Aegis overlay JSON importer |
| Input formats | BVH, normalized JSON, and future FBX/AMC adapters |
| Output format | Aegis overlay JSON |

### Optional development tools

| Area | Tooling |
|---|---|
| Data inspection | Python / Jupyter notebooks |
| JSON inspection | VS Code, jq |
| Kafka inspection | Kafka UI, Redpanda Console, or CLI consumers |
| Redis inspection | RedisInsight or `redis-cli` |

---

## 4. Repository structure

Recommended structure:

```text
aegis-aaa-animation-pipeline/
│
├── pom.xml
├── docker-compose.yml
├── README.md
│
├── common/
│   ├── pom.xml
│   └── src/main/java/com/aegis/common/
│       ├── dto/
│       ├── events/
│       ├── model/
│       ├── schema/
│       └── validation/
│
├── ingestion-service/
│   ├── pom.xml
│   └── src/main/java/com/aegis/ingestion/
│       ├── IngestionServiceApplication.java
│       ├── controller/
│       ├── service/
│       ├── parser/
│       └── config/
│
├── processing-service/
│   ├── pom.xml
│   └── src/main/java/com/aegis/processing/
│       ├── ProcessingServiceApplication.java
│       ├── consumer/
│       ├── service/
│       ├── mapping/
│       ├── curves/
│       ├── validation/
│       └── export/
│
├── samples/
│   ├── bvh/
│   ├── json/
│   └── mappings/
│
└── output/
    └── overlays/
```

### Module responsibilities

#### `common`

Shared code used by all services.

Typical contents:

- Job DTOs.
- Kafka event contracts.
- Animation frame models.
- Skeleton mapping models.
- Overlay JSON schema models.
- Validation result objects.
- Shared constants for Kafka topics and Redis keys.

#### `ingestion-service`

Entry point for animation files.

Responsibilities:

- Accept uploaded animation files.
- Create a job ID.
- Store the raw input temporarily.
- Parse basic file metadata.
- Publish an animation ingestion event to Kafka.
- Save job state to Redis.

#### `processing-service`

Main transformation service.

Responsibilities:

- Consume ingestion events from Kafka.
- Load source animation data.
- Parse frames, hierarchy, and channels.
- Apply skeleton mapping.
- Normalize transforms into Unreal/Aegis conventions.
- Generate pitch, roll, yaw, and optional translation curves.
- Validate the generated overlay.
- Write the final JSON artifact.
- Update job status in Redis.

---

## 5. Runtime architecture

```text
Client / CLI / Postman
        |
        | POST /api/animation/jobs
        v
+---------------------------+
| ingestion-service         |
|---------------------------|
| - accepts file upload     |
| - creates job id          |
| - stores raw input        |
| - writes Redis job state  |
| - publishes Kafka event   |
+---------------------------+
        |
        | topic: aegis.animation.ingested
        v
+---------------------------+
| Kafka                     |
|---------------------------|
| - durable event transport |
| - decouples services      |
+---------------------------+
        |
        v
+---------------------------+
| processing-service        |
|---------------------------|
| - consumes job event      |
| - parses animation        |
| - maps skeleton joints    |
| - normalizes transforms   |
| - generates curves        |
| - validates overlay JSON  |
| - writes final artifact   |
+---------------------------+
        |
        v
+---------------------------+
| Redis                     |
|---------------------------|
| - job status              |
| - progress                |
| - errors                  |
| - artifact path           |
+---------------------------+
        |
        v
output/overlays/{jobId}.aegis-overlay.json
```

---

## 6. Kafka topics

Recommended topics:

| Topic | Producer | Consumer | Purpose |
|---|---|---|---|
| `aegis.animation.ingested` | ingestion-service | processing-service | A new source file is ready for processing. |
| `aegis.animation.processed` | processing-service | optional downstream service | Overlay JSON was generated successfully. |
| `aegis.animation.failed` | ingestion-service / processing-service | monitoring tools | Job failed and includes failure context. |

Example ingestion event:

```json
{
  "jobId": "5fb21071-68c6-4469-9075-3995e3d4afad",
  "sourceFileName": "soccer_kick.bvh",
  "sourceFormat": "BVH",
  "inputPath": "storage/raw/5fb21071-68c6-4469-9075-3995e3d4afad/soccer_kick.bvh",
  "mappingProfile": "ue5_manny_quinn_default",
  "requestedOutput": "AEGIS_OVERLAY_JSON",
  "createdAt": "2026-05-07T21:00:00Z"
}
```

---

## 7. Redis job state

Recommended Redis key format:

```text
aegis:animation:job:{jobId}
```

Example value:

```json
{
  "jobId": "5fb21071-68c6-4469-9075-3995e3d4afad",
  "status": "PROCESSING",
  "progress": 65,
  "stage": "GENERATING_CURVES",
  "sourceFileName": "soccer_kick.bvh",
  "artifactPath": null,
  "error": null,
  "updatedAt": "2026-05-07T21:02:14Z"
}
```

Recommended statuses:

| Status | Meaning |
|---|---|
| `CREATED` | Job was created but has not been published. |
| `QUEUED` | Job event was published to Kafka. |
| `PROCESSING` | Processing service is actively working on the job. |
| `VALIDATING` | Overlay JSON is being checked before export. |
| `COMPLETED` | JSON artifact was created successfully. |
| `FAILED` | Job failed with a known error. |
| `CANCELLED` | Job was manually cancelled or superseded. |

---

## 8. Aegis overlay JSON contract

The final artifact should be named like this:

```text
{actionName}.aegis-overlay.json
```

Example:

```text
soccer_kick.aegis-overlay.json
```

Recommended schema:

```json
{
  "schemaVersion": "aegis.overlay.v1",
  "jobId": "5fb21071-68c6-4469-9075-3995e3d4afad",
  "actionName": "SoccerKick",
  "source": {
    "fileName": "soccer_kick.bvh",
    "format": "BVH",
    "frameRate": 60,
    "frameCount": 96
  },
  "durationSeconds": 1.6,
  "coordinateSystem": {
    "sourceUpAxis": "Y",
    "targetUpAxis": "Z",
    "rotationUnit": "degrees",
    "translationUnit": "centimeters"
  },
  "skeleton": {
    "targetProfile": "UE5_Manny_Quinn",
    "rootBone": "pelvis"
  },
  "bindings": [
    {
      "sourceJoint": "Hips",
      "targetBone": "pelvis",
      "enabled": true
    },
    {
      "sourceJoint": "RightUpLeg",
      "targetBone": "thigh_r",
      "enabled": true
    }
  ],
  "curves": [
    {
      "targetBone": "thigh_r",
      "channels": {
        "pitch": [
          { "time": 0.0, "value": 0.0 },
          { "time": 0.25, "value": -32.0 },
          { "time": 0.55, "value": 61.0 },
          { "time": 1.0, "value": 0.0 }
        ],
        "roll": [],
        "yaw": []
      }
    }
  ],
  "validation": {
    "hasCurves": true,
    "curveCount": 1,
    "warnings": []
  },
  "metadata": {
    "createdBy": "aegis-aaa-animation-pipeline",
    "notes": "Generated for Aegis Motion Unreal importer."
  }
}
```

### Required fields

| Field | Required | Purpose |
|---|---:|---|
| `schemaVersion` | Yes | Allows the Unreal importer to choose the correct parser. |
| `actionName` | Yes | Human-readable action name used when creating/importing the asset. |
| `durationSeconds` | Yes | Runtime duration of the generated action. |
| `bindings` | Yes | Maps source joints to target Unreal bones. |
| `curves` | Yes | Contains animation data for target bones. |
| `targetBone` | Yes | Unreal skeleton bone to receive the curve data. |
| `channels` | Yes | Pitch, roll, yaw, and optional translation channels. |

### Curve time convention

Curve key times should be normalized from `0.0` to `1.0`.

```text
0.0 = start of action
0.5 = middle of action
1.0 = end of action
```

The Unreal plugin can then scale the normalized curve time by the action asset's `DurationSeconds`.

### Rotation convention

- Rotation values are stored in degrees.
- Channels are stored as pitch, roll, and yaw.
- The processing service should normalize source rotations into the convention expected by the Unreal importer.
- Sign corrections should be handled in the processing layer or through a mapping profile, not manually edited after every import.

---

## 9. Default UE5 Manny/Quinn mapping profile

Recommended default mapping:

```json
{
  "profileName": "ue5_manny_quinn_default",
  "bindings": [
    { "sourceJoint": "Hips", "targetBone": "pelvis" },
    { "sourceJoint": "Spine", "targetBone": "spine_01" },
    { "sourceJoint": "Chest", "targetBone": "spine_02" },
    { "sourceJoint": "UpperChest", "targetBone": "spine_03" },

    { "sourceJoint": "RightUpLeg", "targetBone": "thigh_r" },
    { "sourceJoint": "RightLeg", "targetBone": "calf_r" },
    { "sourceJoint": "RightFoot", "targetBone": "foot_r" },

    { "sourceJoint": "LeftUpLeg", "targetBone": "thigh_l" },
    { "sourceJoint": "LeftLeg", "targetBone": "calf_l" },
    { "sourceJoint": "LeftFoot", "targetBone": "foot_l" },

    { "sourceJoint": "RightArm", "targetBone": "upperarm_r" },
    { "sourceJoint": "RightForeArm", "targetBone": "lowerarm_r" },
    { "sourceJoint": "RightHand", "targetBone": "hand_r" },

    { "sourceJoint": "LeftArm", "targetBone": "upperarm_l" },
    { "sourceJoint": "LeftForeArm", "targetBone": "lowerarm_l" },
    { "sourceJoint": "LeftHand", "targetBone": "hand_l" },

    { "sourceJoint": "Neck", "targetBone": "neck_01" },
    { "sourceJoint": "Head", "targetBone": "head" }
  ]
}
```

Store this as:

```text
samples/mappings/ue5_manny_quinn_default.json
```

---

## 10. Local setup

### Prerequisites

Install:

- Java 17 or newer.
- Maven 3.9 or newer.
- Docker Desktop.
- Git.
- Unreal Engine 5.x for the import step.

Verify:

```bash
java -version
mvn -version
docker --version
```

On Windows PowerShell:

```powershell
java -version
mvn -version
docker --version
```

---

## 11. Docker Compose for Kafka and Redis

Example `docker-compose.yml`:

```yaml
services:
  redis:
    image: redis:7
    container_name: aegis-redis
    ports:
      - "6379:6379"

  zookeeper:
    image: confluentinc/cp-zookeeper:7.6.1
    container_name: aegis-zookeeper
    environment:
      ZOOKEEPER_CLIENT_PORT: 2181
      ZOOKEEPER_TICK_TIME: 2000
    ports:
      - "2181:2181"

  kafka:
    image: confluentinc/cp-kafka:7.6.1
    container_name: aegis-kafka
    depends_on:
      - zookeeper
    ports:
      - "9092:9092"
    environment:
      KAFKA_BROKER_ID: 1
      KAFKA_ZOOKEEPER_CONNECT: zookeeper:2181
      KAFKA_ADVERTISED_LISTENERS: PLAINTEXT://localhost:9092
      KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR: 1
```

Start infrastructure:

```bash
docker compose up -d
```

Stop infrastructure:

```bash
docker compose down
```

---

## 12. Application configuration

Recommended `application.yml` for `ingestion-service`:

```yaml
server:
  port: 8081

spring:
  application:
    name: ingestion-service
  kafka:
    bootstrap-servers: localhost:9092
  data:
    redis:
      host: localhost
      port: 6379

aegis:
  storage:
    raw-input-dir: storage/raw
  kafka:
    topics:
      ingested: aegis.animation.ingested
      failed: aegis.animation.failed
```

Recommended `application.yml` for `processing-service`:

```yaml
server:
  port: 8082

spring:
  application:
    name: processing-service
  kafka:
    bootstrap-servers: localhost:9092
    consumer:
      group-id: aegis-processing-service
      auto-offset-reset: earliest
  data:
    redis:
      host: localhost
      port: 6379

aegis:
  storage:
    raw-input-dir: storage/raw
    overlay-output-dir: output/overlays
  kafka:
    topics:
      ingested: aegis.animation.ingested
      processed: aegis.animation.processed
      failed: aegis.animation.failed
```

---

## 13. Build the project

From the repository root:

```bash
mvn clean install
```

If a service cannot find the `common` module, build from the root parent project instead of from the individual service folder.

Expected order:

```text
common -> ingestion-service -> processing-service
```

---

## 14. Run the services

Open two terminals.

Terminal 1:

```bash
mvn -pl ingestion-service spring-boot:run
```

Terminal 2:

```bash
mvn -pl processing-service spring-boot:run
```

Alternative from inside each module:

```bash
cd ingestion-service
mvn spring-boot:run
```

```bash
cd processing-service
mvn spring-boot:run
```

If Spring Boot cannot find the main class, add this to the module `pom.xml`:

```xml
<properties>
    <start-class>com.aegis.ingestion.IngestionServiceApplication</start-class>
</properties>
```

For `processing-service`:

```xml
<properties>
    <start-class>com.aegis.processing.ProcessingServiceApplication</start-class>
</properties>
```

---

# 15. Step-by-step: create Aegis overlay JSON from start to finish

This is the canonical workflow for producing a JSON file that can be imported into Unreal.

---

## Step 1: Prepare the source animation file

Use a clean source animation file such as:

```text
samples/bvh/soccer_kick.bvh
```

Recommended source requirements:

- One clear action per file.
- No extra idle frames unless they are intentionally part of the action.
- Consistent frame rate.
- Known skeleton hierarchy.
- Source joint names that can be mapped to UE5 Manny/Quinn bones.

For first validation, BVH is preferred because it is text-based and easier to inspect than FBX.

---

## Step 2: Prepare the skeleton mapping profile

Create or select a mapping file:

```text
samples/mappings/ue5_manny_quinn_default.json
```

This tells the pipeline how source joints map to Unreal bones.

Example:

```json
{
  "profileName": "ue5_manny_quinn_default",
  "bindings": [
    { "sourceJoint": "Hips", "targetBone": "pelvis" },
    { "sourceJoint": "RightUpLeg", "targetBone": "thigh_r" },
    { "sourceJoint": "RightLeg", "targetBone": "calf_r" },
    { "sourceJoint": "RightFoot", "targetBone": "foot_r" }
  ]
}
```

The mapping profile should be treated as a real production asset. If the output animation looks twisted or flipped, fix the mapping or transform normalization rules rather than manually editing the final JSON every time.

---

## Step 3: Start infrastructure

Run Kafka and Redis:

```bash
docker compose up -d
```

Confirm containers are running:

```bash
docker ps
```

---

## Step 4: Start the ingestion service

```bash
mvn -pl ingestion-service spring-boot:run
```

Expected result:

```text
Started IngestionServiceApplication
Tomcat started on port 8081
```

---

## Step 5: Start the processing service

```bash
mvn -pl processing-service spring-boot:run
```

Expected result:

```text
Started ProcessingServiceApplication
Kafka consumer subscribed to aegis.animation.ingested
```

---

## Step 6: Upload the animation file as a job

Using curl:

```bash
curl -X POST "http://localhost:8081/api/animation/jobs" \
  -F "file=@samples/bvh/soccer_kick.bvh" \
  -F "sourceFormat=BVH" \
  -F "mappingProfile=ue5_manny_quinn_default" \
  -F "actionName=SoccerKick"
```

Windows PowerShell:

```powershell
$uri = "http://localhost:8081/api/animation/jobs"
$form = @{
    file = Get-Item "samples/bvh/soccer_kick.bvh"
    sourceFormat = "BVH"
    mappingProfile = "ue5_manny_quinn_default"
    actionName = "SoccerKick"
}
Invoke-RestMethod -Uri $uri -Method Post -Form $form
```

Expected response:

```json
{
  "jobId": "5fb21071-68c6-4469-9075-3995e3d4afad",
  "status": "QUEUED",
  "message": "Animation job queued for processing."
}
```

---

## Step 7: Track job progress

Call:

```bash
curl "http://localhost:8081/api/animation/jobs/5fb21071-68c6-4469-9075-3995e3d4afad"
```

Expected intermediate response:

```json
{
  "jobId": "5fb21071-68c6-4469-9075-3995e3d4afad",
  "status": "PROCESSING",
  "progress": 65,
  "stage": "GENERATING_CURVES"
}
```

Expected final response:

```json
{
  "jobId": "5fb21071-68c6-4469-9075-3995e3d4afad",
  "status": "COMPLETED",
  "progress": 100,
  "stage": "COMPLETED",
  "artifactPath": "output/overlays/SoccerKick.aegis-overlay.json"
}
```

---

## Step 8: Validate the generated JSON

Open the generated file:

```text
output/overlays/SoccerKick.aegis-overlay.json
```

Minimum validation checks:

- `schemaVersion` exists.
- `durationSeconds` is greater than `0`.
- `bindings` is not empty.
- `curves` is not empty.
- At least one curve has at least two keys.
- Curve times are normalized between `0.0` and `1.0`.
- Rotation values are in degrees.
- Target bone names match the Unreal skeleton.

Optional command-line check with `jq`:

```bash
jq '.schemaVersion, .actionName, .durationSeconds, (.curves | length)' output/overlays/SoccerKick.aegis-overlay.json
```

Expected output:

```text
"aegis.overlay.v1"
"SoccerKick"
1.6
12
```

---

## Step 9: Import the JSON into Unreal

In Unreal Engine:

1. Open `AegisMotionSandbox`.
2. Make sure the Aegis Motion plugin is enabled.
3. Open the Content Browser.
4. Create or select an `Aegis Procedural Action Asset`.
5. Use the Aegis importer tool.
6. Select `output/overlays/SoccerKick.aegis-overlay.json`.
7. Confirm the target skeleton/profile is UE5 Manny/Quinn.
8. Import the curves into the action asset.
9. Save the asset.

Recommended asset location:

```text
/Game/Aegis/Actions/SoccerKick_AegisAction
```

---

## Step 10: Test the imported action in the animation driver

In Unreal:

1. Open the Character Blueprint.
2. Add or confirm the `AegisProceduralActionComponent` exists.
3. Assign or trigger the imported `SoccerKick_AegisAction` asset.
4. Open the Animation Blueprint.
5. Confirm the `Aegis Procedural Motion Driver` node is in the graph.
6. Feed the base pose into the driver.
7. Play in editor.
8. Trigger the action.
9. Enable debug if needed:

```text
aegis.Motion.DebugProceduralDriver 2
```

Expected result:

- The action runs from start to finish.
- Curves drive the mapped bones.
- The motion blends back into the base pose.
- Debug visualization shows affected bones and applied rotations.

---

## Step 11: Tune the action in Unreal

After import, the JSON should not be treated as final animation polish. It should be treated as generated procedural source data.

Technical animators can tune:

- Rotation multipliers.
- Per-bone alpha values.
- Curve keys.
- Blend-in and blend-out settings.
- Damping and smoothing values.
- Which bones/chains are enabled.
- Debug visualization settings.

Recommended workflow:

```text
Generated JSON gives the first accurate pass.
Unreal data asset gives the designer-tuned final behavior.
```

---

# 16. Expected processing stages

The processing service should progress through these stages:

```text
CREATED
QUEUED
READING_SOURCE_FILE
PARSING_HIERARCHY
PARSING_MOTION_FRAMES
APPLYING_SKELETON_MAPPING
NORMALIZING_TRANSFORMS
GENERATING_CURVES
VALIDATING_JSON
WRITING_ARTIFACT
COMPLETED
```

A single hung job should not block another job. Each job must be tracked by its own `jobId`, and the processing service should catch failures per job rather than failing the entire consumer loop.

---

## 17. Transform normalization rules

This is the most important part of the pipeline for animation quality.

The processing service should own the conversion from source animation space into Aegis/Unreal space.

Recommended normalization responsibilities:

- Convert source units to centimeters.
- Convert source up-axis to Unreal's expected orientation.
- Convert source rotations to degrees.
- Preserve signed rotation direction.
- Apply mapping-profile-specific axis corrections.
- Remove or isolate root motion if the overlay should only be additive.
- Generate clean per-bone local curves.
- Avoid baking temporary retargeting mistakes into the final JSON.

If the animation imports with strange rotations, the likely causes are:

- Incorrect source-to-target joint mapping.
- Axis mismatch.
- Wrong rotation order.
- Incorrect handedness conversion.
- Parent/child local transform conversion error.
- Applying world-space transforms where local-space transforms are expected.

Do not solve these by randomly changing curve values. Fix the conversion layer.

---

## 18. Recommended API endpoints

### Create job

```http
POST /api/animation/jobs
Content-Type: multipart/form-data
```

Form fields:

| Field | Required | Example |
|---|---:|---|
| `file` | Yes | `soccer_kick.bvh` |
| `sourceFormat` | Yes | `BVH` |
| `mappingProfile` | Yes | `ue5_manny_quinn_default` |
| `actionName` | Yes | `SoccerKick` |

### Get job status

```http
GET /api/animation/jobs/{jobId}
```

### Download artifact

```http
GET /api/animation/jobs/{jobId}/artifact
```

### Validate overlay JSON

```http
POST /api/animation/overlays/validate
Content-Type: application/json
```

---

## 19. Error handling expectations

A professional pipeline should fail clearly and recover safely.

Recommended failure behavior:

- Mark only the affected job as `FAILED`.
- Store a clear error message in Redis.
- Publish a failure event to `aegis.animation.failed`.
- Continue processing later jobs.
- Never allow one stuck or malformed animation file to block the whole pipeline.

Example failure response:

```json
{
  "jobId": "5fb21071-68c6-4469-9075-3995e3d4afad",
  "status": "FAILED",
  "stage": "APPLYING_SKELETON_MAPPING",
  "error": "Source joint RightUpLeg could not be mapped to target skeleton profile ue5_manny_quinn_default."
}
```

---

## 20. Troubleshooting

### Maven cannot find `com.aegis:common`

Build from the parent root:

```bash
mvn clean install
```

Do not run `mvn spring-boot:run` inside a child module before the shared `common` module has been installed or included in the reactor build.

---

### Spring Boot cannot find the main class

Add `start-class` to the service module `pom.xml`.

Example:

```xml
<properties>
    <start-class>com.aegis.processing.ProcessingServiceApplication</start-class>
</properties>
```

---

### Kafka consumer does not receive jobs

Check:

- Kafka is running.
- The topic name matches in both services.
- The processing service uses the same bootstrap server.
- The consumer group is not stuck on an old offset.
- The ingestion service actually publishes after file upload.

Useful reset during local development:

```bash
docker compose down
docker compose up -d
```

---

### Redis job status stays at `QUEUED`

Likely causes:

- Processing service is not running.
- Kafka event was not published.
- Topic name mismatch.
- Consumer crashed while parsing.
- Redis key was written but not updated after processing started.

---

### Job gets stuck at `VALIDATING_JSON`

Likely causes:

- Validator is throwing but not marking the job as failed.
- JSON artifact is never written to disk.
- Empty curves are being generated.
- The validation stage expects required fields that the exporter did not populate.

Required behavior:

- Catch the validation exception.
- Set the job to `FAILED`.
- Store the exact validation error.
- Allow later jobs to continue.

---

### Generated JSON imports but has no curves

Likely causes:

- Source joints did not match target bindings.
- Curves were filtered out because values were all zero.
- Exporter wrote metadata but skipped channels.
- The Unreal importer expects a different field name than the JSON exporter wrote.
- The pipeline wrote frame samples but not curve key arrays.

Check:

```bash
jq '.curves | length' output/overlays/SoccerKick.aegis-overlay.json
jq '.curves[0]' output/overlays/SoccerKick.aegis-overlay.json
```

---

## 21. Quality checklist before importing into Unreal

Before importing a generated overlay JSON, confirm:

- The file opens as valid JSON.
- `schemaVersion` is correct.
- `durationSeconds` is correct.
- Target bone names match the Unreal skeleton.
- Curves are present.
- Curve keys are normalized from `0.0` to `1.0`.
- Rotation values are in degrees.
- Translation values are in centimeters if present.
- There are no unmapped critical joints.
- The action name is clean and editor-friendly.

---

## 22. Professional portfolio framing

This pipeline demonstrates backend engineering applied to game-development tooling:

- Java/Spring Boot service design.
- Kafka-based asynchronous job processing.
- Redis-backed job state and progress tracking.
- JSON schema design and validation.
- Data transformation from source files to engine-facing runtime data.
- Toolchain integration between backend services and Unreal Engine.
- Practical animation pipeline thinking: source data, mapping, normalization, validation, import, tuning, and runtime playback.

The important portfolio message is not that this replaces a studio animation pipeline. The stronger message is that it shows the ability to build reliable tools around complex data, integrate them with game-engine workflows, and think about production concerns such as validation, observability, failure isolation, and maintainability.

---

## 23. Minimal end-to-end command summary

```bash
# 1. Start Kafka and Redis
docker compose up -d

# 2. Build all modules
mvn clean install

# 3. Run ingestion service
mvn -pl ingestion-service spring-boot:run

# 4. Run processing service in another terminal
mvn -pl processing-service spring-boot:run

# 5. Upload BVH source animation
curl -X POST "http://localhost:8081/api/animation/jobs" \
  -F "file=@samples/bvh/soccer_kick.bvh" \
  -F "sourceFormat=BVH" \
  -F "mappingProfile=ue5_manny_quinn_default" \
  -F "actionName=SoccerKick"

# 6. Poll job status
curl "http://localhost:8081/api/animation/jobs/{jobId}"

# 7. Inspect generated JSON
jq '.schemaVersion, .actionName, .durationSeconds, (.curves | length)' output/overlays/SoccerKick.aegis-overlay.json

# 8. Import the JSON into the Aegis Motion Unreal plugin
```

---

## 24. Roadmap

Planned improvements:

- Full FBX adapter with explicit transform-space validation.
- AMC adapter for CMU mocap workflows.
- JSON schema file published under `common/schema`.
- Web dashboard for job status and artifact download.
- Visual curve inspection page.
- Batch processing for animation folders.
- Golden test fixtures for known motions such as kick, punch, recoil, and locomotion lean.
- Automated Unreal import test fixtures.
- Better detection of axis and handedness errors.
- Per-bone confidence/warning output in generated JSON.

---

## 25. Definition of done for a generated JSON

A generated overlay JSON is considered complete when:

1. The pipeline job reaches `COMPLETED`.
2. The artifact exists under `output/overlays`.
3. JSON validation passes.
4. The file contains non-empty curves.
5. All required UE target bones are mapped.
6. Unreal importer accepts the file.
7. An Aegis Procedural Action Asset is populated.
8. The action plays through the Aegis Procedural Motion Driver.
9. The motion can be debugged and tuned in Unreal.
10. Starting a new job is not affected by any previous failed or completed job.

