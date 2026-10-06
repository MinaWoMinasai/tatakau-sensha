param(
    [string]$VisualStudioPath = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$neonDepthRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$neonDepthProject = Join-Path $neonDepthRepo 'project'
$neonDepthOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else {
    Join-Path $neonDepthRepo 'generated/neon_depth_combat_tests'
}
$neonDepthCandidates = [Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $neonDepthCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $neonDepthCandidates.Add($env:VSINSTALLDIR) }
$neonDepthWhereCommand = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$neonDepthWhere = if ($neonDepthWhereCommand) { $neonDepthWhereCommand.Source } else {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
}
if (Test-Path -LiteralPath $neonDepthWhere) {
    $neonDepthFound = & $neonDepthWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $neonDepthFound) { $neonDepthCandidates.Add(([string]$neonDepthFound).Trim()) }
}
$neonDepthDevCmd = $null
foreach ($neonDepthCandidate in $neonDepthCandidates) {
    $neonDepthPossible = Join-Path $neonDepthCandidate 'Common7/Tools/VsDevCmd.bat'
    if (Test-Path -LiteralPath $neonDepthPossible) { $neonDepthDevCmd = $neonDepthPossible; break }
}
if (!$neonDepthDevCmd) { throw 'Visual Studio C++ tools were not found. Pass -VisualStudioPath with an existing C++ installation.' }
New-Item -ItemType Directory -Path $neonDepthOutput -Force | Out-Null
$neonDepthBatch = @'
@echo off
call "%NEON_DEPTH_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%NEON_DEPTH_TEST_PROJECT%" /Fe:neon_depth_combat_tests.exe /Fo:.\ "%NEON_DEPTH_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
neon_depth_combat_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $neonDepthOutput 'build_neon_depth_combat_tests.cmd'),$neonDepthBatch,[Text.Encoding]::ASCII)
$neonDepthSettings = @{
    NEON_DEPTH_TEST_DEV_CMD = $neonDepthDevCmd
    NEON_DEPTH_TEST_PROJECT = $neonDepthProject
    NEON_DEPTH_TEST_SOURCE = Join-Path $PSScriptRoot 'neon_depth_combat_tests.cpp'
}
$neonDepthPrevious = @{}
try {
    foreach ($name in $neonDepthSettings.Keys) {
        $neonDepthPrevious[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
        [Environment]::SetEnvironmentVariable($name,$neonDepthSettings[$name],'Process')
    }
    Push-Location -LiteralPath $neonDepthOutput
    try {
        & $env:ComSpec /d /c build_neon_depth_combat_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Neon Depth contract build/tests failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $neonDepthPrevious.Keys) { [Environment]::SetEnvironmentVariable($name,$neonDepthPrevious[$name],'Process') }
}
Write-Host "Neon Depth contract test suite passed. Executable: $neonDepthOutput/neon_depth_combat_tests.exe"
