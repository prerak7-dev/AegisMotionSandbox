from __future__ import annotations

import fnmatch
import json
import shutil
import urllib.parse
import urllib.request
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional

from .source_registry import SOURCES

@dataclass
class FetchResult:
    source: str
    files: List[str]
    manifest_path: str
    notes: List[str]

def _safe_name(value: str) -> str:
    value = value.replace("\\", "/").split("/")[-1]
    value = "".join(c if c.isalnum() or c in "._-" else "_" for c in value)
    return value or "downloaded_asset"

def _download(url: str, out_dir: Path) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    parsed = urllib.parse.urlparse(url)
    name = _safe_name(Path(parsed.path).name or "download")
    path = out_dir / name
    with urllib.request.urlopen(url) as response:
        path.write_bytes(response.read())
    return path

def _extract_zip(path: Path, out_dir: Path) -> List[Path]:
    extract_dir = out_dir / (path.stem + "_extracted")
    extract_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "r") as z:
        z.extractall(extract_dir)
    return [p for p in extract_dir.rglob("*") if p.is_file()]

def _glob_files(files: List[Path], pattern: str | None) -> List[Path]:
    if not pattern:
        return files
    pattern = pattern.replace("\\", "/")
    return [p for p in files if fnmatch.fnmatch(str(p).replace("\\", "/"), pattern) or fnmatch.fnmatch(p.name, pattern)]

def _format_from_path(path: Path, fallback: str) -> str:
    ext = path.suffix.lower().lstrip(".")
    return ext.upper() if ext else fallback

def _append_manifest(manifest_path: Path, clips: List[Dict[str, Any]]) -> None:
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    else:
        manifest = {
            "datasetName": "aegis_v38_harvested_motion_data",
            "skeletonProfile": "SOURCE_RAW_NOT_YET_RETARGETED",
            "clips": []
        }
    existing = {c.get("path") for c in manifest.get("clips", [])}
    for clip in clips:
        if clip["path"] not in existing:
            manifest["clips"].append(clip)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

def _make_clip(path: Path, root: Path, source: str, request: Dict[str, Any], source_info: Dict[str, Any]) -> Dict[str, Any]:
    rel = path.relative_to(root).as_posix() if path.is_relative_to(root) else path.as_posix()
    return {
        "id": f"{source}_{path.stem}",
        "path": rel,
        "source": source,
        "format": _format_from_path(path, source_info.get("defaultFormat", "UNKNOWN")),
        "action": request.get("action", "unknown"),
        "style": request.get("style", "unknown"),
        "dominantLeg": request.get("dominantLeg", "unknown"),
        "licenseSummary": source_info.get("licenseSummary"),
        "trainingAllowed": source_info.get("trainingAllowed"),
        "riskLevel": source_info.get("riskLevel"),
        "retargetStatus": "raw_needs_retarget_to_UE5_Mannequin",
        "notes": "Raw fetched asset. Retarget/export to Aegis JSON before using aegis_motion_prior.dataset.",
    }

def fetch_source(request: Dict[str, Any], root_dir: str | Path = ".") -> FetchResult:
    root_dir = Path(root_dir).resolve()
    source = request.get("source")
    if source not in SOURCES:
        raise ValueError(f"Unknown source '{source}'.")

    source_info = SOURCES[source]
    if source_info.get("trainingAllowed") is False:
        raise ValueError(f"{source_info['label']} is marked as not allowed for ML training. Refusing to fetch into training data.")

    raw_root = root_dir / "sample-data" / "raw" / source
    raw_root.mkdir(parents=True, exist_ok=True)

    files: List[Path] = []
    notes: List[str] = []

    if source == "cmu_original":
        subject = request.get("subject", "").strip()
        trial = request.get("trial", "").strip()
        asf_url = request.get("asfUrl") or (f"https://mocap.cs.cmu.edu/subjects/{subject}/{subject}.asf" if subject else "")
        amc_url = request.get("amcUrl") or (f"https://mocap.cs.cmu.edu/subjects/{subject}/{subject}_{trial}.amc" if subject and trial else "")
        if not asf_url or not amc_url:
            raise ValueError("CMU original requires ASF URL and AMC URL, or subject + trial.")
        files.append(_download(asf_url, raw_root))
        files.append(_download(amc_url, raw_root))
        notes.append("Downloaded CMU ASF/AMC pair. Next step: ASF+AMC import/retarget to Manny/Quinn and export Aegis JSON.")

    elif source == "cmu_fbx_hf":
        repo_id = request.get("repoId", "gbionics/cmu-fbx")
        allow_patterns = request.get("allowPatterns", "**/*.fbx")
        try:
            from huggingface_hub import snapshot_download
        except Exception as exc:
            raise RuntimeError("Install huggingface_hub first: pip install huggingface_hub") from exc
        local = snapshot_download(
            repo_id=repo_id,
            repo_type="dataset",
            allow_patterns=allow_patterns,
            local_dir=str(raw_root / repo_id.replace("/", "__")),
            local_dir_use_symlinks=False,
        )
        files = [p for p in Path(local).rglob("*") if p.is_file()]
        notes.append("Downloaded from Hugging Face. Retarget FBX to UE5 Manny/Quinn before tensorization.")

    elif source in ("accad", "style100", "manual"):
        url = request.get("downloadUrl")
        local_path = request.get("localPath")
        if url:
            downloaded = _download(url, raw_root)
            if downloaded.suffix.lower() == ".zip":
                files = _extract_zip(downloaded, raw_root)
            else:
                files = [downloaded]
        elif local_path:
            p = Path(local_path)
            if not p.exists():
                raise FileNotFoundError(local_path)
            target = raw_root / p.name
            shutil.copy2(p, target)
            files = [target]
        else:
            raise ValueError("Provide downloadUrl or localPath.")
        notes.append("Fetched raw asset. Retarget/export to Aegis JSON before training.")

    elif source in ("lafan1", "bandai_namco"):
        url = request.get("githubZipUrl")
        if not url:
            raise ValueError("Provide githubZipUrl.")
        zip_path = _download(url, raw_root)
        files = _extract_zip(zip_path, raw_root)
        subset = request.get("subsetPath")
        if subset:
            files = _glob_files(files, subset)
        notes.append("Fetched GitHub ZIP source. Check license restrictions before training/output use.")

    else:
        raise ValueError(f"Fetcher not implemented for {source}.")

    # Filter to animation-ish files
    wanted_exts = {".asf", ".amc", ".bvh", ".fbx", ".json", ".c3d"}
    files = [p for p in files if p.suffix.lower() in wanted_exts]

    clips = [_make_clip(p, root_dir, source, request, source_info) for p in files]
    manifest_path = root_dir / "sample-data" / "manifests" / "harvested_raw_manifest.json"
    _append_manifest(manifest_path, clips)

    return FetchResult(
        source=source,
        files=[str(p.relative_to(root_dir)) if p.is_relative_to(root_dir) else str(p) for p in files],
        manifest_path=str(manifest_path.relative_to(root_dir)),
        notes=notes,
    )
