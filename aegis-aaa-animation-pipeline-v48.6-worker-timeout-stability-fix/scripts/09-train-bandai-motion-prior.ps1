param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=TRAIN_NEURAL_MOTION_PRIOR|progress=82|message=Training V47 neural overlay prior"
. "$PSScriptRoot\v45_common.ps1"
$config = Get-AegisV45Config $ConfigPath
Set-AegisPythonPath

python -m aegis_motion_prior.train_neural_v47 `
  --dataset "$($config.training.datasetOutput)" `
  --out "$($config.training.checkpointOutput)" `
  --epochs $($config.training.epochs)
