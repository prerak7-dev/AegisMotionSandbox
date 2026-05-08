# Aegis V46.35 Batch FBX Import
#
# Stable raw-Bandai import path.
#
# This intentionally does NOT use the neutral-skeleton bake path from V46.27-V46.34.
# That branch generated imported assets, but the animation data became frozen or
# incompatible for retargeting. V46.35 returns to the path that preserved motion:
#   first raw Bandai FBX -> source SkeletonSource
#   every raw Bandai FBX -> AnimSequence against that source skeleton

import json
import os
import pathlib
import traceback
import unreal


VERSION = "V46.35"
AEGIS_IMPORT_FPS = 30


def log(msg):
    unreal.log(f"[Aegis {VERSION}] {msg}")


def warn(msg):
    unreal.log_warning(f"[Aegis {VERSION}] {msg}")


def err(msg):
    unreal.log_error(f"[Aegis {VERSION}] {msg}")


def try_quit_editor():
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception as exc:
        warn(f"Could not auto-quit editor: {exc}")


def set_prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as exc:
        warn(f"Could not set {name} on {obj}: {exc}")
        return False


def report_path():
    explicit = os.environ.get("AEGIS_IMPORT_REPORT_PATH")
    if explicit:
        return pathlib.Path(explicit)
    return pathlib.Path(unreal.Paths.project_saved_dir()) / "AegisV46ImportReport.json"


def write_report(report):
    path = report_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    log(f"Wrote import report: {path}")
    return path


def load_config():
    path = os.environ.get("AEGIS_V45_CONFIG")
    if not path:
        raise RuntimeError("AEGIS_V45_CONFIG environment variable is not set.")
    log(f"Loading config: {path}")
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def ensure_dir(path: str):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        log(f"Creating content folder: {path}")
        unreal.EditorAssetLibrary.make_directory(path)


def list_assets(path: str):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        return []
    return list(unreal.EditorAssetLibrary.list_assets(path, recursive=True, include_folder=False))


def load_assets_of_class(path: str, cls):
    assets = []
    for asset_path in list_assets(path):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset and isinstance(asset, cls):
            assets.append(asset)
    return assets


