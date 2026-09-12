param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'

$inkRepoDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$inkOutputDir = Join-Path $inkRepoDir 'generated\ink_tests'
$inkCandidates = [System.Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $inkCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $inkCandidates.Add($env:VSINSTALLDIR) }

# Use an existing VS C++ installation. This script never installs components.
$inkVsWhere = Get-Command vswhere.exe -ErrorAction SilentlyContinue
if ($inkVsWhere) { $inkVsWherePath = $inkVsWhere.Source }
else { $inkVsWherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe' }
if (Test-Path -LiteralPath $inkVsWherePath) {
    $inkFound = & $inkVsWherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $inkFound) { $inkCandidates.Add(([string]$inkFound).Trim()) }
}
foreach ($inkProgramRoot in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
    if (!$inkProgramRoot) { continue }
    $inkVsRoot = Join-Path $inkProgramRoot 'Microsoft Visual Studio'
    if (!(Test-Path -LiteralPath $inkVsRoot)) { continue }
    foreach ($inkVersion in (Get-ChildItem -LiteralPath $inkVsRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
        foreach ($inkEdition in (Get-ChildItem -LiteralPath $inkVersion.FullName -Directory -ErrorAction SilentlyContinue)) {
            $inkCandidates.Add($inkEdition.FullName)
        }
    }
}
$inkDevCmd = $null
foreach ($inkCandidate in $inkCandidates) {
    $inkPossible = Join-Path $inkCandidate 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $inkPossible) { $inkDevCmd = $inkPossible; break }
}
if (!$inkDevCmd) {
    throw 'Visual Studio C++ tools were not found. Use an existing C++ installation, or pass -VisualStudioPath with its installation directory.'
}

New-Item -ItemType Directory -Path $inkOutputDir -Force | Out-Null
$inkBuildCmd = Join-Path $inkOutputDir 'build_ink_tests.cmd'
# Paths travel through environment variables so Unicode repository names and
# spaces survive cmd.exe's source-file code page. Compilation runs in generated/.
$inkBatch = @'
@echo off
call "%INK_TEST_VS_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /Fe:ink_simulation_tests.exe /Fo:.\ "%INK_TEST_SOURCE%" "%INK_TEST_SIMULATION%" "%INK_STRINGER_SIMULATION%"
if errorlevel 1 exit /b %errorlevel%
ink_simulation_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /Fe:ink_emission_pattern_tests.exe /Fo:.\ "%INK_EMISSION_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
ink_emission_pattern_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /Fe:ink_stringer_pattern_tests.exe /Fo:.\ "%INK_STRINGER_PATTERN_TEST%"
if errorlevel 1 exit /b %errorlevel%
ink_stringer_pattern_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /Fe:ink_stringer_simulation_tests.exe /Fo:.\ "%INK_STRINGER_TEST%" "%INK_TEST_SIMULATION%" "%INK_STRINGER_SIMULATION%"
if errorlevel 1 exit /b %errorlevel%
ink_stringer_simulation_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /I"%INK_JSON_INCLUDE%" /Fe:ink_weapon_catalog_tests.exe /Fo:.\ "%INK_CATALOG_TEST%" "%INK_CATALOG_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
ink_weapon_catalog_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /Fe:ink_reticle_tests.exe /Fo:.\ "%INK_RETICLE_TEST%"
if errorlevel 1 exit /b %errorlevel%
ink_reticle_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /Fe:ink_movement_carry_tests.exe /Fo:.\ "%INK_CARRY_TEST%" "%INK_TEST_SIMULATION%" "%INK_STRINGER_SIMULATION%"
if errorlevel 1 exit /b %errorlevel%
ink_movement_carry_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /Fe:ink_audio_event_tests.exe /Fo:.\ "%INK_AUDIO_EVENT_TEST%" "%INK_TEST_SIMULATION%" "%INK_STRINGER_SIMULATION%"
if errorlevel 1 exit /b %errorlevel%
ink_audio_event_tests.exe
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText($inkBuildCmd, $inkBatch, [System.Text.Encoding]::ASCII)
$inkEnvironment = @{
    INK_TEST_VS_DEV_CMD = $inkDevCmd
    INK_TEST_SOURCE = Join-Path $PSScriptRoot 'ink_simulation_tests.cpp'
    INK_TEST_SIMULATION = Join-Path $inkRepoDir 'project\game\ink\InkSimulation.cpp'
    INK_EMISSION_TEST_SOURCE = Join-Path $PSScriptRoot 'ink_emission_pattern_tests.cpp'
    INK_STRINGER_SIMULATION = Join-Path $inkRepoDir 'project\game\ink\InkSimulation.Stringer.cpp'
    INK_STRINGER_PATTERN_TEST = Join-Path $PSScriptRoot 'ink_stringer_pattern_tests.cpp'
    INK_STRINGER_TEST = Join-Path $PSScriptRoot 'ink_stringer_simulation_tests.cpp'
    INK_CATALOG_TEST = Join-Path $PSScriptRoot 'ink_weapon_catalog_tests.cpp'
    INK_CATALOG_SOURCE = Join-Path $inkRepoDir 'project\game\ink\WeaponCatalog.cpp'
    INK_JSON_INCLUDE = Join-Path $inkRepoDir 'project\externals'
    INK_CARRY_TEST = Join-Path $PSScriptRoot 'ink_movement_carry_tests.cpp'
    INK_AUDIO_EVENT_TEST = Join-Path $PSScriptRoot 'ink_audio_event_tests.cpp'
    INK_RETICLE_TEST = Join-Path $PSScriptRoot 'ink_reticle_tests.cpp'
}
$inkPreviousEnvironment = @{}
foreach ($inkName in $inkEnvironment.Keys) {
    $inkPreviousEnvironment[$inkName] = [Environment]::GetEnvironmentVariable($inkName, 'Process')
    [Environment]::SetEnvironmentVariable($inkName, $inkEnvironment[$inkName], 'Process')
}
try {
    Push-Location -LiteralPath $inkOutputDir
    try {
        & $env:ComSpec /d /c build_ink_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Ink simulation build/tests failed with exit code $LASTEXITCODE." }
    } finally { Pop-Location }
} finally {
    foreach ($inkName in $inkEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($inkName, $inkPreviousEnvironment[$inkName], 'Process')
    }
}
Write-Host "Test executable: $(Join-Path $inkOutputDir 'ink_simulation_tests.exe')"
