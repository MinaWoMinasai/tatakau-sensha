param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'

$inkAudioRepoDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$inkAudioOutputDir = Join-Path $inkAudioRepoDir 'generated\ink_audio_tests'
$inkAudioCandidates = [System.Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $inkAudioCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $inkAudioCandidates.Add($env:VSINSTALLDIR) }
$inkAudioVsWhere = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$inkAudioVsWherePath = if ($inkAudioVsWhere) { $inkAudioVsWhere.Source } else {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
}
if (Test-Path -LiteralPath $inkAudioVsWherePath) {
    $inkAudioFound = & $inkAudioVsWherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $inkAudioFound) { $inkAudioCandidates.Add(([string]$inkAudioFound).Trim()) }
}
$inkAudioDevCmd = $null
foreach ($inkAudioCandidate in $inkAudioCandidates) {
    $inkAudioPossible = Join-Path $inkAudioCandidate 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $inkAudioPossible) { $inkAudioDevCmd = $inkAudioPossible; break }
}
if (!$inkAudioDevCmd) {
    throw 'An existing Visual Studio C++ installation is required. Pass -VisualStudioPath if automatic discovery fails.'
}

New-Item -ItemType Directory -Path $inkAudioOutputDir -Force | Out-Null
$inkAudioBatchPath = Join-Path $inkAudioOutputDir 'build_ink_audio_tests.cmd'
$inkAudioBatch = @'
@echo off
call "%INK_AUDIO_TEST_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%INK_AUDIO_TEST_OUTPUT%"
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /Fe:ink_audio_runtime_tests.exe /Fo:.\ "%INK_AUDIO_TEST_SOURCE%" /link ole32.lib
if errorlevel 1 exit /b %errorlevel%
ink_audio_runtime_tests.exe "%INK_AUDIO_TEST_MP3%"
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText($inkAudioBatchPath, $inkAudioBatch, [System.Text.Encoding]::ASCII)
$inkAudioEnvironment = @{
    INK_AUDIO_TEST_VS = $inkAudioDevCmd
    INK_AUDIO_TEST_OUTPUT = $inkAudioOutputDir
    INK_AUDIO_TEST_SOURCE = Join-Path $PSScriptRoot 'ink_audio_runtime_tests.cpp'
    INK_AUDIO_TEST_MP3 = Join-Path $inkAudioRepoDir 'project\resources\bulletShoot.mp3'
}
$inkAudioPreviousEnvironment = @{}
foreach ($inkAudioName in $inkAudioEnvironment.Keys) {
    $inkAudioPreviousEnvironment[$inkAudioName] = [Environment]::GetEnvironmentVariable($inkAudioName, 'Process')
    [Environment]::SetEnvironmentVariable($inkAudioName, $inkAudioEnvironment[$inkAudioName], 'Process')
}
try {
    & $inkAudioBatchPath
    if ($LASTEXITCODE -ne 0) { throw "Audio runtime checks failed with exit code $LASTEXITCODE." }
} finally {
    foreach ($inkAudioName in $inkAudioPreviousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($inkAudioName, $inkAudioPreviousEnvironment[$inkAudioName], 'Process')
    }
}
