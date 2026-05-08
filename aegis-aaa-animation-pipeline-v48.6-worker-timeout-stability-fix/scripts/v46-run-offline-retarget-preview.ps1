param(
    [string]$TargetFbx = "C:\Mocap\AegisTargets\SKM_Quinn.fbx"
)

$ErrorActionPreference = "Stop"
Write-Host "Running V46.37 offline retarget preview for one clip..."
.\scripts\06-offline-retarget-bandai-to-manny-json.ps1 -TargetFbx $TargetFbx -MaxClips 1
Write-Host "Open: sample-data\training-json\offline-retargeted\offline_retarget_summary.json"
Write-Host "Open: generated\overlays\offline-retargeted"
