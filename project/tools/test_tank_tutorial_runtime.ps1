param([ValidateSet('Development','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$tutorialProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$tutorialRepo = [IO.Path]::GetFullPath((Join-Path $tutorialProject '..'))
$tutorialExe = Join-Path $tutorialRepo "generated/outputs/$Configuration/CG2.exe"
$tutorialDirectory = Join-Path $tutorialProject 'generated/tank_expedition/tutorial_validation'
$tutorialResultPath = Join-Path $tutorialDirectory 'validation.json'
if (!(Test-Path -LiteralPath $tutorialExe)) { throw 'Build the requested configuration first.' }
$tutorialSettings = @{
    CG2_TANK_TUTORIAL_AUTOTEST = '1'
    CG2_TANK_AUTOTEST = $null
    CG2_TITLE_AUTOTEST = $null
    CG2_TANK_EXPEDITION_VARIANT = $null
    CG2_EXPEDITION_TUTORIAL = '1'
    CG2_PERF_DISABLED = $null
    CG2_PERF_OVERLAY = '0'
    CG2_PERF_CAPTURE_FRAMES = $null
    CG2_PERF_EXIT_AFTER_CAPTURE = $null
    CG2_PERF_STRESS_TRAILS = $null
    CG2_FRAME_LIMIT = '1'
}
$tutorialPrevious = @{}
try {
    foreach ($tutorialKey in $tutorialSettings.Keys) {
        $tutorialPrevious[$tutorialKey] = [Environment]::GetEnvironmentVariable($tutorialKey,'Process')
        [Environment]::SetEnvironmentVariable($tutorialKey,$tutorialSettings[$tutorialKey],'Process')
    }
    $tutorialStart = [DateTime]::UtcNow
    $tutorialProcess = Start-Process -FilePath $tutorialExe -WorkingDirectory $tutorialProject -ArgumentList @('--project','resources/projects/tank_expedition.project.json') -WindowStyle Hidden -PassThru
    Write-Host "Tutorial runtime validation started (PID $($tutorialProcess.Id), $Configuration)."
    if (!$tutorialProcess.WaitForExit(150000)) {
        Stop-Process -Id $tutorialProcess.Id
        throw "Tutorial validation exceeded 150 seconds. Inspect captures and partial result in $tutorialDirectory"
    }
    if ($tutorialProcess.ExitCode -ne 0 -or !(Test-Path -LiteralPath $tutorialResultPath) -or
        (Get-Item -LiteralPath $tutorialResultPath).LastWriteTimeUtc -lt $tutorialStart) {
        throw "Tutorial validation did not finish successfully (exit $($tutorialProcess.ExitCode)). Inspect $tutorialResultPath"
    }
    $tutorialResult = Get-Content -LiteralPath $tutorialResultPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if (!$tutorialResult.completed -or $tutorialResult.observedStepMask -ne 255 -or $tutorialResult.realProjectileKills -lt 1 -or
        $tutorialResult.movementDistance -lt 5 -or $tutorialResult.route -lt 0 -or $tutorialResult.cards -lt 1 -or
        $tutorialResult.tutorialVisible -or $tutorialResult.forcedDamage) {
        throw "Tutorial runtime evidence is incomplete: $tutorialResultPath"
    }
    foreach ($tutorialStep in 1..8) {
        $tutorialCapture = Join-Path $tutorialDirectory "step_$tutorialStep.png"
        if (!(Test-Path -LiteralPath $tutorialCapture) -or (Get-Item -LiteralPath $tutorialCapture).LastWriteTimeUtc -lt $tutorialStart -or
            (Get-Item -LiteralPath $tutorialCapture).Length -lt 512) {
            throw "Missing fresh tutorial screenshot: $tutorialCapture"
        }
    }
    Write-Host "PASS: real movement, projectile kills, dash, room clear, route, reward, COMPLETE, and hidden tutorial UI."
    Write-Output $tutorialResultPath
} finally {
    foreach ($tutorialKey in $tutorialPrevious.Keys) {
        [Environment]::SetEnvironmentVariable($tutorialKey,$tutorialPrevious[$tutorialKey],'Process')
    }
}
