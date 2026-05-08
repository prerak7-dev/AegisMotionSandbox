$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
docker compose -f docker-compose.v46.yml up -d
Write-Host "Kafka: localhost:9092"
Write-Host "Redis: localhost:6379"
Write-Host "Kafka UI: http://localhost:8099"
