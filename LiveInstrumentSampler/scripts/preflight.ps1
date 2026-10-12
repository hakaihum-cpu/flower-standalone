$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
Set-Location $repoRoot

Write-Host "AN-72 Live Instrument Sampler preflight"
Write-Host "HEAD: $(git rev-parse HEAD)"

git fetch origin main --no-tags --quiet
$changed = @(git diff --name-only origin/main...HEAD)

$allowedExact = @(
    ".github/workflows/build-live-instrument-sampler-windows.yml",
    ".github/live-instrument-sampler-build-trigger.txt"
)

$bad = @(
    $changed | Where-Object {
        ($_ -notlike "LiveInstrumentSampler/*") -and ($allowedExact -notcontains $_)
    }
)

if ($bad.Count -gt 0) {
    throw "Unexpected files outside AN-72 scope: $($bad -join ', ')"
}

$requiredFiles = @(
    "LiveInstrumentSampler/CMakeLists.txt",
    "LiveInstrumentSampler/App/Main.cpp",
    "LiveInstrumentSampler/App/MainComponent.h",
    "LiveInstrumentSampler/App/MainComponentUi.cpp",
    "LiveInstrumentSampler/App/MainComponentAudio.cpp",
    "LiveInstrumentSampler/App/MainComponentSession.cpp",
    "LiveInstrumentSampler/App/MainComponentFinish.cpp",
    "LiveInstrumentSampler/App/AudioEngine.h",
    "LiveInstrumentSampler/App/AudioEngine.cpp",
    "LiveInstrumentSampler/App/WaveformTrimComponent.h",
    "LiveInstrumentSampler/App/WaveformTrimComponent.cpp",
    "LiveInstrumentSampler/Core/WavIO.h",
    "LiveInstrumentSampler/Core/WavIO.cpp",
    "LiveInstrumentSampler/Core/SamplerCore.h",
    "LiveInstrumentSampler/Core/SamplerCore.cpp",
    "LiveInstrumentSampler/tests/test_core.cpp",
    ".github/workflows/build-live-instrument-sampler-windows.yml"
)
foreach ($f in $requiredFiles) {
    if (-not (Test-Path $f)) { throw "Missing required file: $f" }
}

$cmake = Get-Content -Raw "LiveInstrumentSampler/CMakeLists.txt"
$audioH = Get-Content -Raw "LiveInstrumentSampler/App/AudioEngine.h"
$audio = Get-Content -Raw "LiveInstrumentSampler/App/AudioEngine.cpp"
$ui = Get-Content -Raw "LiveInstrumentSampler/App/MainComponentUi.cpp"
$audioUi = Get-Content -Raw "LiveInstrumentSampler/App/MainComponentAudio.cpp"
$session = Get-Content -Raw "LiveInstrumentSampler/App/MainComponentSession.cpp"
$finish = Get-Content -Raw "LiveInstrumentSampler/App/MainComponentFinish.cpp"
$core = Get-Content -Raw "LiveInstrumentSampler/Core/SamplerCore.cpp"
$tests = Get-Content -Raw "LiveInstrumentSampler/tests/test_core.cpp"
$workflow = Get-Content -Raw ".github/workflows/build-live-instrument-sampler-windows.yml"

if ($cmake -notmatch 'LANGUAGES\s+C\s+CXX') {
    throw "CMake must enable both C and CXX for JUCE."
}
foreach ($required in @(
    'JUCE_ASIO=1',
    'JUCE_ASIO_USE_EXTERNAL_SDK=0',
    'JUCE_WASAPI=1',
    'LiveInstrumentSamplerCoreTests',
    'EP_Live_Instrument_Sampler'
)) {
    if ($cmake -notmatch [regex]::Escape($required)) {
        throw "CMake missing required setting: $required"
    }
}

