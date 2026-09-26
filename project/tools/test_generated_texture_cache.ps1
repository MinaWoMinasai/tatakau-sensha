param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$cacheRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$cacheOutput = Join-Path $cacheRepo 'generated\generated_texture_cache_tests'
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
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /O2 /MT /UNDEBUG /Fe:generated_texture_cache_tests.exe /Fo:.\ /I"%CACHE_TEST_PROJECT%" /I"%CACHE_TEST_PROJECT%\DirectX\engine\2d" /I"%CACHE_TEST_PROJECT%\DirectX\engine\commom" "%CACHE_TEST_SOURCE%" "%CACHE_TEST_PROJECT%\externals\generated\outputs\Release\DirectXTex.lib" ole32.lib
if errorlevel 1 exit /b %errorlevel%
generated_texture_cache_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $cacheOutput 'build.cmd'), $cacheBatch, [Text.Encoding]::ASCII)
$cacheEnv = @{ CACHE_TEST_DEV_CMD=$cacheDevCmd; CACHE_TEST_PROJECT=(Join-Path $cacheRepo 'project'); CACHE_TEST_SOURCE=(Join-Path $PSScriptRoot 'generated_texture_cache_tests.cpp') }
$cachePrevious = @{}
foreach ($cacheKey in $cacheEnv.Keys) { $cachePrevious[$cacheKey]=[Environment]::GetEnvironmentVariable($cacheKey,'Process'); [Environment]::SetEnvironmentVariable($cacheKey,$cacheEnv[$cacheKey],'Process') }
try {
    Push-Location -LiteralPath $cacheOutput
    try { & $env:ComSpec /d /c build.cmd; if ($LASTEXITCODE -ne 0) { throw "Generated texture cache tests failed: $LASTEXITCODE" } }
    finally { Pop-Location }
} finally { foreach ($cacheKey in $cacheEnv.Keys) { [Environment]::SetEnvironmentVariable($cacheKey,$cachePrevious[$cacheKey],'Process') } }
