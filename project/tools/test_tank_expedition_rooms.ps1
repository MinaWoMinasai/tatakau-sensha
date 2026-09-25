param([switch]$WriteDefaults)
$ErrorActionPreference='Stop'
$roomsRepo=[System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$roomsOutput=Join-Path $roomsRepo 'generated\tank_expedition_rooms_tests'
New-Item -ItemType Directory -Path $roomsOutput -Force | Out-Null
$roomsVsWhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$roomsVs=(& $roomsVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
$roomsDev=Join-Path $roomsVs 'Common7\Tools\VsDevCmd.bat'
if(!(Test-Path -LiteralPath $roomsDev)){throw 'Visual Studio C++ tools were not found.'}
$roomsBatch=@'
@echo off
call "%ROOMS_TEST_DEV%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%ROOMS_TEST_INCLUDE%" /Fe:room_tests.exe /Fo:.\ "%ROOMS_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
room_tests.exe %ROOMS_TEST_WRITE%
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText((Join-Path $roomsOutput 'build.cmd'),$roomsBatch,[System.Text.Encoding]::ASCII)
$roomsVariables=@{
    ROOMS_TEST_DEV=$roomsDev
    ROOMS_TEST_INCLUDE=(Join-Path $roomsRepo 'project\externals')
    ROOMS_TEST_SOURCE=(Join-Path $PSScriptRoot 'tank_expedition_rooms_tests.cpp')
    ROOMS_TEST_WRITE=$(if($WriteDefaults){'"../../project/resources/maps/expedition_layouts.json"'}else{''})
}
$roomsPrevious=@{}
foreach($key in $roomsVariables.Keys){$roomsPrevious[$key]=[Environment]::GetEnvironmentVariable($key,'Process');[Environment]::SetEnvironmentVariable($key,$roomsVariables[$key],'Process')}
try {Push-Location -LiteralPath $roomsOutput;try{& $env:ComSpec /d /c build.cmd;if($LASTEXITCODE -ne 0){throw "Room tests failed ($LASTEXITCODE)."}}finally{Pop-Location}}
finally{foreach($key in $roomsVariables.Keys){[Environment]::SetEnvironmentVariable($key,$roomsPrevious[$key],'Process')}}
