param([string]$ConfigPath = "")

$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=CLONE_BANDAI_REPO|progress=5|message=Cloning or updating Bandai repository"
. "$PSScriptRoot\v45_common.ps1"
$config = Get-AegisV45Config $ConfigPath
$repo = $config.bandai.repoDir
$git = $config.tools.gitExe

if (Test-Path (Join-Path $repo ".git")) {
    Write-Host "Bandai repo exists. Pulling latest..."
    & $git -C $repo pull
} else {
    $parent = Split-Path -Parent $repo
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    & $git clone $config.bandai.repoUrl $repo
}

# Bandai data.zip files may be Git-LFS-backed or may need direct raw download.
# Try Git LFS first if available, but do not fail only because LFS is unavailable.
try {
    & $git lfs version | Out-Null
    Write-Host "Git LFS is available. Pulling LFS files..."
    & $git -C $repo lfs install
    & $git -C $repo lfs pull
} catch {
    Write-Warning "Git LFS is not available or lfs pull failed. The extract step will try direct data.zip downloads."
}
