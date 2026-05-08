from __future__ import annotations

from typing import Any, Dict, List

# Registry is intentionally explicit about licensing/risk.
# The UI reads this to show only the fields needed for each source.

SOURCES: Dict[str, Dict[str, Any]] = {
    "cmu_original": {
        "label": "CMU Graphics Lab Mocap — original ASF/AMC",
        "recommendedUse": "Best first choice for permissive training/prototyping.",
        "licenseSummary": "CMU states the motion capture data may be copied, modified, or redistributed without permission; commercial products may include the data, but you should not resell the data directly.",
        "riskLevel": "low",
        "trainingAllowed": True,
        "defaultFormat": "ASF_AMC",
        "fields": [
            {"name": "subject", "label": "Subject number", "type": "text", "placeholder": "10"},
            {"name": "trial", "label": "Trial number", "type": "text", "placeholder": "03"},
            {"name": "asfUrl", "label": "ASF URL", "type": "url", "placeholder": "https://mocap.cs.cmu.edu/subjects/10/10.asf"},
            {"name": "amcUrl", "label": "AMC URL", "type": "url", "placeholder": "https://mocap.cs.cmu.edu/subjects/10/10_03.amc"},
            {"name": "action", "label": "Action label", "type": "text", "placeholder": "soccer_kick"},
            {"name": "style", "label": "Style", "type": "text", "placeholder": "athletic"},
            {"name": "dominantLeg", "label": "Dominant leg", "type": "select", "options": ["right", "left", "unknown"]},
        ],
        "docsUrl": "https://mocap.cs.cmu.edu/",
    },
    "cmu_fbx_hf": {
        "label": "CMU FBX converted dataset — Hugging Face gbionics/cmu-fbx",
        "recommendedUse": "Good if you want FBX files instead of ASF/AMC. Still retarget to Manny/Quinn before tensorizing.",
        "licenseSummary": "Derived from CMU mocap; dataset page repeats CMU terms and attribution. Verify before redistribution.",
        "riskLevel": "low_medium",
        "trainingAllowed": True,
        "defaultFormat": "FBX",
        "fields": [
            {"name": "repoId", "label": "Hugging Face dataset repo", "type": "text", "placeholder": "gbionics/cmu-fbx"},
            {"name": "allowPatterns", "label": "Files/glob to download", "type": "text", "placeholder": "**/10_03*.fbx"},
            {"name": "action", "label": "Action label", "type": "text", "placeholder": "soccer_kick"},
            {"name": "style", "label": "Style", "type": "text", "placeholder": "athletic"},
            {"name": "dominantLeg", "label": "Dominant leg", "type": "select", "options": ["right", "left", "unknown"]},
        ],
        "docsUrl": "https://huggingface.co/datasets/gbionics/cmu-fbx",
    },
    "accad": {
        "label": "ACCAD / Ohio State Open Motion Project",
        "recommendedUse": "Useful for martial arts kicks, walks, runs. Download source packages from direct URLs.",
        "licenseSummary": "ACCAD page says Open Motion Project is CC BY 3.0 Unported.",
        "riskLevel": "low_medium",
        "trainingAllowed": True,
        "defaultFormat": "BVH_OR_C3D",
        "fields": [
            {"name": "downloadUrl", "label": "Direct ACCAD download URL", "type": "url", "placeholder": "Paste direct .zip/.bvh/.c3d link from ACCAD"},
            {"name": "action", "label": "Action label", "type": "text", "placeholder": "kick"},
            {"name": "style", "label": "Style", "type": "text", "placeholder": "martial_arts"},
            {"name": "dominantLeg", "label": "Dominant leg", "type": "select", "options": ["right", "left", "unknown"]},
        ],
        "docsUrl": "https://accad.osu.edu/research/motion-lab/mocap-system-and-data",
    },
    "lafan1": {
        "label": "Ubisoft LaFAN1",
        "recommendedUse": "Research/in-betweening experiments only unless you verify license implications.",
        "licenseSummary": "CC BY-NC-ND 4.0. Good for non-commercial research evaluation; be careful using it to train a model intended for commercial distribution.",
        "riskLevel": "high_license_restriction",
        "trainingAllowed": "research_only_check_license",
        "defaultFormat": "BVH_OR_FBX",
        "fields": [
            {"name": "githubZipUrl", "label": "GitHub ZIP URL", "type": "url", "placeholder": "https://github.com/ubisoft/ubisoft-laforge-animation-dataset/archive/refs/heads/master.zip"},
            {"name": "subsetPath", "label": "Optional subset path/glob", "type": "text", "placeholder": "lafan1/**/*.bvh"},
            {"name": "action", "label": "Action label", "type": "text", "placeholder": "locomotion_transition"},
            {"name": "style", "label": "Style", "type": "text", "placeholder": "neutral"},
        ],
        "docsUrl": "https://github.com/ubisoft/ubisoft-laforge-animation-dataset",
    },
    "bandai_namco": {
        "label": "Bandai Namco Research Motion Dataset",
        "recommendedUse": "Research-only style/action data; useful for motion stylization experiments.",
        "licenseSummary": "Repository states datasets are CC BY-NC 4.0; third-party summaries describe research/personal use. Verify before commercial training.",
        "riskLevel": "noncommercial",
        "trainingAllowed": "research_only_noncommercial",
        "defaultFormat": "BVH",
        "fields": [
            {"name": "githubZipUrl", "label": "GitHub ZIP URL", "type": "url", "placeholder": "https://github.com/BandaiNamcoResearchInc/Bandai-Namco-Research-Motiondataset/archive/refs/heads/master.zip"},
            {"name": "subsetPath", "label": "Optional subset path/glob", "type": "text", "placeholder": "dataset/**/*.bvh"},
            {"name": "action", "label": "Action label", "type": "text", "placeholder": "locomotion"},
            {"name": "style", "label": "Style", "type": "text", "placeholder": "active"},
        ],
        "docsUrl": "https://github.com/BandaiNamcoResearchInc/Bandai-Namco-Research-Motiondataset",
    },
    "style100": {
        "label": "100STYLE Locomotion Dataset",
        "recommendedUse": "Excellent for locomotion/run style prior if license terms are acceptable for your use.",
        "licenseSummary": "Dataset page says it contains 4M+ frames and 100 styles of locomotion; verify license from the download/Zenodo record before training for commercial use.",
        "riskLevel": "check_license",
        "trainingAllowed": "check_license",
        "defaultFormat": "BVH",
        "fields": [
            {"name": "downloadUrl", "label": "Download URL", "type": "url", "placeholder": "Zenodo or direct BVH zip URL"},
            {"name": "subsetPath", "label": "Optional subset path/glob", "type": "text", "placeholder": "*_FR.bvh"},
            {"name": "action", "label": "Action label", "type": "text", "placeholder": "run"},
            {"name": "style", "label": "Style", "type": "text", "placeholder": "athletic"},
        ],
        "docsUrl": "https://www.ianxmason.com/100style/",
    },
    "manual": {
        "label": "Manual direct URL / local file registration",
        "recommendedUse": "Use for your own authored clips, purchased packs you can train on, or already-retargeted Aegis JSON.",
        "licenseSummary": "User-provided. You are responsible for confirming training rights.",
        "riskLevel": "user_responsibility",
        "trainingAllowed": "user_confirmed",
        "defaultFormat": "AegisJSON_OR_BVH_OR_FBX",
        "fields": [
            {"name": "downloadUrl", "label": "Direct URL", "type": "url", "placeholder": "https://.../clip.fbx or .bvh or .json"},
            {"name": "localPath", "label": "Local path already on disk", "type": "text", "placeholder": "C:\\Mocap\\clip.fbx"},
            {"name": "action", "label": "Action label", "type": "text", "placeholder": "soccer_kick_overlay"},
            {"name": "style", "label": "Style", "type": "text", "placeholder": "powerful"},
            {"name": "dominantLeg", "label": "Dominant leg", "type": "select", "options": ["right", "left", "unknown"]},
        ],
        "docsUrl": "",
    },
    "mixamo_blocked_ml": {
        "label": "Mixamo — blocked for ML training",
        "recommendedUse": "Use in projects per Adobe terms, but do not use Mixamo animations to train this motion-prior model.",
        "licenseSummary": "Adobe community FAQ says the only research application Mixamo content cannot be used in is training machine-learning models.",
        "riskLevel": "do_not_train",
        "trainingAllowed": False,
        "defaultFormat": "FBX",
        "fields": [],
        "docsUrl": "https://community.adobe.com/questions-696/mixamo-faq-licensing-royalties-ownership-eula-and-tos-589400",
    },
}

def get_source_list() -> List[Dict[str, Any]]:
    return [{"id": key, **value} for key, value in SOURCES.items()]
