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
cl /nologo /std:c++17 /EHsc /W4 /O2 /Fe:ink_simulation_tests.exe /Fo:.\ "%INK_TEST_SOURCE%" "%INK_TEST_SIMULATION%"
if errorlevel 1 exit /b %errorlevel%
ink_simulation_tests.exe
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText($inkBuildCmd, $inkBatch, [System.Text.Encoding]::ASCII)
$inkEnvironment = @{
    INK_TEST_VS_DEV_CMD = $inkDevCmd
    INK_TEST_SOURCE = Join-Path $PSScriptRoot 'ink_simulation_tests.cpp'
    INK_TEST_SIMULATION = Join-Path $inkRepoDir 'project\game\ink\InkSimulation.cpp'
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
