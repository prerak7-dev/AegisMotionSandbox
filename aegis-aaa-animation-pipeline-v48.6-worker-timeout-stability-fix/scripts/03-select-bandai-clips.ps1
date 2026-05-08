param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=SELECT_BANDAI_CLIPS|progress=18|message=Selecting Bandai BVH clips"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$configPathResolved = Get-AegisV45ConfigPath $ConfigPath
Set-AegisPythonPath
python "$Root\tools\python\select_bandai_clips.py" --config "$configPathResolved" --root "$Root"
