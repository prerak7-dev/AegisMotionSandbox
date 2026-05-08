$ErrorActionPreference = "Stop"
Invoke-RestMethod -Method Delete -Uri "http://localhost:8088/api/v1/jobs?terminalOnly=true"
