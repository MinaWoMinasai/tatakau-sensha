param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'

$audioRuntimeRepoDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$audioRuntimeOutputDir = Join-Path $audioRuntimeRepoDir 'generated\audio_runtime_tests'
$audioRuntimeCandidates = [System.Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $audioRuntimeCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $audioRuntimeCandidates.Add($env:VSINSTALLDIR) }
$audioRuntimeVsWhere = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$audioRuntimeVsWherePath = if ($audioRuntimeVsWhere) { $audioRuntimeVsWhere.Source } else {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
}
if (Test-Path -LiteralPath $audioRuntimeVsWherePath) {
    $audioRuntimeFound = & $audioRuntimeVsWherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $audioRuntimeFound) { $audioRuntimeCandidates.Add(([string]$audioRuntimeFound).Trim()) }
}
$audioRuntimeDevCmd = $null
foreach ($audioRuntimeCandidate in $audioRuntimeCandidates) {
    $audioRuntimePossible = Join-Path $audioRuntimeCandidate 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $audioRuntimePossible) { $audioRuntimeDevCmd = $audioRuntimePossible; break }
}
if (!$audioRuntimeDevCmd) {
    throw 'An existing Visual Studio C++ installation is required. Pass -VisualStudioPath if automatic discovery fails.'
}

New-Item -ItemType Directory -Path $audioRuntimeOutputDir -Force | Out-Null
$audioRuntimeBatchPath = Join-Path $audioRuntimeOutputDir 'build_audio_runtime_tests.cmd'
$audioRuntimeBatch = @'
@echo off
call "%AUDIO_RUNTIME_TEST_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
pushd "%AUDIO_RUNTIME_TEST_OUTPUT%"
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /Fe:audio_runtime_tests.exe /Fo:.\ "%AUDIO_RUNTIME_TEST_SOURCE%" /link ole32.lib
if errorlevel 1 exit /b %errorlevel%
audio_runtime_tests.exe "%AUDIO_RUNTIME_TEST_MP3%"
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText($audioRuntimeBatchPath, $audioRuntimeBatch, [System.Text.Encoding]::ASCII)
$audioRuntimeEnvironment = @{
    AUDIO_RUNTIME_TEST_VS = $audioRuntimeDevCmd
    AUDIO_RUNTIME_TEST_OUTPUT = $audioRuntimeOutputDir
    AUDIO_RUNTIME_TEST_SOURCE = Join-Path $PSScriptRoot 'audio_runtime_tests.cpp'
    AUDIO_RUNTIME_TEST_MP3 = Join-Path $audioRuntimeRepoDir 'project\resources\bulletShoot.mp3'
}
$audioRuntimePreviousEnvironment = @{}
foreach ($audioRuntimeName in $audioRuntimeEnvironment.Keys) {
    $audioRuntimePreviousEnvironment[$audioRuntimeName] = [Environment]::GetEnvironmentVariable($audioRuntimeName, 'Process')
    [Environment]::SetEnvironmentVariable($audioRuntimeName, $audioRuntimeEnvironment[$audioRuntimeName], 'Process')
}
try {
    & $audioRuntimeBatchPath
    if ($LASTEXITCODE -ne 0) { throw "Audio runtime checks failed with exit code $LASTEXITCODE." }
} finally {
    foreach ($audioRuntimeName in $audioRuntimePreviousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($audioRuntimeName, $audioRuntimePreviousEnvironment[$audioRuntimeName], 'Process')
    }
}
