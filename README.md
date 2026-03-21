# Aegis Motion

Aegis Motion is an Unreal Engine plugin for authoring and applying procedural, data-driven skeletal motion on top of an existing animation pose. The current implementation focuses on **action-driven procedural overlays**: a runtime component starts an action, an AnimGraph skeletal control node evaluates the action asset, and per-bone phase curves generate parent-space rotation and translation offsets across named chains such as spine, legs, or arms.

The plugin is split into a **runtime module** and an **editor module**:

- **AegisMotion** - runtime data structures, action component, skeletal control node, cache building, phase evaluation, smoothing, and debug drawing.
- **AegisMotionEditor** - editor-only details customization, viewport visualizer, hit proxies, scrub workflow, and the AnimGraph editor node wrapper.

The system is already strong as a prototype for AAA-style procedural animation tooling because it combines:

- data assets for motion authoring
- chain-based bone organization
- per-phase procedural layering
- per-bone motion limits and motion profiles
- curve-driven rotation and translation channels
- debug scrubbing inside the editor
- in-viewport curve inspection and key creation

---

## What is implemented today

### 1) Action asset authoring
The core authored asset is `UAegisProceduralActionAsset`.

It stores:

- a target skeletal mesh
- an action duration in seconds
- one or more **chains** (`FAegisChainDef_Inline`)

Each chain contains:

- `ChainName`
- `StartBone` and `EndBone`
- an optional chain alpha curve and multiplier
- chain-level smoothing/debug flags
- auto-populated ordered socket bones
- one or more **phases** (`FAegisActionPhaseBlendDef`)

Each socket bone contains:

- `BoneName`
- `BoneWeight`
- rotation limits in degrees per axis
- translation limits in centimeters per axis
- a per-bone motion profile

Each phase contains:

- `PhaseName`
- `StartTime01`, `PeakTime01`, `EndTime01`
- ease-in and ease-out exponents
- optional `PhaseAlpha01`
- a `BoneCurves` slot list matching the chain's socket bones

Each bone slot can author:

- `Alpha01`
- `RotX01`, `RotY01`, `RotZ01`
- `PosX01`, `PosY01`, `PosZ01`
- rotation and translation multipliers

### 2) Automatic asset fixup tools
`UAegisProceduralActionAsset` includes editor callable auto-fixup methods:

- **Auto Fixup: Phase Names + Defaults**
- **Auto Fixup: Populate Socket Bones**
- **Auto Fixup: Phase Bone Slots**

These are also triggered from `PostLoad` and `PostEditChangeProperty` in editor builds.

Current fixups do the following:

- create a default phase if a chain has none
- normalize invalid phase timing ranges
- ensure phase names are unique and non-empty
- build an inclusive ref-skeleton path from `StartBone` to `EndBone`
- auto-populate `SocketBones` in chain order
- preserve existing authored settings when possible
- keep each phase's `BoneCurves` slots aligned with the socket bone list

This is one of the strongest parts of the current codebase because it reduces authoring drift between chain definitions and curve slots.

### 3) Runtime action playback component
`UAegisProceduralActionComponent` is the runtime entry point used by gameplay and the animation blueprint.

It provides:

- `StartAction(UAegisProceduralActionAsset*, float InAlpha)`
- `StopAction()`
- `GetState()`
- `IsActionActive()`
- `GetCurrentActionAsset()`
- `GetActionTime01()`
- `GetActionAlpha()`
- `GetActionInstanceId()`

It tracks:

- active action asset
- normalized time
- action alpha
- running state
- elapsed seconds
- instance id used by the anim node to detect action restarts

It also supports **editor debug scrubbing**:

- `bDebugScrubEnabled`
- `DebugScrubTimeSeconds`
- `bFreezeTimeWhenScrubbing`
- `GetEffectiveTime01()`

When scrubbing is active in editor, the component forces animation refresh on the skeletal mesh component so the procedural pose updates while the slider is manipulated.

### 4) AnimGraph skeletal control node
The runtime procedural application happens inside `FAnimNode_AegisProceduralMotionDriver`, a skeletal control node.

It resolves action state from either:

1. an `ActionComponent`, or
2. manual overrides (`ActionAssetOverride`, `ActionTime01Override`, `ActionAlphaOverride`)

