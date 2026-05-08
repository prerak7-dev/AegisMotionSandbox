from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

import bpy
from mathutils import Vector


VERSION = "V46.35"

# Bandai clips are 30fps. Keep the exported FBX on integer 30fps frame boundaries.
AEGIS_EXPORT_FPS = 30

# Bandai BVH values come into Unreal about 100x too large if left unscaled.
# This keeps source hips around Quinn/Manny scale (~90-100cm) instead of ~9000cm.
AEGIS_BANDAI_TO_UE_SCALE = float(os.environ.get("AEGIS_BANDAI_TO_UE_SCALE", "0.01"))

# Unreal-friendly FBX export basis. If this ever needs testing, override via env vars:
#   $env:AEGIS_FBX_AXIS_FORWARD="X"
#   $env:AEGIS_FBX_AXIS_UP="Z"
AEGIS_FBX_AXIS_FORWARD = os.environ.get("AEGIS_FBX_AXIS_FORWARD", "X")
AEGIS_FBX_AXIS_UP = os.environ.get("AEGIS_FBX_AXIS_UP", "Z")


def safe_unreal_name(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]+", "_", value)
    value = re.sub(r"_+", "_", value).strip("_")
    return value or "AegisClip"


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def bandai_load_bvh(path: Path):
    """Load BVH using the Bandai visualization script's native basis.

    Bandai's own Blender visualizer uses:
      target='ARMATURE'
      use_fps_scale=False
      rotate_mode='NATIVE'
      axis_forward='-Z'
      axis_up='Y'

    We preserve that loading basis so Blender sees the motion the same way the official
    visualizer does, then we export through a UE-specific FBX basis.
    """
    bpy.ops.import_anim.bvh(
        filepath=str(path),
        filter_glob="*.bvh",
        target="ARMATURE",
        global_scale=AEGIS_BANDAI_TO_UE_SCALE,
        frame_start=1,
        use_fps_scale=False,
        use_cyclic=False,
        rotate_mode="NATIVE",
        axis_forward="-Z",
        axis_up="Y",
    )


def find_imported_armature():
    armatures = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
    if not armatures:
        raise RuntimeError("BVH import produced no armature.")
    return armatures[0]


def find_pose_bone(armature, names):
    for name in names:
        pb = armature.pose.bones.get(name)
        if pb:
            return pb
    return None


def find_rebase_pose_bone(armature):
    return (
        find_pose_bone(armature, ["Hips", "hips", "Pelvis", "pelvis", "joint_Root", "Joint_Root", "Root", "root"])
        or (armature.pose.bones[0] if armature.pose.bones else None)
    )


def iter_action_fcurves(action):
    """Blender-version-safe FCurve iterator.

    Blender 5.x may use layered actions where action.fcurves is not exposed.
    Key clamping is best-effort only; FBX baking below is the authoritative output.
    """
    if action is None:
        return []

    direct = getattr(action, "fcurves", None)
    if direct is not None:
        try:
            return list(direct)
        except Exception:
            pass

    found = []
    try:
        for layer in getattr(action, "layers", []):
            for strip in getattr(layer, "strips", []):
                bags = []
                for attr in ("channelbags", "channel_bags"):
                    value = getattr(strip, attr, None)
                    if value is not None:
                        try:
                            bags.extend(list(value))
                        except Exception:
                            pass
                for attr in ("channelbag", "channel_bag"):
                    value = getattr(strip, attr, None)
                    if value is not None and not callable(value):
                        bags.append(value)
                for bag in bags:
                    fcurves = getattr(bag, "fcurves", None)
                    if fcurves is not None:
                        try:
                            found.extend(list(fcurves))
                        except Exception:
                            pass
    except Exception:
        pass

    return found


def force_integer_30fps_scene(armature):
    scene = bpy.context.scene
    scene.render.fps = AEGIS_EXPORT_FPS
    scene.render.fps_base = 1.0

    action = armature.animation_data.action if armature.animation_data else None
    if not action:
        scene.frame_start = 1
        scene.frame_end = 2
        print("[Aegis Blender] WARNING: No action found; using fallback frame range 1..2")
        return

    try:
        start, end = action.frame_range
    except Exception:
        start, end = scene.frame_start, scene.frame_end

    start_i = int(round(start))
    end_i = int(round(end))
    if end_i <= start_i:
        end_i = start_i + 1

    scene.frame_start = start_i
    scene.frame_end = end_i

    fcurves = iter_action_fcurves(action)
    if fcurves:
        for fcurve in fcurves:
            for key in fcurve.keyframe_points:
                key.co.x = round(key.co.x)
                key.handle_left.x = round(key.handle_left.x)
                key.handle_right.x = round(key.handle_right.x)
            try:
                fcurve.update()
            except Exception:
                pass
        print(f"[Aegis Blender] Clamped {len(fcurves)} fcurves to integer frames.")
    else:
        print("[Aegis Blender] Blender 5 layered action: no direct fcurves exposed; relying on FBX bake step.")

    print(f"[Aegis Blender] Forced integer {AEGIS_EXPORT_FPS}fps range: {scene.frame_start}..{scene.frame_end}")


