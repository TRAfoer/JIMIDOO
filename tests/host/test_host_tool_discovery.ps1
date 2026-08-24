$ErrorActionPreference = 'Stop'

$resolver = Join-Path $PSScriptRoot 'resolve_host_tool.ps1'
if (-not (Test-Path -LiteralPath $resolver -PathType Leaf)) {
    throw "host tool resolver is missing: $resolver"
}

$temporary = Join-Path ([IO.Path]::GetTempPath()) `
    ("jimidou-host-tools-" + [Guid]::NewGuid().ToString('N'))
$systemPath = [Environment]::GetFolderPath('System')
$originalPath = $env:PATH
New-Item -ItemType Directory -Path $temporary | Out-Null

function Assert-Equal([string]$actual, [string]$expected, [string]$message) {
    if ($actual -ne $expected) {
        throw "$message`: expected '$expected', got '$actual'"
    }
}

try {
    $pythonOnPath = Join-Path $temporary 'python.cmd'
    Set-Content -LiteralPath $pythonOnPath -Encoding ASCII -Value @(
        '@echo off'
        "echo $pythonOnPath"
    )
    $env:PATH = "$temporary;$systemPath"
    Assert-Equal (& $resolver -Tool Python) $pythonOnPath `
        'Python PATH discovery failed'

    Remove-Item -LiteralPath $pythonOnPath
    $launcherPython = Join-Path $temporary 'launcher-python.exe'
    New-Item -ItemType File -Path $launcherPython | Out-Null
    $launcher = Join-Path $temporary 'py.cmd'
    Set-Content -LiteralPath $launcher -Encoding ASCII -Value @(
        '@echo off'
        "echo $launcherPython"
    )
    Assert-Equal (& $resolver -Tool Python) $launcherPython `
        'Python launcher discovery failed'

    Remove-Item -LiteralPath $launcher
    $installation = Join-Path $temporary 'Visual Studio Test'
    $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
    New-Item -ItemType Directory -Path (Split-Path -Parent $vcvars) | Out-Null
    New-Item -ItemType File -Path $vcvars | Out-Null
    $vswhere = Join-Path $temporary 'vswhere.cmd'
    Set-Content -LiteralPath $vswhere -Encoding ASCII -Value @(
        '@echo off'
        "echo $installation"
    )
    Assert-Equal (& $resolver -Tool VcVars) $vcvars `
        'vswhere discovery failed'

    Remove-Item -LiteralPath $vswhere
    $compiler = Join-Path $temporary 'cl.cmd'
    Set-Content -LiteralPath $compiler -Encoding ASCII -Value '@exit /b 0'
    Assert-Equal (& $resolver -Tool VcVars) '__PATH__' `
        'compiler PATH discovery failed'

    $overridePython = Join-Path $temporary 'override-python.exe'
    New-Item -ItemType File -Path $overridePython | Out-Null
    Assert-Equal (& $resolver -Tool Python -Override $overridePython) `
        $overridePython 'Python override failed'
    Assert-Equal (& $resolver -Tool VcVars -Override $vcvars) `
        $vcvars 'vcvars override failed'
}
finally {
    $env:PATH = $originalPath
    Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'host tool discovery PASS'
