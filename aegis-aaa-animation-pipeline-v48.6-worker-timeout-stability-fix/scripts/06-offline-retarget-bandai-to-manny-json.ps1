param(
    [string]$ConfigPath = "",
    [string]$TargetFbx = "",
    [int]$MaxClips = -1,
    [switch]$AllClips
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"

$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath

$offlineConfigPath = Join-Path $Root "config\offline_retarget_v46.config.json"
if (!(Test-Path $offlineConfigPath)) {
    throw "Offline retarget config not found: $offlineConfigPath"
}
$offline = Get-Content $offlineConfigPath -Raw | ConvertFrom-Json

$targetFbxResolved = $TargetFbx
if ([string]::IsNullOrWhiteSpace($targetFbxResolved)) {
    $targetFbxResolved = $offline.targetSkeletonFbx
}

if (!(Test-Path $targetFbxResolved)) {
    throw "Target Manny/Quinn FBX not found: $targetFbxResolved. Export SKM_Quinn or SKM_Manny from Unreal as an FBX to C:\Mocap\AegisTargets first. Use the skeletal mesh asset, not the Skeleton asset."
}

$manifestPath = Join-Path $Root $config.bandai.selectedClipManifest
if (!(Test-Path $manifestPath)) {
    throw "Selected clip manifest not found: $manifestPath. Run the Bandai selection step first."
}

$blender = $config.tools.blenderExe
if (!(Test-Path $blender)) {
    throw "Blender executable not found: $blender"
}

$tool = Join-Path $Root "tools\blender\offline_retarget_bandai_to_manny_json.py"
if (!(Test-Path $tool)) {
    throw "Offline retarget tool not found: $tool"
}

$maxClipArg = $offline.maxClips
if ($null -eq $maxClipArg -or $maxClipArg -lt 0) { $maxClipArg = 16 }
if ($MaxClips -gt 0) { $maxClipArg = $MaxClips }
if ($AllClips) { $maxClipArg = 0 }

Write-Host "AEGIS_PROGRESS|step=OFFLINE_RETARGET_TO_MANNY_JSON|progress=24|message=Starting offline BVH-to-Manny retarget JSON export"
Write-Host "Target FBX: $targetFbxResolved"
Write-Host "Manifest: $manifestPath"
Write-Host "Max clips: $maxClipArg"

& $blender --background --python "$tool" -- `
    --root "$Root" `
    --manifest "$manifestPath" `
    --target-fbx "$targetFbxResolved" `
    --bone-map "$($offline.boneMap)" `
    --offsets "$($offline.offsets)" `
    --output-training-dir "$($offline.outputTrainingJsonDir)" `
    --output-overlay-dir "$($offline.outputOverlayJsonDir)" `
    --fps $($offline.fps) `
    --source-scale $($offline.sourceBvhScale) `
    --emit-every $($offline.emitEveryNthFrame) `
    --max-clips $maxClipArg `
    --root-translation-scale $($offline.rootTranslationScale) `
    --contact-height-threshold $($offline.contact.heightThresholdCm) `
    --contact-velocity-threshold $($offline.contact.velocityThresholdCmPerSec) `
    --contact-smooth-window $($offline.contact.smoothWindowFrames)

$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    throw "Offline retarget failed with exit code $exitCode"
}

Write-Host "AEGIS_PROGRESS|step=OFFLINE_RETARGET_TO_MANNY_JSON|progress=48|message=Offline retarget training JSON export complete"
Write-Host "Done."
