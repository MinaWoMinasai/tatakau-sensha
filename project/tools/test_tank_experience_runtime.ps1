param(
    [ValidateSet('Development','Release')][string]$Configuration = 'Release',
    [ValidateSet('Both','Upper','Lower')][string]$Route = 'Both',
    [ValidateSet('Shooter','Drone','Melee')][string]$Style = 'Melee'
)
$ErrorActionPreference = 'Stop'
$experienceProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$experienceRepo = [IO.Path]::GetFullPath((Join-Path $experienceProject '..'))
$experienceExe = Join-Path $experienceRepo "generated/outputs/$Configuration/CG2.exe"
if (!(Test-Path -LiteralPath $experienceExe)) { throw 'Build the requested configuration first.' }
$experienceSettings = @{
    CG2_TANK_SPECIAL_AUTOTEST = $null
    CG2_TANK_EXPERIENCE_AUTOTEST = '1'
    CG2_TANK_EXPERIENCE_STYLE = [string](@('Shooter','Drone','Melee').IndexOf($Style))
    CG2_TANK_COMBAT_AUTOTEST = $null
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
$experiencePrevious = @{}
$experienceProcess = $null
$experienceResults = @{}
try {
    foreach ($experienceKey in $experienceSettings.Keys) {
        $experiencePrevious[$experienceKey] = [Environment]::GetEnvironmentVariable($experienceKey,'Process')
        [Environment]::SetEnvironmentVariable($experienceKey,$experienceSettings[$experienceKey],'Process')
    }
    $experienceVariants = if ($Route -eq 'Upper') { @(1) } elseif ($Route -eq 'Lower') { @(2) } else { @(1,2) }
    foreach ($experienceVariant in $experienceVariants) {
        $experienceName = if ($experienceVariant -eq 1) { 'upper' } else { 'lower' }
        [Environment]::SetEnvironmentVariable('CG2_TANK_EXPERIENCE_AUTOTEST',[string]$experienceVariant,'Process')
        $experienceDirectory = Join-Path $experienceProject "generated/experience_validation/$experienceName"
        $experienceResultPath = Join-Path $experienceDirectory 'validation.json'
        $experienceStart = [DateTime]::UtcNow
        $experienceProcess = Start-Process -FilePath $experienceExe -WorkingDirectory $experienceProject -ArgumentList @('--project','resources/projects/tank_expedition.project.json') -WindowStyle Hidden -PassThru
        Write-Host "Experience validation $experienceName started (PID $($experienceProcess.Id), $Configuration)."
        while (!$experienceProcess.WaitForExit(10000)) {
            if (([DateTime]::UtcNow - $experienceStart).TotalSeconds -gt 170) {
                Stop-Process -Id $experienceProcess.Id -ErrorAction SilentlyContinue
                throw "Experience validation $experienceName timed out. Inspect $experienceDirectory"
            }
        }
        if (!(Test-Path -LiteralPath $experienceResultPath) -or (Get-Item -LiteralPath $experienceResultPath).LastWriteTimeUtc -lt $experienceStart) {
            throw "Experience validation produced no fresh report (exit $($experienceProcess.ExitCode))."
        }
        $experienceResult = Get-Content -LiteralPath $experienceResultPath -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($experienceProcess.ExitCode -ne 0 -or !$experienceResult.completed -or !$experienceResult.testMode -or
            $experienceResult.forcedTutorialClear -or !$experienceResult.forcedLaterCombat -or
            @($experienceResult.errors).Count -ne 0 -or $experienceResult.seed -ne 20260926 -or
            $experienceResult.level -ne 1 -or $experienceResult.experience -ne 0) {
            throw "Experience validation failed ($experienceName, exit $($experienceProcess.ExitCode)): $($experienceResult.errors -join '; '). State: $($experienceResult.state). Report: $experienceResultPath"
        }
        $experienceExpectedWallet = if ($experienceVariant -eq 1) { 43 } else { 58 }
        if ($experienceResult.introWallet -ne 58 -or $experienceResult.afterIntroWallet -ne $experienceExpectedWallet -or
            @($experienceResult.introOffers).Count -ne 3 -or $experienceResult.prematureCredits -ne 0 -or
            $experienceResult.earlyFlightSamples -lt 1 -or !$experienceResult.meleeProbeCompleted -or !$experienceResult.buildPreserved -or !$experienceResult.additiveGrowthVerified -or
            $experienceResult.meleeTargetHp -ge 500 -or $experienceResult.buildStyle -ne $Style.ToLowerInvariant() -or
            @($experienceResult.visited).Count -lt 18 -or @($experienceResult.visited).Count -gt 22) {
            throw "Currency, offer, route or melee evidence incomplete: $experienceResultPath"
        }
        if ($Style -eq 'Melee') {
            if ($experienceResult.meleeSlashSamples -lt 1 -or $experienceResult.meleePlayerBulletSamples -ne 0 -or $experienceResult.meleeTargetDisplacement -lt 0.10) { throw 'Melee attack evidence incomplete.' }
        } elseif ($experienceResult.meleeSlashSamples -ne 0 -or $experienceResult.meleePlayerBulletSamples -lt 1 -or
            ($Style -eq 'Drone' -and $experienceResult.droneSamples -lt 1)) { throw 'Projectile style evidence incomplete.' }
        foreach ($experienceOffer in $experienceResult.introOfferDetails) {
            if ($experienceOffer.price -ne 15 -or $experienceOffer.rarity -ne 0) { throw 'Intro offer price/rarity mismatch.' }
        }
        $experienceCaptures = @('map','transition','credits_flight','intro_upgrades','build_choice','rarity_0','rarity_1','rarity_2','rarity_3','rarity_4','boss','complete')
        $experienceCaptures += if ($Style -eq 'Melee') { 'melee' } else { 'build_attack' }
        if ($experienceVariant -eq 1) {
            if ($experienceResult.initialKills -lt 2 -or $experienceResult.initialPlayerBulletSamples -lt 1 -or
                $experienceResult.successfulDashes -ne 3 -or !$experienceResult.tutorialInvulnerable -or $experienceResult.groundOrbSamples -lt 1 -or
                ($experienceResult.guideStageMask -band 127) -ne 127) {
                throw 'Actual guided shooting/pickup/dash sequence was not completed.'
            }
            $experienceCaptures += @('briefing','collect','hp_stamina','dash')
        }
        foreach ($experienceCaptureName in $experienceCaptures) {
            $experienceCapture = Join-Path $experienceDirectory "$experienceCaptureName.png"
            if (!(Test-Path -LiteralPath $experienceCapture) -or (Get-Item -LiteralPath $experienceCapture).LastWriteTimeUtc -lt $experienceStart -or
                (Get-Item -LiteralPath $experienceCapture).Length -lt 512) {
                throw "Missing fresh experience screenshot: $experienceCapture"
            }
        }
        $experienceResults[$experienceName] = $experienceResult
        $experienceArchive = Join-Path $experienceProject ("generated/experience_validation/{0}_{1}" -f $experienceName,$Style.ToLowerInvariant())
        New-Item -ItemType Directory -Path $experienceArchive -Force | Out-Null
        Copy-Item -Path (Join-Path $experienceDirectory '*') -Destination $experienceArchive -Force
        Write-Host ("PASS {0}: {1} visited nodes, intro {2} Cr -> {3} Cr, {4} completed dashes, attack damage {5}, target movement {6:N2}. Later combat clears were forced." -f
            $experienceName,@($experienceResult.visited).Count,$experienceResult.introWallet,$experienceResult.afterIntroWallet,
            $experienceResult.successfulDashes,(500-$experienceResult.meleeTargetHp),$experienceResult.meleeTargetDisplacement)
        Write-Output $experienceResultPath
    }
    if ($Route -eq 'Both') {
        if (($experienceResults.upper.introOffers -join '|') -ne ($experienceResults.lower.introOffers -join '|') -or
            $experienceResults.upper.introWallet -ne $experienceResults.lower.introWallet) {
            throw 'Upper and lower opening routes do not have identical offers and currency.'
        }
        Write-Host 'PASS: both opening routes deliver identical 58 Cr and the same three introductory offers for seed 20260926.'
    }
} finally {
    if ($experienceProcess -and !$experienceProcess.HasExited) { Stop-Process -Id $experienceProcess.Id -ErrorAction SilentlyContinue }
    foreach ($experienceKey in $experiencePrevious.Keys) {
        [Environment]::SetEnvironmentVariable($experienceKey,$experiencePrevious[$experienceKey],'Process')
    }
}
