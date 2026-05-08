$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$env:PYTHONPATH = "$Root\motion-prior-service"

uvicorn aegis_motion_prior.source_web:app --host 0.0.0.0 --port 8092
