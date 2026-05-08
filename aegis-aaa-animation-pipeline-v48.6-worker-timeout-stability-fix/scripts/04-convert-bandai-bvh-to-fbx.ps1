param(
    [string]$ConfigPath = "",
    [switch]$ForceRebuild
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath
$manifestPath = Join-Path $Root $config.bandai.selectedClipManifest

if (!(Test-Path $manifestPath)) {
    throw "Selected clip manifest not found: $manifestPath. Run 03-select-bandai-clips.ps1 first."
}

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
if ($manifest.clipCount -eq 0 -or $manifest.clips.Count -eq 0) {
    throw "Selected clip manifest has 0 clips. Run 02-extract-bandai-data.ps1 and 03-select-bandai-clips.ps1 again."
}

$blender = $config.tools.blenderExe
if (!(Test-Path $blender)) {
    throw "Blender executable not found: $blender. Fix tools.blenderExe in config/aegis_bandai_v45.config.json"
}

$outRoot = $config.bandai.fbxOutputDir
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null

$converterScript = Join-Path $Root "tools\blender\convert_bvh_to_fbx.py"
if (!(Test-Path $converterScript)) {
    throw "Blender converter script not found: $converterScript"
}

$totalClips = $manifest.clips.Count
$index = 0
Write-Host "AEGIS_PROGRESS|step=CONVERT_BVH_TO_FBX|progress=20|message=V46.35 raw BVH to FBX conversion for $($totalClips) clips"

foreach ($clip in $manifest.clips) {
    $index++
    $src = $clip.path
    if (!(Test-Path $src)) {
        throw "BVH source file missing: $src"
    }

    $safeName = ($clip.id -replace '[^a-zA-Z0-9_\-]', '_') + ".fbx"
    $dst = Join-Path $outRoot $safeName

    if ((Test-Path $dst) -and $ForceRebuild) {
        Remove-Item $dst -Force
    }

    if (Test-Path $dst) {
        Write-Host "Exists: $dst"
        continue
    }

    $percent = 20 + [int](($index / [double]$totalClips) * 20)
    Write-Host "AEGIS_PROGRESS|step=CONVERT_BVH_TO_FBX|progress=$($percent)|message=Converting $($index)/$($totalClips) : $($clip.id)"
    Write-Host "Converting $($index)/$($totalClips): $src -> $dst"

    $env:AEGIS_BVH_SRC = $src
    $env:AEGIS_FBX_DST = $dst

    & $blender --background --python "$converterScript"

    $exitCode = $LASTEXITCODE
    Remove-Item Env:\AEGIS_BVH_SRC -ErrorAction SilentlyContinue
    Remove-Item Env:\AEGIS_FBX_DST -ErrorAction SilentlyContinue

    if ($exitCode -ne 0) {
        throw "Blender conversion failed with exit code $exitCode for $src"
    }

    if (!(Test-Path $dst)) {
        throw "Blender did not create expected FBX: $dst"
    }
}

$fbxCount = (Get-ChildItem -Path $outRoot -Recurse -Filter "*.fbx" -ErrorAction SilentlyContinue).Count
Write-Host "AEGIS_PROGRESS|step=CONVERT_BVH_TO_FBX|progress=40|message=Raw BVH to FBX conversion complete. FBX files: $($fbxCount)"
Write-Host "FBX files available: $fbxCount"
if ($fbxCount -eq 0) {
    throw "No FBX files were created under $outRoot."
}
