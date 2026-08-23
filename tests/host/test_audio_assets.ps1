$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$sfxDir = Join-Path $repoRoot 'assets/audio/sfx'
$musicDir = Join-Path $repoRoot 'nitrofs/audio'
$manifest = Get-Content -LiteralPath (Join-Path $repoRoot 'tools/audio_manifest.json') `
    -Raw -Encoding UTF8 | ConvertFrom-Json

$expectedSfx = @(
    'start.wav',
    'yowl.wav',
    'scratch_1.wav',
    'scratch_2.wav',
    'heal.wav',
    'death.wav',
    'maodie_combined.wav',
    'banana_attack.wav',
    'banana_heal.wav',
    'banana_death.wav'
)

$sfx = @(Get-ChildItem -LiteralPath $sfxDir -Filter '*.wav' -File -ErrorAction SilentlyContinue)
if ($sfx.Count -ne 10) {
    throw "expected 10 converted SFX, found $($sfx.Count)"
}

$actualSfx = @($sfx.Name | Sort-Object)
$wantedSfx = @($expectedSfx | Sort-Object)
if (Compare-Object $wantedSfx $actualSfx) {
    throw "converted SFX filenames do not match the manifest contract"
}

$logicalNames = @($manifest.logical_sfx.PSObject.Properties.Name | Sort-Object)
$expectedLogicalNames = @(
    'banana_attack', 'banana_death', 'banana_heal', 'death', 'heal',
    'maodie_combined', 'normal_hiss', 'scratch_1', 'scratch_2', 'start', 'yowl'
) | Sort-Object
if ($logicalNames.Count -ne 11 -or (Compare-Object $expectedLogicalNames $logicalNames)) {
    throw 'expected 11 logical SFX manifest routes'
}
if ($manifest.logical_sfx.normal_hiss -ne 'yowl') {
    throw 'normal hiss must alias the yowl bank sample'
}

$music = @(Get-ChildItem -LiteralPath $musicDir -Filter '*.wav' -File -ErrorAction SilentlyContinue)
if ($music.Count -ne 2 -or @($music.Name | Sort-Object) -join ',' -ne 'battle.wav,menu.wav') {
    throw "expected menu.wav and battle.wav BGM outputs"
}

function Resolve-Ffprobe {
    if ($env:FFPROBE) {
        return $env:FFPROBE
    }

    if ($env:FFMPEG) {
        $sibling = Join-Path (Split-Path -Parent $env:FFMPEG) 'ffprobe.exe'
        if (Test-Path -LiteralPath $sibling -PathType Leaf) {
            return $sibling
        }
    }

    $known = 'C:\Desktop\格式工厂_v5.15.0_x64\格式工厂_v5.15.0.0_x64\ffprobe.exe'
    if (Test-Path -LiteralPath $known -PathType Leaf) {
        return $known
    }

    $command = Get-Command ffprobe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    return $null
}

function Read-WavFormat([string]$path) {
    $bytes = [System.IO.File]::ReadAllBytes($path)
    if ($bytes.Length -lt 44 -or
        [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4) -ne 'RIFF' -or
        [System.Text.Encoding]::ASCII.GetString($bytes, 8, 4) -ne 'WAVE') {
        throw "invalid RIFF/WAVE file: $path"
    }

    $offset = 12
    while ($offset + 8 -le $bytes.Length) {
        $chunkId = [System.Text.Encoding]::ASCII.GetString($bytes, $offset, 4)
        $chunkSize = [BitConverter]::ToUInt32($bytes, $offset + 4)
        if ($chunkId -eq 'fmt ') {
            if ($chunkSize -lt 16 -or $offset + 8 + $chunkSize -gt $bytes.Length) {
                throw "invalid WAV fmt chunk: $path"
            }
            $format = [BitConverter]::ToUInt16($bytes, $offset + 8)
            $channels = [BitConverter]::ToUInt16($bytes, $offset + 10)
            $sampleRate = [BitConverter]::ToUInt32($bytes, $offset + 12)
            $bits = [BitConverter]::ToUInt16($bytes, $offset + 22)
            if ($format -ne 1) {
                throw "expected PCM WAV format for $path, got $format"
            }
            return "$sampleRate,$channels,$bits"
        }
        $offset += 8 + $chunkSize + ($chunkSize % 2)
    }

    throw "WAV fmt chunk not found: $path"
}

function Read-WavDataSize([string]$path) {
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $offset = 12
    while ($offset + 8 -le $bytes.Length) {
        $chunkId = [System.Text.Encoding]::ASCII.GetString($bytes, $offset, 4)
        $chunkSize = [BitConverter]::ToUInt32($bytes, $offset + 4)
        if ($chunkId -eq 'data') {
            return $chunkSize
        }
        $offset += 8 + $chunkSize + ($chunkSize % 2)
    }
    throw "WAV data chunk not found: $path"
}

function Assert-AudioFormat([string]$path, [string]$expected) {
    if ($script:ffprobe) {
        $actual = (& $script:ffprobe -v error -select_streams a:0 `
            -show_entries stream=sample_rate,channels,bits_per_sample `
            -of 'csv=p=0' $path).Trim()
        if ($LASTEXITCODE -ne 0) {
            throw "ffprobe failed for $path"
        }
    }
    else {
        $actual = Read-WavFormat $path
    }
    if ($actual -ne $expected) {
        throw "unexpected format for $path`: expected $expected, got $actual"
    }
    Write-Host "$([System.IO.Path]::GetFileName($path)): $actual"
}

$script:ffprobe = Resolve-Ffprobe
foreach ($path in $sfx.FullName) {
    Assert-AudioFormat $path '16000,1,8'
    if ((Read-WavDataSize $path) % 2 -ne 0) {
        throw "Maxmod requires an even SFX data size: $path"
    }
}
foreach ($path in $music.FullName) {
    Assert-AudioFormat $path '22050,1,16'
}

Write-Host 'audio asset formats PASS'
