param([ValidateRange(30,180)][int]$TimeoutSeconds = 90)
$ErrorActionPreference = 'Stop'
$bossProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$bossRepo = [IO.Path]::GetFullPath((Join-Path $bossProject '..'))
$bossExe = Join-Path $bossRepo 'generated/outputs/Development/CG2.exe'
$bossDirectory = Join-Path $bossProject 'generated/neon_boss_gameplay'
$bossReportPath = Join-Path $bossDirectory 'validation.json'
if (!(Test-Path -LiteralPath $bossExe)) { throw 'Build Development x64 first.' }
$bossSettings = @{
    CG2_NEON_BOSS_AUTOTEST = '1'
    CG2_TANK_COMBAT_AUTOTEST = $null
    CG2_TANK_SPECIAL_AUTOTEST = $null
    CG2_TANK_EXPERIENCE_AUTOTEST = $null
    CG2_TANK_MAP_AUTOTEST = $null
    CG2_TANK_TUTORIAL_AUTOTEST = $null
    CG2_TANK_AUTOTEST = $null
    CG2_TITLE_AUTOTEST = $null
    CG2_STARTUP_AUTOTEST = $null
    CG2_TANK_EXPEDITION_VARIANT = $null
    CG2_EXPEDITION_TUTORIAL = $null
    CG2_PERF_DISABLED = $null
    CG2_PERF_OVERLAY = '0'
    CG2_PERF_CAPTURE_FRAMES = $null
    CG2_PERF_CAPTURE_PATH = $null
    CG2_PERF_EXIT_AFTER_CAPTURE = $null
    CG2_PERF_STRESS_TRAILS = $null
    CG2_FRAME_LIMIT = '1'
}
$bossPrevious = @{}
$bossProcess = $null
function Assert-Boss([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function Read-BossCapture([string]$Name) {
    foreach ($extension in @('png', 'json')) {
        $path = Join-Path $bossDirectory ($Name + '.' + $extension)
        Assert-Boss ((Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).LastWriteTimeUtc -ge $bossStart) "Missing fresh $extension capture: $Name"
    }
    Assert-Boss ((Get-Item -LiteralPath (Join-Path $bossDirectory ($Name + '.png'))).Length -gt 512) "Empty image: $Name"
    Get-Content -LiteralPath (Join-Path $bossDirectory ($Name + '.json')) -Raw -Encoding UTF8 | ConvertFrom-Json
}
try {
    foreach ($key in $bossSettings.Keys) {
        $bossPrevious[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $bossSettings[$key], 'Process')
    }
    $bossStart = [DateTime]::UtcNow
    $bossProcess = Start-Process -FilePath $bossExe -WorkingDirectory $bossProject -ArgumentList @('--project','resources/projects/tank_expedition.project.json') -WindowStyle Hidden -PassThru
    Write-Host "Actual Neon boss gameplay validation started (PID $($bossProcess.Id), Development x64)."
    while (!$bossProcess.WaitForExit(10000)) {
        if (([DateTime]::UtcNow - $bossStart).TotalSeconds -gt $TimeoutSeconds) {
            Stop-Process -Id $bossProcess.Id -ErrorAction SilentlyContinue
            throw "Neon boss validation exceeded $TimeoutSeconds seconds. Inspect $bossDirectory"
        }
    }
    Assert-Boss ((Test-Path -LiteralPath $bossReportPath) -and (Get-Item -LiteralPath $bossReportPath).LastWriteTimeUtc -ge $bossStart) "No fresh validation report (exit $($bossProcess.ExitCode))."
    $report = Get-Content -LiteralPath $bossReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-Boss ($bossProcess.ExitCode -eq 0 -and $report.completed -and $report.testMode -and @($report.errors).Count -eq 0) "Gameplay validation failed: $($report.errors -join '; ')"
    Assert-Boss (@($report.captures).Count -eq 13) 'Expected 5 same-state comparison pairs and 3 dissolve endpoints.'
    foreach ($name in @('idle','telegraph','attack','dash','low_hp')) {
        $legacy = Read-BossCapture ($name + '_legacy')
        $neon = Read-BossCapture $name
        Assert-Boss (!$legacy.visualEnabled -and $neon.visualEnabled) "ON/OFF comparison missing: $name"
        Assert-Boss ($legacy.samePoseFreeze -and $neon.samePoseFreeze) "Comparison pose was not frozen: $name"
        Assert-Boss (($legacy.gameplay | ConvertTo-Json -Depth 12 -Compress) -ceq ($neon.gameplay | ConvertTo-Json -Depth 12 -Compress)) "Visual toggle changed gameplay state: $name"
        Assert-Boss (($legacy.camera | ConvertTo-Json -Depth 12 -Compress) -ceq ($neon.camera | ConvertTo-Json -Depth 12 -Compress)) "Comparison changed camera: $name"
        Assert-Boss ($legacy.presentation.animation -ceq $neon.presentation.animation -and $neon.presentation.hasResources) "Visual toggle changed animation or resources: $name"
        Assert-Boss ($null -ne $legacy.presentation.animationTime -and $legacy.presentation.animationTime -eq $neon.presentation.animationTime) "Comparison changed animation time: $name"
        Assert-Boss ($null -ne $legacy.presentation.world -and
            ($legacy.presentation.world | ConvertTo-Json -Depth 12 -Compress) -ceq ($neon.presentation.world | ConvertTo-Json -Depth 12 -Compress)) "Comparison changed model World transform: $name"
        Assert-Boss (!$legacy.globalBloomModified -and !$neon.globalBloomModified) "Gameplay Bloom was changed: $name"
        Assert-Boss ($legacy.descriptorCount -eq $neon.descriptorCount) "Visual toggle allocated descriptors: $name"
        Assert-Boss ($neon.presentation.drawConstantBuffers -eq 1) "Per-frame draw buffer arena grew while frozen: $name"
    }
    $zero = Read-BossCapture 'dissolve_000'
    $middle = Read-BossCapture 'dissolve_050'
    $end = Read-BossCapture 'dissolve_100'
    Assert-Boss ($zero.presentation.dissolveProgress -eq 0 -and $middle.presentation.dissolveProgress -ge 0.5 -and $middle.presentation.dissolveProgress -lt 1 -and $end.presentation.dissolveProgress -eq 1) 'Dissolve start/midpoint/end coverage incomplete.'
    foreach ($frame in @($zero,$middle,$end)) {
        Assert-Boss ($frame.gameplay.dead -and $frame.gameplay.hp -eq 0) 'Dissolve kept the gameplay boss alive.'
        Assert-Boss ($frame.gameplay.shots -eq $zero.gameplay.shots -and $frame.gameplay.dashCount -eq $zero.gameplay.dashCount -and
            ($frame.gameplay.position | ConvertTo-Json -Compress) -ceq ($zero.gameplay.position | ConvertTo-Json -Compress)) 'Gameplay moved/fired/dashed during dissolve.'
    }
    Assert-Boss ($end.presentation.finished -and !$end.presentation.hasResources -and $end.presentation.resourceCreates -eq 1 -and $end.presentation.resourceReleases -eq 1) 'Terminal Visual did not release its resources exactly once.'
    Assert-Boss ($end.presentation.drawConstantBuffers -eq 0) 'Terminal Visual retained draw constant buffers.'
    foreach ($name in @('profile_off','profile_on')) {
        $path = Join-Path $bossDirectory ($name + '.csv')
        Assert-Boss ((Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).LastWriteTimeUtc -ge $bossStart) "Missing fresh profiler CSV: $name"
        $rows = @(Import-Csv -LiteralPath $path | Where-Object { $_.category -eq 'frame' -and $_.name -eq 'elapsed_ms' })
        Assert-Boss ($rows.Count -eq 120) "Expected 120 measured frames: $name"
    }
    Write-Host 'PASS: real authored boss room/AI, same-state 2D/3D pairs, HP phase, dash, gameplay death freeze, dissolve endpoints, one resource lifecycle, stable descriptors and ON/OFF GPU profiler CSVs.'
    Write-Host 'HP is injected for phase/death coverage; the stationary player is invulnerable. Inspect PNGs separately for visual quality.'
    Write-Output $bossReportPath
} finally {
    if ($bossProcess -and !$bossProcess.HasExited) { Stop-Process -Id $bossProcess.Id -ErrorAction SilentlyContinue }
    foreach ($key in $bossPrevious.Keys) { [Environment]::SetEnvironmentVariable($key, $bossPrevious[$key], 'Process') }
}
