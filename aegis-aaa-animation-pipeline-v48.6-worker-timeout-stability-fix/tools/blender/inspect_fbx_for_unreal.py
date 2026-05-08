from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
import bpy

def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()

def user_args():
    argv = list(sys.argv)
    if "--" in argv:
        return argv[argv.index("--") + 1:]
    return []

def inspect_fbx(path: Path):
    clear_scene()
    bpy.ops.import_scene.fbx(filepath=str(path))
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    armatures = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
    actions = list(bpy.data.actions)

    print(f"[Aegis FBX Inspect] {path}")
    print(f"  mesh_count={len(meshes)}")
    print(f"  armature_count={len(armatures)}")
    print(f"  action_count={len(actions)}")
    print(f"  mesh_names={[o.name for o in meshes[:10]]}")
    print(f"  armature_names={[o.name for o in armatures[:10]]}")
    print(f"  action_names={[a.name for a in actions[:10]]}")

    if len(meshes) == 0 or len(armatures) == 0:
        raise SystemExit("FBX is not Unreal-import-ready: expected at least one mesh and one armature.")
    if len(actions) == 0:
        raise SystemExit("FBX has no animation action/take.")
    if not any(path.stem.lower().replace('-', '_') in a.name.lower() for a in actions):
        print("[Aegis FBX Inspect] WARNING: action name does not appear to include the unique FBX filename stem.")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fbx", default=os.environ.get("AEGIS_FBX_INSPECT_PATH"))
    args = parser.parse_args(user_args())
    if not args.fbx:
        raise SystemExit("Missing --fbx or AEGIS_FBX_INSPECT_PATH")
    inspect_fbx(Path(args.fbx))

if __name__ == "__main__":
    main()
