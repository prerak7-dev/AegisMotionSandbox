# Java Pipeline Integration

V37 introduces a Python motion-prior service because neural training/inference is more practical in Python than inside Spring Boot.

Recommended integration:

```text
Spring API endpoint
→ calls http://localhost:8091/generate
→ receives Aegis JSON
→ saves/export-service writes JSON
```

## Endpoint to add in Java generation service

```text
POST /api/animations/generate-motion-prior
```

Body:

```json
{
  "action": "soccer_kick_overlay",
  "style": "powerful",
  "dominantLeg": "right",
  "durationSeconds": 1.35,
  "skeletonProfile": "UE5_Mannequin"
}
```

Java should forward this to:

```text
POST http://localhost:8091/generate
```

The returned JSON can be imported directly by the V36+ plugin.
