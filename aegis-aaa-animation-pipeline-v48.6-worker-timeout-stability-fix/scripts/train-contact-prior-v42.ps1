$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$env:PYTHONPATH = "$Root\motion-prior-service"

python -m aegis_motion_prior.train_contact_v42 `
  --dataset "datasets/v37_real" `
  --out "checkpoints/v42_contact" `
  --epochs 150
