param(
    [string]$ConfigPath = "",
    [string]$BvhPath = ""
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath

if ([string]::IsNullOrWhiteSpace($BvhPath)) {
    $manifestPath = Join-Path $Root $config.bandai.selectedClipManifest
    if (!(Test-Path $manifestPath)) {
        throw "Selected clip manifest not found. Run 03-select-bandai-clips.ps1 first."
    }
    $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.clips.Count -eq 0) {
        throw "Selected clip manifest has 0 clips."
    }
    $BvhPath = $manifest.clips[0].path
}

$blender = $config.tools.blenderExe
$outRoot = $config.bandai.fbxOutputDir
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null
$outPath = Join-Path $outRoot "aegis_test_one_clip_proxy_mesh.fbx"
if (Test-Path $outPath) {
    Remove-Item $outPath -Force
}
$converterScript = Join-Path $Root "tools\blender\convert_bvh_to_fbx.py"

Write-Host "Testing one BVH conversion with Unreal proxy mesh:"
Write-Host "BVH: $BvhPath"
Write-Host "FBX: $outPath"

$env:AEGIS_BVH_SRC = $BvhPath
$env:AEGIS_FBX_DST = $outPath
& $blender --background --python "$converterScript"
$exitCode = $LASTEXITCODE
Remove-Item Env:\AEGIS_BVH_SRC -ErrorAction SilentlyContinue
Remove-Item Env:\AEGIS_FBX_DST -ErrorAction SilentlyContinue

if ($exitCode -ne 0) {
    throw "Blender conversion failed with exit code $exitCode"
}
if (!(Test-Path $outPath)) {
    throw "Expected output not created: $outPath"
}
Write-Host "Success: $outPath"
Write-Host "Now run 05-launch-unreal-batch-import.ps1 to verify Unreal import."
