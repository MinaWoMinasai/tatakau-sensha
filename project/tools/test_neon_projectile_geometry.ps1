param([string]$VisualStudioPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$projectileHeadRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$OutputDirectory) { $OutputDirectory = Join-Path $projectileHeadRoot 'generated/neon_projectile_geometry_tests' }
$projectileHeadOutput = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($projectileHeadOutput) | Out-Null
$projectileHeadSources = @(
    (Join-Path $projectileHeadRoot 'project/game/render/NeonProjectileRenderer.h'),
    (Join-Path $projectileHeadRoot 'project/game/render/NeonProjectileRenderer.cpp'))
$projectileHeadProduction = foreach ($source in $projectileHeadSources) {
    [regex]::Replace((Get-Content -LiteralPath $source -Raw), '(?m)^\s*#(?:include|pragma)[^\r\n]*\r?\n', '')
}
[IO.File]::WriteAllText((Join-Path $projectileHeadOutput 'neon_projectile_production.inc'),
    ($projectileHeadProduction -join "`n"), [Text.UTF8Encoding]::new($false))
if (!$VisualStudioPath) {
    $projectileHeadVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $projectileHeadVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
}
$projectileHeadEnvironment = @{
    HEAD_TEST_DEV_CMD = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    HEAD_TEST_SOURCE = Join-Path $PSScriptRoot 'neon_projectile_geometry_tests.cpp'
    HEAD_TEST_INCLUDE = $projectileHeadOutput
}
$projectileHeadBatch = @'
@echo off
call "%HEAD_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%HEAD_TEST_INCLUDE%" /Fe:neon_projectile_geometry_tests.exe /Fo:.\ "%HEAD_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
neon_projectile_geometry_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $projectileHeadOutput 'build_cpu_tests.cmd'), $projectileHeadBatch, [Text.Encoding]::ASCII)
$projectileHeadPrevious = @{}
try {
    foreach ($key in $projectileHeadEnvironment.Keys) {
        $projectileHeadPrevious[$key] = [Environment]::GetEnvironmentVariable($key,'Process')
        [Environment]::SetEnvironmentVariable($key,$projectileHeadEnvironment[$key],'Process')
    }
    Push-Location -LiteralPath $projectileHeadOutput
    try { & $env:ComSpec /d /c build_cpu_tests.cmd; if ($LASTEXITCODE -ne 0) { throw "Projectile presentation CPU tests failed: $LASTEXITCODE" } }
    finally { Pop-Location }
} finally {
    foreach ($key in $projectileHeadPrevious.Keys) { [Environment]::SetEnvironmentVariable($key,$projectileHeadPrevious[$key],'Process') }
}
