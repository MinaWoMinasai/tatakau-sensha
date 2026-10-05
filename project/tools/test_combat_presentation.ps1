param([string]$VisualStudioPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$presentationRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$presentationProject = Join-Path $presentationRepo 'project'
$presentationOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $presentationRepo 'generated/combat_presentation_tests' }
if (!$VisualStudioPath) {
    $presentationWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $presentationWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$presentationDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $presentationDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $presentationOutput -Force | Out-Null
$presentationBatch = @'
@echo off
call "%COMBAT_PRESENTATION_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MT /I"%COMBAT_PRESENTATION_TEST_PROJECT%" /Fe:combat_presentation_tests.exe /Fo:.\ "%COMBAT_PRESENTATION_TEST_PROJECT%\tools\combat_presentation_tests.cpp"
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $presentationOutput 'build.cmd'), $presentationBatch, [Text.Encoding]::ASCII)
$presentationSettings = @{ COMBAT_PRESENTATION_TEST_DEV_CMD=$presentationDevCmd; COMBAT_PRESENTATION_TEST_PROJECT=$presentationProject }
$presentationPrevious = @{}
try {
    foreach ($name in $presentationSettings.Keys) {
        $presentationPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $presentationSettings[$name], 'Process')
    }
    Push-Location -LiteralPath $presentationOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Combat presentation test build failed: $LASTEXITCODE" }
        & (Join-Path $presentationOutput 'combat_presentation_tests.exe')
        if ($LASTEXITCODE -ne 0) { throw "Combat presentation tests failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $presentationPrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $presentationPrevious[$name], 'Process') }
}
