param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=BUILD_TRAINING_MANIFEST|progress=68|message=Building training manifest"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$configPathResolved = Get-AegisV45ConfigPath $ConfigPath
Set-AegisPythonPath
python "$Root\tools\python\build_training_manifest_from_json.py" --root "$Root" --config "$configPathResolved"
