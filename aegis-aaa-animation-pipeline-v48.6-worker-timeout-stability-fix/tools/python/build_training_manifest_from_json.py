from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, Iterable, List


def _load_json(path: Path) -> Dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _rel_to_sample_data(root: Path, p: Path) -> str:
    p = p.resolve()
    sample = (root / "sample-data").resolve()
    try:
        return p.relative_to(sample).as_posix()
    except ValueError:
        return p.as_posix()


def _iter_training_jsons(json_dirs: Iterable[Path]) -> Iterable[Path]:
    seen = set()
    for d in json_dirs:
        if not d.exists():
            continue
        for p in sorted(d.glob("**/*.json")):
            if p.name.lower().endswith("summary.json"):
                continue
            rp = p.resolve()
            if rp in seen:
                continue
            seen.add(rp)
            yield p



def _infer_content_from_name(*values: str) -> str:
    hay = " ".join(str(v or "") for v in values).lower().replace("-", "_")
    if "kick" in hay:
        return "kick"
    if "dash" in hay:
        return "dash"
    if "run" in hay:
        return "run"
    if "walk_turn_left" in hay or "walk_left" in hay:
        return "walk_turn_left"
    if "walk_turn_right" in hay or "walk_right" in hay:
        return "walk_turn_right"
    if "walk" in hay:
        return "walk"
    return "unknown"

def _action_from_content(content: str, default_action: str) -> str:
    if content == "kick":
        # The Aegis runtime action that consumes a kick overlay.
        return "soccer_kick_overlay"
    if content in {"dash", "run", "walk", "walk_turn_left", "walk_turn_right"}:
        return content
    return default_action

def _infer_style_from_name(*values: str, default_style: str = "active") -> str:
    hay = " ".join(str(v or "") for v in values).lower().replace("-", "_")
    for style in ["active", "normal", "masculine", "youthful", "feminine", "elderly", "unknown"]:
        if f"_{style}_" in f"_{hay}_" or hay.endswith(f"_{style}"):
            return style
    return default_style

def _infer_dominant_leg(content: str, default_leg: str) -> str:
    # Bandai file names in this dataset do not consistently encode the kick leg.
    # Keep the config default, but preserve this hook for future source-specific tags.
    return default_leg

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--config", required=True)
    ap.add_argument("--offline-config", default="config/offline_retarget_v46.config.json")
    args = ap.parse_args()

    root = Path(args.root).resolve()
    cfg_path = Path(args.config).resolve()
    cfg = _load_json(cfg_path)
    offline_cfg_path = root / args.offline_config
    offline = _load_json(offline_cfg_path) if offline_cfg_path.exists() else {}

    json_dirs: List[Path] = []
    if offline.get("outputTrainingJsonDir"):
        json_dirs.append(root / offline["outputTrainingJsonDir"])
    if cfg.get("unreal", {}).get("trainingJsonOutput"):
        json_dirs.append(root / cfg["unreal"]["trainingJsonOutput"])

    out_path = root / cfg["training"]["manifestPath"]
    clips = []
    for p in _iter_training_jsons(json_dirs):
        try:
            doc = _load_json(p)
        except Exception as exc:
            print(f"Skipping unreadable JSON {p}: {exc}")
            continue

        schema = doc.get("schema", "")
        if not (doc.get("frames") or doc.get("curves")):
            print(f"Skipping non-motion JSON {p}")
            continue

        gen = doc.get("generationParameters", {}) or {}
        source_bvh = doc.get("sourceBvh")
        frame_count = len(doc.get("frames", [])) if isinstance(doc.get("frames"), list) else int(doc.get("frameCount", 0) or 0)
        quality = "offline_retargeted_bvh_to_manny" if schema.startswith("aegis.offlineRetarget") else gen.get("quality", "retargeted_animsequence_sampled")
        default_action = cfg["metadata"].get("defaultAction", "soccer_kick_overlay")
        default_style = cfg["metadata"].get("defaultStyle", "active")
        default_leg = cfg["metadata"].get("dominantLeg", "right")
        content = gen.get("content") or _infer_content_from_name(p.stem, source_bvh, doc.get("id", ""))
        inferred_action = gen.get("action") or _action_from_content(content, default_action)
        inferred_style = gen.get("style") or _infer_style_from_name(p.stem, source_bvh, doc.get("id", ""), default_style=default_style)
        inferred_leg = gen.get("dominantLeg") or _infer_dominant_leg(content, default_leg)

        clips.append({
            "id": doc.get("id", p.stem),
            "path": _rel_to_sample_data(root, p),
            "action": inferred_action,
            "content": content,
            "style": inferred_style,
            "dominantLeg": inferred_leg,
            "quality": quality,
            "license": gen.get("license", cfg["metadata"].get("license", "UNKNOWN")),
            "sourceFormat": doc.get("sourceFormat", schema or "unknown"),
            "sourceBvh": source_bvh,
            "durationSeconds": doc.get("durationSeconds"),
            "frameCount": frame_count,
            "notes": "Offline-retargeted Manny/Quinn JSON for V47 neural overlay training" if schema.startswith("aegis.offlineRetarget") else "Exported by AegisMotion commandlet",
        })

    if not clips:
        searched = ", ".join(str(d) for d in json_dirs)
        raise RuntimeError(f"No training motion JSON files found. Searched: {searched}")

    manifest = {
        "datasetName": "aegis_bandai_v47_neural_overlay_training",
        "version": "V47",
        "skeletonProfile": cfg["metadata"].get("skeletonProfile", "UE5_Mannequin"),
        "sourceDirs": [str(d) for d in json_dirs],
        "clips": clips,
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Wrote {out_path} with {len(clips)} clips")
    for c in clips[:12]:
        print(f"  - {c['id']} :: {c['quality']} :: {c['path']}")
    if len(clips) > 12:
        print(f"  ... {len(clips) - 12} more")


if __name__ == "__main__":
    main()
