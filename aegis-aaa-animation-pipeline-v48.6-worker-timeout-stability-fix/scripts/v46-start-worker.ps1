$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$ServiceRoot = Join-Path $Root "backend\worker-service"

if (!(Test-Path $ServiceRoot)) {
    throw "worker-service folder not found: $ServiceRoot"
}

Set-Location $ServiceRoot

$env:KAFKA_BOOTSTRAP_SERVERS = "localhost:9092"
$env:REDIS_HOST = "localhost"
$env:REDIS_PORT = "6379"
$env:AEGIS_PIPELINE_ROOT = $Root

Write-Host "Starting Aegis V47 Worker Service on http://localhost:8090 ..."
Write-Host "Kafka: $env:KAFKA_BOOTSTRAP_SERVERS"
Write-Host "Redis: $env:REDIS_HOST`:$env:REDIS_PORT"
Write-Host "Pipeline root: $env:AEGIS_PIPELINE_ROOT"
Write-Host "IMPORTANT: keep this window open. If the worker is not running, jobs stay QUEUED and the neural overlay job will stay QUEUED."

mvn spring-boot:run
