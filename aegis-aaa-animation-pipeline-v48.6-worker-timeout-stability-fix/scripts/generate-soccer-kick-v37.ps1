$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$env:PYTHONPATH = "$Root\motion-prior-service"

$checkpoint = "checkpoints/v37/motion_prior_v37.pt"

if (Test-Path $checkpoint) {
  python -m aegis_motion_prior.infer `
    --dataset "datasets/v37" `
    --checkpoint $checkpoint `
    --out "exports/ai_soccer_kick_motion_prior_v37.json" `
    --action "soccer_kick_overlay" `
    --style "powerful" `
    --dominant-leg "right" `
    --duration 1.35 `
    --fps 60
} else {
  Write-Host "No checkpoint found. Using retrieval fallback from dataset."
  python -m aegis_motion_prior.infer `
    --dataset "datasets/v37" `
    --out "exports/ai_soccer_kick_motion_prior_v37.json" `
    --action "soccer_kick_overlay" `
    --style "powerful" `
    --dominant-leg "right" `
    --duration 1.35 `
    --fps 60
}
