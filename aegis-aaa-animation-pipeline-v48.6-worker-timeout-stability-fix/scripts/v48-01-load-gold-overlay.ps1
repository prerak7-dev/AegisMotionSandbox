param([string]$ConfigPath = "")
$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=V48_LOAD_GOLD_OVERLAY|progress=8|message=Loading V36 gold quaternion live-base overlay"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$config = Get-AegisV45Config $ConfigPath
$gold = Resolve-AegisV45Path $config.v48.goldOverlayPath
if (!(Test-Path $gold)) { throw "V48 gold overlay not found: $gold" }
$text = Get-Content $gold -Raw
if ($text -notmatch 'LiveBaseGeneratedOverlay' -or $text -notmatch 'rot_qx' -or $text -notmatch 'rot_qw') {
  throw "Gold overlay is not a quaternion LiveBaseGeneratedOverlay: $gold"
}
Write-Host "Gold overlay OK: $gold"