At evaluation time, the node:

- resolves the action asset, normalized time, and alpha
- validates and/or rebuilds runtime caches when the asset changes
- detects new action instances and resets smoothed state
- captures the input pose at action start
- evaluates each enabled chain
- computes per-bone target offsets from curves and phase weights
- smooths offsets using per-bone motion profiles
- applies the offsets in parent/local space and writes component-space transforms back to the pose

### 5) Cache building and runtime ordering
Each chain builds a runtime cache (`FAegisActionChainRuntimeCache`) containing:

- ordered chain bones
- ordered socket bone runtime entries (`FAegisSocketBoneRuntimeCache`)

Each socket bone runtime entry stores:

- compact pose bone index
- motion limits
- motion profile
- smoothed rotation/translation state
- spring velocities
- captured local transform from the incoming pose

The node calculates a signature for the action asset and rebuilds caches when:

- the asset pointer changes
- chain count changes
- key authored properties affecting layout change

### 6) Phase blending and procedural evaluation
Per-bone targets are computed by layering:

- action alpha
- chain alpha curve and multiplier
- automatic phase envelope
- optional phase alpha curve
- per-bone alpha curve
- per-bone weight
- per-bone axis curves and multipliers
- authored limits in degrees and centimeters

The phase envelope is not hard-switched. It uses a triangular/eased window based on:

- start
- peak
- end
- ease-in exponent
- ease-out exponent

This makes overlapping phases possible and allows windup / strike / follow-through style authoring without explicit state machines inside the asset.

### 7) Motion smoothing
The current smoothing model is per-bone and per-axis.

Each socket bone stores:

- damping half-life
- spring strength
- inertia
- max rotation speed
- max translation speed

The node uses:

- `HalfLifeAlpha()`
- `StepSpringDamperFloat()`

This gives a blend of:

- inertial lag
- damped target settling
- critical damping behavior
- speed clamping

That is why the current system already feels closer to production procedural motion than a simple direct-curve application.

### 8) Debugging and editor visualization
The plugin already contains two layers of debug support.

#### Runtime node debug
`FAnimNode_AegisProceduralMotionDriver` supports:

- `bDebugDraw`
- `bDebugDrawJointNumbers`
- console variable `aegis.Motion.DebugProceduralDriver`

It can draw:

- bone axes
- approximate rotation cones
- text labels with current smoothed rotation and translation values

#### Editor visualizer
`FAegisProceduralActionComponentVisualizer` provides:

- viewport bone markers for socket bones
- clickable hit proxies per bone
- ghost pose markers from evaluated offsets
- cone visualization from rotation limits
- HUD labels in the viewport
- contextual curve discovery for the selected bone
- context menu actions to add/update a key at the current scrub time

This makes the authoring loop far better than raw data asset editing alone.

### 9) Details customization and scrub UI
`FAegisProceduralActionComponentDetails` adds a custom details panel for the action component.

It exposes:

- scrub enable toggle
- freeze toggle
- a normalized slider mapped to the action duration
- direct numeric entry for scrub seconds

This is the key bridge between authored curves and interactive viewport review.

---

## Current system architecture

```text
Gameplay / Blueprint
    |
    v
UAegisProceduralActionComponent
    - owns active action state
    - advances time or exposes scrubbed time
    - emits action instance id
    |
    v
Animation Blueprint
    |
    v
FAnimNode_AegisProceduralMotionDriver
    - resolves action state
    - builds chain/socket runtime caches
    - captures incoming pose at action start
    - evaluates phase/curve weights
    - smooths target offsets
    - writes final component-space transforms
    |
    v
Final skeletal pose

Editor Layer
    |
    +-- FAegisProceduralActionComponentDetails
    |      - scrub slider and debug controls
    |
    +-- FAegisProceduralActionComponentVisualizer
           - markers, cones, ghost pose, click selection, key creation
```

---

## Authoring workflow

### Basic workflow
1. Create an `Aegis Procedural Action Asset`.
2. Assign the skeletal mesh.
3. Set `DurationSeconds`.
4. Add one or more chains.
5. For each chain, set `StartBone` and `EndBone`.
6. Run auto-fixup to populate socket bones and phase slots.
7. Author chain-level and phase-level timing.
8. Assign curves per bone and per axis.
9. Add an `Aegis Procedural Action Component` to the actor.
10. Start the action from gameplay or Blueprint.
11. In the Animation Blueprint, place **Aegis Procedural Motion Driver** after the source pose you want to modify.
12. Feed the component reference into the node.
13. Scrub, inspect, and iterate.

