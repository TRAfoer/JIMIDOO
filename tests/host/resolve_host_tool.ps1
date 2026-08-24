[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Python', 'VcVars')]
    [string]$Tool,
    [string]$Override
)

$ErrorActionPreference = 'Stop'

function Resolve-ExistingFile([string]$value, [string]$description) {
    if (Test-Path -LiteralPath $value -PathType Leaf) {
        return (Resolve-Path -LiteralPath $value).Path
    }

    $command = Get-Command $value -CommandType Application, ExternalScript `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command -and $command.Source -and
        (Test-Path -LiteralPath $command.Source -PathType Leaf)) {
        return (Resolve-Path -LiteralPath $command.Source).Path
    }
    throw "$description does not exist: $value"
}

function Resolve-Python([string]$overridePath) {
    if ($overridePath) {
        return Resolve-ExistingFile $overridePath 'Python override'
    }

    foreach ($name in @('python', 'python3')) {
        $command = Get-Command $name -CommandType Application, ExternalScript `
            -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $command) {
            continue
        }
        try {
            $result = @(& $command.Source -c `
                'import sys; print(sys.executable)' 2>$null)
        }
        catch {
            $result = @()
        }
        if ($LASTEXITCODE -eq 0 -and $result.Count -gt 0) {
            $candidate = ([string]$result[-1]).Trim()
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }

    $launcher = Get-Command py -CommandType Application, ExternalScript `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $launcher) {
        $launcherCandidates = @(
            (Join-Path $env:LocalAppData 'Programs\Python\Launcher\py.exe'),
            (Join-Path $env:SystemRoot 'py.exe')
        )
        foreach ($candidate in $launcherCandidates) {
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                $launcher = Get-Item -LiteralPath $candidate
                break
            }
        }
    }
    if ($launcher) {
        $launcherPath = if ($launcher.Source) {
            $launcher.Source
        }
        else {
            $launcher.FullName
        }
        try {
            $result = @(& $launcherPath -3 -c `
                'import sys; print(sys.executable)' 2>$null)
        }
        catch {
            $result = @()
        }
        if ($LASTEXITCODE -eq 0 -and $result.Count -gt 0) {
            $candidate = ([string]$result[-1]).Trim()
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return (Resolve-Path -LiteralPath $candidate).Path
            }
        }
    }

    $installRoots = @(
        (Join-Path $env:LocalAppData 'Programs\Python'),
        $env:ProgramFiles
    )
    foreach ($root in $installRoots) {
        if (-not $root -or -not (Test-Path -LiteralPath $root -PathType Container)) {
            continue
        }
        $candidates = @(Get-ChildItem -Path (Join-Path $root 'Python*\python.exe') `
            -File -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)
        foreach ($candidate in $candidates) {
            try {
                $result = @(& $candidate.FullName -c `
                    'import sys; print(sys.executable)' 2>$null)
            }
            catch {
                $result = @()
            }
            if ($LASTEXITCODE -eq 0 -and $result.Count -gt 0) {
                $resolved = ([string]$result[-1]).Trim()
                if (Test-Path -LiteralPath $resolved -PathType Leaf) {
                    return (Resolve-Path -LiteralPath $resolved).Path
                }
            }
        }
    }

    throw 'Python 3 was not found on PATH, through the py launcher, or in a standard installation directory'
}

function Resolve-VcVars([string]$overridePath) {
    if ($overridePath) {
        return Resolve-ExistingFile $overridePath 'vcvars override'
    }

    $compiler = Get-Command cl -CommandType Application, ExternalScript `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($compiler) {
        return '__PATH__'
    }

    $vswhere = Get-Command vswhere -CommandType Application, ExternalScript `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $vswhere) {
        $installer = Join-Path ${env:ProgramFiles(x86)} `
            'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $installer -PathType Leaf) {
            $vswhere = Get-Item -LiteralPath $installer
        }
    }
    if (-not $vswhere) {
        throw 'cl was not found on PATH and vswhere is unavailable'
    }

    $vswherePath = if ($vswhere.Source) { $vswhere.Source } else { $vswhere.FullName }
    $installations = @(& $vswherePath -latest -products '*' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath)
    if ($LASTEXITCODE -ne 0 -or $installations.Count -eq 0) {
        throw 'vswhere did not find a Visual Studio C++ installation'
    }
    $installation = ([string]$installations[0]).Trim()
    $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path -LiteralPath $vcvars -PathType Leaf)) {
        throw "vcvars64.bat is missing from the Visual Studio installation: $vcvars"
    }
    return (Resolve-Path -LiteralPath $vcvars).Path
}

if ($Tool -eq 'Python') {
    Resolve-Python $Override
}
else {
    Resolve-VcVars $Override
}
