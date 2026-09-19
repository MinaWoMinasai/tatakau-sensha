param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$trailTestRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$trailTestOutput = Join-Path $trailTestRoot 'generated/tank_trail_tests'
New-Item -ItemType Directory -Path $trailTestOutput -Force | Out-Null
$trailInstanceHeader = Get-Content -LiteralPath (Join-Path $trailTestRoot 'project/DirectX/engine/commom/TrailInstance.h') -Raw
$trailManagerHeader = Get-Content -LiteralPath (Join-Path $trailTestRoot 'project/DirectX/engine/commom/TrailManager.h') -Raw
# Use the production data layout/classes while omitting only DirectX and JSON
# serialization dependencies. All mutation, geometry and draw methods below are
# the actual production code, exercised through a recording DirectX adapter.
$trailConfigStart = $trailInstanceHeader.IndexOf('struct SwordSection {', [StringComparison]::Ordinal)
$trailConfigEnd = $trailInstanceHeader.IndexOf('    // JSON', [StringComparison]::Ordinal)
$trailClassStart = $trailInstanceHeader.IndexOf('class TrailInstance {', [StringComparison]::Ordinal)
$trailManagerStart = $trailManagerHeader.IndexOf('class TrailManager {', [StringComparison]::Ordinal)
if ($trailConfigStart -lt 0 -or $trailConfigEnd -lt 0 -or $trailClassStart -lt 0 -or $trailManagerStart -lt 0) {
    throw 'Production trail declaration anchors changed.'
}
$trailDeclarations = $trailInstanceHeader.Substring($trailConfigStart, $trailConfigEnd - $trailConfigStart) + "};`n" +
    $trailInstanceHeader.Substring($trailClassStart) + "`n" + $trailManagerHeader.Substring($trailManagerStart)
[IO.File]::WriteAllText((Join-Path $trailTestOutput 'trail_production_declarations.inc'), $trailDeclarations, [Text.UTF8Encoding]::new($false))
$trailMethods = foreach ($trailSource in @('TrailInstance.cpp', 'TrailManager.cpp')) {
    $trailText = Get-Content -LiteralPath (Join-Path $trailTestRoot "project/DirectX/engine/commom/$trailSource") -Raw
    [regex]::Replace($trailText, '(?m)^#include[^\r\n]*\r?\n', '')
}
[IO.File]::WriteAllText((Join-Path $trailTestOutput 'trail_production_methods.inc'), ($trailMethods -join "`n"), [Text.UTF8Encoding]::new($false))
if (!$VisualStudioPath) {
    $trailVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $trailVsWhere) {
        $VisualStudioPath = (& $trailVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    }
}
if (!$VisualStudioPath) { throw 'Existing Visual Studio C++ installation not found.' }
$trailTestEnvironment = @{
    TRAIL_TEST_VS = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    TRAIL_TEST_SOURCE = Join-Path $PSScriptRoot 'tank_trail_tests.cpp'
    TRAIL_TEST_INCLUDE = $trailTestOutput
}
$trailTestPrevious = @{}
foreach ($trailName in $trailTestEnvironment.Keys) {
    $trailTestPrevious[$trailName] = [Environment]::GetEnvironmentVariable($trailName, 'Process')
    [Environment]::SetEnvironmentVariable($trailName, $trailTestEnvironment[$trailName], 'Process')
}
$trailBuildBatch = @'
@echo off
call "%TRAIL_TEST_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%TRAIL_TEST_INCLUDE%" /Fe:tank_trail_tests.exe /Fo:.\ "%TRAIL_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
tank_trail_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $trailTestOutput 'build_tank_trail_tests.cmd'), $trailBuildBatch, [Text.Encoding]::ASCII)
try {
    Push-Location -LiteralPath $trailTestOutput
    try {
        & $env:ComSpec /d /c build_tank_trail_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Trail regression failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($trailName in $trailTestEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($trailName, $trailTestPrevious[$trailName], 'Process')
    }
}
