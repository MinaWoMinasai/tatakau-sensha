param([string]$VisualStudioPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$bossRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$bossProject = Join-Path $bossRepo 'project'
$bossOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $bossRepo 'generated/neon_boss_visual_tests' }
if (!$VisualStudioPath) {
    $bossWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $bossWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$bossDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $bossDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $bossOutput -Force | Out-Null
$bossBatch = @'
@echo off
call "%NEON_BOSS_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MT /I"%NEON_BOSS_TEST_PROJECT%" /Fe:neon_boss_visual_tests.exe /Fo:.\ "%NEON_BOSS_TEST_PROJECT%\tools\neon_boss_visual_tests.cpp"
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $bossOutput 'build.cmd'), $bossBatch, [Text.Encoding]::ASCII)
$bossSettings = @{ NEON_BOSS_TEST_DEV_CMD=$bossDevCmd; NEON_BOSS_TEST_PROJECT=$bossProject }
$bossPrevious = @{}
try {
    foreach ($name in $bossSettings.Keys) {
        $bossPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $bossSettings[$name], 'Process')
    }
    Push-Location -LiteralPath $bossOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Neon boss visual test build failed: $LASTEXITCODE" }
        & (Join-Path $bossOutput 'neon_boss_visual_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw "Neon boss visual tests failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $bossPrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $bossPrevious[$name], 'Process') }
}
