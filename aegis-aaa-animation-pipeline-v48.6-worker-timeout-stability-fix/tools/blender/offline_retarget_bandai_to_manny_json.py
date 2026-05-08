from __future__ import annotations
import argparse, json, math, sys, traceback
from pathlib import Path
import bpy
from mathutils import Quaternion, Vector, Euler

VERSION = "V46.37"

def log(s): print(f"[Aegis {VERSION}] {s}")

def load_json(p): return json.loads(Path(p).read_text(encoding="utf-8"))

def save_json(p, data):
    p = Path(p); p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(data, indent=2), encoding="utf-8")
    log(f"Wrote {p}")

def safe_name(s):
    import re
    return re.sub(r"_+", "_", re.sub(r"[^A-Za-z0-9_-]+", "_", str(s))).strip("_") or "clip"

def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()

def setup_scene(fps):
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 0.01
    try: sc.unit_settings.length_unit = "CENTIMETERS"
    except Exception: pass
    sc.render.fps = fps
    sc.render.fps_base = 1.0

def import_target_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path), automatic_bone_orientation=False)
    new = list(set(bpy.data.objects) - before)
    arms = [o for o in new if o.type == "ARMATURE"] or [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    if not arms: raise RuntimeError(f"Target FBX produced no armature: {path}")
    arms[0].name = "Aegis_Target_MannyQuinn"
    log(f"Target armature bones={len(arms[0].pose.bones)}")
    return arms[0]

def import_bandai_bvh(path, scale):
    before = set(bpy.data.objects)
    bpy.ops.import_anim.bvh(filepath=str(path), filter_glob="*.bvh", target="ARMATURE",
                            global_scale=scale, frame_start=1, use_fps_scale=False,
                            use_cyclic=False, rotate_mode="NATIVE", axis_forward="-Z", axis_up="Y")
    new = list(set(bpy.data.objects) - before)
    arms = [o for o in new if o.type == "ARMATURE"]
    if not arms: raise RuntimeError(f"BVH produced no armature: {path}")
    arms[0].name = "Aegis_Source_Bandai"
    log(f"Source armature bones={len(arms[0].pose.bones)}")
    return arms[0]

def rest_world(arm, pb): return arm.matrix_world @ pb.bone.matrix_local
def pose_world(arm, pb): return arm.matrix_world @ pb.matrix

def height(arm):
    pts = [(arm.matrix_world @ pb.bone.matrix_local).translation for pb in arm.pose.bones]
    return max(1.0, max(p.z for p in pts) - min(p.z for p in pts)) if pts else 1.0

def qlist(q):
    q = q.copy(); q.normalize()
    return [float(q.w), float(q.x), float(q.y), float(q.z)]

def vlist(v): return [float(v.x), float(v.y), float(v.z)]

def euler_deg(q):
    e = q.to_euler("XYZ")
    return [math.degrees(e.x), math.degrees(e.y), math.degrees(e.z)]

def offset_q(vals):
    vals = vals or [0,0,0]
    return Euler(tuple(math.radians(float(v)) for v in vals), "XYZ").to_quaternion()

def frame_range(src):
    a = src.animation_data.action if src.animation_data else None
    if a:
        s,e = a.frame_range
        return int(round(s)), int(round(e))
    return int(bpy.context.scene.frame_start), int(bpy.context.scene.frame_end)

def ordered_targets(target, names):
    names = set(names); out=[]
    def visit(b):
        if b.name in names and b.name not in out: out.append(b.name)
        for c in b.children: visit(c)
    for b in target.data.bones:
        if b.parent is None: visit(b)
    for n in names:
        if n not in out: out.append(n)
    return out

def mapped_parent_name(target, bone_name, target_to_source):
    b = target.data.bones.get(bone_name)
    p = b.parent if b else None
    while p:
        if p.name in target_to_source: return p.name
        p = p.parent
    return None

def contacts(frames, fps, cfg):
    def calc(bone):
        pos = [Vector(f["bones"].get(bone, {}).get("worldTranslation", [0,0,0])) for f in frames]
        if not pos: return []
        floor = min(p.z for p in pos)
        raw=[]
        for i,p in enumerate(pos):
            speed = 0 if i == 0 else (p - pos[i-1]).length * fps
            raw.append(1.0 if p.z <= floor + cfg["heightThresholdCm"] and speed <= cfg["velocityThresholdCmPerSec"] else 0.0)
        w = int(cfg.get("smoothWindowFrames", 2))
        if w <= 0: return raw
        sm=[]
        for i in range(len(raw)):
            a=max(0,i-w); b=min(len(raw),i+w+1)
            sm.append(sum(raw[a:b])/(b-a))
        return sm
    return {"leftFootContact": calc("foot_l"), "rightFootContact": calc("foot_r")}

def retarget_clip(root, clip, target_fbx, bone_map_path, offsets_path, out_train, out_overlay, fps, source_scale, emit_every, root_scale, contact_cfg):
    clear_scene(); setup_scene(fps)
    bm = load_json(bone_map_path)
    offs = load_json(offsets_path).get("perBone", {}) if Path(offsets_path).exists() else {}
    target = import_target_fbx(target_fbx)
    source = import_bandai_bvh(clip, source_scale)

    chains = bm["chains"]
    target_to_source = {v["target"]: v["source"] for v in chains.values()}
    source_to_target = {v["source"]: v["target"] for v in chains.values()}
    missing_s = sorted([s for s in source_to_target if not source.pose.bones.get(s)])
    missing_t = sorted([t for t in target_to_source if not target.pose.bones.get(t)])
    if missing_s: log(f"WARNING missing source bones: {missing_s}")
    if missing_t: log(f"WARNING missing target bones: {missing_t}")

    scale = height(target) / max(1.0, height(source))
    start,end = frame_range(source)
    order = ordered_targets(target, list(target_to_source.keys()))

    src_root_name = bm.get("sourceRoot", "joint_Root")
    tgt_root_name = bm.get("targetRoot", "pelvis")
    src_root = source.pose.bones.get(src_root_name)
    tgt_root = target.pose.bones.get(tgt_root_name)
    if not src_root or not tgt_root: raise RuntimeError("Missing source or target root bone")

    bpy.context.scene.frame_set(start); bpy.context.view_layer.update()
    src_root_first = pose_world(source, src_root).translation.copy()
    tgt_root_rest = rest_world(target, tgt_root).translation.copy()

    frames=[]; curves={}
    for fr in range(start, end+1, max(1, int(emit_every))):
        bpy.context.scene.frame_set(fr); bpy.context.view_layer.update()
        world_rot={}; bones={}
        for tgt_name in order:
            src_name = target_to_source[tgt_name]
            spb = source.pose.bones.get(src_name); tpb = target.pose.bones.get(tgt_name)
            if not spb or not tpb: continue

            src_rest_q = rest_world(source, spb).to_quaternion()
            src_pose_q = pose_world(source, spb).to_quaternion()
            tgt_rest_q = rest_world(target, tpb).to_quaternion()

            delta = src_pose_q @ src_rest_q.inverted()
            tgt_world_q = delta @ tgt_rest_q
            tgt_world_q.normalize()

            parent_mapped = mapped_parent_name(target, tgt_name, target_to_source)
            if parent_mapped and parent_mapped in world_rot:
                parent_q = world_rot[parent_mapped]
            else:
                parent = tpb.bone.parent
                parent_q = rest_world(target, target.pose.bones[parent.name]).to_quaternion() if parent else target.matrix_world.to_quaternion()

            local_q = parent_q.inverted() @ tgt_world_q
            local_q = local_q @ offset_q(offs.get(tgt_name, [0,0,0]))
            local_q.normalize()
            world_rot[tgt_name] = tgt_world_q

            local_t = Vector((0,0,0))
            world_t = rest_world(target, tpb).translation.copy()
            if tgt_name == tgt_root_name:
                src_delta = (pose_world(source, src_root).translation - src_root_first) * scale * float(root_scale)
                local_t = src_delta
                world_t = tgt_root_rest + src_delta

            ed = euler_deg(local_q)
            bones[tgt_name] = {"sourceBone": src_name, "rotationQuaternion": qlist(local_q),
                               "rotationEulerXYZDegrees": ed, "localTranslation": vlist(local_t),
                               "worldTranslation": vlist(world_t)}
            for i,axis in enumerate(["x","y","z"]):
                curves.setdefault(f"{tgt_name}.rot_{axis}", []).append({"time": (fr-start)/fps, "value": ed[i]})
            if tgt_name == tgt_root_name:
                for i,axis in enumerate(["x","y","z"]):
                    curves.setdefault(f"{tgt_name}.trans_{axis}", []).append({"time": (fr-start)/fps, "value": local_t[i]})

        frames.append({"frame": fr, "time": (fr-start)/fps, "bones": bones})

    meta = {"version": VERSION, "sourceBvh": str(clip), "targetFbx": str(target_fbx), "fps": fps,
            "frameStart": start, "frameEnd": end, "durationSeconds": (end-start)/fps,
            "heightScale": scale, "missingSourceBones": missing_s, "missingTargetBones": missing_t,
            "retargetMethod": "offline_world_delta_to_target_rest_no_unreal_ik"}

    con = contacts(frames, fps, contact_cfg)
    save_json(out_train, {**meta, "schema": "aegis.offlineRetarget.trainingFrames.v1", "frames": frames, "contactCurves": con})
    save_json(out_overlay, {**meta, "schema": "aegis.offlineRetarget.overlayCurves.v1", "curves": curves, "contactCurves": con})
    return {"training": str(out_train), "overlay": str(out_overlay), "frames": len(frames), "missingSourceBones": missing_s, "missingTargetBones": missing_t}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--target-fbx", required=True)
    parser.add_argument("--bone-map", required=True)
    parser.add_argument("--offsets", required=True)
    parser.add_argument("--output-training-dir", required=True)
    parser.add_argument("--output-overlay-dir", required=True)
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--source-scale", type=float, default=1.0)
    parser.add_argument("--emit-every", type=int, default=1)
    parser.add_argument("--max-clips", type=int, default=1)
    parser.add_argument("--root-translation-scale", type=float, default=1.0)
    parser.add_argument("--contact-height-threshold", type=float, default=4.0)
    parser.add_argument("--contact-velocity-threshold", type=float, default=45.0)
    parser.add_argument("--contact-smooth-window", type=int, default=2)
    argv = sys.argv[sys.argv.index("--")+1:] if "--" in sys.argv else []
    a = parser.parse_args(argv)

    root = Path(a.root)
    manifest = Path(a.manifest)
    clips = load_json(manifest).get("clips", [])
    if a.max_clips > 0: clips = clips[:a.max_clips]
    if not clips: raise RuntimeError(f"No clips in manifest: {manifest}")

    summary = {"version": VERSION, "targetFbx": a.target_fbx, "manifest": str(manifest), "clipCount": len(clips), "results": []}
    contact_cfg = {"heightThresholdCm": a.contact_height_threshold, "velocityThresholdCmPerSec": a.contact_velocity_threshold, "smoothWindowFrames": a.contact_smooth_window}

    for idx, c in enumerate(clips, 1):
        src = Path(c["path"])
        cid = safe_name(c.get("id") or src.stem)
        log(f"[{idx}/{len(clips)}] {cid}")
        result = retarget_clip(root, src, Path(a.target_fbx), root/a.bone_map, root/a.offsets,
                               root/a.output_training_dir/f"{cid}_manny_training.json",
                               root/a.output_overlay_dir/f"{cid}_manny_overlay.json",
                               a.fps, a.source_scale, a.emit_every, a.root_translation_scale, contact_cfg)
        result["clipId"] = cid
        summary["results"].append(result)

    save_json(root/a.output_training_dir/"offline_retarget_summary.json", summary)

if __name__ == "__main__":
    try:
        main()
    except Exception:
        print(f"[Aegis {VERSION}] FATAL")
        print(traceback.format_exc())
        raise
