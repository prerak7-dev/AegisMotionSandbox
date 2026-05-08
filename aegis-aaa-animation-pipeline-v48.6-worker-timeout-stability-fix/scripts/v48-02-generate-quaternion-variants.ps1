param([string]$ConfigPath = "", [string]$Style = "", [string]$DominantLeg = "", [int]$VariantCount = 0, [double]$Intensity = -1, [double]$FollowThrough = -1, [double]$PlantStability = -1, [double]$UpperBodyCounterbalance = -1)
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=V48_GENERATE_SYNTHETIC_VARIANTS|progress=25|message=Generating Manny/Quinn-native quaternion kick variants from V36 gold reference"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath
Set-AegisPythonPath
$styleValue = if ([string]::IsNullOrWhiteSpace($Style)) { $config.v48.style } else { $Style }
$legValue = if ([string]::IsNullOrWhiteSpace($DominantLeg)) { $config.v48.dominantLeg } else { $DominantLeg }
$countValue = if ($VariantCount -gt 0) { $VariantCount } else { [int]$config.v48.variantCount }
$intensityValue = if ($Intensity -ge 0) { $Intensity } else { [double]$config.v48.intensity }
$followValue = if ($FollowThrough -ge 0) { $FollowThrough } else { [double]$config.v48.followThrough }
$plantValue = if ($PlantStability -ge 0) { $PlantStability } else { [double]$config.v48.plantStability }
$counterValue = if ($UpperBodyCounterbalance -ge 0) { $UpperBodyCounterbalance } else { [double]$config.v48.upperBodyCounterbalance }
python -m aegis_motion_prior.v48_generate_quaternion_variants `
  --gold (Resolve-AegisV45Path $config.v48.goldOverlayPath) `
  --out-dir (Resolve-AegisV45Path $config.v48.variantOutputDir) `
  --manifest (Resolve-AegisV45Path $config.v48.variantManifestPath) `
  --count $countValue `
  --fps $($config.v48.fps) `
  --dominant-leg $legValue `
  --style $styleValue `
  --intensity $intensityValue `
  --follow-through $followValue `
  --plant-stability $plantValue `
  --upper-body-counterbalance $counterValue `
  --seed $($config.v48.seed)
