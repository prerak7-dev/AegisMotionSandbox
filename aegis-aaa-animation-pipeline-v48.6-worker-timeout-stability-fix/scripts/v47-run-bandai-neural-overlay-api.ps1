param(
  [string]$ConfigPath = "",
  [switch]$SkipClone,
  [switch]$SkipTraining,
  [string]$TargetFbx = "",
  [int]$MaxClips = 0
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
  $ConfigPath = Join-Path $Root "config\aegis_bandai_v45.config.json"
}

if (!(Test-Path $ConfigPath)) {
  throw "Config not found: $ConfigPath. Run .\scripts\00-create-config.ps1 and edit config\aegis_bandai_v45.config.json first."
}

Write-Host "Checking orchestrator health..."
try {
  Invoke-RestMethod -Method Get -Uri "http://localhost:8088/api/v1/health" | ConvertTo-Json
} catch {
  throw "Orchestrator is not reachable at http://localhost:8088. Start it with .\scripts\v46-start-orchestrator.ps1"
}

$bodyObj = @{
  configPath = $ConfigPath
  skipClone = [bool]$SkipClone
  skipTraining = [bool]$SkipTraining
  useOfflineRetarget = $true
  pauseForManualRetarget = $false
  action = "soccer_kick_overlay"
  style = "active"
  dominantLeg = "right"
}
if (![string]::IsNullOrWhiteSpace($TargetFbx)) { $bodyObj.targetFbx = $TargetFbx }
if ($MaxClips -gt 0) { $bodyObj.maxClips = $MaxClips }

$body = $bodyObj | ConvertTo-Json
Write-Host "Starting V47 neural overlay pipeline through Spring Boot API..."
Write-Host $body

try {
  Invoke-RestMethod -Method Post -Uri "http://localhost:8088/api/v1/pipelines/bandai/run" -ContentType "application/json" -Body $body
} catch {
  Write-Host "Request failed. Reading server error body..."
  $resp = $_.Exception.Response
  if ($resp -ne $null) {
    $stream = $resp.GetResponseStream()
    $reader = New-Object System.IO.StreamReader($stream)
    $errorBody = $reader.ReadToEnd()
    Write-Host $errorBody
  } else {
    Write-Host $_.Exception.Message
  }
  throw
}
