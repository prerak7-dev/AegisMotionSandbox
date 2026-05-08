param([string]$ConfigPath = "", [string]$Style = "", [string]$DominantLeg = "", [double]$Duration = 0, [double]$Intensity = -1, [double]$FollowThrough = -1, [double]$PlantStability = -1, [double]$UpperBodyCounterbalance = -1)
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=V48_GENERATE_IMPORT_JSON|progress=92|message=Generating final V48 quaternion import JSON for Aegis custom data asset"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath
Set-AegisPythonPath
$styleValue = if ([string]::IsNullOrWhiteSpace($Style)) { $config.v48.style } else { $Style }
$legValue = if ([string]::IsNullOrWhiteSpace($DominantLeg)) { $config.v48.dominantLeg } else { $DominantLeg }
$durationValue = if ($Duration -gt 0) { $Duration } else { [double]$config.v48.durationSeconds }
$intensityValue = if ($Intensity -ge 0) { $Intensity } else { [double]$config.v48.intensity }
$followValue = if ($FollowThrough -ge 0) { $FollowThrough } else { [double]$config.v48.followThrough }
$plantValue = if ($PlantStability -ge 0) { $PlantStability } else { [double]$config.v48.plantStability }
$counterValue = if ($UpperBodyCounterbalance -ge 0) { $UpperBodyCounterbalance } else { [double]$config.v48.upperBodyCounterbalance }
$checkpoint = Join-Path (Resolve-AegisV45Path $config.training.checkpointOutput) "quaternion_kick_prior_v48.pt"
$args = @(
  "-m", "aegis_motion_prior.infer_v48_quaternion_overlay",
  "--dataset", (Resolve-AegisV45Path $config.training.datasetOutput),
  "--out", (Resolve-AegisV45Path $config.training.exportOutput),
  "--action", $config.metadata.defaultAction,
  "--style", $styleValue,
  "--dominant-leg", $legValue,
  "--duration", "$durationValue",
  "--fps", "$($config.v48.fps)",
  "--intensity", "$intensityValue",
  "--follow-through", "$followValue",
  "--plant-stability", "$plantValue",
  "--upper-body-counterbalance", "$counterValue",
  "--neural-blend", "$($config.v48.neuralBlend)"
)
if (Test-Path $checkpoint) { $args += @("--checkpoint", $checkpoint) } else { Write-Host "No V48 checkpoint found; using phase-variant retrieval fallback." }
python @args
Write-Host "Generated import JSON: $(Resolve-AegisV45Path $config.training.exportOutput)"
