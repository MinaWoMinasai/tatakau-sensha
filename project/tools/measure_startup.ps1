param(
    [ValidateSet('Release','Development')][string]$Configuration = 'Release',
    [ValidateSet('Compare','Baseline','Cold','Warm')][string]$Mode = 'Compare',
    [string]$OutputDirectory = '',
    [string]$CacheDirectory = ''
)
$ErrorActionPreference = 'Stop'
$startupProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$startupRoot = [IO.Path]::GetFullPath((Join-Path $startupProject '..'))
$startupExe = Join-Path $startupRoot "generated/outputs/$Configuration/CG2.exe"
if (!(Test-Path -LiteralPath $startupExe)) { throw "Build $Configuration first." }
if (!$OutputDirectory) { $OutputDirectory = Join-Path $startupRoot ('generated/startup_measurements/' + [DateTime]::Now.ToString('yyyyMMdd-HHmmss')) }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (!$CacheDirectory) { $CacheDirectory = Join-Path $OutputDirectory 'cache' }
$CacheDirectory = [IO.Path]::GetFullPath($CacheDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$startupVariables = @('CG2_STARTUP_AUTOTEST','CG2_STARTUP_CACHE','CG2_STARTUP_TRACE','CG2_STARTUP_TRACE_PATH','CG2_SHADER_CACHE_DIR','CG2_TEXTURE_CACHE_DIR',
    'CG2_TITLE_AUTOTEST','CG2_TANK_AUTOTEST','CG2_TANK_TUTORIAL_AUTOTEST','CG2_TANK_MAP_AUTOTEST','CG2_TANK_COMBAT_AUTOTEST','CG2_TANK_EXPERIENCE_AUTOTEST',
    'CG2_PERF_EXIT_AFTER_CAPTURE','CG2_PERF_STRESS_TRAILS','CG2_FRAME_LIMIT')
$startupSaved = @{}
foreach ($name in $startupVariables) { $startupSaved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$startupResults = @()
try {
    foreach ($name in $startupVariables) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    $env:CG2_STARTUP_AUTOTEST = '1'
    $env:CG2_STARTUP_TRACE = '1'
    $env:CG2_SHADER_CACHE_DIR = Join-Path $CacheDirectory 'shaders'
    $env:CG2_TEXTURE_CACHE_DIR = Join-Path $CacheDirectory 'textures'
    $startupModes = if ($Mode -eq 'Compare') { @('Baseline','Cold','Warm') } else { @($Mode) }
    $startupCacheExists = Test-Path -LiteralPath $CacheDirectory
    if ($startupCacheExists -and !(Test-Path -LiteralPath $CacheDirectory -PathType Container)) {
        throw 'CacheDirectory must be a directory.'
    }
    $startupCacheNonempty = $startupCacheExists -and @(Get-ChildItem -LiteralPath $CacheDirectory -Force | Select-Object -First 1).Count -gt 0
    if (($Mode -eq 'Compare' -or $Mode -eq 'Cold') -and $startupCacheNonempty) {
        throw 'Cold comparison requires an empty cache directory. Choose a new -CacheDirectory; existing caches will not be deleted.'
    }
    if ($Mode -eq 'Warm' -and (!$startupCacheExists -or
        @(Get-ChildItem -LiteralPath $CacheDirectory -File -Recurse -Force | Select-Object -First 1).Count -eq 0)) {
        throw 'Warm measurement requires an existing populated cache. First run -Mode Cold, then use its -CacheDirectory.'
    }
    foreach ($sample in $startupModes) {
        $env:CG2_STARTUP_CACHE = if ($sample -eq 'Baseline') { '0' } else { '1' }
        $env:CG2_STARTUP_TRACE_PATH = Join-Path $OutputDirectory ($sample.ToLowerInvariant() + '.json')
        $started = [DateTime]::UtcNow
        $wall = [Diagnostics.Stopwatch]::StartNew()
        $gameProcess = Start-Process -FilePath $startupExe -WorkingDirectory $startupProject -ArgumentList @('--project','resources/projects/tank_game.project.json') -WindowStyle Hidden -PassThru
        Write-Host "Measuring $sample (PID $($gameProcess.Id)): title first frame and normal Game Start transition..."
        if (!$gameProcess.WaitForExit(300000)) {
            Stop-Process -Id $gameProcess.Id
            throw "$sample exceeded 300 seconds. Inspect the partial startup trace."
        }
        $wall.Stop()
        if ($gameProcess.ExitCode -ne 0) { throw "$sample failed with exit code $($gameProcess.ExitCode)." }
        if (!(Test-Path -LiteralPath $env:CG2_STARTUP_TRACE_PATH) -or (Get-Item -LiteralPath $env:CG2_STARTUP_TRACE_PATH).LastWriteTimeUtc -lt $started) {
            throw "No fresh trace for $sample."
        }
        $trace = Get-Content -LiteralPath $env:CG2_STARTUP_TRACE_PATH -Raw -Encoding UTF8 | ConvertFrom-Json
        $title = @($trace.events | Where-Object name -eq 'first_frame.TITLE')
        $request = @($trace.events | Where-Object name -eq 'transition.request.TANK_EXPEDITION')
        $playable = @($trace.events | Where-Object name -eq 'first_frame.TANK_EXPEDITION')
        $init = @($trace.events | Where-Object name -eq 'Scene.Initialize.TANK_EXPEDITION')
        if ($title.Count -ne 1 -or $request.Count -ne 1 -or $playable.Count -ne 1 -or $init.Count -ne 1) {
            throw "Incomplete title/start transition trace: $sample."
        }
        $startupResults += [pscustomobject]@{
            sample = $sample
            titleSeconds = [Math]::Round($title[0].startMs / 1000, 3)
            gameStartSeconds = [Math]::Round(($playable[0].startMs - $request[0].startMs) / 1000, 3)
            sceneInitializeSeconds = [Math]::Round($init[0].durationMs / 1000, 3)
            processSeconds = [Math]::Round($wall.Elapsed.TotalSeconds, 3)
            counters = $trace.counters
            topSelf = @($trace.events | Sort-Object selfMs -Descending | Select-Object -First 20 name,startMs,durationMs,selfMs)
            trace = $env:CG2_STARTUP_TRACE_PATH
        }
        $startupResults[-1] | Select-Object sample,titleSeconds,gameStartSeconds,sceneInitializeSeconds | Format-Table -AutoSize
    }
    $startupResults | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'summary.json') -Encoding UTF8
    Write-Host "Measurements saved: $OutputDirectory"
} finally {
    foreach ($name in $startupVariables) { [Environment]::SetEnvironmentVariable($name, $startupSaved[$name], 'Process') }
}
