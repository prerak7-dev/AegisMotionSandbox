$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$env:PYTHONPATH = "$Root\motion-prior-service"

$checkpoint = "checkpoints/v42_contact/motion_prior_contact_v42.pt"
if (Test-Path $checkpoint) {
  python -m aegis_motion_prior.infer_v43 `
    --dataset "datasets/v37_real" `
    --checkpoint $checkpoint `
    --out "exports/aegis_v43_soccer_kick_overlay.json" `
    --action "soccer_kick_overlay" `
    --style "powerful" `
    --dominant-leg "right" `
    --duration 1.35 `
    --fps 60
} else {
  Write-Host "No V42 checkpoint found. Using retrieval + timewarp fallback."
  python -m aegis_motion_prior.infer_v43 `
    --dataset "datasets/v37_real" `
    --out "exports/aegis_v43_soccer_kick_overlay.json" `
    --action "soccer_kick_overlay" `
    --style "powerful" `
    --dominant-leg "right" `
    --duration 1.35 `
    --fps 60
}
