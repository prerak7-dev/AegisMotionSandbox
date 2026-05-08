param(
    [string]$ConfigPath = "",
    [string]$FbxPath = ""
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath

if ([string]::IsNullOrWhiteSpace($FbxPath)) {
    $fbxDir = $config.bandai.fbxOutputDir
    $first = Get-ChildItem -Path $fbxDir -Recurse -Filter "*.fbx" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $first) {
        throw "No FBX files found in $fbxDir"
    }
    $FbxPath = $first.FullName
}

$script = Join-Path $Root "tools\blender\inspect_fbx_for_unreal.py"
$env:AEGIS_FBX_INSPECT_PATH = $FbxPath
& $config.tools.blenderExe --background --python "$script"
$exit = $LASTEXITCODE
Remove-Item Env:\AEGIS_FBX_INSPECT_PATH -ErrorAction SilentlyContinue

if ($exit -ne 0) {
    throw "FBX inspect failed with exit code $exit"
}
