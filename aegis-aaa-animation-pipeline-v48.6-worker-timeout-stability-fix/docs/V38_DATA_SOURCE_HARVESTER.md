# V38 Data Source Harvester

V38 adds a local web page to collect raw animation data from public sources.

Start it:

```powershell
.\scripts\install-harvester-deps.ps1
.\scripts\start-data-harvester.ps1
```

Open:

```text
http://localhost:8092
```

## Supported source profiles

- CMU original ASF/AMC
- CMU converted FBX on Hugging Face
- ACCAD / Ohio State Open Motion Project
- Ubisoft LaFAN1
- Bandai Namco Research Motion Dataset
- 100STYLE
- manual direct URL/local file
- Mixamo is shown as blocked for ML training

## Important

This harvester downloads/registers **raw source motion files**.

Raw BVH/FBX/ASF/AMC is not enough for the learned motion prior. You still need:

```text
raw source motion
→ retarget to UE5 Manny/Quinn
→ export Aegis JSON
→ add exported JSON to training manifest
→ build tensors
→ train motion prior
```

## Manifest created

The harvester writes:

```text
sample-data/manifests/harvested_raw_manifest.json
```

That manifest is a raw-source manifest, not the final training manifest. The final training manifest should point to retargeted Aegis JSON clips.
