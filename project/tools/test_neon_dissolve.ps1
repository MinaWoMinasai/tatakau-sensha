param([string]$VisualStudioPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$dissolveRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$dissolveProject = Join-Path $dissolveRepo 'project'
$dissolveOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $dissolveRepo 'generated/neon_dissolve_tests' }
if (!$VisualStudioPath) {
    $dissolveWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $dissolveWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$dissolveDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $dissolveDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $dissolveOutput -Force | Out-Null
$dissolveBatch = @'
@echo off
call "%DISSOLVE_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /Gy /Gw /MT /I"%DISSOLVE_TEST_PROJECT%" /I"%DISSOLVE_TEST_PROJECT%\DirectX\engine\commom" /I"%DISSOLVE_TEST_PROJECT%\DirectX\engine\calc" /I"%DISSOLVE_TEST_PROJECT%\DirectX\engine\struct" /I"%DISSOLVE_TEST_PROJECT%\DirectX\engine\3d" /I"%DISSOLVE_TEST_PROJECT%\DirectX\engine\2d" /I"%DISSOLVE_TEST_PROJECT%\DirectX\engine\debugCamera" /Fe:neon_dissolve_tests.exe /Fo:.\ "%DISSOLVE_TEST_PROJECT%\tools\neon_dissolve_tests.cpp" /link /OPT:REF /OPT:ICF
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $dissolveOutput 'build.cmd'), $dissolveBatch, [Text.Encoding]::ASCII)
$dissolveSettings = @{ DISSOLVE_TEST_DEV_CMD=$dissolveDevCmd; DISSOLVE_TEST_PROJECT=$dissolveProject }
$dissolvePrevious = @{}
try {
    foreach ($name in $dissolveSettings.Keys) {
        $dissolvePrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $dissolveSettings[$name], 'Process')
    }
    Push-Location -LiteralPath $dissolveOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Dissolve numeric test build failed: $LASTEXITCODE" }
        & (Join-Path $dissolveOutput 'neon_dissolve_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw "Dissolve numeric tests failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $dissolvePrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $dissolvePrevious[$name], 'Process') }
}
