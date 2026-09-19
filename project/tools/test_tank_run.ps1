param(
    [string]$VisualStudioPath = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'

$tankRunRepoDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$tankRunOutputDir = if ($OutputDirectory) {
    [System.IO.Path]::GetFullPath($OutputDirectory)
} else {
    Join-Path $tankRunRepoDir 'generated\tank_run_tests'
}
$tankRunCandidates = [System.Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $tankRunCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $tankRunCandidates.Add($env:VSINSTALLDIR) }

# Discover an existing compiler; do not install or modify Visual Studio.
$tankRunVsWhere = Get-Command vswhere.exe -ErrorAction SilentlyContinue
if ($tankRunVsWhere) { $tankRunVsWherePath = $tankRunVsWhere.Source }
else { $tankRunVsWherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe' }
if (Test-Path -LiteralPath $tankRunVsWherePath) {
    $tankRunFound = & $tankRunVsWherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $tankRunFound) { $tankRunCandidates.Add(([string]$tankRunFound).Trim()) }
}
foreach ($tankRunProgramRoot in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
    if (!$tankRunProgramRoot) { continue }
    $tankRunVsRoot = Join-Path $tankRunProgramRoot 'Microsoft Visual Studio'
    if (!(Test-Path -LiteralPath $tankRunVsRoot)) { continue }
    foreach ($tankRunVersion in (Get-ChildItem -LiteralPath $tankRunVsRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
        foreach ($tankRunEdition in (Get-ChildItem -LiteralPath $tankRunVersion.FullName -Directory -ErrorAction SilentlyContinue)) {
            $tankRunCandidates.Add($tankRunEdition.FullName)
        }
    }
}
$tankRunDevCmd = $null
foreach ($tankRunCandidate in $tankRunCandidates) {
    $tankRunPossible = Join-Path $tankRunCandidate 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $tankRunPossible) { $tankRunDevCmd = $tankRunPossible; break }
}
if (!$tankRunDevCmd) {
    throw 'Visual Studio C++ tools were not found. Pass -VisualStudioPath with an existing C++ installation directory.'
}

New-Item -ItemType Directory -Path $tankRunOutputDir -Force | Out-Null
$tankRunBuildCmd = Join-Path $tankRunOutputDir 'build_tank_run_tests.cmd'
# Environment variables preserve Unicode repository paths through cmd.exe.
$tankRunBatch = @'
@echo off
call "%TANK_RUN_TEST_VS_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /Fe:tank_run_tests.exe /Fo:.\ "%TANK_RUN_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
tank_run_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /Fe:prototype_boss_combat_tests.exe /Fo:.\ "%TANK_RUN_BOSS_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
prototype_boss_combat_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /Fe:tank_run_modifier_tests.exe /Fo:.\ "%TANK_RUN_MODIFIER_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
tank_run_modifier_tests.exe
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText($tankRunBuildCmd, $tankRunBatch, [System.Text.Encoding]::ASCII)
$tankRunEnvironment = @{
    TANK_RUN_TEST_VS_DEV_CMD = $tankRunDevCmd
    TANK_RUN_TEST_SOURCE = Join-Path $PSScriptRoot 'tank_run_tests.cpp'
    TANK_RUN_BOSS_TEST_SOURCE = Join-Path $PSScriptRoot 'prototype_boss_combat_tests.cpp'
    TANK_RUN_MODIFIER_TEST_SOURCE = Join-Path $PSScriptRoot 'tank_run_modifier_tests.cpp'
}
$tankRunPreviousEnvironment = @{}
foreach ($tankRunName in $tankRunEnvironment.Keys) {
    $tankRunPreviousEnvironment[$tankRunName] = [Environment]::GetEnvironmentVariable($tankRunName, 'Process')
    [Environment]::SetEnvironmentVariable($tankRunName, $tankRunEnvironment[$tankRunName], 'Process')
}
try {
    Push-Location -LiteralPath $tankRunOutputDir
    try {
        & $env:ComSpec /d /c build_tank_run_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Tank run build/tests failed with exit code $LASTEXITCODE." }
    } finally { Pop-Location }
} finally {
    foreach ($tankRunName in $tankRunEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($tankRunName, $tankRunPreviousEnvironment[$tankRunName], 'Process')
    }
}
Write-Host "All three tank run test suites passed. Executables: $tankRunOutputDir"
