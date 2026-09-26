param([ValidateSet('Development','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$combatProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$combatRepo = [IO.Path]::GetFullPath((Join-Path $combatProject '..'))
$combatExe = Join-Path $combatRepo "generated/outputs/$Configuration/CG2.exe"
$combatDirectory = Join-Path $combatProject 'generated/combat_validation'
$combatResultPath = Join-Path $combatDirectory 'validation.json'
if (!(Test-Path -LiteralPath $combatExe)) { throw 'Build the requested configuration first.' }
$combatSettings = @{
    CG2_TANK_SPECIAL_AUTOTEST = $null
    CG2_TANK_COMBAT_AUTOTEST = '1'
    CG2_TANK_EXPERIENCE_AUTOTEST = $null
    CG2_TANK_MAP_AUTOTEST = $null
    CG2_TANK_AUTOTEST = $null
    CG2_TANK_TUTORIAL_AUTOTEST = $null
    CG2_TITLE_AUTOTEST = $null
    CG2_TANK_EXPEDITION_VARIANT = $null
    CG2_EXPEDITION_TUTORIAL = $null
    CG2_PERF_DISABLED = $null
    CG2_PERF_OVERLAY = '0'
    CG2_PERF_CAPTURE_FRAMES = $null
    CG2_PERF_EXIT_AFTER_CAPTURE = $null
    CG2_PERF_STRESS_TRAILS = $null
    CG2_FRAME_LIMIT = '1'
}
$combatPrevious = @{}
$combatProcess = $null
try {
    foreach ($combatKey in $combatSettings.Keys) {
        $combatPrevious[$combatKey] = [Environment]::GetEnvironmentVariable($combatKey,'Process')
        [Environment]::SetEnvironmentVariable($combatKey,$combatSettings[$combatKey],'Process')
    }
    $combatStart = [DateTime]::UtcNow
    $combatProcess = Start-Process -FilePath $combatExe -WorkingDirectory $combatProject -ArgumentList @('--project','resources/projects/tank_expedition.project.json') -WindowStyle Hidden -PassThru
    Write-Host "Actual combat AI validation started (PID $($combatProcess.Id), $Configuration)."
    # Each wait is bounded so hosts can keep reporting progress.
    while (!$combatProcess.WaitForExit(10000)) {
        if (([DateTime]::UtcNow - $combatStart).TotalSeconds -gt 150) {
            Stop-Process -Id $combatProcess.Id -ErrorAction SilentlyContinue
            throw "Combat validation exceeded 150 seconds. Inspect $combatDirectory"
        }
    }
    if (!(Test-Path -LiteralPath $combatResultPath) -or (Get-Item -LiteralPath $combatResultPath).LastWriteTimeUtc -lt $combatStart) {
        throw "Combat validation produced no fresh report (exit $($combatProcess.ExitCode))."
    }
    $combatResult = Get-Content -LiteralPath $combatResultPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($combatProcess.ExitCode -ne 0 -or !$combatResult.completed -or !$combatResult.testMode -or
        $combatResult.forcedCombatClear -or !$combatResult.phase2HpInjection -or
        @($combatResult.errors).Count -ne 0 -or @($combatResult.probes).Count -ne 6) {
        throw "Combat AI validation failed: $($combatResult.errors -join '; '). Report: $combatResultPath"
    }
    foreach ($combatProbe in $combatResult.probes) {
        if ($combatProbe.pathLength -lt 1 -or $combatProbe.wallIntersections -ne 0 -or
            $combatProbe.tunnelingViolations -ne 0 -or $combatProbe.reloadViolations -ne 0 -or
            $combatProbe.playerBulletSamples -ne 0) {
            throw "Movement/collision/reload evidence incomplete: $($combatProbe.id)"
        }
        if ($combatProbe.id -ne 'Charger' -and
            ($combatProbe.shots -lt 2 -or $combatProbe.reloads -lt 1 -or $combatProbe.projectileSamples -lt 1)) {
            throw "Real projectile/reload evidence incomplete: $($combatProbe.id)"
        }
        Write-Host ("{0}: movement {1:N1}, shots {2}, dashes {3}, reloads {4}, max bullets {5}" -f
            $combatProbe.id,$combatProbe.pathLength,$combatProbe.shots,$combatProbe.dashes,$combatProbe.reloads,$combatProbe.maxProjectiles)
    }
    $combatBoss = @($combatResult.probes | Where-Object id -eq 'Rival')[0]
    if (!$combatBoss.phase2 -or $combatBoss.patternMask -ne 7 -or $combatBoss.dashes -lt 1) {
        throw 'Rival phase/pattern/dash evidence incomplete.'
    }
    foreach ($combatCaptureName in @('map','Charger','Sniper','Skirmisher','Flanker','Suppressor','Rival','Rival_dash','Rival_reload','Rival_phase2')) {
        $combatCapture = Join-Path $combatDirectory "$combatCaptureName.png"
        if (!(Test-Path -LiteralPath $combatCapture) -or (Get-Item -LiteralPath $combatCapture).LastWriteTimeUtc -lt $combatStart -or
            (Get-Item -LiteralPath $combatCapture).Length -lt 512) {
            throw "Missing fresh combat screenshot: $combatCapture"
        }
    }
    Write-Host 'PASS: actual moving AI, real bullets, finite magazines/reloads, dashes, solid-wall collision and rival second phase. Invulnerable moving target; no player shooting or forced kills. Boss HP was injected for phase coverage.'
    Write-Output $combatResultPath
} finally {
    if ($combatProcess -and !$combatProcess.HasExited) { Stop-Process -Id $combatProcess.Id -ErrorAction SilentlyContinue }
    foreach ($combatKey in $combatPrevious.Keys) {
        [Environment]::SetEnvironmentVariable($combatKey,$combatPrevious[$combatKey],'Process')
    }
}
