param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$playerMovementRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$playerMovementOutput = Join-Path $playerMovementRoot 'generated/player_movement_tests'
New-Item -ItemType Directory -Path $playerMovementOutput -Force | Out-Null

# Use the actual engine vector values and math implementations without creating GPU objects.
$playerMovementMath = Get-Content -LiteralPath (Join-Path $playerMovementRoot 'project/DirectX/engine/calc/Calculation.cpp') -Raw -Encoding UTF8
$playerMovementMethods = foreach ($signature in @('Vector3 Add(', 'Vector3 Subtract(', 'Vector3 Multiply(', 'float Length(', 'Vector3 Normalize(')) {
    $start = $playerMovementMath.IndexOf($signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing production math method: $signature" }
    $opening = $playerMovementMath.IndexOf('{', $start)
    $depth = 0
    for ($index = $opening; $index -lt $playerMovementMath.Length; ++$index) {
        if ($playerMovementMath[$index] -eq '{') { ++$depth }
        if ($playerMovementMath[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) { $playerMovementMath.Substring($start, $index - $start + 1); break }
        }
    }
    if ($depth -ne 0) { throw "Unclosed production math method: $signature" }
}
[IO.File]::WriteAllText((Join-Path $playerMovementOutput 'player_component_math.inc'), ($playerMovementMethods -join "`n"), [Text.UTF8Encoding]::new($false))
if (!$VisualStudioPath) {
    $playerMovementVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $playerMovementVsWhere) {
        $VisualStudioPath = (& $playerMovementVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    }
}
if (!$VisualStudioPath) { throw 'Existing Visual Studio C++ installation not found.' }
$playerMovementEnvironment = @{
    PLAYER_MOVEMENT_TEST_VS = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    PLAYER_MOVEMENT_TEST_SOURCE = Join-Path $PSScriptRoot 'player_movement_tests.cpp'
    PLAYER_MOVEMENT_TEST_INCLUDE = Join-Path $playerMovementRoot 'project'
    PLAYER_MOVEMENT_TEST_MATH = Join-Path $playerMovementRoot 'project/DirectX/engine/calc'
    PLAYER_MOVEMENT_TEST_TYPES = Join-Path $playerMovementRoot 'project/DirectX/engine/struct'
    PLAYER_MOVEMENT_TEST_OUTPUT = $playerMovementOutput
}
$playerMovementPrevious = @{}
foreach ($name in $playerMovementEnvironment.Keys) {
    $playerMovementPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $playerMovementEnvironment[$name], 'Process')
}
$playerMovementBatch = @'
@echo off
call "%PLAYER_MOVEMENT_TEST_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%PLAYER_MOVEMENT_TEST_INCLUDE%" /I"%PLAYER_MOVEMENT_TEST_MATH%" /I"%PLAYER_MOVEMENT_TEST_TYPES%" /I"%PLAYER_MOVEMENT_TEST_OUTPUT%" /Fe:player_movement_tests.exe /Fo:.\ "%PLAYER_MOVEMENT_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
player_movement_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $playerMovementOutput 'build_player_movement_tests.cmd'), $playerMovementBatch, [Text.Encoding]::ASCII)
try {
    Push-Location -LiteralPath $playerMovementOutput
    try {
        & $env:ComSpec /d /c build_player_movement_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Player movement regression failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $playerMovementEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $playerMovementPrevious[$name], 'Process')
    }
}
