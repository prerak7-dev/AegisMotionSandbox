$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$ServiceRoot = Join-Path $Root "backend\orchestrator-service"

if (!(Test-Path $ServiceRoot)) {
    throw "orchestrator-service folder not found: $ServiceRoot"
}

Set-Location $ServiceRoot

$env:KAFKA_BOOTSTRAP_SERVERS = "localhost:9092"
$env:REDIS_HOST = "localhost"
$env:REDIS_PORT = "6379"

Write-Host "Starting Aegis V47 Orchestrator Service on http://localhost:8088 ..."
Write-Host "Kafka: $env:KAFKA_BOOTSTRAP_SERVERS"
Write-Host "Redis: $env:REDIS_HOST`:$env:REDIS_PORT"

mvn spring-boot:run
