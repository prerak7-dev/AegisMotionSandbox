param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=V48_VALIDATE_IMPORT_JSON|progress=98|message=Validating final V48 quaternion import JSON"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath
Set-AegisPythonPath
$InputPath = Resolve-AegisV45Path $config.training.exportOutput
$ReportPath = "$InputPath.validation.json"
Write-Host "Validation input: $InputPath"
Write-Host "Validation report target: $ReportPath"
if (!(Test-Path $InputPath)) {
    throw "V48 import JSON not found before validation: $InputPath"
}
python -m aegis_motion_prior.validate_v48_quaternion_overlay --input $InputPath --out $ReportPath
$pythonExit = $LASTEXITCODE
if ($pythonExit -ne 0) {
    throw "V48 quaternion validation failed with exit code $pythonExit"
}
if (!(Test-Path $ReportPath)) {
    throw "V48 quaternion validation finished but did not create report: $ReportPath"
}
Write-Host "Validation report: $ReportPath"
Write-Host "AEGIS_PROGRESS|step=V48_VALIDATE_IMPORT_JSON|progress=100|message=V48 validation finished successfully"
