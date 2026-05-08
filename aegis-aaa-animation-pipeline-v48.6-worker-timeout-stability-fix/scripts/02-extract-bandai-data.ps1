param([string]$ConfigPath = "")

$ErrorActionPreference = "Stop"
Write-Host "AEGIS_PROGRESS|step=EXTRACT_BANDAI_DATA|progress=12|message=Validating Bandai data folders"
. "$PSScriptRoot\v45_common.ps1"
$config = Get-AegisV45Config $ConfigPath

$repo = $config.bandai.repoDir
$extractRoot = $config.bandai.rawExtractDir
New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null

Write-Host "Checking Bandai repository data folders..."

$dataDirs = @()
foreach ($datasetName in $config.bandai.datasetFolders) {
    $candidate = Join-Path $repo "dataset\$datasetName\data"
    if (Test-Path $candidate) {
        $dataDirs += $candidate
        Write-Host "Found data folder: $candidate"
    } else {
        Write-Warning "Data folder not found: $candidate"
    }
}

# Fallback: discover any Bandai dataset data folders in the repo.
if ($dataDirs.Count -eq 0) {
    $dataDirs = Get-ChildItem -Path (Join-Path $repo "dataset") -Directory -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "data" } |
        Select-Object -ExpandProperty FullName
}

if ($dataDirs.Count -eq 0) {
    throw "No Bandai data folders found. Expected paths like dataset\Bandai-Namco-Research-Motiondataset-1\data and dataset\Bandai-Namco-Research-Motiondataset-2\data."
}

$totalBvh = 0
$totalJson = 0
foreach ($dir in $dataDirs) {
    $bvhCount = (Get-ChildItem -Path $dir -Recurse -Include "*.bvh","*.BVH" -ErrorAction SilentlyContinue).Count
    $jsonCount = (Get-ChildItem -Path $dir -Recurse -Include "*.json","*.JSON" -ErrorAction SilentlyContinue).Count
    $totalBvh += $bvhCount
    $totalJson += $jsonCount
    Write-Host "Data folder: $dir"
    Write-Host "  BVH files:  $bvhCount"
    Write-Host "  JSON files: $jsonCount"
}

if ($totalBvh -eq 0) {
    throw "Bandai data folders exist, but no BVH files were found. Check that the GitHub repository cloned all files correctly."
}

Write-Host "Bandai data is ready. No data.zip extraction is required for the current repository layout."
Write-Host "Total BVH files found: $totalBvh"
Write-Host "Total JSON files found: $totalJson"
