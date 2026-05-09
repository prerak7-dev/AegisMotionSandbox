# AegisMotionSandbox

AegisMotionSandbox is an Unreal Engine portfolio project focused on data-driven procedural animation, animation-authoring tools, and an external Java/Spring Boot animation processing pipeline. The project is designed to demonstrate how gameplay-facing animation systems, editor tooling, and backend-style processing services can work together in a professional game-development workflow.

The core idea is simple: animation intent should be authored as structured data, debugged visually in the editor, triggered by gameplay, and generated or refined through repeatable tools instead of being locked inside one-off hard-coded animation logic.

## What this project demonstrates

- A custom Unreal Engine C++ plugin for procedural character motion.
- Data assets that describe procedural actions, bone chains, phases, curves, and blending behavior.
- Runtime components that trigger actions from gameplay and feed state into a custom animation node.
- Editor tooling for debugging, scrubbing, visualizing, and authoring procedural motion.
- A mocap-to-curves workflow for turning BVH/FBX/JSON animation data into editable Unreal-friendly procedural curves.
- A companion Java/Spring Boot + Redis + Kafka processing pipeline for asynchronous animation ingestion, validation, and overlay generation.

This is not positioned as a shipped game feature. It is a focused engineering sandbox intended to show production-minded architecture, maintainable systems design, and the ability to connect backend services, data pipelines, Unreal runtime code, and editor-facing tools.

## High-level architecture

```text
Animation Source Data
BVH / FBX / JSON / authored curves
        |
        v
Java/Spring Boot Animation Pipeline
ingestion-service -> Kafka topics -> processing-service -> Redis job state/cache
        |
        v
Validated Aegis Overlay JSON
curve data, skeleton mapping, duration, schema metadata
        |
        v
Aegis Motion Unreal Plugin
Editor importer -> Aegis Procedural Action Asset -> AnimGraph driver
        |
        v
Runtime Character Motion
curve-driven procedural pose layers, debug visualization, designer tuning
```

## Unreal plugin overview

The Aegis Motion plugin is the primary runtime/editor deliverable. It is built around a declarative action asset and a custom animation driver that evaluates action data at runtime.

### Core runtime responsibilities

- Start, update, blend, and finish procedural actions from gameplay.
- Read action data from `UAegisProceduralActionAsset`.
- Evaluate phase timing, curve values, alpha blending, and per-bone transforms.
- Apply procedural pose offsets through a custom AnimGraph node.
- Support predictable blending back to the base animation pose.
- Provide runtime debug logging and optional on-screen visualization.

### Core editor responsibilities

- Expose action data cleanly in Unreal's Details panel.
- Allow scrub-based inspection of action timing and curve values.
- Visualize affected bones, chains, constraints, and debug labels in the viewport.
- Import external animation data into editable procedural action assets.
- Keep authoring workflows data-driven so designers can tune motion without recompiling code.

## Main Unreal components

### `UAegisProceduralActionAsset`

The main authoring asset for procedural actions.

It stores:

- Action duration.
- Named motion phases such as windup, strike, recovery, or a single combined phase.
- Bone or chain definitions.
- Per-bone pitch, roll, yaw, and optional translation curves.
- Blending and smoothing parameters.
- Runtime/debug flags used by editor and animation systems.

The asset is intentionally data-first. The animation node evaluates this data rather than hard-coding a specific kick, lean, recoil, or gesture.

### `UAegisProceduralActionComponent`

A gameplay-facing component that owns action state.

Typical responsibilities:

- Start an action from a Character Blueprint or C++ gameplay call.
- Track action time, normalized time, alpha, and active state.
- Expose debug scrub controls for editor workflows.
- Provide the current action asset to the animation driver.
- Prevent action state from being scattered across the Anim Blueprint.

Typical usage:

```text
Character Blueprint
Input Action -> StartAction(AegisActionAsset)
        |
        v
AegisProceduralActionComponent
tracks current action state
        |
        v
Anim Blueprint Driver Node
evaluates action data into pose offsets
```

