param(
    [string]$VisualStudioPath = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$framePacerRepoDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$framePacerOutputDir = if ($OutputDirectory) {
    [System.IO.Path]::GetFullPath($OutputDirectory)
} else {
    Join-Path $framePacerRepoDir 'generated\frame_pacer_tests'
}
$framePacerCandidates = [System.Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $framePacerCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $framePacerCandidates.Add($env:VSINSTALLDIR) }
$framePacerVsWhereCommand = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$framePacerVsWhere = if ($framePacerVsWhereCommand) { $framePacerVsWhereCommand.Source } else {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
}
if (Test-Path -LiteralPath $framePacerVsWhere) {
    $framePacerFound = & $framePacerVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $framePacerFound) { $framePacerCandidates.Add(([string]$framePacerFound).Trim()) }
}
$framePacerDevCmd = $null
foreach ($framePacerCandidate in $framePacerCandidates) {
    $framePacerPossible = Join-Path $framePacerCandidate 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $framePacerPossible) { $framePacerDevCmd = $framePacerPossible; break }
}
if (!$framePacerDevCmd) {
    throw 'Visual Studio C++ tools were not found. Pass -VisualStudioPath with an existing C++ installation directory.'
}

New-Item -ItemType Directory -Path $framePacerOutputDir -Force | Out-Null
$framePacerBuildCmd = Join-Path $framePacerOutputDir 'build_frame_pacer_tests.cmd'
# Pass Unicode source paths through process environment variables.
$framePacerBatch = @'
@echo off
call "%FRAME_PACER_TEST_VS_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /Fe:frame_pacer_tests.exe /Fo:.\ "%FRAME_PACER_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
frame_pacer_tests.exe
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText($framePacerBuildCmd, $framePacerBatch, [System.Text.Encoding]::ASCII)
$framePacerEnvironment = @{
    FRAME_PACER_TEST_VS_DEV_CMD = $framePacerDevCmd
    FRAME_PACER_TEST_SOURCE = Join-Path $PSScriptRoot 'frame_pacer_tests.cpp'
}
$framePacerPreviousEnvironment = @{}
foreach ($framePacerName in $framePacerEnvironment.Keys) {
    $framePacerPreviousEnvironment[$framePacerName] = [Environment]::GetEnvironmentVariable($framePacerName, 'Process')
    [Environment]::SetEnvironmentVariable($framePacerName, $framePacerEnvironment[$framePacerName], 'Process')
}
try {
    Push-Location -LiteralPath $framePacerOutputDir
    try {
        & $env:ComSpec /d /c build_frame_pacer_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Frame pacer build/tests failed with exit code $LASTEXITCODE." }
    } finally { Pop-Location }
} finally {
    foreach ($framePacerName in $framePacerEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($framePacerName, $framePacerPreviousEnvironment[$framePacerName], 'Process')
    }
}
Write-Host "Frame pacer test suite passed. Executable: $framePacerOutputDir\frame_pacer_tests.exe"
