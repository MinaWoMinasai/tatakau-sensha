param([string]$VisualStudioPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$traceRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$traceOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $traceRepo 'generated\startup_trace_tests' }
if (!$VisualStudioPath) {
    $traceWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudioPath = (& $traceWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$traceDevCmd = Join-Path $VisualStudioPath 'Common7\Tools\VsDevCmd.bat'
if (!(Test-Path -LiteralPath $traceDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $traceOutput -Force | Out-Null
$traceBatch = @'
@echo off
call "%TRACE_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%TRACE_TEST_JSON%" /I"%TRACE_TEST_JSON%\.." /Fe:startup_trace_tests.exe /Fo:.\ "%TRACE_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
startup_trace_tests.exe enabled
if not "%errorlevel%"=="0" exit /b %errorlevel%
startup_trace_tests.exe disabled
if not "%errorlevel%"=="0" exit /b %errorlevel%
startup_trace_tests.exe unavailable
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $traceOutput 'build.cmd'), $traceBatch, [Text.Encoding]::ASCII)
$traceEnv = @{ TRACE_TEST_DEV_CMD=$traceDevCmd; TRACE_TEST_SOURCE=(Join-Path $PSScriptRoot 'startup_trace_tests.cpp'); TRACE_TEST_JSON=(Join-Path $traceRepo 'project\externals\nlohmann') }
$tracePrevious = @{}
foreach ($traceKey in $traceEnv.Keys) { $tracePrevious[$traceKey]=[Environment]::GetEnvironmentVariable($traceKey,'Process'); [Environment]::SetEnvironmentVariable($traceKey,$traceEnv[$traceKey],'Process') }
try {
    Push-Location -LiteralPath $traceOutput
    try { & $env:ComSpec /d /c build.cmd; if ($LASTEXITCODE -ne 0) { throw "Startup trace tests failed: $LASTEXITCODE" } }
    finally { Pop-Location }
} finally { foreach ($traceKey in $traceEnv.Keys) { [Environment]::SetEnvironmentVariable($traceKey,$tracePrevious[$traceKey],'Process') } }
