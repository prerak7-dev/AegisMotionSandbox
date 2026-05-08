function Get-AegisV45Root {
    # Resolve the repository/pipeline root from the scripts folder, independent of the
    # process working directory. This is used by both legacy V45/V47 jobs and V48.
    return (Split-Path -Parent $PSScriptRoot)
}

function Resolve-AegisV45Path {
    param([string]$Path)

    $Root = Get-AegisV45Root
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $Root
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return (Join-Path $Root $Path)
}

function Get-AegisV45ConfigPath {
    param([string]$ConfigPath = "")

    $Root = Get-AegisV45Root
    if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
        return (Join-Path $Root "config\aegis_bandai_v45.config.json")
    }

    if ([System.IO.Path]::IsPathRooted($ConfigPath)) {
        return $ConfigPath
    }

    return (Join-Path $Root $ConfigPath)
}

function Get-AegisV45Config {
    param([string]$ConfigPath = "")

    $Resolved = Get-AegisV45ConfigPath -ConfigPath $ConfigPath
    if (!(Test-Path $Resolved)) {
        throw "Config not found: $Resolved. Run scripts\00-create-config.ps1 first, or pass -ConfigPath config\aegis_v48_no_retarget.config.json for V48."
    }
    return Get-Content $Resolved -Raw | ConvertFrom-Json
}

function Set-AegisPythonPath {
    $Root = Get-AegisV45Root
    $env:PYTHONPATH = "$Root\motion-prior-service"
}
