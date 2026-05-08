param(
  [string]$Input = "exports/bandai_soccer_kick_overlay_v47.json",
  [string]$Out = "exports/bandai_soccer_kick_overlay_v47_5_knee_coupled_repaired.json",
  [double]$Duration = 1.35,
  [int]$Fps = 60,
  [ValidateSet("right", "left")]
  [string]$DominantLeg = "right",
  [switch]$StabilizeOnly
)
$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
Set-AegisPythonPath

if ($StabilizeOnly) {
  python -m aegis_motion_prior.repair_overlay_json_v47 --input $Input --out $Out --duration $Duration --fps $Fps --dominant-leg $DominantLeg
} else {
  python -m aegis_motion_prior.repair_scalar_overlay_kick_v47_5 --input $Input --out $Out --dominant-leg $DominantLeg
}
python -m aegis_motion_prior.validate_overlay_json --overlay $Out --report "$Out.validation.json"
Write-Host "Repaired overlay: $Out"
