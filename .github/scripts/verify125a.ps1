# 125A P018 independent ZIP compatibility check.
# Exercises .NET's standard ZIP reader rather than our C++ archive loader.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
$archives = @(Get-ChildItem -Path 'p005-real-vst3-output' -Recurse -File |
    Where-Object { $_.Extension -ceq '.125A' })
if ($archives.Count -ne 1) {
    throw "Expected exactly one finished .125A, found $($archives.Count)"
}
$zip = [System.IO.Compression.ZipFile]::OpenRead($archives[0].FullName)
try {
    $manifestEntry = $zip.GetEntry('instrument.json')
    if (-not $manifestEntry) { throw '125A ZIP does not include instrument.json' }
    $reader = [System.IO.StreamReader]::new($manifestEntry.Open())
    try { $manifest = $reader.ReadToEnd() | ConvertFrom-Json }
    finally { $reader.Dispose() }
    if ($manifest.format -cne '125A' -or $manifest.version -ne 1 -or
        $manifest.name -cne 'Untitled Instrument' -or
        $manifest.sampleCount -ne 8) {
        throw '125A manifest rejected by independent ZIP/JSON reader'
    }
    if (@($manifest.playbackNormalized).Count -ne 7) {
        throw '125A did not preserve playback parameters'
    }
    $wavs = @($zip.Entries | Where-Object { $_.Name -match '^N[0-9]+_V[0-9]+_RR[0-9]+\.wav$' })
    if ($wavs.Count -ne 8 -or $zip.Entries.Count -ne 9) {
        throw '125A archive did not retain 8 individual WAV files'
    }
    $outPath = Join-Path $env:RUNNER_TEMP '125a-emulatorIII-extracted-sample.wav'
    if (Test-Path $outPath) { Remove-Item $outPath -Force }
    $inputStream = $wavs[0].Open()
    $outputStream = [System.IO.File]::Create($outPath)
    try { $inputStream.CopyTo($outputStream) }
    finally { $inputStream.Dispose(); $outputStream.Dispose() }
    $header = [System.IO.File]::ReadAllBytes($outPath)
    if ($header.Length -le 44 -or
        [System.Text.Encoding]::ASCII.GetString($header,0,4) -cne 'RIFF' -or
        [System.Text.Encoding]::ASCII.GetString($header,8,4) -cne 'WAVE' -or
        $header[22] -ne 2 -or $header[34] -ne 24) {
        throw 'WAV extracted with standard .NET ZIP reader is invalid PCM24 stereo'
    }
    Write-Host "[PASS] P018 STANDARD ZIP COMPATIBILITY: $($wavs.Count) WAVs, manifest, individual PCM24 extraction"
}
finally { $zip.Dispose() }