### `FAnimNode_AegisProceduralMotionDriver`

The custom AnimGraph runtime node that applies procedural motion.

Responsibilities include:

- Resolve the active `UAegisProceduralActionComponent`.
- Read the currently active action asset.
- Evaluate normalized action time.
- Sample the authored curves for affected bones.
- Apply local or component-space transforms in a stable order.
- Blend against the incoming base pose.
- Support debug drawing and logging when enabled.

The node is designed to act as the single procedural pose driver rather than spreading motion logic across many one-off graph nodes.

### Editor details customization and visualizer

The editor layer improves iteration speed by exposing runtime and authoring data in a visual workflow.

Current authoring/debugging goals:

- Scrub an action at any normalized time value.
- See which bones and chains are affected.
- Inspect curve values and applied rotations.
- Display bone markers, debug text, constraints, and ghost-pose style guides.
- Select and tune procedural data from editor-facing panels.

## Java/Spring Boot animation processing pipeline

AegisMotionSandbox also includes a backend-style animation processing concept designed to mirror real online-service engineering patterns.

The service pipeline is useful because raw animation data can be expensive, noisy, or inconsistent. Instead of importing everything directly into Unreal, the external pipeline can normalize and validate animation data before Unreal consumes it.

### Service layout

```text
common
shared DTOs, job schemas, validation contracts, curve models

 ingestion-service
accepts animation source files or requests, creates jobs, publishes ingestion events

 processing-service
consumes queued jobs, parses animation data, normalizes transforms, writes overlay JSON
```

### Messaging and state

- Kafka is used for asynchronous job messages between pipeline stages.
- Redis is used for job status, short-lived cache data, worker coordination, and preventing one hung job from blocking unrelated jobs.
- JSON is used as the engine-facing artifact because it is easy to validate, diff, review, and import into Unreal editor tooling.

### Why this matters for online services engineering

Although the content domain is animation, the pipeline exercises the same engineering concerns that appear in online game services:

- asynchronous work queues,
- service boundaries,
- state isolation,
- schema validation,
- job health tracking,
- structured logging,
- retry-safe processing stages,
- observable failure states,
- clear API/data contracts between systems.

## Data flow

### 1. Author or import source animation

Source animation can come from:

- hand-authored procedural curves,
- BVH mocap files,
- FBX animation exports,
- JSON generated by an external tool or pipeline.

### 2. Normalize into Aegis overlay data

The animation processing layer maps external animation channels into Unreal-friendly procedural channels.

Example mapping intent:

```text
Mocap Hips      -> pelvis
Mocap Spine     -> spine_01
Mocap Chest     -> spine_02
Mocap RightLeg  -> calf_r
Mocap RightFoot -> foot_r
```

### 3. Import into Unreal

The Unreal editor importer reads validated overlay JSON and populates an Aegis procedural action asset with editable curves.

### 4. Tune in the editor

Designers and technical animators can adjust:

- rotation multipliers,
- alpha blending,
- curve keys,
- affected bone lists,
- smoothing values,
- phase timing,
- debug and visualization settings.

### 5. Trigger from gameplay

Gameplay calls `StartAction` on the procedural action component. The AnimGraph driver reads the active action and applies the procedural pose layer over the incoming animation pose.

## Example overlay JSON contract

The exact schema can evolve, but the intended contract is structured around explicit metadata, skeleton mapping, duration, and animation curves.

```json
{
  "schemaVersion": "aegis.motion.overlay.v1",
  "source": "pipeline/generated",
  "skeleton": "UE5_Mannequin",
  "durationSeconds": 1.20,
  "curves": [
    {
      "bone": "thigh_r",
      "channel": "pitch",
      "keys": [
        { "time": 0.00, "value": 0.0 },
        { "time": 0.35, "value": -18.0 },
        { "time": 0.60, "value": 42.0 },
        { "time": 1.00, "value": 0.0 }
      ]
    }
  ]
}
```

