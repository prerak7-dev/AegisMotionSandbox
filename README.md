
# AegisMotionSandbox

**AegisMotionSandbox** is a high-fidelity Unreal Engine 5 sandbox focused on **procedural character animation for competitive sports gameplay**, built to showcase **engine-level animation systems, data-driven motion, and runtime extensibility**.

This project demonstrates how **gameplay intent** (player input, ball position, action phase) can be transformed into **deterministic, multiplayer-safe procedural motion** using custom C++ plugins, animation nodes, and curve-driven motion profiles — without relying on authored animation clips.

> This repository is designed as a **technical portfolio project** for building complex systemic animation.

---

## Why This Project

**AegisMotionSandbox** was built with the vision of deploying a new gameplay experience where motion is driven procedurally through curve profiles such that different actions visually represent the emotion behind player inputs. The idea is to dynamically update curves based on the certain aspects of the players inputs such as rhythms/specific patterns and base phases of action motions on these aspects. The system will then contain checkpoint curves that depict the essence of different things: well known melodies/ gait rhythms of different animals, natural elemenets(such as wind, water,lightning), mythological entities, which will be compared to player input curves to evoke abilities based on the entities depicted along with changing player stats and visual aspects such as aura. The system is designed explicitly with these principles in mind:

- Strong separation between **gameplay systems and animation**
- Heavy use of **procedural motion layered on authored data**
- Robust **tooling and engine-level extensibility**
- Characters that must feel *physical, grounded, and reactive* in systemic worlds



### Design that aligns with the principles so far:
- **Data over clips**: Motion is driven by curves and profiles, not baked animations.
- **Runtime-first architecture**: All logic lives in reusable C++ plugins.
- **Layered procedural control**: Spine, legs, IK, and action phases are independently authored and composed.
- **Determinism-friendly**: No randomness or hidden engine state in evaluation paths.
- **Editor tooling**: Custom AnimGraph nodes with editor wrappers, not Blueprint hacks.

This project tries to answer the question:
> *“How would you design a modern, systemic animation framework for action-heavy characters that must react dynamically to gameplay?”*

---

## Key Features

### Custom Animation Plugin (AegisMotion)
- Runtime + Editor plugin architecture
- Clean module separation
- Reusable across projects

### Procedural Skeletal Control
- Custom AnimGraph skeletal control nodes
- Multi-bone chains (spine, legs)
- Pivot-weighted rotation distribution
- Per-joint constraints

### Soccer Action System (Example Use Case)
- Procedural kick action built from first principles
- Explicit action phases:
  - Wind-up
  - Contact
  - Follow-through
- Dominant-foot aware logic

### Curve-Driven Motion Profiles
- All motion authored as data
- Designers tune motion without recompiling
- Profiles define:
  - Joint angles over time
  - Phase windows
  - Follow-through shaping

---

## High-Level Architecture

### System Flow Diagram

```
Player Input / Ball State
        |
        v
+-------------------------+
| Action Motion Component |
| (C++ Runtime Logic)     |
+-------------------------+
        |
        v
+-------------------------+
| Curve-Based Motion      |
| Profiles (DataAssets)   |
+-------------------------+
        |
        v
+-------------------------+
| Action Motion Output    |
| (Angles, Phases, Flags)|
+-------------------------+
        |
        v
+-------------------------+
| AnimBP Event Graph      |
| (Data Plumbing)         |
+-------------------------+
        |
        v
+-------------------------+
| Custom AnimGraph Nodes  |
| (Procedural Control)    |
+-------------------------+
        |
        v
Final Skeletal Pose
```

---

## Animation Graph Layering (Conceptual)

```
Base Locomotion
      |
Control Rig (Stability / Grounding)
      |
Procedural Spine (Aim, Lean)
      |
Procedural Kick (Hinge Nodes)
      |
Plant Foot IK (Support Leg)
      |
Strike Foot Replant IK
      |
Final Output Pose
```

This ordering ensures:
- Stability from Control Rig
- Freedom for procedural strike motion
- Correct grounding during and after action

---

## Repository Structure

```
AegisMotionSandbox/
├─ Plugins/
│  └─ AegisMotion/
│     ├─ Source/
│     │  ├─ AegisMotion/          # Runtime module
│     │  └─ AegisMotionEditor/    # AnimGraph editor nodes
│     └─ AegisMotion.uplugin
│
├─ Content/
│  ├─ AegisMotion/
│  │  ├─ Curves/                  # Motion profiles
│  │  ├─ DataAssets/              # Action definitions
│  │  └─ AnimBPs/                 # Animation blueprints
│
├─ Source/
├─ Config/
└─ README.md
```

---

## Getting Started

### Requirements
- Unreal Engine **5.7**
- Visual Studio 2022 (C++ workload)
- Windows 10 / 11

### Setup
```bash
git clone https://github.com/prerak7-dev/AegisMotionSandbox.git
```

1. Right-click `AegisMotionSandbox.uproject`
2. Select **Generate Visual Studio project files**
3. Open the solution
4. Build **Development Editor**
5. Launch the project

---

## Controls (Default)
- **WASD** – Move
- **Mouse** – Camera
- **Shoot Input** – Trigger procedural kick

(Input uses Enhanced Input and is fully configurable.)

---

## Technical Highlights

- Custom `FAnimNode_SkeletalControlBase` implementations
- Editor graph node wrappers (`UAnimGraphNode_*`)
- Parent-space bone rotation
- Explicit phase-driven animation
- IK vs procedural conflict resolution
- Multiplayer-safe evaluation patterns

---

## Initial Roadmap

- Automatic dominant-foot selection based on ball side
- Anticipatory motion using velocity prediction
- Procedural heading and diving actions
- Multiplayer replication validation
- Player “style” profiles (personality-driven motion)

---

## Author

**Prerak Pandey**  

---

## Disclaimer

This project is a **technical sandbox and portfolio piece**.  
It is not a complete game and is intended to demonstrate **systems design and implementation quality** rather than content polish.
