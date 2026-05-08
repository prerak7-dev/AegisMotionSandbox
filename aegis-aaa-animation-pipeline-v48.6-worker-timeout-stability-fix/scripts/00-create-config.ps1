$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$template = Join-Path $Root "config\aegis_bandai_v45.config.template.json"
$config = Join-Path $Root "config\aegis_bandai_v45.config.json"
if (!(Test-Path $config)) {
    Copy-Item $template $config
    Write-Host "Created $config"
} else {
    Write-Host "Config already exists: $config"
}
Write-Host "Edit this file before running the pipeline:"
Write-Host $config