## Runtime workflow

1. Add `UAegisProceduralActionComponent` to the character.
2. Create or import a `UAegisProceduralActionAsset`.
3. In the Character Blueprint, call `StartAction` when gameplay input should trigger the procedural action.
4. In the Anim Blueprint, place the Aegis procedural motion driver after the base locomotion pose.
5. Point the driver at the character/component state.
6. Play in editor and use debug tools to inspect timing, curves, and applied transforms.

## Recommended Anim Blueprint placement

```text
Locomotion State Machine
        |
        v
Cached Base Pose
        |
        v
Aegis Procedural Motion Driver
        |
        v
Output Pose
```

This placement keeps the system additive and easier to reason about. Base locomotion remains responsible for normal movement, while Aegis contributes short-lived procedural action layers.

## Debugging

The project uses a console variable for runtime visibility:

```text
aegis.Motion.DebugProceduralDriver 0
```

Common values:

```text
0 = off
1 = log procedural driver state
2 = draw debug visualization
```

Useful debug checks:

- Confirm that the character owns an `AegisProceduralActionComponent`.
- Confirm that `StartAction` is called with a valid action asset.
- Confirm that normalized time moves from 0 to 1.
- Confirm that expected curve channels have keys.
- Confirm that the affected bone names match the active skeleton.
- Confirm that debug scrub mode is disabled during normal runtime playback.

## Build requirements

Recommended environment:

- Unreal Engine 5.x.
- Visual Studio 2022 on Windows.
- C++ development tools for Unreal.
- Git.
- Java 21+ for the external service pipeline.
- Spring Boot 3.x for pipeline services.
- Redis for job state/cache workflows.
- Kafka for asynchronous pipeline messaging.

## Repository layout

A typical repository layout is expected to look like this:

```text
AegisMotionSandbox/
  AegisMotionSandbox.uproject
  Plugins/
    AegisMotion/
      Source/
        AegisMotion/
          Public/
          Private/
        AegisMotionEditor/
          Public/
          Private/
  pipeline/
    common/
    ingestion-service/
    processing-service/
  README.md
```

The Unreal plugin should keep runtime code and editor-only code separate. Runtime animation nodes and gameplay components belong in the runtime module. Details customizations, visualizers, menu entries, and import UI belong in the editor module.

## Professional engineering principles used

- Keep runtime and editor modules separated.
- Keep data assets declarative.
- Keep animation transforms inspectable.
- Prefer one coherent driver node over many disconnected experimental nodes.
- Treat generated data as a contract with validation, schema metadata, and predictable failure states.
- Design long-running processing jobs so one stuck job does not block unrelated jobs.
- Use structured logging and visible status states when a pipeline stage fails.
- Make technical animation tooling understandable to engineers, animators, and reviewers.

## Current portfolio focus

The strongest portfolio narrative for this project is the combination of:

1. Unreal Engine runtime animation systems.
2. Editor tooling for technical animation iteration.
3. Java/Spring Boot service architecture for asynchronous animation data processing.
4. Redis/Kafka-backed job orchestration and validation concepts.
5. Clean documentation that explains how a reviewer should evaluate the code.

This makes AegisMotionSandbox useful not only as an animation plugin, but also as evidence of system design, tooling sensibility, and backend-service thinking applied to game development.


## Roadmap

Planned improvements:

- Stronger overlay JSON schema validation.
- More robust skeleton mapping UI.
- Better isolation and cancellation behavior for hung processing jobs.
- Persistent import reports for unmatched bones and discarded curves.
- More automated tests for curve evaluation and transform correctness.
- Cleaner sample actions for kick, recoil, upper-body aim, and locomotion response.
- Demo-ready documentation and Java code sample packaging.

## License

This repository is intended as a personal portfolio and learning project unless a separate license file states otherwise.
