from __future__ import annotations

import argparse
import json
from pathlib import Path

from .exporter import export_aegis_overlay_json
from .tensorize import aegis_json_to_tensor, rebase_tensor_to_additive_first_frame


def main() -> None:
    p = argparse.ArgumentParser(description="Repair a legacy V47 overlay JSON into V47.4 stabilized additive scalar curves. Stabilization only; not semantic source correction.")
    p.add_argument("--input", required=True, help="Existing overlay JSON to repair.")
    p.add_argument("--out", required=True, help="Output repaired overlay JSON.")
    p.add_argument("--duration", type=float, default=1.35)
    p.add_argument("--fps", type=int, default=60)
    p.add_argument("--action", default="soccer_kick_overlay")
    p.add_argument("--style", default="active")
    p.add_argument("--dominant-leg", default="right")
    p.add_argument("--include-quaternion-reference", action="store_true")
    args = p.parse_args()

    tensor, meta = aegis_json_to_tensor(args.input, fps=args.fps, max_frames=max(2, int(round(args.duration * args.fps)) + 1))
    tensor = rebase_tensor_to_additive_first_frame(tensor)
    condition = {
        "id": "aegis-v47-3-repaired-overlay",
        "name": "Aegis V47.4 Repaired Neural Overlay",
        "action": args.action,
        "style": args.style,
        "dominantLeg": args.dominant_leg,
        "durationSeconds": args.duration,
        "fps": args.fps,
        "skeletonProfile": "UE5_Mannequin",
        "includeQuaternionReference": args.include_quaternion_reference,
    }
    doc = export_aegis_overlay_json(tensor, condition, {"generationMode": "v47_4_repair_existing_overlay_stabilization_only", "sourceKind": "repair_only_not_semantic", **meta})
    doc.setdefault("qualityReport", {})["semanticWarning"] = (
        "This repair only stabilizes scalar rotations. It does not make a dash/run/walk source into a kick. "
        "Use it only for importer/runtime debugging, not as a final soccer kick overlay."
    )
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(f"Wrote repaired overlay: {out}")
    print(f"Curve count: {len(doc.get('curves', []))}")
    print(f"Max abs rotation degrees: {doc.get('qualityReport', {}).get('maxAbsRotationDegrees')}")


if __name__ == "__main__":
    main()
