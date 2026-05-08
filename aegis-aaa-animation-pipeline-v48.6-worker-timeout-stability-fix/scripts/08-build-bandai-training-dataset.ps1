param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=BUILD_TENSOR_DATASET|progress=65|message=Building V47 tensor dataset from retargeted Manny JSON"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath
Set-AegisPythonPath

python -m aegis_motion_prior.dataset `
  --manifest "$($config.training.manifestPath)" `
  --out "$($config.training.datasetOutput)" `
  --fps $($config.training.fps) `
  --max-frames $($config.training.maxFrames)

$requestedAction = $config.metadata.defaultAction
Write-Host "Verifying tensor dataset contains semantically valid source clips for action=$requestedAction"
python -m aegis_motion_prior.verify_dataset_for_action `
  --dataset "$($config.training.datasetOutput)" `
  --action "$requestedAction"
