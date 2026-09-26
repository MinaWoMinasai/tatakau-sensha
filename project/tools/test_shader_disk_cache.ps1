param([string]$VisualStudioPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$cacheRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$cacheOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $cacheRepo 'generated\shader_disk_cache_tests' }
if (!$VisualStudioPath) {
    $cacheWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudioPath = (& $cacheWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$cacheDevCmd = Join-Path $VisualStudioPath 'Common7\Tools\VsDevCmd.bat'
if (!(Test-Path -LiteralPath $cacheDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $cacheOutput -Force | Out-Null
$cacheBatch = @'
@echo off
call "%CACHE_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%CACHE_TEST_JSON%" /I"%CACHE_TEST_JSON%\.." /Fe:shader_disk_cache_tests.exe /Fo:.\ "%CACHE_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
copy /y "%WindowsSdkDir%bin\%WindowsSDKVersion%x64\dxcompiler.dll" . >nul
if errorlevel 1 exit /b %errorlevel%
copy /y "%WindowsSdkDir%bin\%WindowsSDKVersion%x64\dxil.dll" . >nul
if errorlevel 1 exit /b %errorlevel%
shader_disk_cache_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $cacheOutput 'build.cmd'), $cacheBatch, [Text.Encoding]::ASCII)
$cacheEnv = @{ CACHE_TEST_DEV_CMD=$cacheDevCmd; CACHE_TEST_SOURCE=(Join-Path $PSScriptRoot 'shader_disk_cache_tests.cpp'); CACHE_TEST_JSON=(Join-Path $cacheRepo 'project\externals\nlohmann') }
$cachePrevious = @{}
foreach ($cacheKey in $cacheEnv.Keys) { $cachePrevious[$cacheKey]=[Environment]::GetEnvironmentVariable($cacheKey,'Process'); [Environment]::SetEnvironmentVariable($cacheKey,$cacheEnv[$cacheKey],'Process') }
try {
    Push-Location -LiteralPath $cacheOutput
    try { & $env:ComSpec /d /c build.cmd; if ($LASTEXITCODE -ne 0) { throw "Shader cache tests failed: $LASTEXITCODE" } }
    finally { Pop-Location }
} finally { foreach ($cacheKey in $cacheEnv.Keys) { [Environment]::SetEnvironmentVariable($cacheKey,$cachePrevious[$cacheKey],'Process') } }
