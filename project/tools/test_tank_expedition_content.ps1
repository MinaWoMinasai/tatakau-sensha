param([switch]$WriteDefaults)
$ErrorActionPreference = 'Stop'
$contentRepo = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$contentOutput = Join-Path $contentRepo 'generated\tank_expedition_content_tests'
New-Item -ItemType Directory -Path $contentOutput -Force | Out-Null
$contentVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$contentVs = & $contentVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$contentVs) { throw 'Visual Studio C++ tools were not found.' }
$contentEnvironment = @{
    CONTENT_DEV = Join-Path $contentVs 'Common7\Tools\VsDevCmd.bat'
    CONTENT_INCLUDE = Join-Path $contentRepo 'project\externals'
    CONTENT_PROJECT = Join-Path $contentRepo 'project'
    CONTENT_SOURCE = Join-Path $PSScriptRoot 'tank_expedition_content_tests.cpp'
    CONTENT_EDITOR = Join-Path $contentRepo 'project\game\editor\ExpeditionContentEditor.cpp'
    CONTENT_DEFAULTS = if($WriteDefaults) { '..\..\project\resources\configs\expedition_content.json' } else { '' }
}
$contentPrevious = @{}
foreach($name in $contentEnvironment.Keys){$contentPrevious[$name]=[Environment]::GetEnvironmentVariable($name,'Process');[Environment]::SetEnvironmentVariable($name,$contentEnvironment[$name],'Process')}
$contentBatch = @'
@echo off
call "%CONTENT_DEV%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%CONTENT_INCLUDE%" /Fe:tank_expedition_content_tests.exe /Fo:.\ "%CONTENT_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
if defined CONTENT_DEFAULTS (tank_expedition_content_tests.exe "%CONTENT_DEFAULTS%") else (tank_expedition_content_tests.exe)
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /c /DUSE_RUNTIME_PROFILER /I"%CONTENT_INCLUDE%" /I"%CONTENT_PROJECT%" /Fo:ExpeditionContentEditor.obj "%CONTENT_EDITOR%"
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText((Join-Path $contentOutput 'build.cmd'),$contentBatch,[System.Text.Encoding]::ASCII)
try{Push-Location -LiteralPath $contentOutput;try{& $env:ComSpec /d /c build.cmd;if($LASTEXITCODE -ne 0){throw "Content tests failed: $LASTEXITCODE"}}finally{Pop-Location}}
finally{foreach($name in $contentPrevious.Keys){[Environment]::SetEnvironmentVariable($name,$contentPrevious[$name],'Process')}}
