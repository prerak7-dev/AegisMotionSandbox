$ErrorActionPreference = "Stop"

Write-Host "Health:"
try {
  Invoke-RestMethod -Method Get -Uri "http://localhost:8088/api/v1/health" | ConvertTo-Json -Depth 8
} catch {
  Write-Host "Orchestrator health failed:"
  Write-Host $_.Exception.Message
}

Write-Host "`nDiagnostics:"
try {
  Invoke-RestMethod -Method Get -Uri "http://localhost:8088/api/v1/diagnostics" | ConvertTo-Json -Depth 8
} catch {
  Write-Host "Diagnostics failed. Rebuild/restart V47 orchestrator if this endpoint does not exist."
  Write-Host $_.Exception.Message
}

Write-Host "`nDocker containers:"
docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
