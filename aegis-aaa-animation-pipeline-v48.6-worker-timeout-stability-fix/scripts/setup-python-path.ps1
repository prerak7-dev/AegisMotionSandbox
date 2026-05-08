$Root = Split-Path -Parent $PSScriptRoot
$env:PYTHONPATH = "$Root\motion-prior-service"
Write-Host "PYTHONPATH=$env:PYTHONPATH"
