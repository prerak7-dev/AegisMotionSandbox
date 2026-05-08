param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=V48_BUILD_QUATERNION_DATASET|progress=45|message=Building V48 quaternion tensor dataset"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath
Set-AegisPythonPath
python -m aegis_motion_prior.dataset `
  --manifest (Resolve-AegisV45Path $config.training.manifestPath) `
  --out (Resolve-AegisV45Path $config.training.datasetOutput) `
  --fps $($config.training.fps) `
  --max-frames $($config.training.maxFrames)
