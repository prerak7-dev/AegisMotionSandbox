param([string]$ConfigPath = "")

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
$config = Get-AegisV45Config $ConfigPath

$projectDir = Split-Path -Parent $config.unreal.project
$diskPath = Join-Path $projectDir "Content\AegisMotionTraining\BandaiNamco\Imported"

Write-Host "This deletes imported Bandai assets on disk:"
Write-Host $diskPath
$answer = Read-Host "Type DELETE to continue"
if ($answer -ne "DELETE") {
    Write-Host "Cancelled."
    exit 0
}
if (Test-Path $diskPath) {
    Remove-Item $diskPath -Recurse -Force
    Write-Host "Deleted $diskPath"
} else {
    Write-Host "Folder did not exist."
}
