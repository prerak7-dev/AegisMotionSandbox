$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$env:PYTHONPATH = "$Root\motion-prior-service"

python -m aegis_motion_prior.dataset `
  --manifest "sample-data/manifest.sample.json" `
  --out "datasets/v37" `
  --fps 60 `
  --max-frames 180
