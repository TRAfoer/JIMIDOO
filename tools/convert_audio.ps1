param(
    [string]$SourceDirectory = $env:AUDIO_SOURCE_DIR
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($SourceDirectory)) {
    $SourceDirectory = Join-Path $repoRoot 'Audios'
}
if (-not (Test-Path -LiteralPath $SourceDirectory -PathType Container)) {
    throw "audio source directory not found: $SourceDirectory"
}
$SourceDirectory = (Resolve-Path -LiteralPath $SourceDirectory).Path

function Resolve-Ffmpeg {
    if ($env:FFMPEG) {
        if (-not (Test-Path -LiteralPath $env:FFMPEG -PathType Leaf)) {
            throw "FFMPEG does not point to a file: $($env:FFMPEG)"
        }
        return (Resolve-Path -LiteralPath $env:FFMPEG).Path
    }

    $known = 'C:\Desktop\格式工厂_v5.15.0_x64\格式工厂_v5.15.0.0_x64\ffmpeg.exe'
    if (Test-Path -LiteralPath $known -PathType Leaf) {
        return $known
    }

    $command = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    throw 'ffmpeg not found; set FFMPEG to the executable path'
}

function Convert-AudioFile {
    param(
        [Parameter(Mandatory = $true)][string]$InputPath,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][int]$SampleRate,
        [Parameter(Mandatory = $true)][string]$Codec,
        [switch]$MaxmodPcmU8
    )

    if (-not (Test-Path -LiteralPath $InputPath -PathType Leaf)) {
        throw "audio source file not found: $InputPath"
    }

    $temporary = "$OutputPath.part.wav"
    try {
        $ffmpegArguments = @(
            '-nostdin', '-hide_banner', '-loglevel', 'error', '-y',
            '-i', $InputPath, '-vn', '-ar', $SampleRate, '-ac', '1',
            '-c:a', $Codec
        )
        $ffmpegArguments += $temporary
        & $script:ffmpeg @ffmpegArguments
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $temporary -PathType Leaf)) {
            throw "ffmpeg conversion failed: $InputPath"
        }
        if ($MaxmodPcmU8) {
            Repair-OddPcmU8DataSize $temporary
        }
        Move-Item -LiteralPath $temporary -Destination $OutputPath -Force
    }
    finally {
        if (Test-Path -LiteralPath $temporary -PathType Leaf) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
}

function Repair-OddPcmU8DataSize([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 44 -or
        [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4) -ne 'RIFF' -or
        [System.Text.Encoding]::ASCII.GetString($bytes, 8, 4) -ne 'WAVE') {
        throw "invalid generated WAV: $Path"
    }

    $offset = 12
    while ($offset + 8 -le $bytes.Length) {
        $chunkId = [System.Text.Encoding]::ASCII.GetString($bytes, $offset, 4)
        $chunkSize = [BitConverter]::ToUInt32($bytes, $offset + 4)
        if ($chunkId -eq 'data') {
            if (($chunkSize % 2) -eq 0) {
                return
            }
            $padOffset = $offset + 8 + $chunkSize
            if ($padOffset -ge $bytes.Length) {
                throw "odd WAV data chunk has no RIFF pad byte: $Path"
            }
            $bytes[$padOffset] = 128
            [BitConverter]::GetBytes([uint32]($chunkSize + 1)).CopyTo($bytes, $offset + 4)
            [System.IO.File]::WriteAllBytes($Path, $bytes)
            return
        }
        $offset += 8 + $chunkSize + ($chunkSize % 2)
    }

    throw "generated WAV has no data chunk: $Path"
}

$manifestPath = Join-Path $PSScriptRoot 'audio_manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$sfxOutput = Join-Path $repoRoot 'assets/audio/sfx'
$musicOutput = Join-Path $repoRoot 'nitrofs/audio'
New-Item -ItemType Directory -Path $sfxOutput -Force | Out-Null
New-Item -ItemType Directory -Path $musicOutput -Force | Out-Null

$script:ffmpeg = Resolve-Ffmpeg
foreach ($entry in $manifest.sfx) {
    Convert-AudioFile `
        -InputPath (Join-Path $SourceDirectory $entry.source) `
        -OutputPath (Join-Path $sfxOutput $entry.output) `
        -SampleRate 16000 `
        -Codec 'pcm_u8' `
        -MaxmodPcmU8
}
foreach ($entry in $manifest.music) {
    Convert-AudioFile `
        -InputPath (Join-Path $SourceDirectory $entry.source) `
        -OutputPath (Join-Path $musicOutput $entry.output) `
        -SampleRate 22050 `
        -Codec 'pcm_s16le'
}

Write-Host "converted $($manifest.sfx.Count) SFX and $($manifest.music.Count) music tracks"
