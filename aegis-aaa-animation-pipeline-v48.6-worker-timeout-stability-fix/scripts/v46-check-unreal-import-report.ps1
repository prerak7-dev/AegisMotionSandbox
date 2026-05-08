param([string]$ConfigPath = "")

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
$config = Get-AegisV45Config $ConfigPath

$projectDir = Split-Path -Parent $config.unreal.project
$report = Join-Path $projectDir "Saved\AegisV46ImportReport.json"
$baseDiskPath = Join-Path $projectDir "Content\AegisMotionTraining\BandaiNamco\Imported"
$animDiskPath = Join-Path $baseDiskPath "Animations"
$skeletonDiskPath = Join-Path $baseDiskPath "SkeletonSource"

Write-Host "Expected source skeleton Content Browser path: /Game/AegisMotionTraining/BandaiNamco/Imported/SkeletonSource"
Write-Host "Expected animation Content Browser path: /Game/AegisMotionTraining/BandaiNamco/Imported/Animations"
Write-Host "Animation disk path: $animDiskPath"
Write-Host "Import report: $report"

foreach ($path in @($skeletonDiskPath, $animDiskPath)) {
    if (Test-Path $path) {
        $uassets = Get-ChildItem -Path $path -Recurse -File -Filter "*.uasset" -ErrorAction SilentlyContinue
        Write-Host "$path"
        Write-Host "  UAsset files: $($uassets.Count)"
        $uassets | Select-Object -First 15 | Format-Table FullName
    } else {
        Write-Host "Missing disk folder: $path"
    }
}

if (Test-Path $report) {
    $json = Get-Content $report -Raw | ConvertFrom-Json
    Write-Host "Report version: $($json.version)"
    Write-Host "Report FBX count: $($json.fbxCount)"
    Write-Host "Source Skeleton: $($json.sourceSkeleton)"
    Write-Host "Report imported AnimSequences: $($json.importedAnimSequencePaths.Count)"
    if ($json.importedAnimSequencePaths.Count -gt 0) {
        $json.importedAnimSequencePaths | Select-Object -First 40
    }
    if ($json.errors.Count -gt 0) {
        Write-Host "Errors:"
        $json.errors | ConvertTo-Json -Depth 8
    }
} else {
    Write-Host "No import report found yet."
}
