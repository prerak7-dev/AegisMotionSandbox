# Aegis V46.20 Batch FBX Import
#
# Two-pass import:
# 1. Import one FBX as a source SkeletalMesh/Skeleton into Imported/SkeletonSource.
# 2. Import every FBX as an AnimSequence against that source Skeleton into Imported/Animations.
#
# Writes an early boot report immediately so the PowerShell launcher can tell whether
# Unreal executed this script at all.

import json
import os
import pathlib
import traceback
import unreal


VERSION = "V46.20"


def log(msg):
    unreal.log(f"[Aegis {VERSION}] {msg}")


def warn(msg):
    unreal.log_warning(f"[Aegis {VERSION}] {msg}")


def err(msg):
    unreal.log_error(f"[Aegis {VERSION}] {msg}")


def set_prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception:
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
        set_prop(options.anim_sequence_import_data, "import_custom_attribute", True)
        set_prop(options.anim_sequence_import_data, "remove_redundant_keys", False)
        set_prop(options.anim_sequence_import_data, "convert_scene", True)
        set_prop(options.anim_sequence_import_data, "convert_scene_unit", True)
    except Exception:
        pass

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


def import_source_skeleton(first_fbx: pathlib.Path, skeleton_dir: str):
    ensure_dir(skeleton_dir)

    existing_skeletons = load_assets_of_class(skeleton_dir, unreal.Skeleton)
    if existing_skeletons:
        log(f"Using existing source Skeleton: {existing_skeletons[0].get_path_name()}")
        return existing_skeletons[0]

    dest_name = "SKEL_SRC_" + safe_name(first_fbx)
    task = import_task(str(first_fbx), skeleton_dir, dest_name, make_skeletal_mesh_import_options())
    log(f"Importing source skeleton FBX: {first_fbx}")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.EditorAssetLibrary.save_directory(skeleton_dir, only_if_is_dirty=False, recursive=True)

    skeletons = load_assets_of_class(skeleton_dir, unreal.Skeleton)
    if not skeletons:
        all_after = list_assets(skeleton_dir)
        raise RuntimeError(
            "Could not create source Skeleton from first FBX. "
            f"Folder={skeleton_dir}. Assets after import={all_after}. "
            "The FBX may not contain an Unreal-compatible skeletal mesh."
        )

    log(f"Created source Skeleton: {skeletons[0].get_path_name()}")
    return skeletons[0]


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

        if index % 5 == 0:
            # Write partial progress report every few clips.
            pass

    unreal.EditorAssetLibrary.save_directory(animation_dir, only_if_is_dirty=False, recursive=True)
    return report_tasks, imported_paths


def main():
    report = {
        "version": VERSION,
        "phase": "boot",
        "errors": [],
        "env": {
            "AEGIS_V45_CONFIG": os.environ.get("AEGIS_V45_CONFIG"),
            "AEGIS_IMPORT_REPORT_PATH": os.environ.get("AEGIS_IMPORT_REPORT_PATH"),
        }
    }

    # Boot report proves the script actually executed.
    write_report(report)

    try:
        cfg = load_config()
        fbx_dir = pathlib.Path(cfg["bandai"]["fbxOutputDir"])
        base_destination = cfg["unreal"]["importPath"]
        skeleton_dir = base_destination + "/SkeletonSource"
        animation_dir = base_destination + "/Animations"

        if not fbx_dir.exists():
            raise RuntimeError(f"FBX directory does not exist: {fbx_dir}")

        files = sorted(fbx_dir.rglob("*.fbx"))
        if not files:
            raise RuntimeError(f"No FBX files found in {fbx_dir}")

        report.update({
            "phase": "importing",
            "fbxDir": str(fbx_dir),
            "destination": base_destination,
            "skeletonDir": skeleton_dir,
            "animationDir": animation_dir,
            "fbxCount": len(files),
            "tasks": [],
            "importedObjectPaths": [],
            "importedAnimSequencePaths": [],
        })
        write_report(report)

        ensure_dir(base_destination)

        source_skeleton = import_source_skeleton(files[0], skeleton_dir)
        report["sourceSkeleton"] = source_skeleton.get_path_name()
        write_report(report)

        tasks, anim_paths = import_animations(files, animation_dir, source_skeleton)
        report["phase"] = "complete"
        report["tasks"] = tasks
        report["importedObjectPaths"] = anim_paths
        report["importedAnimSequencePaths"] = anim_paths
        report["animationAssetRegistryAfter"] = list_assets(animation_dir)

        report_path_final = write_report(report)

        if len(anim_paths) == 0:
            raise RuntimeError(
                "FBX import completed but imported 0 AnimSequences. "
                f"Check report: {report_path_final}. "
                "The source Skeleton may not match the FBX animation hierarchy or Unreal rejected the animation-only import."
            )

        log(f"Imported/located {len(anim_paths)} AnimSequence assets.")
        log(f"Retarget these AnimSequences from {animation_dir} to Quinn/Manny.")

    except Exception as exc:
        report.setdefault("errors", []).append({
            "type": type(exc).__name__,
            "message": str(exc),
            "traceback": traceback.format_exc(),
        })
        report["phase"] = "failed"
        write_report(report)
        err(f"Import failed: {exc}")
        raise


main()
