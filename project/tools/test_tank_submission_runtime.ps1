param([Parameter(Mandatory=$true)][string]$PackageDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'TankSubmissionPackage.ps1')
$submissionAudit=Assert-TankSubmissionPackage $PackageDirectory
$submissionRoot=[IO.Path]::GetFullPath($PackageDirectory)
$submissionExe=Join-Path $submissionRoot 'CG2.exe'
$submissionOutput=Join-Path $submissionRoot 'generated/submission_validation'
New-Item -ItemType Directory -Path $submissionOutput -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $submissionOutput 'isolated-profile'),'QA copy; never submit this played directory.')
$submissionLaunch=Join-Path $submissionRoot 'generated/launch_from_unrelated_cwd'
New-Item -ItemType Directory -Path $submissionLaunch -Force | Out-Null
$submissionSettings=@{
    CG2_SUBMISSION_AUTOTEST='1'; CG2_TITLE_AUTOTEST=$null; CG2_STARTUP_AUTOTEST=$null
    CG2_TANK_AUTOTEST=$null; CG2_TANK_EXPERIENCE_AUTOTEST=$null; CG2_TANK_SPECIAL_AUTOTEST=$null
    CG2_TANK_MAP_AUTOTEST=$null; CG2_TANK_COMBAT_AUTOTEST=$null; CG2_TANK_TUTORIAL_AUTOTEST=$null
    CG2_TANK_EXPERIENCE_STYLE=$null; CG2_EXPEDITION_TUTORIAL=$null; CG2_TANK_EXPEDITION_VARIANT=$null
    CG2_PERF_CAPTURE_FRAMES=$null;CG2_PERF_EXIT_AFTER_CAPTURE=$null;CG2_PERF_STRESS_TRAILS=$null
    CG2_FRAME_LIMIT='1';CG2_PERF_OVERLAY='0'
}
$submissionPrevious=@{};$submissionProcess=$null
try {
    foreach($name in $submissionSettings.Keys) {
        $submissionPrevious[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
        [Environment]::SetEnvironmentVariable($name,$submissionSettings[$name],'Process')
    }
    $submissionStarted=[DateTime]::UtcNow
    $submissionProcess=Start-Process -FilePath $submissionExe -WorkingDirectory $submissionLaunch -WindowStyle Hidden -PassThru
    Write-Host "Submission walkthrough started (PID $($submissionProcess.Id)). Isolated first profile, no project arguments, unrelated cwd."
    while(!$submissionProcess.WaitForExit(10000)) {
        if(([DateTime]::UtcNow-$submissionStarted).TotalSeconds -gt 280){throw 'Submission walkthrough exceeded 280 seconds.'}
    }
    $submissionReportPath=Join-Path $submissionOutput 'validation.json'
    if(!(Test-Path -LiteralPath $submissionReportPath) -or (Get-Item -LiteralPath $submissionReportPath).LastWriteTimeUtc -lt $submissionStarted) {
        throw "No fresh submission report (exit $($submissionProcess.ExitCode))."
    }
    $submissionReport=Get-Content -LiteralPath $submissionReportPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if($submissionProcess.ExitCode -ne 0 -or !$submissionReport.completed -or @($submissionReport.errors).Count -gt 0 -or !$submissionReport.experience.completed) {
        throw "Submission walkthrough failed: $($submissionReport.errors -join '; '); $($submissionReport.experience.errors -join '; '). $submissionReportPath"
    }
    foreach($flag in @('firstFresh','secondFresh','tutorialSaved','repairFullBlocked','repairPoorBlocked','repairPurchased','skipWorksAfterCompletion','returnedToTitle')) {
        if(!$submissionReport.$flag){throw "Unverified submission condition: $flag"}
    }
    if($submissionReport.titles -ne 2 -or $submissionReport.runs -ne 2 -or $submissionReport.newEnemyMask -ne 7){throw 'Incomplete title/run/new-enemy traversal.'}
    foreach($name in @('map','briefing','intro_upgrades','build_choice','additive_upgrade','repair_full','repair_insufficient','repair_available','enemy_1','enemy_2','enemy_4','boss','complete','new_expedition','completed_user_skip')) {
        $file=Join-Path $submissionOutput "$name.png"
        if(!(Test-Path -LiteralPath $file) -or (Get-Item -LiteralPath $file).LastWriteTimeUtc -lt $submissionStarted){throw "Missing walkthrough capture: $name"}
    }
    foreach($number in @(1,2)) {
        if(!(Test-Path -LiteralPath (Join-Path $submissionRoot "generated/title_demo/submission_title_$number.png"))){throw "Missing title capture $number"}
    }
    $submissionUser=Get-Content -LiteralPath (Join-Path $submissionRoot 'resources/configs/expedition_user.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if(!$submissionUser.tutorialCompleted){throw 'Tutorial completion was not saved in the isolated profile.'}
    Write-Host 'PASS: title -> first tutorial -> style -> workshop -> repair states -> three new enemies -> boss/result -> title -> clean new run -> completed-user skip.'
    Write-Host 'Later fights use forced clears and repair uses boundary fixtures; this is a UX/flow regression check, not a difficulty playtest.'
    Write-Output $submissionReportPath
} finally {
    if($submissionProcess -and !$submissionProcess.HasExited){Stop-Process -Id $submissionProcess.Id -ErrorAction SilentlyContinue}
    foreach($name in $submissionPrevious.Keys){[Environment]::SetEnvironmentVariable($name,$submissionPrevious[$name],'Process')}
}
