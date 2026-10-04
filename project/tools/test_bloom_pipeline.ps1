param([string]$VisualStudioPath = '', [string]$OutputDirectory = '', [switch]$Hardware, [switch]$BuildOnly, [switch]$CpuOnly)
$ErrorActionPreference = 'Stop'
$bloomRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$bloomProject = Join-Path $bloomRepo 'project'
$bloomOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $bloomRepo 'generated/bloom_pipeline_tests' }
if (!$VisualStudioPath) {
    $bloomWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $bloomWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$bloomDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $bloomDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $bloomOutput -Force | Out-Null
$bloomBatch = @'
@echo off
call "%BLOOM_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /Gy /Gw /MT /I"%BLOOM_TEST_PROJECT%" /I"%BLOOM_TEST_PROJECT%\DirectX\engine\commom" /I"%BLOOM_TEST_PROJECT%\DirectX\engine\calc" /I"%BLOOM_TEST_PROJECT%\DirectX\engine\struct" /I"%BLOOM_TEST_PROJECT%\DirectX\engine\3d" /I"%BLOOM_TEST_PROJECT%\DirectX\engine\2d" /I"%BLOOM_TEST_PROJECT%\DirectX\engine\debugCamera" /I"%BLOOM_TEST_PROJECT%\DirectX\engine\postEffect" /Fe:bloom_pipeline_tests.exe /Fo:.\ "%BLOOM_TEST_PROJECT%\tools\bloom_pipeline_tests.cpp" "%BLOOM_TEST_PROJECT%\DirectX\engine\postEffect\BloomPyramid.cpp" "%BLOOM_TEST_PROJECT%\DirectX\engine\postEffect\BloomConstantBuffer.cpp" "%BLOOM_TEST_PROJECT%\DirectX\engine\postEffect\RenderTexture.cpp" "%BLOOM_TEST_PROJECT%\DirectX\engine\commom\RtvManager.cpp" /link /OPT:REF /OPT:ICF
if errorlevel 1 exit /b %errorlevel%
copy /y "%WindowsSdkDir%bin\%WindowsSDKVersion%x64\dxcompiler.dll" . >nul
if errorlevel 1 exit /b %errorlevel%
copy /y "%WindowsSdkDir%bin\%WindowsSDKVersion%x64\dxil.dll" . >nul
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $bloomOutput 'build.cmd'), $bloomBatch, [Text.Encoding]::ASCII)
$bloomSettings = @{ BLOOM_TEST_DEV_CMD=$bloomDevCmd; BLOOM_TEST_PROJECT=$bloomProject }
$bloomPrevious = @{}
try {
    foreach ($name in $bloomSettings.Keys) {
        $bloomPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $bloomSettings[$name], 'Process')
    }
    Push-Location -LiteralPath $bloomOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Bloom pipeline test build failed: $LASTEXITCODE" }
    } finally { Pop-Location }
    if ($BuildOnly) { Write-Host 'PASS: Bloom test build only; GPU execution not requested.'; return }
    Push-Location -LiteralPath $bloomProject
    try {
        $bloomArguments = @('--output', $bloomOutput)
        if ($CpuOnly) { $bloomArguments += '--cpu-only' }
        if ($Hardware) { $bloomArguments += '--hardware' }
        & (Join-Path $bloomOutput 'bloom_pipeline_tests.exe') @bloomArguments
        if ($LASTEXITCODE -ne 0) { throw "Bloom pipeline tests failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $bloomPrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $bloomPrevious[$name], 'Process') }
}