def normalize_animation_names(armature, clip_name: str):
    safe = safe_unreal_name(clip_name)

    # Keep a stable, simple armature wrapper name. The actual useful retarget root
    # remains Hips/joint_Root inside the skeleton.
    armature.name = f"BNR_{safe}"
    armature.data.name = f"BNR_Skeleton_{safe}"

    if not armature.animation_data:
        armature.animation_data_create()

    if armature.animation_data.action:
        armature.animation_data.action.name = f"ANIM_{safe}"
    else:
        action = bpy.data.actions.new(f"ANIM_{safe}")
        armature.animation_data.action = action

    for action in bpy.data.actions:
        if action != armature.animation_data.action:
            action.use_fake_user = False

    print(f"[Aegis Blender] Unique FBX action/take: {armature.animation_data.action.name}")


def log_pose_diagnostics(armature, label):
    scene = bpy.context.scene
    scene.frame_set(scene.frame_start)
    bpy.context.view_layer.update()

    hip = find_rebase_pose_bone(armature)
    if not hip:
        print(f"[Aegis Blender] {label}: no hip/root pose bone found.")
        return

    world_loc = armature.matrix_world @ hip.matrix.translation
    all_locs = [armature.matrix_world @ pb.matrix.translation for pb in armature.pose.bones]
    if all_locs:
        mn = Vector((min(v.x for v in all_locs), min(v.y for v in all_locs), min(v.z for v in all_locs)))
        mx = Vector((max(v.x for v in all_locs), max(v.y for v in all_locs), max(v.z for v in all_locs)))
        ext = mx - mn
        print(
            f"[Aegis Blender] {label}: hip={hip.name} world=({world_loc.x:.3f}, {world_loc.y:.3f}, {world_loc.z:.3f}) "
            f"bbox_min=({mn.x:.3f}, {mn.y:.3f}, {mn.z:.3f}) "
            f"bbox_max=({mx.x:.3f}, {mx.y:.3f}, {mx.z:.3f}) "
            f"extent=({ext.x:.3f}, {ext.y:.3f}, {ext.z:.3f})"
        )


def rebase_armature_to_first_frame(armature):
    """Place first-frame hips near world origin horizontally while preserving height."""
    scene = bpy.context.scene
    scene.frame_set(scene.frame_start)
    bpy.context.view_layer.update()

    pb = find_rebase_pose_bone(armature)
    if not pb:
        print("[Aegis Blender] WARNING: Could not find rebase pose bone; skipping origin rebase.")
        return

    world_loc = armature.matrix_world @ pb.matrix.translation
    horizontal_offset = Vector((-world_loc.x, -world_loc.y, 0.0))
    armature.location += horizontal_offset

    bpy.context.view_layer.update()
    new_world_loc = armature.matrix_world @ pb.matrix.translation

    print(
        "[Aegis Blender] Rebasing first-frame source hips/root. "
        f"Bone={pb.name} old_world=({world_loc.x:.3f}, {world_loc.y:.3f}, {world_loc.z:.3f}) "
        f"new_world=({new_world_loc.x:.3f}, {new_world_loc.y:.3f}, {new_world_loc.z:.3f})"
    )


def add_unreal_proxy_mesh(armature, clip_name: str):
    """Add a tiny skinned mesh so Unreal creates a Skeleton/Anim import target.

    The visual mesh is irrelevant for retargeting; the skeleton/bones are the data.
    """
    if not armature.data.bones:
        raise RuntimeError("Armature has no bones; cannot create proxy mesh.")

    safe = safe_unreal_name(clip_name)
    root_bone_name = armature.data.bones[0].name

    mesh = bpy.data.meshes.new(f"AegisProxyMeshData_{safe}")
    verts = [
        (-1.0, -1.0, 0.0),
        (1.0, -1.0, 0.0),
        (0.0, 1.0, 0.0),
        (0.0, 0.0, 2.0),
    ]
    faces = [(0, 1, 2), (0, 1, 3), (1, 2, 3), (2, 0, 3)]
    mesh.from_pydata(verts, [], faces)
    mesh.update()

    obj = bpy.data.objects.new(f"AegisProxyMesh_{safe}", mesh)
    bpy.context.collection.objects.link(obj)
    obj.parent = armature
    obj.matrix_parent_inverse = armature.matrix_world.inverted()

    vg = obj.vertex_groups.new(name=root_bone_name)
    vg.add(list(range(len(verts))), 1.0, "ADD")

    modifier = obj.modifiers.new("AegisArmature", "ARMATURE")
    modifier.object = armature

    print(f"[Aegis Blender] Added Unreal proxy mesh weighted to root bone: {root_bone_name}")
    return obj


