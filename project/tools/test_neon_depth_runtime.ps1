# Actual Developer fixture assertions. Requires the final Development binary.
param(
    [string]$ExecutablePath='', [string]$OutputDirectory='',
    [ValidateRange(2,3)][int]$Repeats=2,
    [ValidateRange(30,360)][int]$TimeoutSeconds=240,
    [string[]]$Case=@(), [switch]$Capture
)
$ErrorActionPreference='Stop'
$depthRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!(Test-Path -LiteralPath (Join-Path $depthRepo 'project/CG2.sln'))) { throw 'Run the promoted project/tools script, not the ignored draft.' }
if (!$ExecutablePath) { $ExecutablePath=Join-Path $depthRepo 'generated/outputs/Development/CG2.exe' }
$ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath)
if (!(Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { throw 'Build the final Development binary first.' }
$depthGenerated=Join-Path $depthRepo 'generated'
if (!$OutputDirectory) { $OutputDirectory=Join-Path $depthGenerated ('neon-boss-depth/runtime/'+[DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)) }
$depthOutput=[IO.Path]::GetFullPath($OutputDirectory)
if (!$depthOutput.StartsWith($depthGenerated+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Depth evidence must stay under workspace generated/.' }
if ((Test-Path -LiteralPath $depthOutput) -and @(Get-ChildItem -LiteralPath $depthOutput -Force).Count) { throw 'Use a fresh Depth evidence directory.' }
New-Item -ItemType Directory -Path $depthOutput -Force | Out-Null
function Assert-Depth([bool]$Condition,[string]$Message) { if (!$Condition) { throw $Message } }
function Write-DepthJson([string]$Path,$Object) { [IO.File]::WriteAllText($Path,($Object|ConvertTo-Json -Depth 60),[Text.UTF8Encoding]::new($false)) }
function Copy-DepthResources([string]$Source,[string]$Destination) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($entry in Get-ChildItem -LiteralPath $Source -Force) {
        if ($entry.PSIsContainer) { if ($entry.Name -ine 'generated') { Copy-DepthResources $entry.FullName (Join-Path $Destination $entry.Name) } }
        else { Copy-Item -LiteralPath $entry.FullName -Destination (Join-Path $Destination $entry.Name) -Force }
    }
}
$depthWorking=Join-Path $depthOutput 'runtime-project'
$depthResources=Join-Path $depthWorking 'resources'
Copy-DepthResources (Join-Path $depthRepo 'project/resources') $depthResources
$depthConfig=Join-Path $depthOutput 'runtime-configs'
Copy-DepthResources (Join-Path $depthResources 'configs') $depthConfig
$depthAssets=@(foreach ($file in Get-ChildItem -LiteralPath $depthResources -Recurse -File) {
    [ordered]@{path=$file.FullName.Substring($depthWorking.Length+1);sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}
})
Write-DepthJson (Join-Path $depthOutput 'runtime-assets-manifest.json') $depthAssets
$depthProvenance=[ordered]@{
    executable=$ExecutablePath; executableSha256=(Get-FileHash -LiteralPath $ExecutablePath -Algorithm SHA256).Hash
    resourceManifest='runtime-assets-manifest.json'; configurationResetSource=$depthConfig
    workingDirectory=$depthWorking; normalMainRoute=$false; shortcutFixture=$true; audio='none'
}

function New-DepthCase([string]$Name,[string]$Scenario,[int]$Style=0,[string]$Probe='none',[string]$Attack='volley',[string]$Phase='active',[double]$Progress=0,[bool]$Visual=$true,[bool]$Reduced=$false) {
    $frames=if ($Scenario -in @('neon_depth_cycles','neon_depth_parity')) {2400} elseif ($Attack -eq 'beam') {1080} else {900}
    [pscustomobject]@{name=$Name;scenario=$Scenario;style=$Style;probe=$Probe;attack=$Attack;phase=$Phase;progress=$Progress;visual=$Visual;reduced=$Reduced;frames=$frames}
}
$depthCases=[Collections.Generic.List[object]]::new()
$depthCases.Add((New-DepthCase 'cycles' 'neon_depth_cycles'))
foreach ($style in 0..2) {
    $depthCases.Add((New-DepthCase "damage_style_$style" 'neon_depth_damage' $style))
    $depthCases.Add((New-DepthCase "dodge_style_$style" 'neon_depth_dodge' $style))
}
$depthCases.Add((New-DepthCase 'stationary_damage_control' 'neon_depth_lifecycle' 0 'stationary_damage'))
$depthCases.Add((New-DepthCase 'hp0_intro' 'neon_depth_lifecycle' 0 'hp0' 'volley' 'intro' .2))
$depthCases.Add((New-DepthCase 'hp0_reposition' 'neon_depth_lifecycle' 0 'hp0' 'volley' 'reposition' .2))
$depthCases.Add((New-DepthCase 'hp0_volley_before_arrival' 'neon_depth_lifecycle' 0 'hp0' 'volley' 'airborne' .9))
$depthCases.Add((New-DepthCase 'hp0_dive_before_landing' 'neon_depth_lifecycle' 0 'hp0' 'dive' 'airborne' .9))
$depthCases.Add((New-DepthCase 'hp0_beam_active' 'neon_depth_lifecycle' 0 'hp0' 'beam' 'active' .2))
foreach ($attack in @('volley','dive','beam')) {
    foreach ($phase in @('telegraph','locked','active','recovery')) {
        if($attack -eq 'beam' -and $phase -eq 'active') {continue} # Existing named active fixture above.
        $depthCases.Add((New-DepthCase "hp0_${attack}_$phase" 'neon_depth_lifecycle' 0 'hp0' $attack $phase .5))
    }
}
foreach ($attack in @('volley','dive','beam')) { $depthCases.Add((New-DepthCase "abort_$attack" 'neon_depth_lifecycle' 0 'abort' $attack 'locked' .5)) }
$depthCases.Add((New-DepthCase 'pause_beam_active' 'neon_depth_lifecycle' 0 'pause' 'beam' 'active' .2))
$depthCases.Add((New-DepthCase 'intro_skip' 'neon_depth_lifecycle' 0 'intro_skip' 'volley' 'intro' .2))
$depthCases.Add((New-DepthCase 'player_death' 'neon_depth_lifecycle' 0 'player_death' 'dive' 'telegraph' .2))
$depthCases.Add((New-DepthCase 'simultaneous_death' 'neon_depth_lifecycle' 0 'simultaneous_death' 'dive' 'telegraph' .2))
$depthCases.Add((New-DepthCase 'actual_retry' 'neon_depth_lifecycle' 0 'retry' 'volley' 'telegraph' .2))
foreach ($style in 1..2) {
    $depthCases.Add((New-DepthCase "player_death_style_$style" 'neon_depth_lifecycle' $style 'player_death' 'dive' 'telegraph' .2))
    $depthCases.Add((New-DepthCase "actual_retry_style_$style" 'neon_depth_lifecycle' $style 'retry' 'volley' 'telegraph' .2))
}
$depthCases.Add((New-DepthCase 'actual_title_return' 'neon_depth_lifecycle' 0 'title_return' 'volley' 'telegraph' .2))
$depthCases.Add((New-DepthCase 'actual_multidraw' 'neon_depth_lifecycle' 0 'multidraw'))
$depthCases.Add((New-DepthCase 'parity_on' 'neon_depth_parity'))
$depthCases.Add((New-DepthCase 'parity_off' 'neon_depth_parity' 0 'none' 'volley' 'active' 0 $false))
$depthCases.Add((New-DepthCase 'parity_reduced' 'neon_depth_parity' 0 'none' 'volley' 'active' 0 $true $true))
$depthAllCases=@($depthCases.ToArray())
if ($Case.Count) {
    foreach ($requested in $Case) { Assert-Depth (@($depthCases|Where-Object name -CEQ $requested).Count -eq 1) "Unknown Depth case $requested" }
    $depthCases=@($depthCases|Where-Object {$_.name -cin $Case})
}

function New-DepthSettings($Definition,[string]$Directory) {
    $manifest=[ordered]@{schemaVersion=1;scenario=$Definition.scenario;seed=20261005;fixedDeltaTime=1/60.0;frames=$Definition.frames
        playerStyle=$Definition.style;playerHp=-1;bossHp=10000;enemyCount=0;initialProjectiles=0;room='arena'
        upgrades=@();enemyWave=@();input=@();captureFrames=@(if($Capture){240;480})
        depthFixture=@{probe=$Definition.probe;attack=$Definition.attack;phase=$Definition.phase;minimumProgress=$Definition.progress
            visualEnabled=$Definition.visual;reducedMotion=$Definition.reduced;injectPhaseTwo=($Definition.scenario -in @('neon_depth_cycles','neon_depth_parity'))}
        outputDirectory=$Directory}
    if($Definition.probe -eq 'intro_skip') {
        # Ordinary fire/dash attempts make the locked zero-tick check meaningful.
        # The helper preserves this explicit input and never teleports an actor.
        $manifest.input=@(@{firstFrame=0;endFrame=$Definition.frames;movement=@(0,0);aim=@(62,30,0);shoot=$true;dash=$true})
    }
    return $manifest
}
function Assert-DepthNumber($Value,$Expected,[string]$Label,[double]$Tolerance=0) {
    $numeric=@('Byte','SByte','Int16','UInt16','Int32','UInt32','Int64','UInt64','Single','Double','Decimal')
    Assert-Depth ($null -ne $Value -and $numeric -ccontains [Type]::GetTypeCode($Value.GetType()).ToString() -and
        [double]::IsFinite([double]$Value) -and [Math]::Abs([double]$Value-[double]$Expected) -le $Tolerance) "Requested Depth $Label differs from actual settings/observations."
}
function Assert-DepthFlag($Value,[bool]$Expected,[string]$Label) {
    Assert-Depth ($Value -is [bool] -and $Value -eq $Expected) "Requested Depth $Label is missing, not boolean, or differs from the actual flag."
}
function Assert-DepthText($Value,[string]$Expected,[string]$Label) {
    Assert-Depth ($Value -is [string] -and [string]::Equals($Value,$Expected,[StringComparison]::Ordinal)) "Requested Depth $Label differs from actual settings/observations."
}
function Assert-DepthRequestedSettings($Definition,$Settings,$Report,[object[]]$Rows) {
    $actual=$Report.settings; $requested=$Settings.depthFixture; $parsed=$actual.depthFixture
    Assert-Depth ($null -ne $actual -and $null -ne $parsed -and $Rows.Count -gt 0) 'Actual Depth settings or observation rows are missing.'
    Assert-DepthText $Settings.scenario $Definition.scenario 'constructed scenario'
    Assert-DepthText $Report.scenario $Definition.scenario 'report scenario'
    foreach($key in @('scenario','room')) { Assert-DepthText $actual.$key $Settings.$key ('parsed '+$key) }
    foreach($key in @('seed','frames','playerStyle','playerHp','bossHp','enemyCount','initialProjectiles')) {
        Assert-DepthNumber $actual.$key $Settings.$key ('parsed '+$key)
    }
    Assert-DepthNumber $Settings.frames $Definition.frames 'constructed frames'
    Assert-DepthNumber $Settings.playerStyle $Definition.style 'constructed style'
    Assert-DepthNumber $Report.depth.expectedMaxFrames $Definition.frames 'actual session maximum frames'
    Assert-DepthNumber $actual.fixedDeltaTime $Settings.fixedDeltaTime 'parsed fixedDeltaTime' 1e-7
    foreach($key in @('probe','attack','phase')) {
        Assert-DepthText $requested.$key $Definition.$key ('constructed '+$key)
        Assert-DepthText $parsed.$key $requested.$key ('parsed '+$key)
    }
    Assert-DepthNumber $requested.minimumProgress $Definition.progress 'constructed minimumProgress' 1e-6
    Assert-DepthNumber $parsed.minimumProgress $requested.minimumProgress 'parsed minimumProgress' 1e-6
    Assert-DepthFlag $requested.visualEnabled $Definition.visual 'constructed visualEnabled'
    Assert-DepthFlag $requested.reducedMotion $Definition.reduced 'constructed reducedMotion'
    Assert-DepthFlag $requested.injectPhaseTwo ($Definition.scenario -in @('neon_depth_cycles','neon_depth_parity')) 'constructed injectPhaseTwo'
    foreach($key in @('visualEnabled','reducedMotion','injectPhaseTwo')) { Assert-DepthFlag $parsed.$key $requested.$key ('parsed '+$key) }
    $realDamage=$Definition.scenario -in @('neon_depth_damage','neon_depth_dodge') -or $Definition.probe -ceq 'stationary_damage'
    $playerDeath=$Definition.probe -cin @('player_death','simultaneous_death','retry','title_return')
    $expectedFlags=@{shortcutFixture=$true;normalMainRoute=$false;automatedInput=$true;initialPositionAssistance=$false
        comparisonFreeze=$false;HUDIncluded=$true;debugUIIncluded=$false;dynamicProbeInput=(@($Settings.input).Count -eq 0)
        visualEnabled=$Definition.visual;reducedMotion=$Definition.reduced;reducedMotionApplied=$Definition.reduced
        phaseTwoHpInjectionRequested=$requested.injectPhaseTwo;playerInvulnerable=(!$realDamage -and !$playerDeath)
        forcedBossDefeatRequested=($Definition.probe -cin @('hp0','simultaneous_death'))
        forcedPlayerLethalDamageRequested=$playerDeath}
    foreach($row in $Rows) {
        $conditions=$row.presentation.conditions
        Assert-Depth ($null -ne $conditions) 'Actual Depth row conditions are missing.'
        Assert-DepthNumber $row.gameplay.playerStyle $Definition.style 'observed player style'
        Assert-DepthNumber $conditions.bossHpConfigured $Settings.bossHp 'observed boss HP configuration'
        Assert-DepthNumber $conditions.playerHpConfigured $Settings.playerHp 'observed player HP configuration'
        Assert-DepthNumber $conditions.initialRoomProtectionSeconds .45 'declared ordinary initial protection' 1e-7
        Assert-DepthText $conditions.actualBossPolicy 'depth' 'observed boss policy'
        Assert-DepthText $conditions.probe $Definition.probe 'observed probe'
        foreach($key in $expectedFlags.Keys) { Assert-DepthFlag $conditions.$key $expectedFlags[$key] ('observed '+$key+' at frame '+$row.frame) }
        Assert-Depth ($conditions.operations -is [Array]) 'Actual bounded fixture operation array is missing.'
        $operations=@($conditions.operations)
        Assert-Depth ($operations.Count -le 64) 'Actual bounded fixture operation log exceeded its limit.'
        Assert-DepthFlag $conditions.forcedBossDefeat (@($operations|Where-Object {$_.operation -ceq 'actual_enemy_die'}).Count -gt 0) 'observed applied boss defeat'
        Assert-DepthFlag $conditions.forcedPlayerLethalDamage (@($operations|Where-Object {$_.operation -ceq 'actual_player_lethal_damage'}).Count -gt 0) 'observed applied player lethal damage'
        Assert-DepthFlag $conditions.phaseTwoHpInjected (@($operations|Where-Object {$_.operation -ceq 'phase2_hp_injection'}).Count -gt 0) 'observed applied phase2 injection'
    }
}
function Find-DepthOperation([object[]]$Rows,[string]$Name) {
    @($Rows|ForEach-Object {$_.presentation.conditions.operations}|Where-Object {$_.operation -ceq $Name}|Select-Object -First 1)
}
function Assert-DepthProbePhase($Definition,[object[]]$Rows,[string]$Name) {
    $operation=Find-DepthOperation $Rows $Name
    Assert-Depth ($operation.Count -eq 1 -and $operation[0].actualPhase -ceq $Definition.phase -and
        ($Definition.phase -in @('intro','reposition') -or $operation[0].actualAttack -ceq $Definition.attack)) "Actual $Name was not applied in requested $($Definition.attack)/$($Definition.phase)."
    Assert-Depth (@($Rows|Where-Object {$_.frame -eq $operation[0].beforeCompletedFrame -and $_.sceneEpoch -eq $operation[0].sceneEpoch}).Count -eq 1) 'Probe operation has no corresponding completed actual update.'
    $prior=@($Rows|Where-Object {$_.frame -eq ($operation[0].beforeCompletedFrame-1) -and $_.sceneEpoch -eq $operation[0].sceneEpoch})
    Assert-Depth ($prior.Count -eq 1 -and $prior[0].gameplay.boss.generation -eq $operation[0].generation -and
        $prior[0].gameplay.boss.plan.instance -eq $operation[0].instance -and
        $prior[0].gameplay.boss.phase -ceq $operation[0].actualPhase -and
        $prior[0].gameplay.boss.plan.attack -ceq $operation[0].actualAttack) 'Probe lacks its immediately preceding actual phase/encounter evidence.'
    $progress=$prior[0].gameplay.boss.progress
    Assert-Depth ($null -ne $progress -and [double]::IsFinite([double]$progress) -and
        ([double]$progress+1e-6) -ge $Definition.progress) 'Probe was applied before the requested actual phase progress.'
}
function Assert-DepthTerminalTransition($Definition,[object[]]$Rows,[string]$OperationName,[string]$Terminal) {
    $operations=@(Find-DepthOperation $Rows $OperationName)
    Assert-Depth ($operations.Count -eq 1) 'Terminal transition has no actual operation.'
    $operation=$operations[0]
    $prior=@($Rows|Where-Object {$_.frame -eq ($operation.beforeCompletedFrame-1) -and $_.sceneEpoch -eq $operation.sceneEpoch})
    $transition=@($Rows|Where-Object {$_.frame -eq $operation.beforeCompletedFrame -and $_.sceneEpoch -eq $operation.sceneEpoch})
    Assert-Depth ($prior.Count -eq 1 -and $transition.Count -eq 1) 'Terminal transition lacks its immediately preceding actual update.'
    $before=$prior[0];$after=$transition[0]
    Assert-Depth ($before.gameplay.boss.generation -eq $operation.generation -and
        $after.gameplay.boss.generation -eq $operation.generation -and
        $before.gameplay.boss.plan.instance -eq $operation.instance -and
        $before.gameplay.boss.phase -ceq $operation.actualPhase -and
        $after.gameplay.boss.phase -ceq $Terminal) 'The requested terminal state was not reached in the exact actual operation frame/encounter.'
    Assert-Depth ($after.gameplay.boss.activationEvents -eq $before.gameplay.boss.activationEvents -and
        $after.gameplay.contactClaims -eq $before.gameplay.contactClaims -and
        $after.gameplay.acceptedHits -eq $before.gameplay.acceptedHits -and
        $after.gameplay.boss.contactClaims -eq $before.gameplay.boss.contactClaims -and
        $after.gameplay.boss.acceptedHits -eq $before.gameplay.boss.acceptedHits) 'The terminal operation tick emitted/claimed/accepted a hazard.'
    Assert-Depth ($after.gameplay.boss.activeCircleMask -eq 0 -and !$after.gameplay.boss.activeBeamClipped -and
        !$after.gameplay.boss.planning -and !$after.gameplay.boss.inputLocked -and !$after.gameplay.boss.vulnerable) 'The terminal operation tick retained a hazard, pending plan, input lock or vulnerable core.'
    if($Definition.probe -in @('hp0','simultaneous_death')) {
        Assert-Depth ($after.gameplay.bossHp -eq 0 -and $after.gameplay.bossDead) 'The actual terminal operation frame did not contain Enemy HP0/dead.'
    }
}
function Assert-DepthCameraRestored([object[]]$Rows,[string]$OperationName) {
    $operation=Find-DepthOperation $Rows $OperationName
    Assert-Depth ($operation.Count -eq 1) 'Camera restoration has no actual terminal operation.'
    for($index=1;$index -lt $Rows.Count;++$index) {
        $before=$Rows[$index-1];$after=$Rows[$index]
        if($after.sceneEpoch -ne $operation[0].sceneEpoch -or $before.sceneEpoch -ne $after.sceneEpoch -or
            $after.frame -lt $operation[0].beforeCompletedFrame -or !$before.presentation.conditions.cameraScope.scoped -or
            $after.presentation.conditions.cameraScope.scoped) {continue}
        $saved=$before.presentation.conditions.cameraScope.saved
        $current=$after.presentation.conditions.cameraScope.current
        foreach($key in @('fovY','aspect','nearClip','farClip','debugCamera')) {
            Assert-Depth ($current.$key -eq $saved.$key) "Actual camera $key did not restore to its recorded saved value."
        }
        foreach($key in @('rotation','jitter')) {
            $currentValues=@($current.$key);$savedValues=@($saved.$key)
            Assert-Depth ($currentValues.Count -eq $savedValues.Count) "Actual restored camera $key dimensions differ."
            for($element=0;$element -lt $savedValues.Count;++$element) {
                Assert-Depth ($currentValues[$element] -eq $savedValues[$element]) "Actual camera $key did not restore to its recorded saved value."
            }
        }
        return
    }
    throw "No actual camera scoped-to-restored update after $OperationName was recorded."
}
function Assert-DepthEffects($Definition,[object[]]$Rows) {
    $cumulative=@('resourceCreates','resourceReleases','resourceFailures','updates','duplicateUpdates',
        'invalidFrames','capacityRejected','depthBindingRejected','airDraws','floorDraws')
    $required=$cumulative+@('vertices','airVertices','floorVertices','lineCommands','fillCommands','launchLatches')
    $numericTypes=@('Byte','SByte','Int16','UInt16','Int32','UInt32','Int64','UInt64','Single','Double','Decimal')
    $previous=$null
    foreach($row in $Rows) {
        $effects=$row.presentation.effects
        Assert-Depth ($null -ne $effects -and $effects.hasResources -is [bool] -and
            $row.presentation.visualFinished -is [bool]) 'Actual Effects resource/lifecycle telemetry is missing.'
        foreach($key in $required) {
            $value=$effects.$key
            Assert-Depth ($null -ne $value -and $numericTypes -ccontains [Type]::GetTypeCode($value.GetType()).ToString() -and
                [double]::IsFinite([double]$value) -and [double]$value -ge 0 -and
                [Math]::Floor([double]$value) -eq [double]$value) "Actual Effects $key counter is missing or invalid."
        }
        foreach($key in @('resourceFailures','duplicateUpdates','invalidFrames','capacityRejected','depthBindingRejected')) {
            Assert-Depth ($effects.$key -eq 0) "Actual Effects reported $key at completed frame $($row.frame)."
        }
        Assert-Depth ($effects.vertices -le 50688 -and $effects.vertices -eq ($effects.airVertices+$effects.floorVertices)) 'Actual Effects vertex ranges exceed their shared budget or disagree.'
        $terminal=$row.gameplay.boss.phase -cin @('defeated','aborted')
        if(!$terminal -or !$row.presentation.visualFinished) {
            Assert-Depth ($effects.hasResources -and $effects.resourceCreates -ge 1 -and
                ($effects.resourceCreates-$effects.resourceReleases) -eq 1) 'A live or unfinished terminal encounter lost its single Effects allocation.'
        } else {
            Assert-Depth (!$effects.hasResources -and $effects.resourceCreates -eq $effects.resourceReleases -and
                $effects.vertices -eq 0 -and $effects.airVertices -eq 0 -and $effects.floorVertices -eq 0) 'Finished terminal Effects retained resources or frame geometry.'
        }
        if(!$terminal) { Assert-Depth (!$row.presentation.visualFinished) 'Alive gameplay unexpectedly has a Finished Visual lifecycle.' }
        if(!$Definition.visual) { Assert-Depth ($effects.airVertices -eq 0 -and $effects.airDraws -eq 0) 'Visual OFF still generated or submitted body air effects.' }
        if($null -ne $previous -and $previous.sceneEpoch -eq $row.sceneEpoch) {
            foreach($key in $cumulative) {
                Assert-Depth ($effects.$key -ge $previous.presentation.effects.$key) "Effects cumulative $key reset within an actual scene epoch."
            }
        }
        $previous=$row
    }
    Assert-Depth (@($Rows|Where-Object {$_.presentation.effects.floorVertices -gt 0}).Count -gt 0 -and
        @($Rows|Where-Object {$_.presentation.effects.floorDraws -gt 0}).Count -gt 0) 'Actual initialized Effects never populated and submitted their required floor cues.'
    foreach($group in @($Rows|Group-Object {"$($_.sceneEpoch):$($_.gameplay.boss.generation)"})) {
        $encounter=@($group.Group)
        $alive=@($encounter|Where-Object {$_.gameplay.boss.phase -cnotin @('defeated','aborted')})
        Assert-Depth ($alive.Count -gt 0) 'Effects encounter lacks its recorded preceding alive allocation.'
        $baseline=$alive[0].presentation.effects
        Assert-Depth (@($alive|Where-Object {$_.presentation.effects.resourceCreates -ne $baseline.resourceCreates -or
            $_.presentation.effects.resourceReleases -ne $baseline.resourceReleases}).Count -eq 0) 'Effects resources were recreated or released during one live encounter.'
        $terminal=@($encounter|Where-Object {$_.gameplay.boss.phase -cin @('defeated','aborted')})
        if(!$terminal.Count) {continue}
        Assert-Depth (@($terminal|Where-Object {$_.frame -lt $alive[-1].frame}).Count -eq 0) 'Gameplay resumed after its terminal encounter.'
        Assert-Depth (@($terminal|Where-Object {$_.presentation.effects.resourceCreates -ne $baseline.resourceCreates -or
            $_.presentation.effects.resourceReleases -lt $baseline.resourceReleases -or
            $_.presentation.effects.resourceReleases -gt ($baseline.resourceReleases+1)}).Count -eq 0) 'Terminal Effects were recreated or released more than once in one encounter.'
        $finished=@($terminal|Where-Object {$_.presentation.visualFinished})
        Assert-Depth ($finished.Count -gt 0) 'Terminal encounter ended without an actual Finished Effects release observation.'
        $first=$finished[0]
        Assert-Depth ($first.presentation.effects.resourceReleases -eq ($baseline.resourceReleases+1)) 'First actual Finished update did not release Effects exactly once relative to its live encounter.'
        foreach($row in @($terminal|Where-Object {$_.frame -ge $first.frame})) {
            Assert-Depth ($row.presentation.visualFinished -and !$row.presentation.effects.hasResources) 'Finished terminal Effects became active again.'
            foreach($key in @('resourceCreates','resourceReleases','updates','airDraws','floorDraws')) {
                Assert-Depth ($row.presentation.effects.$key -eq $first.presentation.effects.$key) "Finished terminal Effects continued $key after their release."
            }
        }
    }
    if($Definition.probe -in @('hp0','simultaneous_death','abort','player_death','title_return')) {
        Assert-Depth ($Rows[-1].presentation.visualFinished -and !$Rows[-1].presentation.effects.hasResources) 'The terminal probe ended with live Effects resources.'
    }
}
function Assert-DepthTerminal($Definition,$Report,[object[]]$Rows) {
    Assert-Depth ($Report.completed -and @($Report.errors).Count -eq 0) "Depth $($Definition.name) failed: $($Report.errors -join '; ')"
    Assert-Depth ($Rows.Count -ge 1 -and $Rows.Count -le $Definition.frames -and $Rows.Count -eq $Report.frameCount -and $Rows.Count -eq $Report.depth.frameCount) 'Actual Depth frame/row counts differ.'
    foreach ($index in 0..($Rows.Count-1)) {
        Assert-Depth ($Rows[$index].frame -eq $index+1) 'Depth rows must begin at actual completed frame1 with no omissions.'
        Assert-Depth ($Rows[$index].presentation.conditions.shortcutFixture -and !$Rows[$index].presentation.conditions.normalMainRoute) 'A fixture was mislabelled as the normal route.'
        Assert-Depth ($Rows[$index].presentation.conditions.actualBossPolicy -ceq 'depth') 'An old boss policy was exercised by a new Depth case.'
        Assert-Depth ($Rows[$index].gameplay.playerStyle -eq $Definition.style) 'Actual player style differs from the requested fixture.'
        $scope=$Rows[$index].presentation.conditions.cameraScope
        Assert-Depth ($scope -and @($scope.current.jitter).Count -eq 2 -and @($scope.current.rotation).Count -eq 3 -and
            $scope.current.fovY -gt 0 -and $scope.current.aspect -gt 0 -and $scope.current.nearClip -gt 0 -and $scope.current.farClip -gt $scope.current.nearClip) 'Actual camera scope/profile telemetry is missing or invalid.'
        if($scope.scoped) { Assert-Depth (!$scope.current.debugCamera -and $scope.current.jitter[0] -eq 0 -and $scope.current.jitter[1] -eq 0) 'Depth scoped rendering used an inconsistent debug camera or projection jitter.' }
    }
    Assert-DepthEffects $Definition $Rows
    if ($Definition.probe -eq 'title_return') {
        Assert-Depth ($Report.depth.earlyTitleCompletion -and @($Report.depth.actualSceneNotifications|Where-Object {$_.scene -ceq 'TITLE' -and $_.actualInitializedScene}).Count -ge 1) 'Title return completed without actual initialized TITLE notification.'
        Assert-Depth (@($Rows|Where-Object {$_.gameplay.playerDead -and $_.gameplay.playerHp -eq 0 -and $_.gameplay.flow -eq 3}).Count -gt 0 -and
            @($Rows|ForEach-Object {$_.presentation.conditions.operations}|Where-Object {$_.operation -ceq 'actual_confirm_title_return'}).Count -gt 0) 'Title proof lacks the actual Player HP0/GameOver and result selection call.'
        Assert-DepthProbePhase $Definition $Rows 'actual_player_lethal_damage'
        Assert-DepthCameraRestored $Rows 'actual_player_lethal_damage' # Old scene rows before the actual TITLE initialization.
    } else { Assert-Depth (!$Report.depth.earlyTitleCompletion -and $Rows.Count -eq $Definition.frames) 'A non-title scenario ended early.' }
    $active=@($Rows|Where-Object {$_.gameplay.boss.phase -ceq 'active'})
    switch ($Definition.scenario) {
        {$_ -in @('neon_depth_cycles','neon_depth_parity')} {
            foreach ($phase2 in @($false,$true)) {
                $attacks=@($Rows|Where-Object {$_.gameplay.boss.phase -ceq 'recovery' -and $_.gameplay.boss.plan.phaseTwo -eq $phase2}|ForEach-Object {$_.gameplay.boss.plan.attack}|Select-Object -Unique)
                Assert-Depth ($attacks.Count -eq 3 -and 'volley' -cin $attacks -and 'dive' -cin $attacks -and 'beam' -cin $attacks) 'Three actual attack/recovery cycles were not completed in both phases.'
            }
            Assert-Depth ($Report.details.depthFinal.conditions.phaseTwoHpInjected) 'The declared phase2 fixture injection was not recorded.'
        }
        'neon_depth_damage' {
            Assert-Depth ($Rows[-1].gameplay.actualCoreDamageExcludingInjection -gt 0 -and $Rows[-1].gameplay.primaryAttacks -gt 0) 'This style did not damage the real core through ordinary attacks.'
            Assert-Depth (@($Rows|Where-Object {$_.presentation.conditions.playerInvulnerable}).Count -eq 0) 'Real style damage test used debug invulnerability.'
            Assert-Depth (@($Rows|Where-Object {$_.gameplay.playerStyle -ne $Definition.style}).Count -eq 0) 'Actual player style differs from the manifest.'
            if($Definition.style -eq 1) { Assert-Depth (@($Rows|Where-Object {$_.gameplay.activeDrones -gt 0}).Count -gt 0) 'Drone style never created actual companions.' }
        }
        'neon_depth_dodge' {
            Assert-Depth (@($Rows|Where-Object {$_.presentation.conditions.playerInvulnerable}).Count -eq 0) 'Dodge test used debug invulnerability.'
            foreach ($attack in @('volley','dive','beam')) {
                $safe=$false
                foreach ($group in @($active|Where-Object {$_.gameplay.boss.plan.attack -ceq $attack}|Group-Object {$_.gameplay.boss.plan.instance})) {
                    $instance=$group.Group[0].gameplay.boss.plan.instance
                    $windup=@($Rows|Where-Object {$_.gameplay.boss.plan.instance -eq $instance -and $_.gameplay.boss.phase -ceq 'telegraph'})
                    $recovery=@($Rows|Where-Object {$_.gameplay.boss.plan.instance -eq $instance -and $_.gameplay.boss.phase -ceq 'recovery'})
                    if($windup.Count -eq 0 -or $recovery.Count -eq 0) {continue}
                    $all=@($windup[0])+$group.Group+@($recovery[0])
                    $hp=@($all|ForEach-Object {$_.gameplay.playerHp}|Select-Object -Unique)
                    $hits=@($all|ForEach-Object {$_.gameplay.acceptedHits}|Select-Object -Unique)
                    if($hp.Count -eq 1 -and $hits.Count -eq 1 -and $recovery[0].gameplay.dashStarts -gt $windup[0].gameplay.dashStarts -and
                        $recovery[0].gameplay.playerHp -gt 0) {$safe=$true;break}
                }
                Assert-Depth $safe "Style $($Definition.style) did not demonstrate a real safe dash/avoidance through a complete $attack attack."
            }
        }
    }
    switch ($Definition.probe) {
        'stationary_damage' {
            Assert-Depth (@($Rows|Where-Object {$_.gameplay.acceptedHits -gt 0 -and $_.gameplay.damageTakenCount -gt 0}).Count -gt 0) 'A stationary real-hazard control never reduced actual Player HP.'
        }
        {$_ -in @('hp0','simultaneous_death','abort')} {
            $operationName=if($Definition.probe -eq 'abort'){'actual_enemy_abort'}else{'actual_enemy_die'}
            Assert-DepthProbePhase $Definition $Rows $operationName
            Assert-DepthCameraRestored $Rows $operationName
            $terminal=if($Definition.probe -eq 'abort'){'aborted'}else{'defeated'}
            Assert-DepthTerminalTransition $Definition $Rows $operationName $terminal
            $stopped=@($Rows|Where-Object {$_.gameplay.boss.phase -ceq $terminal})
            Assert-Depth ($stopped.Count -gt 1) 'Actual terminal gameplay state was never observed.'
            Assert-Depth (@($stopped|ForEach-Object {$_.gameplay.boss.activationEvents}|Select-Object -Unique).Count -eq 1 -and
                @($stopped|ForEach-Object {$_.gameplay.acceptedHits}|Select-Object -Unique).Count -eq 1 -and
                @($stopped|ForEach-Object {$_.gameplay.contactClaims}|Select-Object -Unique).Count -eq 1) 'Terminal gameplay continued emitting/claiming/accepting hazards.'
            Assert-Depth (@($stopped|Where-Object {$_.gameplay.boss.activeCircleMask -ne 0 -or $_.gameplay.boss.activeBeamClipped}).Count -eq 0) 'Terminal gameplay kept an active canonical hazard.'
            Assert-Depth (!$Rows[-1].presentation.visualHasResources -and $Rows[-1].presentation.drawConstantBuffers -eq 0) 'Terminal actual Visual retained resources.'
            if($Definition.probe -eq 'simultaneous_death') { Assert-Depth ($Rows[-1].gameplay.flow -eq 3 -and !$Rows[-1].gameplay.bossDefeatHandled -and $Rows[-1].gameplay.playerDead) 'Actual simultaneous expedition death did not preserve player-death priority.' }
        }
        'pause' { Assert-Depth ($Report.details.depthFinal.actualUserPauseResumed -and @($Rows|Where-Object {$_.presentation.conditions.userPause -and $_.actualGameplayDelta -eq 0 -and $_.actualPresentationDelta -eq 0}).Count -ge 59) 'Actual user pause did not freeze both clocks and resume.' }
        'intro_skip' {
            Assert-DepthProbePhase $Definition $Rows 'queued_intro_skip_input'
            $operation=Find-DepthOperation $Rows 'queued_intro_skip_input'
            $skipped=@($Rows|Where-Object {$_.frame -eq $operation[0].beforeCompletedFrame})
            $before=@($Rows|Where-Object {$_.frame -eq ($operation[0].beforeCompletedFrame - 1)})
            Assert-Depth ($before.Count -eq 1 -and $skipped.Count -eq 1 -and $before[0].gameplay.boss.phase -ceq 'intro' -and
                $skipped[0].gameplay.boss.phase -ceq 'reposition' -and !$skipped[0].gameplay.boss.inputLocked -and
                $skipped[0].gameplay.boss.elapsed -eq 0 -and $skipped[0].actualGameplayDelta -eq 0 -and
                !$skipped[0].presentation.conditions.introSkipPending -and
                $skipped[0].gameplay.primaryAttacks -eq $before[0].gameplay.primaryAttacks -and
                $skipped[0].gameplay.dashStarts -eq $before[0].gameplay.dashStarts -and
                $skipped[0].gameplay.boss.activationEvents -eq $before[0].gameplay.boss.activationEvents) 'Normal intro-skip input did not consume exactly the locked zero-tick transition.'
            Assert-Depth (@($Rows|Where-Object {$_.frame -gt $skipped[0].frame -and $_.gameplay.primaryAttacks -gt $skipped[0].gameplay.primaryAttacks -and
                $_.gameplay.dashStarts -gt $skipped[0].gameplay.dashStarts}).Count -gt 0) 'Ordinary requested fire/dash never resumed after the skipped locked frame.'
        }
        'player_death' {
            Assert-DepthProbePhase $Definition $Rows 'actual_player_lethal_damage'
            Assert-DepthCameraRestored $Rows 'actual_player_lethal_damage'
            Assert-Depth ($Rows[-1].gameplay.playerDead -and $Rows[-1].gameplay.playerHp -eq 0 -and $Rows[-1].gameplay.flow -eq 3) 'Actual Player HP0/GameOver was not reached.'
        }
        'retry' {
            Assert-DepthProbePhase $Definition $Rows 'actual_player_lethal_damage'
            Assert-DepthCameraRestored $Rows 'actual_player_lethal_damage'
            Assert-Depth (@($Rows|Where-Object {$_.gameplay.playerDead}).Count -gt 0 -and $Rows[-1].sceneEpoch -ge 2 -and !$Rows[-1].gameplay.playerDead) 'Retry did not initialize an actual new scene with a living player.'
            Assert-Depth (@($Rows|ForEach-Object {$_.presentation.conditions.operations}|Where-Object {$_.operation -ceq 'actual_confirm_retry'}).Count -gt 0) 'Retry proof lacks the actual result selection call.'
            Assert-Depth (@($Report.depth.actualSceneNotifications|Where-Object {$_.scene -ceq 'TANK_EXPEDITION' -and $_.sceneEpoch -ge 2 -and $_.actualInitializedScene}).Count -gt 0) 'Retry proof has no actual initialized new scene notification.'
            Assert-Depth (@($Rows|Where-Object {$_.sceneEpoch -ge 2 -and $_.presentation.conditions.cameraScope.scoped}).Count -gt 0) 'Actual retry scene never established its fresh Depth camera scope.'
        }
        'multidraw' {
            $draws=@($Rows|Where-Object {$_.drawProof.extraDrawCalls -eq 2})
            Assert-Depth ($draws.Count -gt 10 -and @($draws|Where-Object {!$_.drawProof.gameplayUnchanged -or !$_.drawProof.modelUpdatesUnchanged -or $_.drawProof.actualVisibleDraws -ne 2}).Count -eq 0) 'Real extra Draw submissions changed gameplay/model sampling or were not actually visible.'
        }
    }
}
function Compare-DepthGameplay([string]$Label,[object[]]$Before,[object[]]$After) {
    Assert-Depth ($Before.Count -eq $After.Count) "$Label actual frame counts differ."
    for($index=0;$index -lt $Before.Count;++$index) {
        $a=$Before[$index].gameplay|ConvertTo-Json -Depth 40 -Compress
        $b=$After[$index].gameplay|ConvertTo-Json -Depth 40 -Compress
        Assert-Depth ([string]::Equals($a,$b,[StringComparison]::Ordinal) -and $Before[$index].actualGameplayDelta -eq $After[$index].actualGameplayDelta) "$Label actual gameplay differs at completed frame $($index+1)."
    }
}
function Assert-DepthCaptures($Settings,$Report,[string]$Directory,[DateTime]$Start) {
    $expected=@($Settings.captureFrames|Where-Object {$_ -le $Report.frameCount})
    Assert-Depth (@($Report.captures).Count -eq $expected.Count -and @($Report.captures.snapshot.frame|Select-Object -Unique).Count -eq $expected.Count) 'Depth sparse capture count/uniqueness differs from completed source frames.'
    foreach($capture in $Report.captures) {
        Assert-Depth ($capture.snapshot.frame -in $expected -and $capture.name -ceq ('frame_'+$capture.snapshot.frame)) 'Unexpected Depth capture name/frame.'
        $png=Join-Path $Directory ($capture.name+'.png');$metadataPath=Join-Path $Directory ($capture.name+'.json')
        foreach($path in @($png,$metadataPath)) {Assert-Depth ((Test-Path -LiteralPath $path -PathType Leaf) -and (Get-Item -LiteralPath $path).LastWriteTimeUtc -ge $Start) 'Depth capture is missing or stale.'}
        $header=[byte[]]::new(24);$stream=[IO.File]::OpenRead($png)
        try {Assert-Depth ($stream.Read($header,0,24) -eq 24) 'Incomplete PNG header.'} finally {$stream.Dispose()}
        Assert-Depth ([Convert]::ToHexString([byte[]]$header[0..7]) -ceq '89504E470D0A1A0A') 'Capture is not a PNG.'
        $width=[int]$header[16]*16777216+[int]$header[17]*65536+[int]$header[18]*256+[int]$header[19]
        $height=[int]$header[20]*16777216+[int]$header[21]*65536+[int]$header[22]*256+[int]$header[23]
        $metadata=Get-Content -LiteralPath $metadataPath -Raw -Encoding UTF8|ConvertFrom-Json
        Assert-Depth ($metadata.scenario -ceq $Settings.scenario -and $metadata.frame -eq $capture.snapshot.frame -and
            $metadata.sceneEpoch -eq $capture.snapshot.sceneEpoch -and $metadata.seed -eq $Settings.seed) 'Depth capture provenance does not identify the requested actual state.'
        Assert-Depth (@($metadata.resolution).Count -eq 2 -and $width -eq $metadata.resolution[0] -and $height -eq $metadata.resolution[1] -and $width -gt 0 -and $height -gt 0) 'PNG native size differs from actual backbuffer metadata.'
        Assert-Depth ($metadata.depthConditions.actualBossPolicy -ceq 'depth' -and $metadata.depthConditions.shortcutFixture -and !$metadata.depthConditions.normalMainRoute -and
            @($metadata.camera.viewProjection).Count -eq 4 -and $metadata.gpu.adapter) 'Depth capture lacks actual camera/render/fixture conditions.'
    }
}
$depthEnvironment=@{
    CG2_GAMEPLAY_SCENARIO=$null;CG2_NEON_BOSS_AUTOTEST=$null;CG2_TANK_COMBAT_AUTOTEST=$null;CG2_TANK_SPECIAL_AUTOTEST=$null
    CG2_TANK_EXPERIENCE_AUTOTEST=$null;CG2_TANK_MAP_AUTOTEST=$null;CG2_TANK_TUTORIAL_AUTOTEST=$null;CG2_TANK_AUTOTEST=$null
    CG2_TITLE_AUTOTEST=$null;CG2_STARTUP_AUTOTEST=$null;CG2_SUBMISSION_AUTOTEST=$null;CG2_TANK_EXPEDITION_VARIANT=$null
    CG2_EXPEDITION_TUTORIAL=$null;CG2_PERF_DISABLED=$null;CG2_PERF_OVERLAY='0';CG2_PERF_CAPTURE_FRAMES=$null
    CG2_PERF_CAPTURE_WARMUP=$null;CG2_PERF_CAPTURE_PATH=$null;CG2_PERF_EXIT_AFTER_CAPTURE=$null;CG2_PERF_STRESS_TRAILS=$null;CG2_FRAME_LIMIT='1'
}
$depthPrevious=@{};$depthResults=[Collections.Generic.List[object]]::new();$depthFirst=@{};$depthProcess=$null
try {
    foreach($key in $depthEnvironment.Keys) {$depthPrevious[$key]=[Environment]::GetEnvironmentVariable($key,'Process');[Environment]::SetEnvironmentVariable($key,$depthEnvironment[$key],'Process')}
    foreach($definition in $depthCases) {
        for($repeat=1;$repeat -le $Repeats;++$repeat) {
            $directory=Join-Path $depthOutput ($definition.name+'_'+$repeat)
            New-Item -ItemType Directory -Path $directory -Force | Out-Null
            $settings=New-DepthSettings $definition $directory
            $settingsPath=Join-Path $directory 'settings.json';Write-DepthJson $settingsPath $settings
            [Environment]::SetEnvironmentVariable('CG2_GAMEPLAY_SCENARIO',$settingsPath,'Process')
            Copy-DepthResources $depthConfig (Join-Path $depthResources 'configs')
            $start=[DateTime]::UtcNow
            $depthProcess=Start-Process -FilePath $ExecutablePath -WorkingDirectory $depthWorking -WindowStyle Hidden -PassThru -ArgumentList @('--project','resources/projects/tank_expedition.project.json')
            Write-Host "Depth $($definition.name) repeat $repeat started (PID $($depthProcess.Id))."
            while(!$depthProcess.WaitForExit(10000)) {
                if(([DateTime]::UtcNow-$start).TotalSeconds -gt $TimeoutSeconds) {throw "Depth case timed out: $($definition.name). Evidence: $directory"}
            }
            Assert-Depth ($depthProcess.ExitCode -eq 0) "Depth case $($definition.name) exited $($depthProcess.ExitCode). Evidence: $directory"
            $depthProcess=$null
            foreach($file in @('report.json','snapshots.json','depth-frames.json')) {
                $path=Join-Path $directory $file
                Assert-Depth ((Test-Path -LiteralPath $path -PathType Leaf) -and (Get-Item -LiteralPath $path).LastWriteTimeUtc -ge $start) "Missing fresh $file"
            }
            $report=Get-Content -LiteralPath (Join-Path $directory 'report.json') -Raw -Encoding UTF8|ConvertFrom-Json
            $rows=@(Get-Content -LiteralPath (Join-Path $directory 'depth-frames.json') -Raw -Encoding UTF8|ConvertFrom-Json)
            Assert-DepthRequestedSettings $definition $settings $report $rows
            Assert-DepthTerminal $definition $report $rows
            Assert-DepthCaptures $settings $report $directory $start
            if($repeat -eq 1) {$depthFirst[$definition.name]=$rows} else {Compare-DepthGameplay ($definition.name+' reproducibility') $depthFirst[$definition.name] $rows}
            $depthResults.Add([ordered]@{case=$definition.name;repeat=$repeat;completed=$true;frames=$rows.Count
                report=(Join-Path $directory 'report.json');depthRowsSha256=(Get-FileHash -LiteralPath (Join-Path $directory 'depth-frames.json') -Algorithm SHA256).Hash
                shortcutFixture=$true;normalMainRoute=$false;reproducibilityCompared=($repeat -gt 1)})
            Write-Host "PASS: actual Depth $($definition.name) repeat $repeat, $($rows.Count) completed updates."
        }
    }
    $parity=@()
    foreach($variant in @('parity_off','parity_reduced')) {
        if($depthFirst.ContainsKey('parity_on') -and $depthFirst.ContainsKey($variant)) {
            Compare-DepthGameplay ('visual gameplay parity '+$variant) $depthFirst.parity_on $depthFirst[$variant]
            $parity+=@{variant=$variant;completed=$true}
        }
    }
    Write-DepthJson (Join-Path $depthOutput 'suite.json') ([ordered]@{completed=$true;status='ACTUAL_RUNTIME_VERIFIED';provenance=$depthProvenance
        repeats=$Repeats;selectedCases=@($depthCases|ForEach-Object name);capturesRequested=[bool]$Capture;results=$depthResults.ToArray();parity=$parity
        caseCoverage=@{mandatoryDefinitions=$depthAllCases;mandatoryCaseCount=$depthAllCases.Count;selectedCaseCount=$depthCases.Count
            fullSuiteCompleted=($depthCases.Count -eq $depthAllCases.Count);plannedLaunches=$depthCases.Count*$Repeats}
        normalRouteValidation='Separate root-owned Title-to-boss/package run required.'})
    Write-Output (Join-Path $depthOutput 'suite.json')
} catch {
    Write-DepthJson (Join-Path $depthOutput 'suite.json') ([ordered]@{completed=$false;status='FAILED_OR_INCOMPLETE';provenance=$depthProvenance
        error=$_.Exception.Message;results=$depthResults.ToArray()})
    throw
} finally {
    if($depthProcess -and !$depthProcess.HasExited) {Stop-Process -Id $depthProcess.Id -ErrorAction SilentlyContinue}
    foreach($key in $depthPrevious.Keys) {[Environment]::SetEnvironmentVariable($key,$depthPrevious[$key],'Process')}
}
