$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$env:PYTHONPATH = "$Root\motion-prior-service"

$env:AEGIS_MOTION_PRIOR_DATASET = "datasets/v37"
if (Test-Path "checkpoints/v37/motion_prior_v37.pt") {
  $env:AEGIS_MOTION_PRIOR_CHECKPOINT = "checkpoints/v37/motion_prior_v37.pt"
}

uvicorn aegis_motion_prior.service:app --host 0.0.0.0 --port 8091
