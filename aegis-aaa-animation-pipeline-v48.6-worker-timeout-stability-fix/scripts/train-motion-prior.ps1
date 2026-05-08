$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$env:PYTHONPATH = "$Root\motion-prior-service"

python -m aegis_motion_prior.train `
  --dataset "datasets/v37" `
  --out "checkpoints/v37" `
  --epochs 100
