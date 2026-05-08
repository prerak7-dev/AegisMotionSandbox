from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any, Dict, List


def norm(value: Any) -> str:
    return re.sub(r"[^a-z0-9]+", "_", str(value or "").lower()).strip("_")


def semantic_content(clip: Dict[str, Any]) -> str:
    source_text = norm(" ".join(str(clip.get(k) or "") for k in [
        "content", "semanticContent", "id", "path", "sourceBvh", "sourcePath",
    ]))
    for locomotion in ("dash", "run", "walk"):
        if locomotion in source_text:
            return locomotion
    if "kick" in source_text or "shoot" in source_text:
        return "kick"
    action_text = norm(clip.get("action"))
    if "kick" in action_text:
        return "kick"
    if "dash" in action_text:
        return "dash"
    if "run" in action_text:
        return "run"
    if "walk" in action_text:
        return "walk"
    return "unknown"


def main() -> None:
    ap = argparse.ArgumentParser(description="Verify a built V47 tensor dataset contains semantically valid source clips for the requested action.")
    ap.add_argument("--dataset", required=True)
    ap.add_argument("--action", required=True)
    args = ap.parse_args()

    meta_path = Path(args.dataset) / "metadata.json"
    if not meta_path.exists():
        raise RuntimeError(f"Dataset metadata not found: {meta_path}")
    meta = json.loads(meta_path.read_text(encoding="utf-8"))
    clips: List[Dict[str, Any]] = list(meta.get("clips", []))
    action = str(args.action or "").lower()
    rows = []
    exact = []
    for c in clips:
        semantic = semantic_content(c)
        row = {
            "id": c.get("id"),
            "action": c.get("action"),
            "content": c.get("content"),
            "semanticContent": semantic,
            "source": c.get("sourceBvh") or c.get("sourcePath") or c.get("path"),
        }
        rows.append(row)
        if "kick" in action:
            if semantic == "kick" and ("kick" in str(c.get("action") or "").lower() or c.get("action") == args.action):
                exact.append(row)
        elif c.get("action") == args.action:
            exact.append(row)

    print(f"Dataset clip count: {len(clips)}")
    print("Dataset semantic action summary:")
    summary: Dict[str, int] = {}
    for r in rows:
        summary[r["semanticContent"]] = summary.get(r["semanticContent"], 0) + 1
    for k in sorted(summary):
        print(f"  {k}: {summary[k]}")

    if "kick" in action and not exact:
        preview = json.dumps(rows[:20], indent=2)
        raise RuntimeError(
            "No semantically valid kick clips are present after retarget/tensor build. "
            "The job is stopping before training/generation so it does not create another shuffle/dash overlay. "
            f"Requested action: {args.action}. Clip preview: {preview}"
        )
    print(f"Verified source clips for requested action: {args.action}. Matching clips: {len(exact)}")


if __name__ == "__main__":
    main()
