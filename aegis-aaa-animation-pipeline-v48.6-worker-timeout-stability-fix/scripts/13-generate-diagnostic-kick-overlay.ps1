param([string]$Out = "exports/aegis_diagnostic_soccer_kick_overlay_v47_4.json", [double]$Duration = 1.35, [int]$Fps = 60)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
Set-AegisPythonPath
python "$Root\tools\python\make_diagnostic_kick_overlay.py" --out "$Out" --duration "$Duration" --fps "$Fps"
python -m aegis_motion_prior.validate_overlay_json --overlay "$Out" --report "$Out.validation.json"
Write-Host "Diagnostic overlay: $Out"
Write-Host "Validation report: $Out.validation.json"