def convert_one(src: Path, dst: Path, frame_start: int | None = None, frame_end: int | None = None):
    if not src.exists():
        raise FileNotFoundError(f"BVH source does not exist: {src}")

    clear_scene()
    dst.parent.mkdir(parents=True, exist_ok=True)

    clip_name = safe_unreal_name(dst.stem)

    print(f"[Aegis Blender] {VERSION}: raw Bandai-native conversion path; preserving BVH animation tracks.")
    print(f"[Aegis Blender] Loading BVH through Bandai-native loader: {src}")
    print(f"[Aegis Blender] Bandai loader basis: axis_forward=-Z axis_up=Y rotate_mode=NATIVE scale={AEGIS_BANDAI_TO_UE_SCALE}")
    bandai_load_bvh(src)

    armature = find_imported_armature()
    normalize_animation_names(armature, clip_name)
    force_integer_30fps_scene(armature)
    log_pose_diagnostics(armature, "After Bandai-native load")
    rebase_armature_to_first_frame(armature)
    log_pose_diagnostics(armature, "After horizontal rebase")
    add_unreal_proxy_mesh(armature, clip_name)

    if frame_start is not None:
        bpy.context.scene.frame_start = int(frame_start)
    if frame_end is not None:
        bpy.context.scene.frame_end = int(frame_end)

    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.context.scene.objects:
        if obj.type in {"ARMATURE", "MESH"}:
            obj.select_set(True)
    bpy.context.view_layer.objects.active = armature

    print(
        f"[Aegis Blender] Exporting FBX for Unreal: axis_forward={AEGIS_FBX_AXIS_FORWARD} axis_up={AEGIS_FBX_AXIS_UP}"
    )
    bpy.ops.export_scene.fbx(
        filepath=str(dst),
        use_selection=True,
        object_types={"ARMATURE", "MESH"},
        add_leaf_bones=False,
        bake_anim=True,
        bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True,
        bake_anim_step=1.0,
        bake_anim_simplify_factor=0.0,
        apply_unit_scale=True,
        axis_forward=AEGIS_FBX_AXIS_FORWARD,
        axis_up=AEGIS_FBX_AXIS_UP,
    )

    if not dst.exists():
        raise RuntimeError(f"Expected FBX was not created: {dst}")

    print(f"[Aegis Blender] Converted {src} -> {dst}")


def user_args() -> list[str]:
    argv = list(sys.argv)
    if "--" in argv:
        return argv[argv.index("--") + 1:]

    known = {"--src", "--dst", "--frame-start", "--frame-end"}
    out: list[str] = []
    i = 0
    while i < len(argv):
        if argv[i] in known:
            out.append(argv[i])
            if i + 1 < len(argv):
                out.append(argv[i + 1])
                i += 2
                continue
        i += 1
    return out


def main():
    parser = argparse.ArgumentParser(prog="aegis_convert_bvh_to_fbx")
    parser.add_argument("--src", default=os.environ.get("AEGIS_BVH_SRC"))
    parser.add_argument("--dst", default=os.environ.get("AEGIS_FBX_DST"))
    parser.add_argument("--frame-start", type=int, default=None)
    parser.add_argument("--frame-end", type=int, default=None)

    args, unknown = parser.parse_known_args(user_args())
    if unknown:
        print(f"[Aegis Blender] Ignoring unknown args: {unknown}")

    if not args.src or not args.dst:
        print("[Aegis Blender] sys.argv:")
        print(sys.argv)
        print("[Aegis Blender] AEGIS_BVH_SRC:", os.environ.get("AEGIS_BVH_SRC"))
        print("[Aegis Blender] AEGIS_FBX_DST:", os.environ.get("AEGIS_FBX_DST"))
        raise SystemExit("Missing required source/destination. Provide --src/--dst or AEGIS_BVH_SRC/AEGIS_FBX_DST env vars.")

    convert_one(Path(args.src), Path(args.dst), args.frame_start, args.frame_end)


if __name__ == "__main__":
    main()
