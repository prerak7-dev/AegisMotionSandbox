param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=VERIFY_OVERLAY_JSON|progress=98|message=Validating generated overlay JSON for Aegis importer"
. "$PSScriptRoot\v45_common.ps1"
$config = Get-AegisV45Config $ConfigPath
Set-AegisPythonPath

$reportPath = "$($config.training.exportOutput).validation.json"
python -m aegis_motion_prior.validate_overlay_json `
  --overlay "$($config.training.exportOutput)" `
  --report "$reportPath"

Write-Host "Validation report: $reportPath"