### Recommended authoring pattern
For action-style movement, author each chain with a clear intent:

- **Standing leg** - stability, counter-rotation, low translation
- **Kicking leg** - strongest follow-through, wider limits, stronger phase asymmetry
- **Spine** - distributed twist and lean, moderate smoothing
- **Arms / hands** - secondary balance, smaller per-bone amplitudes, shorter damping

### Practical curve strategy
Use phases as semantic layers:

- **Windup** - anticipation and pre-load
- **Strike** - peak energy / acceleration / contact pose
- **FollowThrough** - decay and recovery

Then use per-bone alpha curves to decide which bones are dominant during each phase.

---

## Usage example

### Runtime / Blueprint usage
- Add `UAegisProceduralActionComponent` to your character.
- Keep a reference to a `UAegisProceduralActionAsset`.
- Call `StartAction(ActionAsset, 1.0f)` when the move begins.

### Animation Blueprint usage
- Place `Aegis Procedural Motion Driver` in the AnimGraph.
- Feed your locomotion or action source pose into it.
- Assign the character's `AegisProceduralActionComponent` to the node.

Conceptually:

```text
Base Animation Pose
    -> Aegis Procedural Motion Driver
    -> Output Pose
```

---

## Strengths of the current implementation

- Clean split between runtime and editor modules
- Good data-driven structure for chain/phase/bone authoring
- Strong editor-side fixup utilities
- Useful cache model for runtime evaluation
- Per-bone motion profiles enable more believable motion
- Action instance id handling avoids stale smoothing when a new action begins
- Debug scrubbing already supports fast iteration
- Visualizer-based curve-key workflow is a strong foundation for tool polish

---

## Current gaps and limitations

These are important to recognize clearly.

### Layered animation stacks are not fully implemented
The current node procedurally applies offsets on top of a single incoming pose. It supports multi-phase authoring inside one action asset, but it does **not yet implement a broader layered animation framework** such as:

- upper/lower-body action layers
- multiple simultaneous procedural action assets
- additive stacks with explicit priorities
- pose masks / branch filters per layer
- conflict resolution between multiple active procedural systems

### Incoming animation to procedural transition is functional but still basic
The node captures the source pose when the action begins and uses action alpha in evaluation, but it does **not yet provide a full authored transition framework** for:

- crossfade policies
- time-based ease presets at action boundaries
- blending back into the live incoming pose with controlled decay
- different rules for rotation vs translation channels
- animation-state-aware handoff between authored animation clips and procedural motion

### Debugging tools are valuable but not yet polished for a production demo
The current tools are useful, but visually still engineer-focused. They need styling and UX upgrades for portfolio-grade presentation.

### IK Rig / Control Rig adaptation is not implemented yet
The current system operates as an AnimGraph skeletal control with direct transform writes. It does not yet expose dedicated integration layers for:

- IK Rig goals
- Control Rig controls and space switching
- bidirectional editing between Aegis curves and rig controls
- rig retargeting workflows

---

## Future intent and roadmap

The following roadmap is the logical next step for this codebase and aligns with the direction you requested.

### 1) Layered procedural animation
Add a proper layered runtime model so several procedural effects can coexist.

Recommended direction:

- introduce a **layer container** at runtime
- define layer types such as `Action`, `Secondary`, `Recovery`, `Aim`, `Balance`
- support per-layer blend weights and priorities
- support per-layer bone masks or chain masks
- evaluate all layers into an accumulator before final application
- allow additive, override, and weighted-merge modes

This would let the plugin drive, for example:

- kicking motion
- secondary arm balance
- reactive torso correction
- landing recovery

all as separate controllable layers.

### 2) Smooth transition blending between base animation and procedural motion
This should become a first-class feature.

Recommended direction:

- add explicit `BlendInSeconds` and `BlendOutSeconds`
- maintain a **live base pose reference** and a **captured start pose**
- support multiple blend policies:
  - captured pose -> procedural
  - live pose -> procedural
  - procedural -> live pose
  - inertialized blend-out
