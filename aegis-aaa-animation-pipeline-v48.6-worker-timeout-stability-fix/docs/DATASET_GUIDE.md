# Dataset Guide

## Recommended sources

Use legally licensed animation clips that you are allowed to train on:

- your own authored animations
- purchased/owned animation packs with training permission
- studio-internal clips if you have permission
- public datasets whose license allows ML training and derivative use

Avoid training on game assets you do not have the right to use.

## Retargeting requirement

Before training, every clip must be retargeted to Manny/Quinn/UE5 mannequin.

The easiest workflow:

```text
source animation
→ Unreal IK Retargeter
→ Manny/Quinn animation sequence
→ Aegis JSON export
→ manifest entry
```

## Manifest example

```json
{
  "clips": [
    {
      "id": "kick_001",
      "path": "clips/kick_001.json",
      "action": "soccer_kick_overlay",
      "style": "powerful",
      "dominantLeg": "right",
      "quality": "authored"
    }
  ]
}
```
