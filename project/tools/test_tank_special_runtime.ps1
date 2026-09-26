param([ValidateSet('Development','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$specialProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$specialRepo=[IO.Path]::GetFullPath((Join-Path $specialProject '..'))
$specialExe=Join-Path $specialRepo "generated/outputs/$Configuration/CG2.exe"
if(!(Test-Path -LiteralPath $specialExe)){throw 'Build the requested configuration first.'}
$specialSettings=@{
    CG2_TANK_SPECIAL_AUTOTEST='1';CG2_TANK_EXPERIENCE_AUTOTEST=$null;CG2_TANK_COMBAT_AUTOTEST=$null
    CG2_TANK_MAP_AUTOTEST=$null;CG2_TANK_AUTOTEST=$null;CG2_TANK_TUTORIAL_AUTOTEST=$null
    CG2_TITLE_AUTOTEST=$null;CG2_STARTUP_AUTOTEST=$null;CG2_TANK_EXPEDITION_VARIANT=$null
    CG2_PERF_CAPTURE_FRAMES=$null;CG2_PERF_EXIT_AFTER_CAPTURE=$null;CG2_PERF_STRESS_TRAILS=$null
    CG2_PERF_OVERLAY='0';CG2_FRAME_LIMIT='1'
}
$specialPrevious=@{};$specialProcess=$null
try {
    foreach($key in $specialSettings.Keys){$specialPrevious[$key]=[Environment]::GetEnvironmentVariable($key,'Process');[Environment]::SetEnvironmentVariable($key,$specialSettings[$key],'Process')}
    $specialStarted=[DateTime]::UtcNow
    $specialProcess=Start-Process -FilePath $specialExe -WorkingDirectory $specialProject -ArgumentList @('--project','resources/projects/tank_expedition.project.json') -WindowStyle Hidden -PassThru
    Write-Host "Special abilities runtime started (PID $($specialProcess.Id), $Configuration)."
    while(!$specialProcess.WaitForExit(10000)) {
        if(([DateTime]::UtcNow-$specialStarted).TotalSeconds -gt 100){Stop-Process -Id $specialProcess.Id -ErrorAction SilentlyContinue;throw 'Special runtime timeout.'}
    }
    $specialDirectory=Join-Path $specialProject 'generated/special_validation'
    $specialReport=Join-Path $specialDirectory 'validation.json'
    if(!(Test-Path -LiteralPath $specialReport) -or (Get-Item -LiteralPath $specialReport).LastWriteTimeUtc -lt $specialStarted){throw "No fresh report (exit $($specialProcess.ExitCode))."}
    $result=Get-Content -LiteralPath $specialReport -Raw -Encoding UTF8|ConvertFrom-Json
    if($specialProcess.ExitCode -ne 0 -or !$result.completed -or !$result.testMode -or $result.forcedDamage -or @($result.errors).Count -ne 0 -or @($result.probes).Count -ne 4){throw "Special runtime failed (exit $($specialProcess.ExitCode)): $($result.errors -join '; '). Report: $specialReport"}
    foreach($name in @('rail_charge','rail_fire','drone_link','slash_wave','parry_normal','parry_perfect')) {
        $capture=Join-Path $specialDirectory "$name.png"
        if(!(Test-Path -LiteralPath $capture) -or (Get-Item -LiteralPath $capture).LastWriteTimeUtc -lt $specialStarted -or (Get-Item -LiteralPath $capture).Length -lt 512){throw "Missing fresh special screenshot: $capture"}
    }
    foreach($probe in $result.probes){Write-Host ("PASS {0}: damage {1}, rail {2}, links {3}, waves {4}, parries {5} / perfect {6}" -f $probe.id,$probe.damage,$probe.railShots,$probe.linkTicks,$probe.slashWaves,$probe.parries,$probe.perfectParries)}
    Write-Output $specialReport
} finally {
    if($specialProcess -and !$specialProcess.HasExited){Stop-Process -Id $specialProcess.Id -ErrorAction SilentlyContinue}
    foreach($key in $specialPrevious.Keys){[Environment]::SetEnvironmentVariable($key,$specialPrevious[$key],'Process')}
}
