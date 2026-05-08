$ErrorActionPreference = "Stop"

try {
  Invoke-RestMethod -Method Get -Uri "http://localhost:8088/api/v1/jobs"
} catch {
  Write-Host "Request failed. Reading server error body..."
  $resp = $_.Exception.Response
  if ($resp -ne $null) {
    $stream = $resp.GetResponseStream()
    $reader = New-Object System.IO.StreamReader($stream)
    Write-Host $reader.ReadToEnd()
  } else {
    Write-Host $_.Exception.Message
  }
  throw
}
