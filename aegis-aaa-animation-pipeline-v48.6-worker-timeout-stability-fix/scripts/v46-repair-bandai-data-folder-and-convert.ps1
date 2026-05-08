param([string]$ConfigPath = "")

$ErrorActionPreference = "Stop"

Write-Host "Repairing Bandai current data-folder layout: validate data folders -> select -> convert..."
& "$PSScriptRoot\02-extract-bandai-data.ps1" -ConfigPath $ConfigPath
& "$PSScriptRoot\03-select-bandai-clips.ps1" -ConfigPath $ConfigPath
& "$PSScriptRoot\04-convert-bandai-bvh-to-fbx.ps1" -ConfigPath $ConfigPath
& "$PSScriptRoot\v46-check-bandai-stage.ps1" -ConfigPath $ConfigPath