if ($audio -match 'createWriterFor\s*\(\s*stream\.release\s*\(') {
    throw "Deprecated raw AudioFormat writer API must not be used."
}
if ($audio -notmatch 'AudioFormatWriterOptions') {
    throw "JUCE 9 AudioFormatWriterOptions path is missing."
}
if ($audio -notmatch 'manager_|deviceManager_') {
    throw "AudioDeviceManager use is missing."
}
foreach ($required in @(
    'initialise(2, 2',
    'sampleRate = 48000.0',
    'getInputLevelGetter',
    'playTestSound',
    'getXRunCount',
    'ThreadedWriter',
    'writerDropouts_',
    'writerLock_',
    'activeWriter_->write'
)) {
    if (($audio + $audioH) -notmatch [regex]::Escape($required)) {
        throw "Audio safety path missing: $required"
    }
}
if ($audio -notmatch '!activeWriter_->write\s*\(') {
    throw "ThreadedWriter FIFO failure must be checked."
}

if ($ui -match 'for\s*\(\s*auto\s*\*\s*\w+\s*:\s*\{') {
    throw "Heterogeneous pointer initializer-list pattern is forbidden; it failed MSVC in Run #1."
}

foreach ($required in @(
    'ASIO',
    'MONO ',
    'STEREO ',
    '48000',
    'applyDeviceSetup',
    'signal detected',
    'user confirmed audible',
    'reopenCurrentDevice'
)) {
    if ($audioUi -notmatch [regex]::Escape($required)) {
        throw "ASIO/device verification flow incomplete: $required"
    }
}

foreach ($required in @(
    'RAW/RETAKES',
    'recordingWriteDropouts',
    'recordingXRuns',
    'RETAKE',
    'confirmTake',
    'trimStartFrame',
    'trimEndFrame'
)) {
    if ($session -notmatch [regex]::Escape($required)) {
        throw "Recording/session safety flow incomplete: $required"
    }
}

foreach ($required in @(
    'buildEpBankFromTakes',
    'exportMasterWav',
    'parseEpMapJson',
    'buildEpBankFromMaster'
)) {
    if (($finish + $core) -notmatch [regex]::Escape($required)) {
        throw "BIN/master workflow incomplete: $required"
    }
}

foreach ($required in @(
    '"EPBANK1\0"',
    'kVelocities',
    'kRoundRobins',
    '48000',
    'bitsPerSample = 24'
)) {
    if ($core -notmatch [regex]::Escape($required)) {
        throw "EPBANK1 compatibility condition missing: $required"
    }
}

if ($tests -notmatch 'bin\.close\s*\(\s*\)') {
    throw "Windows test must close direct.bin before cleanup."
}
if ($tests -notmatch 'C2' -and $tests -notmatch 'startNote\s*=\s*36') {
    throw "C2-start coverage is missing."
}
if ($tests -notmatch 'keyCount\s*=\s*61') {
    throw "61-key coverage is missing."
}
if ($tests -notmatch 'short tuned master must fail') {
    throw "Short tuned-master rejection test is missing."
}

foreach ($required in @(
    'runs-on: windows-latest',
    'JUCE 9.0.3',
    'LiveInstrumentSamplerCoreTests',
    'ctest',
    'EP_Live_Instrument_Sampler',
    '0x4D',
    '0x5A',
    'upload-artifact@v4'
)) {
    if ($workflow -notmatch [regex]::Escape($required)) {
        throw "Windows workflow missing required guard: $required"
    }
}
if ($workflow -match '"LiveInstrumentSampler/\*\*"') {
    throw "Source pushes must not trigger expensive Windows builds."
}
if ($workflow -notmatch 'live-instrument-sampler-build-trigger\.txt') {
    throw "Windows build must stay gated by the explicit trigger file."
}


# UI/source encoding guard: App source must stay ASCII-only to avoid mojibake
# on Windows builds and systems with different active code pages.
$appSourceFiles = Get-ChildItem "LiveInstrumentSampler/App" -File -Recurse |
    Where-Object { $_.Extension -in @(".cpp", ".h") }

foreach ($file in $appSourceFiles) {
    $bytes = [IO.File]::ReadAllBytes($file.FullName)
    foreach ($b in $bytes) {
        if ($b -gt 0x7F) {
            throw "Non-ASCII byte found in App source: $($file.FullName)"
        }
    }
}

Write-Host "Changed files:"
$changed | ForEach-Object { Write-Host "  $_" }
Write-Host "AN-72 preflight PASS"
