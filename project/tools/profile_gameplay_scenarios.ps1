param(
    [string]$ExecutablePath = '',
    [string]$ResourceDirectory = '',
    [string]$OutputDirectory = '',
    [ValidateSet('All','Scenario','Legacy')][string]$Mode = 'All',
    [string[]]$CaseName = @(),
    [switch]$SkipLegacyTrail,
    [ValidateRange(1,10)][int]$Repeats = 2,
    [ValidateRange(30,240)][int]$TimeoutSeconds = 180
)
$ErrorActionPreference='Stop'
$profileRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$ExecutablePath) { $ExecutablePath=Join-Path $profileRepo 'generated/outputs/Development/CG2.exe' }
if (!$ResourceDirectory) { $ResourceDirectory=Join-Path $profileRepo 'project/resources' }
if (!$OutputDirectory) { $OutputDirectory=Join-Path $profileRepo ('generated/gameplay-profiles/'+[DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)) }
$ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath)
$ResourceDirectory=[IO.Path]::GetFullPath($ResourceDirectory)
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$profileGenerated=Join-Path $profileRepo 'generated'
if (!$OutputDirectory.StartsWith($profileGenerated+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Profile evidence must stay below repository generated/.' }
if (!(Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { throw 'Build Development x64 first.' }
if (!(Test-Path -LiteralPath $ResourceDirectory -PathType Container)) { throw 'Runtime resource directory does not exist.' }
if ((Test-Path -LiteralPath $OutputDirectory) -and @(Get-ChildItem -LiteralPath $OutputDirectory -Force).Count) { throw 'Use a new output directory to preserve earlier measurement evidence.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$profileWorking=Join-Path $OutputDirectory 'runtime-project'
function Copy-ProfileResources([string]$Source,[string]$Destination) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($entry in Get-ChildItem -LiteralPath $Source) {
        if ($entry.PSIsContainer) {
            # Generated cache is not a runtime input. Preserve every configured
            # input file, including a frozen baseline's tutorial profile.
            if ($entry.Name -ne 'generated') { Copy-ProfileResources $entry.FullName (Join-Path $Destination $entry.Name) }
        } else {
            Copy-Item -LiteralPath $entry.FullName -Destination (Join-Path $Destination $entry.Name)
        }
    }
}
Copy-ProfileResources $ResourceDirectory (Join-Path $profileWorking 'resources')
$profileManifest=[ordered]@{
    executable=$ExecutablePath; executableSha256=(Get-FileHash -LiteralPath $ExecutablePath).Hash;
    resourceSource=$ResourceDirectory; workingDirectory=$profileWorking; mode=$Mode; repeats=$Repeats; skipLegacyTrail=[bool]$SkipLegacyTrail;
    configuration='Development'; frameLimit='existing 60Hz limiter'; overlay=$false;
    scenarioSeed=20261005; fixedDeltaTime=1.0/60.0; scenarioCaptureFrames=@();
    notes=@('Scenario CSVs contain real D3D12 timestamps. Invalid frames must be excluded.',
        'Scenario image copies are disabled. Session duration, not profiler exit, owns termination.',
        'Legacy fixtures include their existing image captures and variable timestep; they are not equivalent to fixed-step scenarios.',
        'Legacy512 trails are renderer stress, not real projectile/enemy stress.');
    results=@()
}
$profileResourceManifest=@(foreach ($file in Get-ChildItem -LiteralPath (Join-Path $profileWorking 'resources') -Recurse -File) {
    [pscustomobject]@{path=$file.FullName.Substring($profileWorking.Length+1);sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
})
$profileResourceManifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'runtime-assets-manifest.json') -Encoding utf8
$profileCases=@()
if ($Mode -in @('All','Scenario')) {
    foreach ($id in @('shooter','projectile_stress','enemy_stress','rival_boss','prototype_boss','neon_boss','boss_death')) {
        $profileCases += @{name=$id;scenario=$id;project='tank_expedition';warmup=30;frames=240;duration=300}
    }
}
if ($Mode -in @('All','Legacy')) {
    $profileCases += @(
        @{name='title-live-demo';project='tank_game';flag='CG2_TITLE_AUTOTEST';warmup=120;frames=300},
        @{name='normal-enemy-ai-charger';project='tank_expedition';flag='CG2_TANK_COMBAT_AUTOTEST';warmup=180;frames=300},
        @{name='special-attacks-mixed-probes';project='tank_expedition';flag='CG2_TANK_SPECIAL_AUTOTEST';warmup=180;frames=300},
        @{name='rival-phase-two';project='tank_expedition';flag='CG2_TANK_COMBAT_AUTOTEST';warmup=4500;frames=300},
        @{name='existing-trail-stress-512';project='tank_expedition';warmup=120;frames=300;trails=512}
    )
}
foreach ($name in $CaseName) { if ($name -notin $profileCases.name) { throw "Unknown profile case: $name" } }
if ($CaseName.Count) { $profileCases=@($profileCases | Where-Object {$_.name -in $CaseName}) }
if ($SkipLegacyTrail) { $profileCases=@($profileCases | Where-Object {$_.name -ne 'existing-trail-stress-512'}) }
$profilePrevious=@{}; $profileProcess=$null
foreach ($entry in Get-ChildItem Env: | Where-Object {$_.Name -like 'CG2_*'}) {
    $profilePrevious[$entry.Name]=$entry.Value
    [Environment]::SetEnvironmentVariable($entry.Name,$null,'Process')
}
try {
    foreach ($case in $profileCases) {
        for ($repeat=1; $repeat -le $Repeats; ++$repeat) {
            $directory=Join-Path $OutputDirectory ($case.name+'_'+$repeat)
            New-Item -ItemType Directory -Path $directory -Force | Out-Null
            foreach ($entry in Get-ChildItem Env: | Where-Object {$_.Name -like 'CG2_*'}) { [Environment]::SetEnvironmentVariable($entry.Name,$null,'Process') }
            $csv=Join-Path $directory 'profile.csv'
            $env:CG2_PERF_CAPTURE_PATH=$csv
            $env:CG2_PERF_CAPTURE_FRAMES=[string]$case.frames
            $env:CG2_PERF_CAPTURE_WARMUP=[string]$case.warmup
            $env:CG2_PERF_OVERLAY='0'; $env:CG2_FRAME_LIMIT='1'
            $env:CG2_STARTUP_TRACE_PATH=Join-Path $directory 'startup-trace.json'
            if ($case.ContainsKey('scenario')) {
                $settings=[ordered]@{
                    scenario=$case.scenario; seed=20261005; fixedDeltaTime=1.0/60.0; frames=$case.duration;
                    playerStyle=-1; playerHp=-1; bossHp=10000;
                    enemyCount=$(if($case.scenario -eq 'enemy_stress'){64}else{3});
                    initialProjectiles=$(if($case.scenario -eq 'projectile_stress'){192}else{0});
                    room='arena'; upgrades=@(); enemyWave=@(); input=@(); captureFrames=@(); outputDirectory=$directory
                }
                $manifest=Join-Path $directory 'settings.json'
                [IO.File]::WriteAllText($manifest,($settings | ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
                $env:CG2_GAMEPLAY_SCENARIO=$manifest
                # Never enable PERF_EXIT_AFTER_CAPTURE: it would skip the
                # Session report and its authoritative terminal invariants.
            } else {
                $env:CG2_PERF_EXIT_AFTER_CAPTURE='1'
                # Match the frozen original's explicit background exemption.
                # Direct Expedition startup does not construct TitleScene.
                $env:CG2_TITLE_AUTOTEST='1'
                if ($case.flag) { [Environment]::SetEnvironmentVariable($case.flag,'1','Process') }
                if ($case.ContainsKey('trails')) { $env:CG2_PERF_STRESS_TRAILS=[string]$case.trails }
            }
            $started=[DateTime]::UtcNow
            $arguments=@('--project',('resources/projects/'+$case.project+'.project.json'))
            Write-Output "START PROFILE $($case.name) repeat $repeat/$Repeats"
            $profileProcess=Start-Process -FilePath $ExecutablePath -WorkingDirectory $profileWorking -ArgumentList $arguments -WindowStyle Hidden -PassThru
            while (!$profileProcess.WaitForExit(10000)) {
                if (([DateTime]::UtcNow-$started).TotalSeconds -gt $TimeoutSeconds) { Stop-Process -Id $profileProcess.Id; throw "Profile timeout: $($case.name)" }
            }
            if ($profileProcess.ExitCode -ne 0) { throw "Profile exited $($profileProcess.ExitCode): $($case.name). Inspect $directory" }
            if (!(Test-Path -LiteralPath $csv) -or (Get-Item -LiteralPath $csv).LastWriteTimeUtc -lt $started) { throw "No fresh profile CSV: $($case.name)" }
            $count=@(Import-Csv -LiteralPath $csv | Where-Object {$_.category -eq 'frame' -and $_.name -eq 'elapsed_ms'}).Count
            if ($count -ne $case.frames) { throw "Profile $($case.name) expected $($case.frames) frames, found $count." }
            $entry=[ordered]@{name=$case.name;repeat=$repeat;csv=$csv;startedUtc=$started.ToString('o');durationSeconds=([DateTime]::UtcNow-$started).TotalSeconds;
                executable=$ExecutablePath;arguments=$arguments;workingDirectory=$profileWorking;exitCode=$profileProcess.ExitCode;
                warmupFrames=$case.warmup;measuredFrames=$count;scenario=($case.ContainsKey('scenario'));passed=$true}
            $entry.environment=@(Get-ChildItem Env: | Where-Object {$_.Name -like 'CG2_*'} | ForEach-Object {[ordered]@{name=$_.Name;value=$_.Value}})
            if ($case.ContainsKey('scenario')) {
                $reportPath=Join-Path $directory 'report.json'
                $snapshotsPath=Join-Path $directory 'snapshots.json'
                foreach ($path in @($reportPath,$snapshotsPath)) {
                    if (!(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).LastWriteTimeUtc -lt $started) { throw "No fresh Session evidence: $path" }
                }
                $report=Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
                if (!$report.completed -or @($report.errors).Count -or $report.frameCount -ne $case.duration -or @($report.captures).Count) { throw "Profile Session invariants/capture exclusion failed: $reportPath" }
                $snapshots=@(Get-Content -LiteralPath $snapshotsPath -Raw -Encoding UTF8 | ConvertFrom-Json)
                if ($snapshots.Count -ne $case.duration) { throw "Profile snapshot count mismatch: $snapshotsPath" }
                if ($case.scenario -eq 'projectile_stress' -and $report.maximumProjectiles -lt 150) { throw 'Profile never exercised the required real projectile stress.' }
                if ($case.scenario -eq 'enemy_stress' -and $snapshots[0].enemies -lt 64) { throw 'Profile never initialized the required 64 real enemies.' }
                $entry.report=$reportPath; $entry.snapshots=$snapshotsPath
                $entry.deathWindow=if($case.scenario -eq 'boss_death'){'Script kills after120 completed updates (first dead snapshot121). The measured window includes living, dissolve and terminal frames; report separately using the actual Scenario simulation frame counter.'}else{''}
            }
            $profileManifest.results+=@($entry)
            $profileManifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'run-manifest.json') -Encoding utf8
            Write-Output "PASS PROFILE $($case.name) repeat $repeat, $count measured frames"
            $profileProcess=$null
        }
    }
    $profileManifest.completed=$true
} catch {
    $profileManifest.completed=$false; $profileManifest.error=$_.Exception.Message
    throw
} finally {
    if ($profileProcess -and !$profileProcess.HasExited) { Stop-Process -Id $profileProcess.Id -ErrorAction SilentlyContinue }
    foreach ($entry in Get-ChildItem Env: | Where-Object {$_.Name -like 'CG2_*'}) { [Environment]::SetEnvironmentVariable($entry.Name,$null,'Process') }
    foreach ($key in $profilePrevious.Keys) { [Environment]::SetEnvironmentVariable($key,$profilePrevious[$key],'Process') }
    $profileManifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'run-manifest.json') -Encoding utf8
}
