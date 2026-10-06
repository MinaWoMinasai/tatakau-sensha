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
$scenarioParserSource = @'
// This focused translation unit includes the actual implementation once so
// ReadSettings and IntegerField are tested without exposing test-only APIs.
#include "game/debug/GameplayScenarioSession.cpp"
#include <cassert>
#include <iostream>
static nlohmann::json RecordingManifest() {
    return {{"schemaVersion",1},{"scenario","neon_boss"},{"frames",120},{"captureFrames",nlohmann::json::array()},
        {"recording",{{"enabled",true},{"firstFrame",1},{"frameCount",120},{"encodedFps",60},
            {"maxOutputBytes",nlohmann::json::number_integer_t(12ll*1024*1024*1024)}}}};
}
template<class Function> static void MustReject(Function&& function) {
    bool rejected = false;
    try { function(); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}
int main() {
    using nlohmann::json;
    const uint64_t cap = 12ull*1024*1024*1024;
    const auto valid = ReadSettings(RecordingManifest());
    assert(valid.recording.enabled && valid.recording.firstFrame == 1 && valid.recording.frameCount == 120);
    assert(valid.recording.maxOutputBytes == cap && valid.captureFrames.empty());
    // Signed positive JSON values must not be compared with UINT64_MAX cast to int64.
    assert(IntegerField<uint64_t>(json{{"n",json::number_integer_t(cap)}},"n",0) == cap);
    assert(IntegerField<uint64_t>(json{{"n",json::number_unsigned_t(cap)}},"n",0) == cap);
    assert(IntegerField<uint64_t>(json{{"n",json::number_unsigned_t((std::numeric_limits<uint64_t>::max)())}},"n",0) ==
        (std::numeric_limits<uint64_t>::max)());
    assert(IntegerField<int>(json{{"n",-1}},"n",0) == -1);
    assert(IntegerField<uint32_t>(json{{"n",json::number_integer_t(4294967295ll)}},"n",0) == 4294967295u);
    MustReject([] { (void)IntegerField<uint64_t>(json{{"n",-1}},"n",0); });
    MustReject([] { (void)IntegerField<uint32_t>(json{{"n",json::number_unsigned_t(4294967296ull)}},"n",0); });
    MustReject([] { (void)IntegerField<int>(json{{"n",json::number_unsigned_t(2147483648ull)}},"n",0); });
    for (const char* key : {"firstFrame","frameCount","encodedFps","maxOutputBytes"}) {
        for (const json& wrong : {json(-1),json(1.5),json(true),json("1")}) {
            auto object = RecordingManifest(); object["recording"][key] = wrong;
            MustReject([&] { (void)ReadSettings(object); });
        }
    }
    auto invalid = RecordingManifest(); invalid["recording"]["maxOutputBytes"] = json::number_unsigned_t(cap+1);
    MustReject([&] { (void)ReadSettings(invalid); });
    invalid = RecordingManifest(); invalid["recording"]["maxOutputBytes"] = json::number_unsigned_t((std::numeric_limits<uint64_t>::max)());
    MustReject([&] { (void)ReadSettings(invalid); });
    invalid = RecordingManifest(); invalid["recording"]["maxOutputBytes"] = 1.8446744073709552e19;
    MustReject([&] { (void)ReadSettings(invalid); });
    invalid = RecordingManifest(); invalid["recording"]["unknown"] = true;
    MustReject([&] { (void)ReadSettings(invalid); });
    invalid = RecordingManifest(); invalid["recording"]["enabled"] = 1;
    MustReject([&] { (void)ReadSettings(invalid); });
    invalid = RecordingManifest(); invalid["recording"]["frameCount"] = 121;
    MustReject([&] { (void)ReadSettings(invalid); });
    invalid = RecordingManifest(); invalid["recording"]["firstFrame"] = json::number_unsigned_t(4294967295ull);
    MustReject([&] { (void)ReadSettings(invalid); });
    invalid = RecordingManifest(); invalid["recording"]["encodedFps"] = 30;
    MustReject([&] { (void)ReadSettings(invalid); });
    invalid = RecordingManifest(); invalid["captureFrames"] = json::array({120});
    MustReject([&] { (void)ReadSettings(invalid); });
    std::cout << "PASS: actual recording settings parser and integer signedness/overflow\n";
}
'@
[IO.File]::WriteAllText((Join-Path $scenarioOutput 'scenario_recording_parser.cpp'), $scenarioParserSource, [Text.UTF8Encoding]::new($false))
$scenarioBatch = @'
@echo off
call "%SCENARIO_UNIT_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MT /UNDEBUG /DCG2_DEVELOPER_TOOLS=1 /I"%SCENARIO_UNIT_PROJECT%" /I"%SCENARIO_UNIT_PROJECT%\externals" /I"%SCENARIO_UNIT_PROJECT%\DirectX\engine\commom" /Fe:gameplay_scenario_tests.exe /Fo:.\ "%SCENARIO_UNIT_PROJECT%\tools\gameplay_scenario_tests.cpp" "%SCENARIO_UNIT_PROJECT%\game\debug\GameplayScenarioSession.cpp" user32.lib
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MT /UNDEBUG /DCG2_DEVELOPER_TOOLS=1 /I"%SCENARIO_UNIT_PROJECT%" /I"%SCENARIO_UNIT_PROJECT%\externals" /I"%SCENARIO_UNIT_PROJECT%\DirectX\engine\commom" /Fe:scenario_recording_parser.exe /Fo:.\ scenario_recording_parser.cpp user32.lib
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
        & (Join-Path $scenarioOutput 'scenario_recording_parser.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Actual recording parser/signedness contract failed.' }
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
        $scenarioRecordingRun = [Guid]::NewGuid().ToString('N')
        $scenarioRecordingModes = @('recording_valid','recording_late','recording_incomplete','recording_order',
            'recording_duplicate','recording_name','recording_bytes','recording_clock','recording_dimensions',
            'recording_resolution','recording_budget','recording_capture_limit')
        foreach ($recordingMode in $scenarioRecordingModes) {
            $scenarioRecording = ($scenarioManifest | ConvertTo-Json -Depth 12) | ConvertFrom-Json -AsHashtable
            $scenarioRecording.captureFrames = @()
            $scenarioRecording.recording = @{ enabled=$true; firstFrame=1; frameCount=120; encodedFps=60; maxOutputBytes=12884901888 }
            $scenarioRecording.outputDirectory = Join-Path $scenarioOutput ("recording-$scenarioRecordingRun/$recordingMode")
            if ($recordingMode -eq 'recording_late') {
                $scenarioRecording.recording.firstFrame=119; $scenarioRecording.recording.frameCount=2
            }
            if ($recordingMode -eq 'recording_budget') { $scenarioRecording.recording.maxOutputBytes=1 }
            Write-ScenarioManifest $scenarioRecording
            & $scenarioExecutable $recordingMode
            if ($LASTEXITCODE -ne 0) { throw "Actual recording session contract failed: $recordingMode" }
            $recordingReport = Get-Content -LiteralPath (Join-Path $scenarioRecording.outputDirectory 'recording/recording-report.json') -Raw | ConvertFrom-Json
            $recordingSession = Get-Content -LiteralPath (Join-Path $scenarioRecording.outputDirectory 'report.json') -Raw | ConvertFrom-Json
            $recordingSuccess = $recordingMode -in @('recording_valid','recording_late')
            if ($recordingReport.completed -ne $recordingSuccess -or $recordingSession.completed -ne $recordingSuccess -or
                $recordingReport.heldDrawCount -ne 0 -or $recordingReport.comparisonFreeze) { throw "Recording completion/freeze contract failed: $recordingMode" }
            $recordingCount = if ($recordingMode -eq 'recording_late') { 2 } elseif ($recordingMode -eq 'recording_incomplete') { 119 } elseif ($recordingMode -in @('recording_dimensions','recording_budget')) { 0 } else { 120 }
            if ($recordingReport.frameCount -ne $recordingCount -or @($recordingReport.frames).Count -ne $recordingCount -or
                $recordingReport.sourceBytes -ne ($recordingCount * 300)) { throw "Recording rows/source-byte contract failed: $recordingMode" }
            if ($recordingSuccess) {
                if ($recordingReport.status -ne 'COMPLETE' -or $recordingReport.errors.Count -ne 0 -or
                    $recordingReport.frames[0].sequenceFrame -ne 0 -or
                    $recordingReport.frames[0].simulationFrame -ne $scenarioRecording.recording.firstFrame -or
                    $recordingReport.frames[-1].simulationFrame -ne 120 -or
                    [math]::Abs($recordingReport.frames[-1].recordingClocks.gameplayElapsed - 1.5) -gt 1e-6 -or
                    [math]::Abs($recordingReport.frames[-1].recordingClocks.presentationElapsed - 80/60.0) -gt 1e-6) {
                    throw "Recording ordinal/actual clock contract failed: $recordingMode"
                }
            } elseif ($recordingReport.status -ne 'INCOMPLETE' -or $recordingReport.errors.Count -lt 1) {
                throw "Failed recording incorrectly passed: $recordingMode"
            }
            if (!$recordingSession.details.cpuRecordingContract.injectedByteCounts -or
                $recordingSession.details.cpuRecordingContract.gpuFramesProduced -or
                @(Get-ChildItem -LiteralPath (Join-Path $scenarioRecording.outputDirectory 'recording') -Filter '*.png' -Recurse).Count -ne 0) {
                throw 'CPU recording contract test must never be described as actual GPU media evidence.'
            }
            if ($recordingMode -eq 'recording_capture_limit' -and @($recordingSession.captures).Count -ne 64) {
                throw 'The original ReportCapture64 cap was changed by continuous recording.'
            }
        }
        Write-Host 'PASS: actual recording session range/clocks/order/bytes/preflight/failure contracts (CPU only; no PNG/GPU evidence).'
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
