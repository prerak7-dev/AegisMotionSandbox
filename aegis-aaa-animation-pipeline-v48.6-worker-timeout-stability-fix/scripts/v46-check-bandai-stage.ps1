$ErrorActionPreference = "Stop"
param([string]$ConfigPath = "")

. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath

$selectedManifest = Join-Path $Root $config.bandai.selectedClipManifest
$fbxDir = $config.bandai.fbxOutputDir
$trainingDir = Join-Path $Root $config.unreal.trainingJsonOutput

Write-Host "Bandai repo: $($config.bandai.repoDir)"
Write-Host "Raw extract dir: $($config.bandai.rawExtractDir)"
Write-Host "Selected clip manifest: $selectedManifest"
Write-Host "FBX output dir: $fbxDir"
Write-Host "Training JSON dir: $trainingDir"
Write-Host ""

if (Test-Path $selectedManifest) {
  $manifest = Get-Content $selectedManifest -Raw | ConvertFrom-Json
  Write-Host "Selected clips: $($manifest.clipCount)"
  if ($manifest.clips.Count -gt 0) {
    Write-Host "First selected clips:"
    $manifest.clips | Select-Object -First 5 | Format-Table id, content, style, path
  }
} else {
  Write-Host "Selected clip manifest does not exist yet."
}

if (Test-Path $fbxDir) {
  $fbx = Get-ChildItem -Path $fbxDir -Recurse -Filter "*.fbx" -ErrorAction SilentlyContinue
  Write-Host "FBX files: $($fbx.Count)"
  $fbx | Select-Object -First 5 | Format-Table FullName
} else {
  Write-Host "FBX output folder does not exist yet."
}

if (Test-Path $trainingDir) {
  $json = Get-ChildItem -Path $trainingDir -Filter "*.json" -ErrorAction SilentlyContinue
  Write-Host "Training JSON files: $($json.Count)"
} else {
  Write-Host "Training JSON folder does not exist yet."
}
