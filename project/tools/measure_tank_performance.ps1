param(
    [ValidateSet('Development','Release')][string]$Configuration = 'Release',
    [ValidateRange(0,512)][int]$Trails = 512,
    [ValidateRange(60,3600)][int]$Frames = 300,
    [switch]$LegacyTrails,
    [switch]$Uncapped
)
$ErrorActionPreference = 'Stop'
$tankPerfProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$tankPerfRepo = [IO.Path]::GetFullPath((Join-Path $tankPerfProject '..'))
$tankPerfExe = Join-Path $tankPerfRepo "generated/outputs/$Configuration/CG2.exe"
if (!(Test-Path -LiteralPath $tankPerfExe)) { throw 'Build the requested configuration first.' }
$tankPerfLabel = "$(if($LegacyTrails){'individual'}else{'batched'})_${Trails}_$(if($Uncapped){'uncapped'}else{'60fps'})"
$tankPerfCsv = Join-Path $tankPerfProject "generated/performance/$tankPerfLabel.csv"
$tankPerfSettings = @{
    CG2_PERF_DISABLED = $null
    CG2_PERF_OVERLAY = '0'
    CG2_PERF_CAPTURE_FRAMES = [string]$Frames
    CG2_PERF_CAPTURE_WARMUP = '120'
    CG2_PERF_CAPTURE_PATH = $tankPerfCsv
    CG2_PERF_EXIT_AFTER_CAPTURE = '1'
    CG2_PERF_STRESS_TRAILS = [string]$Trails
    CG2_TRAIL_BATCHING = $(if($LegacyTrails){'0'}else{'1'})
    CG2_FRAME_LIMIT = $(if($Uncapped){'0'}else{'1'})
    CG2_TANK_AUTOTEST = $null
    CG2_TANK_EXPEDITION_VARIANT = $null
}
$tankPerfPrevious = @{}
try {
    foreach ($tankPerfKey in $tankPerfSettings.Keys) {
        $tankPerfPrevious[$tankPerfKey] = [Environment]::GetEnvironmentVariable($tankPerfKey,'Process')
        [Environment]::SetEnvironmentVariable($tankPerfKey,$tankPerfSettings[$tankPerfKey],'Process')
    }
    $tankPerfStart = [DateTime]::UtcNow
    $tankPerfProcess = Start-Process -FilePath $tankPerfExe -WorkingDirectory $tankPerfProject -ArgumentList @('--project','resources/projects/tank_expedition.project.json') -WindowStyle Hidden -PassThru
    if (!$tankPerfProcess.WaitForExit(180000)) {
        Stop-Process -Id $tankPerfProcess.Id
        throw 'Performance capture exceeded 180 seconds.'
    }
    if ($tankPerfProcess.ExitCode -ne 0 -or !(Test-Path -LiteralPath $tankPerfCsv) -or (Get-Item -LiteralPath $tankPerfCsv).LastWriteTimeUtc -lt $tankPerfStart) {
        throw 'Performance capture failed to produce a fresh CSV.'
    }
    $tankPerfRows = Import-Csv -LiteralPath $tankPerfCsv
    $tankPerfFrameRows = @($tankPerfRows | Where-Object { $_.category -eq 'frame' -and $_.name -eq 'elapsed_ms' })
    if ($tankPerfFrameRows.Count -ne $Frames) { throw "Expected $Frames captured frames." }
    $tankPerfAverage = ($tankPerfFrameRows | ForEach-Object { [double]::Parse($_.value,[Globalization.CultureInfo]::InvariantCulture) } | Measure-Object -Average).Average
    Write-Output ('PASS {0}: {1} frames, {2:F3} ms/frame, {3:F2} FPS' -f $tankPerfLabel,$Frames,$tankPerfAverage,(1000.0/$tankPerfAverage))
    Write-Output $tankPerfCsv
} finally {
    foreach ($tankPerfKey in $tankPerfPrevious.Keys) { [Environment]::SetEnvironmentVariable($tankPerfKey,$tankPerfPrevious[$tankPerfKey],'Process') }
}
