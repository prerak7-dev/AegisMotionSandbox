from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Dict, List

def norm(s: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", s.lower()).strip("_")

def infer_labels(path: Path, content_filters: List[str], style_filters: List[str]) -> Dict[str, str]:
    text = norm(" ".join(path.parts[-8:]) + " " + path.stem)
    content = "unknown"
    style = "unknown"

    for c in sorted(content_filters, key=len, reverse=True):
        if norm(c) in text:
            content = c
            break

    for st in sorted(style_filters, key=len, reverse=True):
        if norm(st) in text:
            style = st
            break

    # Bandai filenames are usually dataset-X_content_style_id.bvh.
    # If filters miss, infer from filename parts.
    stem = path.stem
    parts = stem.split("_")
    if content == "unknown" and len(parts) >= 2:
        maybe = "_".join(parts[1:-2]) if len(parts) > 3 else parts[1]
        maybe = maybe.replace("_", "-")
        if maybe:
            content = maybe
    if style == "unknown" and len(parts) >= 3:
        style = parts[-2]

    return {"content": content, "style": style}

def find_bvh_files(extract_root: Path, repo_root: Path, dataset_folders: List[str]) -> List[Path]:
    roots: List[Path] = []

    # Current Bandai repository layout: dataset/<dataset-name>/data/*.bvh.
    for dataset in dataset_folders:
        data_dir = repo_root / "dataset" / dataset / "data"
        if data_dir.exists():
            roots.append(data_dir)

    # Fallback discovery.
    dataset_root = repo_root / "dataset"
    if dataset_root.exists():
        roots.extend([p for p in dataset_root.rglob("data") if p.is_dir()])

    # Older/extracted layout fallback.
    if extract_root.exists():
        roots.append(extract_root)

    seen = set()
    files: List[Path] = []
    for r in roots:
        for p in list(r.rglob("*.bvh")) + list(r.rglob("*.BVH")):
            key = str(p.resolve()).lower()
            if key not in seen:
                seen.add(key)
                files.append(p)

    return sorted(files)

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", required=True)
    ap.add_argument("--root", required=True)
    args = ap.parse_args()

    root = Path(args.root)
    config = json.loads(Path(args.config).read_text(encoding="utf-8"))

    extract_root = Path(config["bandai"]["rawExtractDir"])
    repo_root = Path(config["bandai"]["repoDir"])
    dataset_folders = config["bandai"].get("datasetFolders", [])
    out_manifest = root / config["bandai"]["selectedClipManifest"]
    content_filters = config["bandai"].get("contentFilters", [])
    style_filters = config["bandai"].get("styleFilters", [])
    max_per = int(config["bandai"].get("maxClipsPerContentStyle", 8))

    bvh_files = find_bvh_files(extract_root, repo_root, dataset_folders)

    print(f"BVH files discovered: {len(bvh_files)}")
    print(f"Content filters: {content_filters}")
    print(f"Style filters: {style_filters}")

    if not bvh_files:
        raise RuntimeError(
            f"No BVH files found in Bandai data folders under {repo_root}. "
            "Expected dataset/<Bandai...>/data/*.bvh."
        )

    buckets: Dict[str, List[Path]] = defaultdict(list)
    all_inferred = defaultdict(int)

    for p in bvh_files:
        labels = infer_labels(p, content_filters, style_filters)
        content = labels["content"]
        style = labels["style"]
        all_inferred[f"{content}__{style}"] += 1

        if content_filters:
            wanted_content = {norm(c) for c in content_filters}
            if norm(content) not in wanted_content:
                continue

        # Keep unknown/normal style for useful content. Dataset 1 action clips such as kick may be normal only.
        if style_filters and style != "unknown":
            wanted_style = {norm(s) for s in style_filters}
            if norm(style) not in wanted_style:
                continue

        key = f"{content}__{style}"
        if len(buckets[key]) < max_per:
            buckets[key].append(p)

    clips = []
    for key, files in sorted(buckets.items()):
        content, style = key.split("__", 1)
        for p in files:
            clips.append({
                "id": f"bandai_{norm(content)}_{norm(style)}_{p.stem}",
                "path": str(p),
                "content": content,
                "style": style,
                "action": "soccer_kick_overlay" if "kick" in norm(content) else content,
                "dominantLeg": config["metadata"].get("dominantLeg", "unknown"),
                "license": config["metadata"].get("license", "CC_BY_NC_4_0"),
                "format": "BVH"
            })

    out_manifest.parent.mkdir(parents=True, exist_ok=True)
    out_manifest.write_text(json.dumps({
        "source": "Bandai-Namco-Research-Motiondataset",
        "clipCount": len(clips),
        "clips": clips,
        "diagnostics": {
            "bvhFilesDiscovered": len(bvh_files),
            "inferredBucketsPreview": dict(list(sorted(all_inferred.items()))[:80])
        }
    }, indent=2), encoding="utf-8")
    print(f"Wrote {out_manifest} with {len(clips)} clips")

    if len(clips) == 0:
        print("No clips matched the current filters. Inferred bucket preview:")
        for key, count in list(sorted(all_inferred.items()))[:80]:
            print(f"  {key}: {count}")
        raise RuntimeError(
            "No clips matched the current content/style filters. "
            "Broaden config.bandai.contentFilters/styleFilters based on the printed inferred bucket names."
        )

if __name__ == "__main__":
    main()
