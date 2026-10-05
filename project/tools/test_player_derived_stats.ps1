param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$playerStatsRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$playerStatsOutput = Join-Path $playerStatsRoot 'generated/player_derived_stats_tests'
New-Item -ItemType Directory -Path $playerStatsOutput -Force | Out-Null
if (!$VisualStudioPath) {
    $playerStatsVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $playerStatsVsWhere) {
        $VisualStudioPath = (& $playerStatsVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    }
}
if (!$VisualStudioPath) { throw 'Existing Visual Studio C++ installation not found.' }
$playerStatsEnvironment = @{
    PLAYER_STATS_TEST_VS = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    PLAYER_STATS_TEST_SOURCE = Join-Path $PSScriptRoot 'player_derived_stats_tests.cpp'
    PLAYER_STATS_TEST_INCLUDE = Join-Path $playerStatsRoot 'project'
}
$playerStatsPrevious = @{}
foreach ($name in $playerStatsEnvironment.Keys) {
    $playerStatsPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $playerStatsEnvironment[$name], 'Process')
}
$playerStatsBatch = @'
@echo off
call "%PLAYER_STATS_TEST_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%PLAYER_STATS_TEST_INCLUDE%" /Fe:player_derived_stats_tests.exe /Fo:.\ "%PLAYER_STATS_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
player_derived_stats_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $playerStatsOutput 'build_player_derived_stats_tests.cmd'), $playerStatsBatch, [Text.Encoding]::ASCII)
try {
    Push-Location -LiteralPath $playerStatsOutput
    try {
        & $env:ComSpec /d /c build_player_derived_stats_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Player stats regression failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $playerStatsEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $playerStatsPrevious[$name], 'Process')
    }
}
