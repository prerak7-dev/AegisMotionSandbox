param([string]$ConfigPath = "", [string]$Action = "", [string]$Style = "", [string]$DominantLeg = "", [double]$Duration = 0)
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=GENERATE_OVERLAY_JSON|progress=94|message=Generating final V47.4 source-gated Aegis custom-data-asset overlay JSON"
. "$PSScriptRoot\v45_common.ps1"
$config = Get-AegisV45Config $ConfigPath
Set-AegisPythonPath

$checkpoint = Join-Path $config.training.checkpointOutput "neural_overlay_prior_v47.pt"
$durationValue = $Duration
if ($durationValue -le 0 -and ($config.training.PSObject.Properties.Name -contains "exportDurationSeconds")) {
    $durationValue = $config.training.exportDurationSeconds
}

$actionValue = if ([string]::IsNullOrWhiteSpace($Action)) { $config.metadata.defaultAction } else { $Action }
$styleValue = if ([string]::IsNullOrWhiteSpace($Style)) { $config.metadata.defaultStyle } else { $Style }
$legValue = if ([string]::IsNullOrWhiteSpace($DominantLeg)) { $config.metadata.dominantLeg } else { $DominantLeg }

Write-Host "Generating overlay for action=$actionValue style=$styleValue dominantLeg=$legValue durationSeconds=$durationValue (0 means use V47.4 action default short-overlay duration)"

$args = @(
  "-m", "aegis_motion_prior.infer_neural_v47",
  "--dataset", $config.training.datasetOutput,
  "--out", $config.training.exportOutput,
  "--action", $actionValue,
  "--style", $styleValue,
  "--dominant-leg", $legValue,
  "--duration", "$durationValue",
  "--fps", "$($config.training.fps)",
  "--default-action-duration", "1.35"
)

if (Test-Path $checkpoint) {
    $args += @("--checkpoint", $checkpoint)
} else {
    Write-Host "No trained V47 checkpoint found. Using validated retrieval + time-warp fallback."
}

python @args
Write-Host "Generated: $($config.training.exportOutput)"
