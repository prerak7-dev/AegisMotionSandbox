param([Parameter(Mandatory=$true)][string]$JobId)
$ErrorActionPreference = "Stop"

$body = @{ note = "Retargeted animations are ready for export." } | ConvertTo-Json
Invoke-RestMethod -Method Post -Uri "http://localhost:8088/api/v1/jobs/$JobId/continue-after-retarget" -ContentType "application/json" -Body $body