- allow separate blend curves for:
  - chain alpha
  - rotation
  - translation
- optionally expose motion warping style correction for contact moments

This will make transitions feel intentional rather than just mathematically smooth.

### 3) Beautified debug tools
The current debug tools should evolve into portfolio-quality tooling.

Recommended upgrades:

- toggleable label categories: names, rotations, translations, weights, phase values
- cleaner marker shapes and color coding by chain / phase / bone role
- scalable marker sizes by camera distance
- viewport legend panel
- curve mini-inspector in details or a dockable editor panel
- selected-bone highlighting with stronger UX states
- optional trajectory or arc previews for extremities
- debug presets: Animator, Technical Animator, Programmer

### 4) IK Rig adaptation
A natural next step is to let procedural outputs drive IK workflows.

Recommended direction:

- expose chain endpoints or virtual targets as IK goals
- let an action asset optionally produce **goal transforms** instead of only bone offsets
- solve feet/hands through IK Rig after procedural target generation
- allow contact locking for foot plant and ball strike windows
- support retarget-safe chain definitions for multiple character rigs

This would make the system more robust for gameplay contact and multi-character reuse.

### 5) Control Rig adaptation
This system is also a strong candidate for a Control Rig bridge.

Recommended direction:

- create a Control Rig unit or data interface that reads an Aegis action asset
- drive Control Rig controls instead of directly writing skeletal transforms when needed
- support bake-out from Aegis procedural motion to rig controls for animator polish
- support reverse authoring: manipulate rig controls, then write back Aegis curves
- provide rig-space channels and per-control remapping

That would move the plugin from a runtime-only procedural tool toward a hybrid runtime/authoring platform.

### 6) Better architecture for production scaling
For a larger production version, the system should likely be split into:

- **Authoring layer** - assets, validators, curve authoring tools, batch fixup
- **Runtime evaluation layer** - pure evaluation and blending
- **Visualization layer** - viewport overlays, inspectors, scrub UI
- **Rig integration layer** - AnimGraph, IK Rig, Control Rig adapters
- **Debug data layer** - structured runtime telemetry and overlays

---

## Suggested next milestone sequence

1. **Blend framework upgrade** - explicit blend in/out and live-pose handoff
2. **Layer stack architecture** - multiple simultaneous procedural layers
3. **Debug UX polish** - better labels, color system, filters, inspector
4. **IK endpoints / contact windows** - especially for legs and arms
5. **Control Rig bridge** - authoring and bake workflows
6. **Retargeting and reuse** - cross-skeleton chain mapping

---

## Source map

### Runtime module
- `Source/AegisMotion/Public/AegisAction/AegisProceduralActionAsset.h`
- `Source/AegisMotion/Private/AegisAction/AegisProceduralActionAsset.cpp`
- `Source/AegisMotion/Public/AegisAction/AegisProceduralActionComponent.h`
- `Source/AegisMotion/Private/AegisAction/AegisProceduralActionComponent.cpp`
- `Source/AegisMotion/Public/ProceduralMotion/AnimNodes/AnimNode_AegisProceduralMotionDriver.h`
- `Source/AegisMotion/Private/ProceduralMotion/AnimNodes/AnimNode_AegisProceduralMotionDriver.cpp`

### Editor module
- `Source/AegisMotionEditor/Private/AegisMotionEditorModule.cpp`
- `Source/AegisMotionEditor/Private/AegisProceduralActionComponentDetails.cpp`
- `Source/AegisMotionEditor/Private/AegisProceduralActionComponentVisualizer.cpp`
- `Source/AegisMotionEditor/Public/AnimGraphNodes/AnimGraphNode_AegisProceduralMotionDriver.h`
- `Source/AegisMotionEditor/Private/AnimGraphNodes/AnimGraphNode_AegisProceduralMotionDriver.cpp`

---

## Summary
Aegis Motion already has the shape of a serious procedural animation tool:

- authorable data assets
- runtime action playback
- curve-driven per-bone motion
- automatic phase blending
- smoothing and motion limits
- viewport debugging and scrubbing

Its next evolution is not about replacing the current design, but about **expanding it into a layered procedural animation platform** with stronger blend transitions, more polished visualization, and first-class rig integration through IK Rig and Control Rig.
