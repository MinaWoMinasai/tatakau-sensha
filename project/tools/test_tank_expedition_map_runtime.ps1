param([ValidateSet('Development','Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$mapProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$mapRepo = [IO.Path]::GetFullPath((Join-Path $mapProject '..'))
$mapExe = Join-Path $mapRepo "generated/outputs/$Configuration/CG2.exe"
$mapDirectory = Join-Path $mapProject 'generated/expedition_map'
$mapResultPath = Join-Path $mapDirectory 'validation.json'
if (!(Test-Path -LiteralPath $mapExe)) { throw 'Build the requested configuration first.' }
$mapSettings = @{
    CG2_TANK_MAP_AUTOTEST = '1'
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
$mapPrevious = @{}
$mapProcess = $null
try {
    foreach ($mapKey in $mapSettings.Keys) {
        $mapPrevious[$mapKey] = [Environment]::GetEnvironmentVariable($mapKey,'Process')
        [Environment]::SetEnvironmentVariable($mapKey,$mapSettings[$mapKey],'Process')
    }
    $mapStart = [DateTime]::UtcNow
    $mapProcess = Start-Process -FilePath $mapExe -WorkingDirectory $mapProject -ArgumentList @('--project','resources/projects/tank_expedition.project.json') -WindowStyle Hidden -PassThru
    Write-Host "Expedition map runtime validation started (PID $($mapProcess.Id), $Configuration)."
    if (!$mapProcess.WaitForExit(180000)) {
        Stop-Process -Id $mapProcess.Id -ErrorAction SilentlyContinue
        throw "Map validation exceeded 180 seconds. Inspect captures and partial result in $mapDirectory"
    }
    if ($mapProcess.ExitCode -ne 0 -or !(Test-Path -LiteralPath $mapResultPath) -or
        (Get-Item -LiteralPath $mapResultPath).LastWriteTimeUtc -lt $mapStart) {
        throw "Map validation did not finish successfully (exit $($mapProcess.ExitCode)). Inspect $mapResultPath"
    }
    $mapResult = Get-Content -LiteralPath $mapResultPath -Raw -Encoding UTF8 | ConvertFrom-Json
    # This scenario intentionally exercises the shipped ten-node route, including
    # purchases, one authored evolution and a repair. Combat is cleared by the test.
    if (!$mapResult.completed -or !$mapResult.testMode -or !$mapResult.forcedCombatClear -or
        $mapResult.level -ne 1 -or $mapResult.experience -ne 0 -or $mapResult.credits -lt 0 -or
        $mapResult.purchases -lt 1 -or $mapResult.evolutions -lt 1 -or $mapResult.repairs -lt 1 -or
        @($mapResult.visited).Count -ne 10 -or @($mapResult.visited | Select-Object -Unique).Count -ne 10 -or
        $mapResult.visited[0] -ne 'outskirts' -or $mapResult.visited[-1] -ne 'core') {
        throw "Map runtime evidence is incomplete: $mapResultPath"
    }
    foreach ($mapState in @('map_','outskirts','first_upgrade','field_repair','evolution','gatekeeper','core')) {
        $mapCapture = Join-Path $mapDirectory "$mapState.png"
        if (!(Test-Path -LiteralPath $mapCapture) -or (Get-Item -LiteralPath $mapCapture).LastWriteTimeUtc -lt $mapStart -or
            (Get-Item -LiteralPath $mapCapture).Length -lt 512) {
            throw "Missing fresh map screenshot: $mapCapture"
        }
    }
    Write-Host "PASS: fresh map, ten visited nodes, currency purchases, authored evolution, repair, boss clear, level 1 / EXP 0. Combat was forced for flow validation."
    Write-Output $mapResultPath
} finally {
    if ($mapProcess -and !$mapProcess.HasExited) { Stop-Process -Id $mapProcess.Id -ErrorAction SilentlyContinue }
    foreach ($mapKey in $mapPrevious.Keys) {
        [Environment]::SetEnvironmentVariable($mapKey,$mapPrevious[$mapKey],'Process')
    }
}
