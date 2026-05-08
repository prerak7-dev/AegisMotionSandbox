param(
    [string]$ConfigPath = "",
    [switch]$SkipClone,
    [switch]$SkipConvert,
    [switch]$SkipUnrealImport,
    [switch]$SkipTraining
)
$ErrorActionPreference = "Stop"

. "$PSScriptRoot\v45_common.ps1"

if (!(Test-Path (Get-AegisV45ConfigPath $ConfigPath))) {
    & "$PSScriptRoot\00-create-config.ps1"
    throw "Edit config\aegis_bandai_v45.config.json, then rerun this script."
}

if (!$SkipClone) {
    & "$PSScriptRoot\01-clone-bandai-dataset.ps1" -ConfigPath $ConfigPath
    & "$PSScriptRoot\02-extract-bandai-data.ps1" -ConfigPath $ConfigPath
}
& "$PSScriptRoot\03-select-bandai-clips.ps1" -ConfigPath $ConfigPath

if (!$SkipConvert) {
    & "$PSScriptRoot\04-convert-bandai-bvh-to-fbx.ps1" -ConfigPath $ConfigPath
}

if (!$SkipUnrealImport) {
    & "$PSScriptRoot\05-launch-unreal-batch-import.ps1" -ConfigPath $ConfigPath

    Write-Host ""
    Write-Host "============================================================"
    Write-Host "MANUAL ONE-TIME STEP REQUIRED:"
    Write-Host "Retarget imported Bandai animations to Manny/Quinn using your IK Retargeter."
    Write-Host "Output them to the retargeted path in config.unreal.retargetedPath."
    Write-Host "After retargeting, press Enter to export training JSON with the V45 commandlet."
    Write-Host "============================================================"
    Read-Host

    & "$PSScriptRoot\06-export-retargeted-animsequences.ps1" -ConfigPath $ConfigPath
}

& "$PSScriptRoot\07-build-training-manifest.ps1" -ConfigPath $ConfigPath
& "$PSScriptRoot\08-build-bandai-training-dataset.ps1" -ConfigPath $ConfigPath

if (!$SkipTraining) {
    & "$PSScriptRoot\09-train-bandai-motion-prior.ps1" -ConfigPath $ConfigPath
}
& "$PSScriptRoot\10-generate-bandai-soccer-kick-overlay.ps1" -ConfigPath $ConfigPath
