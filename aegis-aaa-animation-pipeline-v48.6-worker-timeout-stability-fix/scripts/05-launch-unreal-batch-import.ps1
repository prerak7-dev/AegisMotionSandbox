param([string]$ConfigPath = "")

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\v45_common.ps1"
$Root = Get-AegisV45Root
$configPathResolved = Get-AegisV45ConfigPath $ConfigPath
$config = Get-AegisV45Config $ConfigPath

Write-Host "AEGIS_PROGRESS|step=IMPORT_FBX_TO_UNREAL|progress=42|message=Launching Unreal FBX import"

$scriptPath = Join-Path $Root "unreal\Scripts\Editor\aegis_v45_batch_import_fbx.py"
if (!(Test-Path $scriptPath)) {
    throw "Pipeline-local Unreal import script not found: $scriptPath"
}

$scriptText = Get-Content $scriptPath -Raw
if ($scriptText -notmatch "V46\.35") {
    throw "The Unreal import script is not V46.35. Replace unreal\Scripts\Editor\aegis_v45_batch_import_fbx.py with the V46.35 file."
}

$fbxDir = $config.bandai.fbxOutputDir
$fbxCount = 0
if (Test-Path $fbxDir) {
    $fbxCount = (Get-ChildItem -Path $fbxDir -Recurse -Filter "*.fbx" -ErrorAction SilentlyContinue).Count
}
if ($fbxCount -eq 0) {
    throw "No FBX files found in $fbxDir. Run 04-convert-bandai-bvh-to-fbx.ps1 first."
}

$projectDir = Split-Path -Parent $config.unreal.project
$savedDir = Join-Path $projectDir "Saved"
New-Item -ItemType Directory -Force -Path $savedDir | Out-Null

$reportPath = Join-Path $savedDir "AegisV46ImportReport.json"
$consoleLog = Join-Path $savedDir "AegisV46Import_UnrealConsole.log"
$preflight = Join-Path $savedDir "AegisV46Import_Preflight.txt"

if (Test-Path $reportPath) { Remove-Item $reportPath -Force }
if (Test-Path $consoleLog) { Remove-Item $consoleLog -Force }

@"
V46.35 preflight created by PowerShell before Unreal launch.
Time: $(Get-Date -Format o)
Project: $($config.unreal.project)
PipelineRoot: $Root
ScriptPath: $scriptPath
FBXDir: $fbxDir
FBXCount: $fbxCount
ReportPath: $reportPath
Mode: Full UnrealEditor.exe with -ExecutePythonScript
Branch: raw Bandai source skeleton, no neutral bake.
"@ | Set-Content -Path $preflight -Encoding UTF8

$env:AEGIS_V45_CONFIG = $configPathResolved
$env:AEGIS_IMPORT_REPORT_PATH = $reportPath

$unrealExe = $config.tools.unrealEditorExe
if (!(Test-Path $unrealExe)) {
    throw "UnrealEditor.exe not found: $unrealExe"
}

Write-Host "Launching full Unreal Editor Python import."
Write-Host "Unreal exe: $unrealExe"
Write-Host "Project: $($config.unreal.project)"
Write-Host "FBX dir: $fbxDir"
Write-Host "FBX count: $fbxCount"
Write-Host "Base destination: $($config.unreal.importPath)"
Write-Host "Animation destination: $($config.unreal.importPath)/Animations"
Write-Host "Import script: $scriptPath"
Write-Host "Report path: $reportPath"
Write-Host "Console log: $consoleLog"

$nativeErrorActionPreference = $ErrorActionPreference
$unrealInvocationFailed = $false
$unrealInvocationException = $null
$exitCode = $null

try {
    $ErrorActionPreference = "Continue"

    try {
        & $unrealExe `
          $config.unreal.project `
          "-ExecutePythonScript=$scriptPath" `
          -nop4 `
          -NoSplash `
          -stdout `
          -FullStdOutLogOutput `
          -UTF8Output `
          2>&1 | Tee-Object -FilePath $consoleLog

        $exitCode = $LASTEXITCODE
    } catch {
        $unrealInvocationFailed = $true
        $unrealInvocationException = $_
        $exitCode = $LASTEXITCODE
        Write-Warning "Unreal invocation raised a PowerShell exception. Will validate report before failing. Exception: $($_.Exception.Message)"
    }
} finally {
    $ErrorActionPreference = $nativeErrorActionPreference
    Remove-Item Env:\AEGIS_V45_CONFIG -ErrorAction SilentlyContinue
    Remove-Item Env:\AEGIS_IMPORT_REPORT_PATH -ErrorAction SilentlyContinue
}

if ($null -eq $exitCode) {
    Write-Warning "Unreal returned a null LASTEXITCODE. Continuing to validate report/assets."
    $exitCode = 0
}

if (!(Test-Path $reportPath)) {
    if (Test-Path $consoleLog) {
        Write-Host "---- Last Unreal console log lines ----"
        Get-Content $consoleLog -Tail 160
        Write-Host "---------------------------------------"
    }
    if ($unrealInvocationFailed -and $unrealInvocationException) {
        throw "Unreal did not write report: $reportPath. Invocation exception: $($unrealInvocationException.Exception.Message). Check $consoleLog and $preflight."
    }
    throw "Unreal did not write report: $reportPath. Check $consoleLog and $preflight."
}

$report = Get-Content $reportPath -Raw | ConvertFrom-Json
Write-Host "Import report version: $($report.version)"
Write-Host "Import report phase: $($report.phase)"
Write-Host "Import report FBX count: $($report.fbxCount)"
if ($report.animationFbxCount) {
    Write-Host "Import report animation FBX count: $($report.animationFbxCount)"
}

$importedCount = 0
if ($report.importedAnimSequencePaths) {
    $importedCount = $report.importedAnimSequencePaths.Count
    Write-Host "Import report imported AnimSequences: $importedCount"
} elseif ($report.importedObjectPaths) {
    $importedCount = $report.importedObjectPaths.Count
    Write-Host "Import report imported object paths: $importedCount"
}

$errorsCount = 0
if ($report.errors) {
    $errorsCount = $report.errors.Count
}

if ($report.phase -eq "complete" -and $importedCount -gt 0 -and $errorsCount -eq 0) {
    if ($exitCode -ne 0 -or $unrealInvocationFailed) {
        Write-Warning "Unreal/PowerShell reported exit issue, but the Aegis import report is complete with $importedCount imported assets. Treating import as successful."
    }

    Write-Host "AEGIS_PROGRESS|step=IMPORT_FBX_TO_UNREAL|progress=50|message=Unreal FBX import complete. Imported paths: $($importedCount)"
    Write-Host "Unreal import command completed with imported assets."
    exit 0
}

if ($importedCount -eq 0) {
    if (Test-Path $consoleLog) {
        Write-Host "---- Last Unreal console log lines ----"
        Get-Content $consoleLog -Tail 160
        Write-Host "---------------------------------------"
    }
    throw "Unreal import report has 0 imported paths. Open $reportPath and $consoleLog."
}

if ($errorsCount -gt 0) {
    throw "Unreal import report contains $errorsCount error(s). Open $reportPath."
}

if ($exitCode -ne 0) {
    throw "Unreal returned exit code $exitCode and report was not complete."
}

Write-Host "AEGIS_PROGRESS|step=IMPORT_FBX_TO_UNREAL|progress=50|message=Unreal FBX import complete. Imported paths: $($importedCount)"
Write-Host "Unreal import command completed with imported assets."
