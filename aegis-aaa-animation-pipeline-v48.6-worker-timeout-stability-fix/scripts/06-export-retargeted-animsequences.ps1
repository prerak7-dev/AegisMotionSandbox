param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=EXPORT_RETARGETED_ANIMSEQUENCES|progress=60|message=Exporting retargeted AnimSequences to Aegis training JSON"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath

$outputDir = Join-Path $Root $config.unreal.trainingJsonOutput
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& $config.tools.unrealEditorCmdExe `
  $config.unreal.project `
  -run=AegisExportAnimSequences `
  -ContentPath="$($config.unreal.retargetedPath)" `
  -OutputDir="$outputDir" `
  -Action="$($config.metadata.defaultAction)" `
  -Style="$($config.metadata.defaultStyle)" `
  -DominantLeg="$($config.metadata.dominantLeg)" `
  -License="$($config.metadata.license)" `
  -SkeletonProfile="$($config.metadata.skeletonProfile)" `
  -SampleRate=$($config.training.fps) `
  -GenerateFootContacts=true `
  -unattended -nop4
