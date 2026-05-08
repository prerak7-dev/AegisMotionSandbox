param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=V48_TRAIN_QUATERNION_PRIOR|progress=72|message=Training V48 quaternion neural refinement prior"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath
Set-AegisPythonPath
python -m aegis_motion_prior.train_quaternion_prior_v48 `
  --dataset (Resolve-AegisV45Path $config.training.datasetOutput) `
  --out (Resolve-AegisV45Path $config.training.checkpointOutput) `
  --epochs $($config.training.epochs) `
  --lr $($config.training.lr) `
  --hidden-dim $($config.training.hiddenDim) `
  --batch-size 512 `
  --noise-sigma 0.035
