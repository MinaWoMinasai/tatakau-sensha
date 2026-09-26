param([string]$VisualStudioPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$rewardRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$rewardOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $rewardRepo 'generated\tank_reward_card_tests' }
if (!$VisualStudioPath) {
    $rewardWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudioPath = (& $rewardWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$rewardDevCmd = Join-Path $VisualStudioPath 'Common7\Tools\VsDevCmd.bat'
if (!(Test-Path -LiteralPath $rewardDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $rewardOutput -Force | Out-Null
$rewardBatch = @'
@echo off
call "%REWARD_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /Fe:reward_card_tests.exe /Fo:.\ "%REWARD_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
reward_card_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $rewardOutput 'build.cmd'), $rewardBatch, [Text.Encoding]::ASCII)
$rewardEnv = @{ REWARD_TEST_DEV_CMD=$rewardDevCmd; REWARD_TEST_SOURCE=(Join-Path $PSScriptRoot 'tank_reward_card_tests.cpp') }
$rewardPrevious = @{}
foreach ($rewardKey in $rewardEnv.Keys) { $rewardPrevious[$rewardKey]=[Environment]::GetEnvironmentVariable($rewardKey,'Process'); [Environment]::SetEnvironmentVariable($rewardKey,$rewardEnv[$rewardKey],'Process') }
try {
    Push-Location -LiteralPath $rewardOutput
    try { & $env:ComSpec /d /c build.cmd; if ($LASTEXITCODE -ne 0) { throw "Reward card tests failed: $LASTEXITCODE" } }
    finally { Pop-Location }
} finally { foreach ($rewardKey in $rewardEnv.Keys) { [Environment]::SetEnvironmentVariable($rewardKey,$rewardPrevious[$rewardKey],'Process') } }
