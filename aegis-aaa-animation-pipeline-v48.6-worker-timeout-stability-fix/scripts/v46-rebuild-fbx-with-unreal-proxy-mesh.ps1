param([string]$ConfigPath = "")

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
$config = Get-AegisV45Config $ConfigPath

$fbxDir = $config.bandai.fbxOutputDir
if (Test-Path $fbxDir) {
    Write-Host "Deleting old FBX files from $fbxDir"
    Get-ChildItem -Path $fbxDir -Recurse -Filter "*.fbx" -ErrorAction SilentlyContinue | Remove-Item -Force
}

Write-Host "Rebuilding FBX files with Unreal-compatible proxy mesh..."
& "$PSScriptRoot\04-convert-bandai-bvh-to-fbx.ps1" -ConfigPath $ConfigPath -ForceRebuild