def make_skeletal_mesh_import_options():
    options = unreal.FbxImportUI()
    set_prop(options, "automated_import_should_detect_type", False)
    set_prop(options, "import_mesh", True)
    set_prop(options, "import_as_skeletal", True)
    set_prop(options, "import_animations", False)
    set_prop(options, "import_materials", False)
    set_prop(options, "import_textures", False)
    set_prop(options, "create_physics_asset", False)

    try:
        set_prop(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    except Exception:
        pass

    try:
        set_prop(options.skeletal_mesh_import_data, "import_meshes_in_bone_hierarchy", True)
        set_prop(options.skeletal_mesh_import_data, "convert_scene", True)
        set_prop(options.skeletal_mesh_import_data, "convert_scene_unit", True)
        set_prop(options.skeletal_mesh_import_data, "preserve_smoothing_groups", True)
    except Exception:
        pass

    return options


def make_animation_import_options(source_skeleton):
    options = unreal.FbxImportUI()
    set_prop(options, "automated_import_should_detect_type", False)
    set_prop(options, "import_mesh", False)
    set_prop(options, "import_as_skeletal", False)
    set_prop(options, "import_animations", True)
    set_prop(options, "import_materials", False)
    set_prop(options, "import_textures", False)
    set_prop(options, "skeleton", source_skeleton)

    try:
        set_prop(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    except Exception:
        pass

    try:
        data = options.anim_sequence_import_data
        set_prop(data, "use_default_sample_rate", True)
        set_prop(data, "custom_sample_rate", AEGIS_IMPORT_FPS)
        set_prop(data, "snap_to_closest_frame_boundary", True)
        try:
            set_prop(data, "animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        except Exception:
            pass
        set_prop(data, "import_custom_attribute", True)
        set_prop(data, "remove_redundant_keys", False)
        set_prop(data, "convert_scene", True)
        set_prop(data, "convert_scene_unit", True)
    except Exception as exc:
        warn(f"Could not set AnimSequence import data: {exc}")

    return options


def import_task(filename: str, destination: str, destination_name: str, options):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = destination_name
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = options
    return task


def safe_name(path: pathlib.Path):
    stem = path.stem
    keep = []
    for c in stem:
        keep.append(c if c.isalnum() or c in "_-" else "_")
    return "".join(keep)


def choose_source_fbx(files):
    # Prefer a dash/run clip as the source skeleton because it was the closest working
    # path earlier, and it preserves the raw Bandai hierarchy exactly.
    for keyword in ["dash_active", "dash_normal", "run_active", "walk_normal"]:
        for f in files:
            if keyword in f.name:
                return f
    return files[0]


def import_source_skeleton(source_fbx: pathlib.Path, skeleton_dir: str):
    ensure_dir(skeleton_dir)

    dest_name = "SKEL_SRC_" + safe_name(source_fbx)
    task = import_task(str(source_fbx), skeleton_dir, dest_name, make_skeletal_mesh_import_options())
    log(f"Importing raw source skeleton FBX: {source_fbx}")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.EditorAssetLibrary.save_directory(skeleton_dir, only_if_is_dirty=False, recursive=True)

    skeletons = load_assets_of_class(skeleton_dir, unreal.Skeleton)
    if not skeletons:
        all_after = list_assets(skeleton_dir)
        raise RuntimeError(
            "Could not create source Skeleton from raw source FBX. "
            f"Folder={skeleton_dir}. Assets after import={all_after}."
        )

    preferred = None
    wanted = safe_name(source_fbx)
    for skel in skeletons:
        if wanted in skel.get_name():
            preferred = skel
            break

    source_skeleton = preferred or skeletons[0]
    log(f"Using raw source Skeleton: {source_skeleton.get_path_name()}")
    return source_skeleton


def import_animations(fbx_files, animation_dir: str, source_skeleton):
    ensure_dir(animation_dir)

    report_tasks = []
    imported_paths = []

    for index, f in enumerate(fbx_files):
        dest_name = safe_name(f)
        clip_dir = animation_dir + "/" + dest_name
        ensure_dir(clip_dir)

        before = set(list_assets(clip_dir))
        task = import_task(str(f), clip_dir, dest_name, make_animation_import_options(source_skeleton))
        log(f"Importing animation {index + 1}/{len(fbx_files)}: {f.name} -> {clip_dir}")
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

        after = set(list_assets(clip_dir))
        delta = sorted(after - before)
        task_paths = list(task.imported_object_paths) or delta

        if not task_paths:
            possible = [p for p in after if dest_name.lower() in p.lower() or "anim" in p.lower()]
            task_paths = sorted(possible)

        anim_paths = []
        for p in task_paths:
            asset = unreal.EditorAssetLibrary.load_asset(p)
            if asset and isinstance(asset, unreal.AnimSequence):
                anim_paths.append(p)

        if not anim_paths:
            for p in list_assets(clip_dir):
                asset = unreal.EditorAssetLibrary.load_asset(p)
                if asset and isinstance(asset, unreal.AnimSequence):
                    anim_paths.append(p)

        if anim_paths:
            imported_paths.extend(anim_paths)
        else:
            warn(f"No AnimSequence reported for {f}. Raw task paths={task_paths}. Clip assets={list_assets(clip_dir)}")

        report_tasks.append({
            "filename": str(f),
            "destination": clip_dir,
            "destinationName": dest_name,
            "imported": task_paths,
            "animSequences": anim_paths,
        })

    unreal.EditorAssetLibrary.save_directory(animation_dir, only_if_is_dirty=False, recursive=True)
    return report_tasks, imported_paths


def main():
    report = {
        "version": VERSION,
        "phase": "boot",
        "mode": "raw-bandai-source-skeleton",
        "errors": [],
        "env": {
            "AEGIS_V45_CONFIG": os.environ.get("AEGIS_V45_CONFIG"),
            "AEGIS_IMPORT_REPORT_PATH": os.environ.get("AEGIS_IMPORT_REPORT_PATH"),
        }
    }

    write_report(report)

    try:
        cfg = load_config()
        fbx_dir = pathlib.Path(cfg["bandai"]["fbxOutputDir"])
        base_destination = cfg["unreal"]["importPath"]
        skeleton_dir = base_destination + "/SkeletonSource"
        animation_dir = base_destination + "/Animations"

        if not fbx_dir.exists():
            raise RuntimeError(f"FBX directory does not exist: {fbx_dir}")

        files = sorted([p for p in fbx_dir.rglob("*.fbx") if "NeutralSkeletonSource" not in p.name])
        if not files:
            raise RuntimeError(f"No raw animation FBX files found in {fbx_dir}.")

        source_fbx = choose_source_fbx(files)

        report.update({
            "phase": "importing",
            "fbxDir": str(fbx_dir),
            "sourceFbx": str(source_fbx),
            "destination": base_destination,
            "skeletonDir": skeleton_dir,
            "animationDir": animation_dir,
            "fbxCount": len(files),
            "animationFbxCount": len(files),
            "forcedImportFps": AEGIS_IMPORT_FPS,
            "tasks": [],
            "importedObjectPaths": [],
            "importedAnimSequencePaths": [],
        })
        write_report(report)

        ensure_dir(base_destination)

        source_skeleton = import_source_skeleton(source_fbx, skeleton_dir)
        report["sourceSkeleton"] = source_skeleton.get_path_name()
        write_report(report)

        tasks, anim_paths = import_animations(files, animation_dir, source_skeleton)
        report["phase"] = "complete"
        report["tasks"] = tasks
        report["importedObjectPaths"] = anim_paths
        report["importedAnimSequencePaths"] = anim_paths
        report["animationAssetRegistryAfter"] = list_assets(animation_dir)

        final_report = write_report(report)

        if len(anim_paths) == 0:
            raise RuntimeError(
                "Raw Bandai FBX import completed but imported 0 AnimSequences. "
                f"Check report: {final_report}."
            )

        log(f"Imported/located {len(anim_paths)} AnimSequence assets.")
        log("Retarget using the raw source IK Rig created from this SkeletonSource.")
        try_quit_editor()

    except Exception as exc:
        report.setdefault("errors", []).append({
            "type": type(exc).__name__,
            "message": str(exc),
            "traceback": traceback.format_exc(),
        })
        report["phase"] = "failed"
        write_report(report)
        err(f"Import failed: {exc}")
        try_quit_editor()
        raise


main()
