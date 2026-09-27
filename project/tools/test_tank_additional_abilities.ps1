param([string]$VisualStudioPath='')
$ErrorActionPreference='Stop'
$abilityRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$abilityOutput=Join-Path $abilityRepo 'generated/tank_additional_abilities_tests'
New-Item -ItemType Directory -Path $abilityOutput -Force | Out-Null
if(!$VisualStudioPath) {
    $abilityVsWhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath=(& $abilityVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
}
if(!$VisualStudioPath){throw 'Existing Visual Studio C++ tools not found.'}
$abilityEnvironment=@{ABILITY_TEST_VS=Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat';ABILITY_TEST_SOURCE=Join-Path $PSScriptRoot 'tank_additional_abilities_tests.cpp'}
$abilityPrevious=@{}
foreach($name in $abilityEnvironment.Keys){$abilityPrevious[$name]=[Environment]::GetEnvironmentVariable($name,'Process');[Environment]::SetEnvironmentVariable($name,$abilityEnvironment[$name],'Process')}
$abilityBatch=@'
@echo off
call "%ABILITY_TEST_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /Fe:tank_additional_abilities_tests.exe /Fo:.\ "%ABILITY_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
tank_additional_abilities_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $abilityOutput 'build.cmd'),$abilityBatch,[Text.Encoding]::ASCII)
try {Push-Location -LiteralPath $abilityOutput
    try {& $env:ComSpec /d /c build.cmd;if($LASTEXITCODE -ne 0){throw "Additional ability regression failed: $LASTEXITCODE"}}
    finally {Pop-Location}
} finally {foreach($name in $abilityPrevious.Keys){[Environment]::SetEnvironmentVariable($name,$abilityPrevious[$name],'Process')}}
