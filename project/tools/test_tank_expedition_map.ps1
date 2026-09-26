param(
    [string]$VisualStudioPath = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$tankExpRepoDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$tankExpOutputDir = if ($OutputDirectory) {
    [System.IO.Path]::GetFullPath($OutputDirectory)
} else {
    Join-Path $tankExpRepoDir 'generated\tank_expedition_map_tests'
}
$tankExpCandidates = [System.Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $tankExpCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $tankExpCandidates.Add($env:VSINSTALLDIR) }
$tankExpVsWhereCommand = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$tankExpVsWhere = if ($tankExpVsWhereCommand) { $tankExpVsWhereCommand.Source } else {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
}
if (Test-Path -LiteralPath $tankExpVsWhere) {
    $tankExpFound = & $tankExpVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $tankExpFound) { $tankExpCandidates.Add(([string]$tankExpFound).Trim()) }
}
$tankExpDevCmd = $null
foreach ($tankExpCandidate in $tankExpCandidates) {
    $tankExpPossible = Join-Path $tankExpCandidate 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $tankExpPossible) { $tankExpDevCmd = $tankExpPossible; break }
}
if (!$tankExpDevCmd) {
    throw 'Visual Studio C++ tools were not found. Pass -VisualStudioPath with an existing C++ installation directory.'
}

New-Item -ItemType Directory -Path $tankExpOutputDir -Force | Out-Null
$tankExpBuildCmd = Join-Path $tankExpOutputDir 'build_tank_expedition_map_tests.cmd'
# Pass Unicode source paths through process environment variables.
$tankExpBatch = @'
@echo off
call "%TANK_EXP_TEST_VS_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%TANK_EXP_TEST_INCLUDE%" /Fe:tank_expedition_map_tests.exe /Fo:.\ "%TANK_EXP_TEST_SOURCE%"
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /WX /O2 /DUSE_RUNTIME_PROFILER /I"%TANK_EXP_TEST_INCLUDE%" /I"%TANK_EXP_PROJECT%" /c /Fo:expedition_map_editor.obj "%TANK_EXP_EDITOR_SOURCE%"
if not "%errorlevel%"=="0" exit /b %errorlevel%
tank_expedition_map_tests.exe "%TANK_EXP_MAP_JSON%"
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%TANK_EXP_TEST_INCLUDE%" /Fe:tank_expedition_map_tests_cpp20.exe /Fo:.\ "%TANK_EXP_TEST_SOURCE%"
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /DUSE_RUNTIME_PROFILER /I"%TANK_EXP_TEST_INCLUDE%" /I"%TANK_EXP_PROJECT%" /c /Fo:expedition_map_editor_cpp20.obj "%TANK_EXP_EDITOR_SOURCE%"
if not "%errorlevel%"=="0" exit /b %errorlevel%
tank_expedition_map_tests_cpp20.exe "%TANK_EXP_MAP_JSON%"
if not "%errorlevel%"=="0" exit /b %errorlevel%
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText($tankExpBuildCmd, $tankExpBatch, [System.Text.Encoding]::ASCII)
$tankExpEnvironment = @{
    TANK_EXP_TEST_VS_DEV_CMD = $tankExpDevCmd
    TANK_EXP_TEST_INCLUDE = Join-Path $tankExpRepoDir 'project\externals'
    TANK_EXP_PROJECT = Join-Path $tankExpRepoDir 'project'
    TANK_EXP_EDITOR_SOURCE = Join-Path $tankExpRepoDir 'project\game\editor\ExpeditionMapEditor.cpp'
    TANK_EXP_MAP_JSON = Join-Path $tankExpRepoDir 'project\resources\configs\expedition_map.json'
    TANK_EXP_TEST_SOURCE = Join-Path $PSScriptRoot 'tank_expedition_map_tests.cpp'
}
$tankExpPreviousEnvironment = @{}
foreach ($tankExpName in $tankExpEnvironment.Keys) {
    $tankExpPreviousEnvironment[$tankExpName] = [Environment]::GetEnvironmentVariable($tankExpName, 'Process')
    [Environment]::SetEnvironmentVariable($tankExpName, $tankExpEnvironment[$tankExpName], 'Process')
}
try {
    Push-Location -LiteralPath $tankExpOutputDir
    try {
        & $env:ComSpec /d /c build_tank_expedition_map_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Tank expedition build/tests failed with exit code $LASTEXITCODE." }
    } finally { Pop-Location }
} finally {
    foreach ($tankExpName in $tankExpEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($tankExpName, $tankExpPreviousEnvironment[$tankExpName], 'Process')
    }
}
Write-Host "Tank expedition test suite passed. Executable: $tankExpOutputDir\tank_expedition_map_tests.exe"
