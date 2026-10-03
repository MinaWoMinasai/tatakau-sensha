param([string]$VisualStudioPath = '', [string]$OutputDirectory = '', [switch]$Hardware)
$ErrorActionPreference = 'Stop'
$neonRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$neonProject = Join-Path $neonRepo 'project'
$neonOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $neonRepo 'generated/neon_skinned_pipeline_tests' }
if (!$VisualStudioPath) {
    $neonWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $neonWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$neonDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $neonDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $neonOutput -Force | Out-Null
$neonBatch = @'
@echo off
call "%NEON_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /Gy /Gw /MT /I"%NEON_TEST_PROJECT%" /I"%NEON_TEST_PROJECT%\DirectX\engine\commom" /I"%NEON_TEST_PROJECT%\DirectX\engine\calc" /I"%NEON_TEST_PROJECT%\DirectX\engine\struct" /I"%NEON_TEST_PROJECT%\DirectX\engine\3d" /I"%NEON_TEST_PROJECT%\DirectX\engine\2d" /I"%NEON_TEST_PROJECT%\DirectX\engine\debugCamera" /Fe:neon_skinned_pipeline_tests.exe /Fo:.\ "%NEON_TEST_PROJECT%\tools\neon_skinned_pipeline_tests.cpp" "%NEON_TEST_PROJECT%\DirectX\engine\3d\neon\NeonSkinnedRenderer.cpp" "%NEON_TEST_PROJECT%\DirectX\engine\commom\InputDesc.cpp" /link /OPT:REF /OPT:ICF
if errorlevel 1 exit /b %errorlevel%
copy /y "%WindowsSdkDir%bin\%WindowsSDKVersion%x64\dxcompiler.dll" . >nul
if errorlevel 1 exit /b %errorlevel%
copy /y "%WindowsSdkDir%bin\%WindowsSDKVersion%x64\dxil.dll" . >nul
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $neonOutput 'build.cmd'), $neonBatch, [Text.Encoding]::ASCII)
$neonSettings = @{ NEON_TEST_DEV_CMD=$neonDevCmd; NEON_TEST_PROJECT=$neonProject }
$neonPrevious = @{}
try {
    foreach ($name in $neonSettings.Keys) {
        $neonPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $neonSettings[$name], 'Process')
    }
    Push-Location -LiteralPath $neonOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Neon pipeline test build failed: $LASTEXITCODE" }
    } finally { Pop-Location }
    Push-Location -LiteralPath $neonProject
    try {
        if ($Hardware) {
            & (Join-Path $neonOutput 'neon_skinned_pipeline_tests.exe') --hardware
        } else {
            & (Join-Path $neonOutput 'neon_skinned_pipeline_tests.exe')
        }
        if ($LASTEXITCODE -ne 0) { throw "Neon pipeline tests failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $neonPrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $neonPrevious[$name], 'Process') }
}
