$ErrorActionPreference = "Stop"

Write-Host "Health:"
Invoke-RestMethod -Method Get -Uri "http://localhost:8088/api/v1/health"

Write-Host "Latest export:"
Invoke-RestMethod -Method Get -Uri "http://localhost:8088/api/v1/exports/latest"
