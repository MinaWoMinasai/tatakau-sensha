param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$droneLifecycleRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$droneLifecycleOutput = Join-Path $droneLifecycleRoot 'generated/player_drone_lifecycle_tests'
New-Item -ItemType Directory -Path $droneLifecycleOutput -Force | Out-Null
$droneLifecycleSource = Get-Content -LiteralPath (Join-Path $droneLifecycleRoot 'project/game/player/actor/PlayerDrone.cpp') -Raw -Encoding UTF8
# Execute unedited production lifecycle and attack bodies. GPU, Stage and bullet
# dispatch adapters expose whether a dead drone tries to move or create a shot.
$droneLifecycleMethods = foreach ($signature in @('void PlayerDrone::Attack(', 'void PlayerDrone::Update(', 'void PlayerDrone::OnCollision(', 'void PlayerDrone::Damage(', 'void PlayerDrone::Die(')) {
    $start = $droneLifecycleSource.IndexOf($signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing production drone method: $signature" }
    $opening = $droneLifecycleSource.IndexOf('{', $start)
    $depth = 0
    for ($index = $opening; $index -lt $droneLifecycleSource.Length; ++$index) {
        if ($droneLifecycleSource[$index] -eq '{') { ++$depth }
        if ($droneLifecycleSource[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) {
                $method = $droneLifecycleSource.Substring($start, $index - $start + 1)
                if ($signature -ceq 'void PlayerDrone::Update(') {
                    # Only this unedited baseline method shadows the member dir.
                    "#pragma warning(push)`n#pragma warning(disable:4458)`n$method`n#pragma warning(pop)"
                } else { $method }
                break
            }
        }
    }
    if ($depth -ne 0) { throw "Unclosed production drone method: $signature" }
}
[IO.File]::WriteAllText((Join-Path $droneLifecycleOutput 'drone_lifecycle_methods.inc'), ($droneLifecycleMethods -join "`n`n"), [Text.UTF8Encoding]::new($false))
if (!$VisualStudioPath) {
    $droneLifecycleVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $droneLifecycleVsWhere) {
        $VisualStudioPath = (& $droneLifecycleVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    }
}
if (!$VisualStudioPath) { throw 'Existing Visual Studio C++ installation not found.' }
$droneLifecycleEnvironment = @{
    DRONE_LIFECYCLE_TEST_VS = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    DRONE_LIFECYCLE_TEST_SOURCE = Join-Path $PSScriptRoot 'player_drone_lifecycle_tests.cpp'
    DRONE_LIFECYCLE_TEST_INCLUDE = Join-Path $droneLifecycleRoot 'project'
    DRONE_LIFECYCLE_TEST_OUTPUT = $droneLifecycleOutput
}
$droneLifecyclePrevious = @{}
foreach ($name in $droneLifecycleEnvironment.Keys) {
    $droneLifecyclePrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $droneLifecycleEnvironment[$name], 'Process')
}
# C4458 is scoped to the unchanged Update body above; all other methods and
# every other W4 warning remain errors.
$droneLifecycleBatch = @'
@echo off
call "%DRONE_LIFECYCLE_TEST_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%DRONE_LIFECYCLE_TEST_INCLUDE%" /I"%DRONE_LIFECYCLE_TEST_OUTPUT%" /Fe:player_drone_lifecycle_tests.exe /Fo:.\ "%DRONE_LIFECYCLE_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
player_drone_lifecycle_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $droneLifecycleOutput 'build_player_drone_lifecycle_tests.cmd'), $droneLifecycleBatch, [Text.Encoding]::ASCII)
try {
    Push-Location -LiteralPath $droneLifecycleOutput
    try {
        & $env:ComSpec /d /c build_player_drone_lifecycle_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Drone lifecycle regression failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $droneLifecycleEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $droneLifecyclePrevious[$name], 'Process')
    }
}
