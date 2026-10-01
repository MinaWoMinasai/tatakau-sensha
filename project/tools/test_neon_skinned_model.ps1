param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$avatarRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$avatarProject = Join-Path $avatarRepo 'project'
$avatarOutput = Join-Path $avatarRepo 'generated/neon_skinned_model_tests'
if (!$VisualStudioPath) {
    $avatarWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $avatarWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$avatarDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $avatarDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
$avatarAssimp = if (Test-Path -LiteralPath (Join-Path $avatarProject 'externals/assimp/lib/assimp-vc145-mt.lib')) { 'assimp-vc145-mt' } else { 'assimp-vc143-mt' }
New-Item -ItemType Directory -Path $avatarOutput -Force | Out-Null
$avatarBatch = @'
@echo off
call "%AVATAR_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MD /I"%AVATAR_TEST_PROJECT%" /I"%AVATAR_TEST_PROJECT%\externals" /I"%AVATAR_TEST_PROJECT%\externals\assimp\include" /Fe:neon_skinned_model_tests.exe /Fo:.\ "%AVATAR_TEST_PROJECT%\tools\neon_skinned_model_tests.cpp" /link /LIBPATH:"%AVATAR_TEST_PROJECT%\externals\assimp\lib" %AVATAR_TEST_ASSIMP%.lib
if errorlevel 1 exit /b %errorlevel%
copy /y "%AVATAR_TEST_PROJECT%\externals\assimp\runtime\%AVATAR_TEST_ASSIMP%.dll" . >nul
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $avatarOutput 'build.cmd'), $avatarBatch, [Text.Encoding]::ASCII)
$avatarSettings = @{ AVATAR_TEST_DEV_CMD=$avatarDevCmd; AVATAR_TEST_PROJECT=$avatarProject; AVATAR_TEST_ASSIMP=$avatarAssimp }
$avatarPrevious = @{}
try {
    foreach ($name in $avatarSettings.Keys) {
        $avatarPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $avatarSettings[$name], 'Process')
    }
    Push-Location -LiteralPath $avatarOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Avatar inspection build failed: $LASTEXITCODE" }
    } finally { Pop-Location }
    Push-Location -LiteralPath $avatarProject
    try {
        & (Join-Path $avatarOutput 'neon_skinned_model_tests.exe') 'resources/models/neon_hologram/AvatarSample_B.glb' '../generated/neon_skinned_model_tests/report.json'
        if ($LASTEXITCODE -ne 0) { throw "Avatar model validation failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $avatarPrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $avatarPrevious[$name], 'Process') }
}
