param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$motionRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$motionProject = Join-Path $motionRepo 'project'
$motionOutput = Join-Path $motionRepo 'generated/neon_preview_animation_tests'
if (!$VisualStudioPath) {
    $motionWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $motionWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$motionDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
$motionAssimp = if (Test-Path (Join-Path $motionProject 'externals/assimp/lib/assimp-vc145-mt.lib')) { 'assimp-vc145-mt' } else { 'assimp-vc143-mt' }
New-Item -ItemType Directory -Path $motionOutput -Force | Out-Null
$motionBatch = @'
@echo off
call "%MOTION_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /O2 /MD /Gy /Gw /I"%MOTION_TEST_PROJECT%" /I"%MOTION_TEST_PROJECT%\externals\assimp\include" /I"%MOTION_TEST_PROJECT%\DirectX\engine\commom" /I"%MOTION_TEST_PROJECT%\DirectX\engine\calc" /I"%MOTION_TEST_PROJECT%\DirectX\engine\easing" /I"%MOTION_TEST_PROJECT%\DirectX\engine\struct" /I"%MOTION_TEST_PROJECT%\DirectX\engine\3d" /I"%MOTION_TEST_PROJECT%\DirectX\engine\2d" /I"%MOTION_TEST_PROJECT%\DirectX\engine\debugCamera" /Fe:neon_preview_animation_tests.exe /Fo:.\ "%MOTION_TEST_PROJECT%\tools\neon_preview_animation_tests.cpp" "%MOTION_TEST_PROJECT%\game\debug\NeonPreviewAnimations.cpp" "%MOTION_TEST_PROJECT%\DirectX\engine\3d\Animation.cpp" "%MOTION_TEST_PROJECT%\DirectX\engine\3d\Skeleton.cpp" "%MOTION_TEST_PROJECT%\DirectX\engine\3d\SkinCluster.cpp" "%MOTION_TEST_PROJECT%\DirectX\engine\calc\Calculation.cpp" "%MOTION_TEST_PROJECT%\DirectX\engine\easing\Easing.cpp" /link /OPT:REF /OPT:ICF /LIBPATH:"%MOTION_TEST_PROJECT%\externals\assimp\lib" %MOTION_TEST_ASSIMP%.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "%MOTION_TEST_PROJECT%\externals\assimp\runtime\%MOTION_TEST_ASSIMP%.dll" . >nul
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $motionOutput 'build.cmd'),$motionBatch,[Text.Encoding]::ASCII)
$motionSettings = @{ MOTION_TEST_DEV_CMD=$motionDevCmd; MOTION_TEST_PROJECT=$motionProject; MOTION_TEST_ASSIMP=$motionAssimp }
$motionPrevious = @{}
try {
    foreach ($name in $motionSettings.Keys) {
        $motionPrevious[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
        [Environment]::SetEnvironmentVariable($name,$motionSettings[$name],'Process')
    }
    Push-Location -LiteralPath $motionOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw 'Preview animation test build failed.' }
    } finally { Pop-Location }
    Push-Location -LiteralPath $motionProject
    try {
        & (Join-Path $motionOutput 'neon_preview_animation_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Preview animation tests failed.' }
    } finally { Pop-Location }
} finally {
    foreach ($name in $motionPrevious.Keys) { [Environment]::SetEnvironmentVariable($name,$motionPrevious[$name],'Process') }
}
