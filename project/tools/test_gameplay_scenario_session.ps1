param([string]$VisualStudioPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$scenarioRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$scenarioProject = Join-Path $scenarioRepo 'project'
$scenarioOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $scenarioRepo 'generated/gameplay_scenario_tests' }
New-Item -ItemType Directory -Path $scenarioOutput -Force | Out-Null
if (!$VisualStudioPath) {
    $scenarioWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $scenarioWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$scenarioDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $scenarioDevCmd)) { throw 'Visual Studio C++ tools are required.' }
$scenarioGuardSource = @'
#include "game/debug/GameplayScenarioSession.h"
#include "DirectX/engine/input/Input.h"
class GameplayScenarioSession;
template<class T> concept Complete = requires { sizeof(T); };
template<class T> concept HasValidationInput = requires(T& input) { input.OverrideValidationFrame(cg2::Vector2{}); };
static_assert(Complete<GameplayScenarioSession> == static_cast<bool>(SCENARIO_EXPECTED_API));
static_assert(HasValidationInput<cg2::Input> == static_cast<bool>(SCENARIO_EXPECTED_API));
'@
[IO.File]::WriteAllText((Join-Path $scenarioOutput 'scenario_guards.cpp'), $scenarioGuardSource, [Text.UTF8Encoding]::new($false))
$scenarioBatch = @'
@echo off
call "%SCENARIO_UNIT_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MT /UNDEBUG /DCG2_DEVELOPER_TOOLS=1 /I"%SCENARIO_UNIT_PROJECT%" /I"%SCENARIO_UNIT_PROJECT%\externals" /I"%SCENARIO_UNIT_PROJECT%\DirectX\engine\commom" /Fe:gameplay_scenario_tests.exe /Fo:.\ "%SCENARIO_UNIT_PROJECT%\tools\gameplay_scenario_tests.cpp" "%SCENARIO_UNIT_PROJECT%\game\debug\GameplayScenarioSession.cpp" user32.lib
if errorlevel 1 exit /b %errorlevel%
set "SCENARIO_GUARD_COMMON=/nologo /c /std:c++20 /utf-8 /EHsc /W4 /WX /I"%SCENARIO_UNIT_PROJECT%" /I"%SCENARIO_UNIT_PROJECT%\externals" /I"%SCENARIO_UNIT_PROJECT%\DirectX\engine\commom" /I"%SCENARIO_UNIT_PROJECT%\DirectX\engine\struct""
cl %SCENARIO_GUARD_COMMON% /UNDEBUG /DCG2_DEVELOPER_TOOLS=1 /DSCENARIO_EXPECTED_API=1 /Fo:guard_development.obj scenario_guards.cpp
if errorlevel 1 exit /b %errorlevel%
cl %SCENARIO_GUARD_COMMON% /DNDEBUG /DCG2_DEVELOPER_TOOLS=0 /DSCENARIO_EXPECTED_API=0 /Fo:guard_release.obj scenario_guards.cpp
if errorlevel 1 exit /b %errorlevel%
cl %SCENARIO_GUARD_COMMON% /DNDEBUG /DCG2_DEVELOPER_TOOLS=1 /DSCENARIO_EXPECTED_API=0 /Fo:guard_release_override.obj scenario_guards.cpp
if errorlevel 1 exit /b %errorlevel%
cl %SCENARIO_GUARD_COMMON% /UNDEBUG /DCG2_DEVELOPER_TOOLS=0 /DSCENARIO_EXPECTED_API=0 /Fo:guard_development_disabled.obj scenario_guards.cpp
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $scenarioOutput 'build.cmd'), $scenarioBatch, [Text.Encoding]::ASCII)
$scenarioSettings = @{ SCENARIO_UNIT_DEV_CMD=$scenarioDevCmd; SCENARIO_UNIT_PROJECT=$scenarioProject }
$scenarioPrevious = @{}
$scenarioOldManifest = [Environment]::GetEnvironmentVariable('CG2_GAMEPLAY_SCENARIO', 'Process')
try {
    foreach ($name in $scenarioSettings.Keys) {
        $scenarioPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $scenarioSettings[$name], 'Process')
    }
    Push-Location -LiteralPath $scenarioOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Gameplay scenario isolated build failed: $LASTEXITCODE" }
        Write-Host 'PASS: actual Session/Input APIs compile only in Development with tools enabled (four guard profiles).'
        $scenarioExecutable = Join-Path $scenarioOutput 'gameplay_scenario_tests.exe'
        [Environment]::SetEnvironmentVariable('CG2_GAMEPLAY_SCENARIO', $null, 'Process')
        & $scenarioExecutable disabled
        if ($LASTEXITCODE -ne 0) { throw 'Disabled scenario session failed.' }
        $scenarioManifest = [ordered]@{
            schemaVersion=1; scenario='drone'; frames=120; captureFrames=@(0,120)
            upgrades=@('shooter_damage_1','shared_speed_1'); room='arena'
            enemyWave=@(@{type='tutorial_target';position=@(3,4,0);hp=100}, @{type='special_target';position=@(5,6,0);hp=200})
            input=@(@{firstFrame=3;endFrame=7;movement=@(-1,1);aim=@(9,8,0);shoot=$false;dash=$true})
            outputDirectory=(Join-Path $scenarioOutput '設定と入力')
        }
        $scenarioManifestPath = Join-Path $scenarioOutput 'シナリオ設定.json'
        function Write-ScenarioManifest($manifest) {
            [IO.File]::WriteAllText($scenarioManifestPath, ($manifest | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
            [Environment]::SetEnvironmentVariable('CG2_GAMEPLAY_SCENARIO', $scenarioManifestPath, 'Process')
        }
        Write-ScenarioManifest $scenarioManifest
        & $scenarioExecutable valid
        if ($LASTEXITCODE -ne 0) { throw 'Valid scenario session failed.' }
        $scenarioReport = Get-Content -LiteralPath (Join-Path $scenarioManifest.outputDirectory 'report.json') -Raw | ConvertFrom-Json
        $scenarioSnapshots = Get-Content -LiteralPath (Join-Path $scenarioManifest.outputDirectory 'snapshots.json') -Raw | ConvertFrom-Json
        if (!$scenarioReport.completed -or $scenarioReport.frameCount -ne 120 -or $scenarioReport.sceneEpochs -ne 2 -or $scenarioSnapshots.Count -ne 120) { throw 'Actual JSON report/session restart contract failed.' }
        if ($scenarioSnapshots[0].frame -ne 1 -or $scenarioSnapshots[-1].frame -ne 120 -or !$scenarioReport.sawPlayerDeath) { throw 'Actual snapshot/death evidence failed.' }
        $scenarioDefaultCapture = ($scenarioManifest | ConvertTo-Json -Depth 12) | ConvertFrom-Json -AsHashtable
        [void]$scenarioDefaultCapture.Remove('captureFrames')
        Write-ScenarioManifest $scenarioDefaultCapture
        & $scenarioExecutable valid
        if ($LASTEXITCODE -ne 0) { throw 'Short duration with default captures was rejected.' }
        $scenarioDefaultReport = Get-Content -LiteralPath (Join-Path $scenarioManifest.outputDirectory 'report.json') -Raw | ConvertFrom-Json
        if (@($scenarioDefaultReport.settings.captureFrames).Count -ne 1 -or $scenarioDefaultReport.settings.captureFrames[0] -ne 120) { throw 'Default captures exceeded the configured duration.' }
        Write-ScenarioManifest $scenarioManifest
        & $scenarioExecutable failure
        if ($LASTEXITCODE -ne 0) { throw 'Failed adapter reporting test failed.' }
        $scenarioFailed = Get-Content -LiteralPath (Join-Path $scenarioManifest.outputDirectory 'report.json') -Raw | ConvertFrom-Json
        if ($scenarioFailed.completed -or $scenarioFailed.errors.Count -ne 2) { throw 'Failure report incorrectly passed.' }
        $scenarioInvalidMutations = @(
            { param($m) $m.scenario='unknown' },
            { param($m) $m.frames=-1 },
            { param($m) $m.frames=120.5 },
            { param($m) $m.fixedDeltaTime=1e99 },
            { param($m) $m.playerHp=0 },
            { param($m) $m.upgrades=@(1,2) },
            { param($m) $m.input[0].shoot=1 },
            { param($m) $m.enemyWave[0].position=@(1,2) },
            { param($m) $m.outputDirectory='../outside_generated' },
            { param($m) $m.captureFrames=@(120,120) },
            { param($m) $m.unknownSetting=1 }
        )
        foreach ($mutation in $scenarioInvalidMutations) {
            $scenarioInvalid = ($scenarioManifest | ConvertTo-Json -Depth 12) | ConvertFrom-Json -AsHashtable
            & $mutation $scenarioInvalid
            Write-ScenarioManifest $scenarioInvalid
            & $scenarioExecutable invalid
            if ($LASTEXITCODE -ne 0) { throw 'Malformed manifest was accepted or failed unsafely.' }
            if (@($scenarioInvalid.captureFrames).Count -eq 2 -and $scenarioInvalid.captureFrames[0] -eq $scenarioInvalid.captureFrames[1]) {
                $scenarioValidationFailure = Get-Content -LiteralPath (Join-Path $scenarioRepo 'generated/scenario_error/report.json') -Raw | ConvertFrom-Json
                if ($scenarioValidationFailure.errors[0] -notlike '*Capture frames must be ordered, unique*') { throw 'Settings validation lost its precise error diagnostic.' }
            }
        }
        [IO.File]::WriteAllText($scenarioManifestPath, '{ broken JSON', [Text.UTF8Encoding]::new($false))
        & $scenarioExecutable invalid
        if ($LASTEXITCODE -ne 0) { throw 'Invalid JSON syntax was accepted.' }
        Write-Host 'PASS: actual Unicode manifest/output, session restart, snapshots, failed reports and 12 malformed manifests.'
    } finally { Pop-Location }
} finally {
    [Environment]::SetEnvironmentVariable('CG2_GAMEPLAY_SCENARIO', $scenarioOldManifest, 'Process')
    foreach ($name in $scenarioPrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $scenarioPrevious[$name], 'Process') }
}
