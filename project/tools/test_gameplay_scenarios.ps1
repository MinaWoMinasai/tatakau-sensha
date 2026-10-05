param(
    [string]$ExecutablePath = '',
    [string]$OutputDirectory = '',
    [ValidateSet('shooter','drone','melee','projectile_stress','enemy_stress','rival_boss','prototype_boss','neon_boss','boss_death','player_restart','stage_transition','expedition_transition','preview_lifecycle')]
    [string[]]$Scenario = @('shooter','drone','melee','projectile_stress','enemy_stress','rival_boss','prototype_boss','neon_boss','boss_death','player_restart','stage_transition','expedition_transition','preview_lifecycle'),
    [ValidateRange(1,10)][int]$Repeats = 2,
    [ValidateRange(30,180)][int]$TimeoutSeconds = 180,
    [switch]$SkipCapture,
    [switch]$IncludeConfigurationProbes
)
$ErrorActionPreference = 'Stop'
$scenarioProject = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$scenarioRoot = [IO.Path]::GetFullPath((Join-Path $scenarioProject '..'))
if (!$ExecutablePath) { $ExecutablePath = Join-Path $scenarioRoot 'generated/outputs/Development/CG2.exe' }
$ExecutablePath = [IO.Path]::GetFullPath($ExecutablePath)
if (!(Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { throw 'Build Development x64 with Developer Tools first, or specify -ExecutablePath.' }
if (!$OutputDirectory) { $OutputDirectory = Join-Path $scenarioRoot 'generated/repository-engineering-overhaul/scenarios' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$scenarioAllowedRoots = @((Join-Path $scenarioRoot 'generated'), (Join-Path $scenarioProject 'generated'))
$scenarioOutputAllowed = $false
foreach ($allowed in $scenarioAllowedRoots) {
    if ($OutputDirectory -eq $allowed -or $OutputDirectory.StartsWith($allowed + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { $scenarioOutputAllowed = $true }
}
if (!$scenarioOutputAllowed) { throw 'Scenario evidence must stay in a workspace generated directory.' }
$scenarioRunDirectory = Join-Path $OutputDirectory ('run_' + [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss_fff') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $scenarioRunDirectory -Force | Out-Null
$scenarioWorkingDirectory = Join-Path $scenarioRunDirectory 'runtime-project'
$scenarioResourceSource = Join-Path $scenarioProject 'resources'
$scenarioRuntimeResources = Join-Path $scenarioWorkingDirectory 'resources'
$scenarioConfigSnapshot = Join-Path $scenarioRunDirectory 'runtime-configs'

function Copy-ScenarioResources([string]$Source, [string]$Destination) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($entry in Get-ChildItem -LiteralPath $Source -Force) {
        if ($entry.PSIsContainer) {
            if ($entry.Name -ine 'generated') { Copy-ScenarioResources $entry.FullName (Join-Path $Destination $entry.Name) }
        } else {
            Copy-Item -LiteralPath $entry.FullName -Destination (Join-Path $Destination $entry.Name) -Force
        }
    }
}
Copy-ScenarioResources $scenarioResourceSource $scenarioRuntimeResources
# A profile/config save in one child process must not become the next fixture's input.
Copy-ScenarioResources (Join-Path $scenarioRuntimeResources 'configs') $scenarioConfigSnapshot
$scenarioResourceManifestPath = Join-Path $scenarioRunDirectory 'runtime-assets-manifest.json'
$scenarioResourceManifest = @(foreach ($file in Get-ChildItem -LiteralPath $scenarioRuntimeResources -Recurse -File) {
    [ordered]@{ path=$file.FullName.Substring($scenarioWorkingDirectory.Length+1); sha256=(Get-FileHash -LiteralPath $file.FullName).Hash }
})
[IO.File]::WriteAllText($scenarioResourceManifestPath, (ConvertTo-Json -InputObject $scenarioResourceManifest -Depth 4), [Text.UTF8Encoding]::new($false))
$scenarioProvenance = [ordered]@{
    executable=$ExecutablePath; executableSha256=(Get-FileHash -LiteralPath $ExecutablePath).Hash
    workingDirectory=$scenarioWorkingDirectory; resourceSource=$scenarioResourceSource
    resourceManifest=$scenarioResourceManifestPath; excludedResourceDirectory='generated (at every depth)'
    configurationResetSource=$scenarioConfigSnapshot
}

function Assert-Scenario([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function Assert-FreshScenarioFile([string]$Path, [DateTime]$Start) {
    Assert-Scenario ((Test-Path -LiteralPath $Path -PathType Leaf) -and (Get-Item -LiteralPath $Path).LastWriteTimeUtc -ge $Start) "Missing fresh evidence: $Path"
}
function New-ScenarioSettings([string]$Id, [string]$Directory) {
    $frames = 480
    if ($Id -in @('rival_boss','prototype_boss','neon_boss')) { $frames = 960 }
    if ($Id -in @('player_restart','stage_transition','expedition_transition')) { $frames = 600 }
    if ($Id -eq 'preview_lifecycle') { $frames = 240 }
    $captureFrames = @()
    if (!$SkipCapture) {
        $captureFrames = @(120, [Math]::Min(360,$frames)) | Select-Object -Unique
        if ($Id -eq 'boss_death') { $captureFrames = @(121,165,201) }
    }
    [ordered]@{
        scenario = $Id; seed = 20261005; fixedDeltaTime = 1.0 / 60.0; frames = $frames
        playerStyle = -1; playerHp = -1; bossHp = 10000
        enemyCount = $(if ($Id -eq 'enemy_stress') { 64 } else { 3 })
        initialProjectiles = $(if ($Id -eq 'projectile_stress') { 192 } else { 0 })
        room = 'arena'; upgrades = @(); enemyWave = @(); input = @()
        captureFrames = @($captureFrames)
        outputDirectory = $Directory
    }
}
function Assert-ScenarioBehavior([string]$Id, $Report, [object[]]$Snapshots) {
    Assert-Scenario ($Report.schemaVersion -eq 1 -and $Report.completed -and @($Report.errors).Count -eq 0) "Scenario $Id failed invariants: $($Report.errors -join '; ')"
    Assert-Scenario ($Report.scenario -ceq $Id -and $Report.frameCount -eq $Snapshots.Count -and $Snapshots.Count -eq $Report.settings.frames) "Scenario $Id frame count/settings mismatch."
    for ($index = 0; $index -lt $Snapshots.Count; ++$index) {
        Assert-Scenario ($Snapshots[$index].frame -eq $index + 1) "Scenario $Id missing global simulation frame $($index + 1)."
    }
    $last = $Snapshots[-1]
    switch ($Id) {
        'shooter' { Assert-Scenario ($Report.maximumPrimaryAttacks -gt 0 -and (@($Snapshots | Where-Object { $_.playerProjectiles -gt 0 }).Count -gt 0)) 'Shooter did not fire real player projectiles.' }
        'drone' { Assert-Scenario ((@($Snapshots | Where-Object { $_.activeDrones -gt 0 -and $_.playerProjectiles -gt 0 }).Count -gt 0)) 'Drone scenario did not create companions and real shots.' }
        'melee' { Assert-Scenario ($Report.maximumPrimaryAttacks -gt 0 -and $Report.maximumActiveAttacks -gt 0 -and (@($Snapshots | Where-Object { $_.minimumEnemyHp -gt 0 -and $_.minimumEnemyHp -lt $Snapshots[0].minimumEnemyHp }).Count -gt 0)) 'Melee did not create an active attack and damage a real enemy.' }
        'projectile_stress' { Assert-Scenario ($Report.maximumProjectiles -ge 150) 'Projectile stress did not exercise at least 150 live projectiles.' }
        'enemy_stress' { Assert-Scenario ($Snapshots[0].enemies -ge 64 -and $Report.maximumEnemies -ge 64) 'Enemy stress did not initialize 64 real enemies.' }
        { $_ -in @('rival_boss','prototype_boss','neon_boss') } {
            Assert-Scenario ((@($Snapshots | Where-Object { $_.bossShots -gt 0 }).Count -gt 0) -and (@($Snapshots.bossPhase | Select-Object -Unique).Count -gt 1)) "$Id did not exercise real boss shots and phase transitions."
            if ($Id -eq 'neon_boss') { Assert-Scenario ($last.visualCreates -eq 1 -and $last.visualHasResources) 'Neon boss model did not create one living resource set.' }
        }
        'boss_death' { Assert-Scenario ($last.bossDead -and $last.bossHp -eq 0 -and $last.visualCreates -eq 1 -and $last.visualReleases -eq 1 -and $last.dissolveProgress -eq 1 -and !$last.visualHasResources -and $last.visualConstantBuffers -eq 0) 'Boss death/dissolve/resource terminal state is incomplete.' }
        'player_restart' { Assert-Scenario ($Report.sawPlayerDeath -and $Report.sceneEpochs -ge 2 -and $last.sceneEpoch -ge 2 -and !$last.playerDead -and $last.playerHp -gt 0) 'Player death and actual scene restart/revival were not exercised.' }
        'stage_transition' { Assert-Scenario (@($Snapshots.roomId | Where-Object { $_ } | Select-Object -Unique).Count -ge 2) 'Stage transition did not visit two actual rooms.' }
        'expedition_transition' { Assert-Scenario (@($Snapshots.nodeId | Where-Object { $_ } | Select-Object -Unique).Count -ge 3 -and $last.visitedNodes -ge 2 -and $Report.details.transitions.servicePurchases -ge 1) 'Expedition transition did not traverse combat, actual shop purchase and next combat nodes.' }
        'preview_lifecycle' { Assert-Scenario ($Report.details.previewLifecycle.completed -and $Report.details.previewLifecycle.repeats -ge 3 -and $Report.details.previewLifecycle.baselineStable) 'Actual D3D Preview lifecycle repetitions/resource return are missing.' }
    }
}
function Test-ScenarioInstructionNumber($Value) {
    if ($null -eq $Value) { return $false }
    [Type]::GetTypeCode($Value.GetType()) -in @(
        [TypeCode]::SByte,[TypeCode]::Byte,[TypeCode]::Int16,[TypeCode]::UInt16,
        [TypeCode]::Int32,[TypeCode]::UInt32,[TypeCode]::Int64,[TypeCode]::UInt64,
        [TypeCode]::Single,[TypeCode]::Double,[TypeCode]::Decimal
    )
}

function Assert-ScenarioInstructionValue($Expected, $Actual, [string]$Location, [bool]$Float32 = $false) {
    if ($null -eq $Expected) {
        Assert-Scenario ($null -eq $Actual) "Instruction value differs at $Location."
        return
    }
    $expectedObject = $Expected -is [Collections.IDictionary] -or $Expected.GetType() -eq [System.Management.Automation.PSCustomObject]
    $actualObject = $null -ne $Actual -and ($Actual -is [Collections.IDictionary] -or $Actual.GetType() -eq [System.Management.Automation.PSCustomObject])
    if ($expectedObject) {
        Assert-Scenario $actualObject "Instruction object type differs at $Location."
        $expectedKeys = @(if ($Expected -is [Collections.IDictionary]) { $Expected.Keys } else { $Expected.PSObject.Properties.Name })
        $actualKeys = @(if ($Actual -is [Collections.IDictionary]) { $Actual.Keys } else { $Actual.PSObject.Properties.Name })
        Assert-Scenario ($expectedKeys.Count -eq $actualKeys.Count) "Instruction object keys differ at $Location."
        $actualKeySet = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($key in $actualKeys) {
            Assert-Scenario ($key -is [string]) "Instruction object has a non-string key at $Location."
            Assert-Scenario ($actualKeySet.Add($key)) "Instruction object has a duplicate key at $Location."
        }
        $expectedKeySet = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($key in $expectedKeys) {
            Assert-Scenario ($key -is [string] -and $expectedKeySet.Add($key) -and $actualKeySet.Contains($key)) "Instruction object key differs at $Location : $key."
            if ($Expected -is [Collections.IDictionary]) { $expectedValue = $Expected[$key] } else { $expectedValue = $Expected.$key }
            if ($Actual -is [Collections.IDictionary]) { $actualValue = $Actual[$key] } else { $actualValue = $Actual.$key }
            $floatVector = [string]::Equals($key,'position',[StringComparison]::Ordinal) -or
                [string]::Equals($key,'movement',[StringComparison]::Ordinal) -or [string]::Equals($key,'aim',[StringComparison]::Ordinal)
            Assert-ScenarioInstructionValue $expectedValue $actualValue ($Location + '.' + $key) $floatVector
        }
        return
    }
    if ($Expected -is [Array] -or $Expected -is [Collections.IList]) {
        Assert-Scenario (($Actual -is [Array] -or $Actual -is [Collections.IList]) -and $Expected.Count -eq $Actual.Count) "Instruction array shape differs at $Location."
        for ($index = 0; $index -lt $Expected.Count; ++$index) {
            Assert-ScenarioInstructionValue $Expected[$index] $Actual[$index] ($Location + '[' + $index + ']') $Float32
        }
        return
    }
    if ($Expected -is [bool]) {
        Assert-Scenario ($Actual -is [bool] -and $Expected -eq $Actual) "Instruction boolean differs at $Location."
        return
    }
    if ($Expected -is [string]) {
        Assert-Scenario ($Actual -is [string] -and [string]::Equals($Expected,$Actual,[StringComparison]::Ordinal)) "Instruction string differs at $Location."
        return
    }
    if (Test-ScenarioInstructionNumber $Expected) {
        Assert-Scenario (Test-ScenarioInstructionNumber $Actual) "Instruction numeric type differs at $Location."
        $a = [double]$Expected; $b = [double]$Actual
        Assert-Scenario (![double]::IsNaN($a) -and ![double]::IsInfinity($a) -and ![double]::IsNaN($b) -and ![double]::IsInfinity($b)) "Nonfinite instruction number at $Location."
        if ($Float32) {
            # The declared position/movement/aim vectors are stored as float32.
            # Compare their representable values, rather than widening a tolerance.
            $a32 = [single]$Expected; $b32 = [single]$Actual
            Assert-Scenario (![single]::IsNaN($a32) -and ![single]::IsInfinity($a32) -and ![single]::IsNaN($b32) -and ![single]::IsInfinity($b32) -and $a32 -eq $b32) "Instruction float32 value differs at $Location : $a32 / $b32."
        } else {
            # hp/frame instructions are bounded integers in the validated schema.
            Assert-Scenario ($a -eq $b) "Instruction integer value differs at $Location : $a / $b."
        }
        return
    }
    throw "Unsupported instruction value at $Location."
}

function Test-ScenarioInstructionComparison {
    $wave = @('[{"type":"Charger","position":[38.0,22.0,0.0],"hp":70000},{"type":"Sniper","position":[50.0,34.0,0.0],"hp":60000}]' | ConvertFrom-Json)
    $shuffledWave = @('[{"hp":70000,"position":[38.0,22.0,0.0],"type":"Charger"},{"position":[50.0,34.0,0.0],"type":"Sniper","hp":60000}]' | ConvertFrom-Json)
    $input = @('[{"firstFrame":0,"endFrame":120,"movement":[0.0,0.0],"aim":[38.0,22.0,0.0],"shoot":false,"dash":false},{"firstFrame":120,"endFrame":240,"movement":[0.5,0.0],"aim":[38.0,22.0,0.0],"shoot":true,"dash":false}]' | ConvertFrom-Json)
    $shuffledInput = @('[{"aim":[38.0,22.0,0.0],"dash":false,"endFrame":120,"firstFrame":0,"movement":[0.0,0.0],"shoot":false},{"shoot":true,"movement":[0.5,0.0],"firstFrame":120,"endFrame":240,"dash":false,"aim":[38.0,22.0,0.0]}]' | ConvertFrom-Json)
    Assert-ScenarioInstructionValue $wave $shuffledWave 'shuffled enemyWave'
    Assert-ScenarioInstructionValue $input $shuffledInput 'shuffled input'
    Assert-ScenarioInstructionValue @() @() 'empty array'
    Assert-ScenarioInstructionValue ([ordered]@{hp=70000;type='Charger'}) ([pscustomobject]@{type='Charger';hp=70000}) 'dictionary and JSON object'
    $highCoordinate = @{position=@(9000.001,22.0,0.0);aim=@(9000.001,0.3,0.0)}
    $storedHighCoordinate = @{position=@([double][single]9000.001,22.0,0.0);aim=@([double][single]9000.001,[double][single]0.3,0.0)}
    Assert-ScenarioInstructionValue $highCoordinate $storedHighCoordinate 'declared float32 normalization'

    $positionChanged = @($wave | ConvertTo-Json -Depth 8 | ConvertFrom-Json); $positionChanged[0].position[0] += 0.25
    $hpChanged = @($wave | ConvertTo-Json -Depth 8 | ConvertFrom-Json); $hpChanged[0].hp += 1
    $inputChanged = @($input | ConvertTo-Json -Depth 8 | ConvertFrom-Json); $inputChanged[0].shoot = $true
    $movementChanged = @($input | ConvertTo-Json -Depth 8 | ConvertFrom-Json); $movementChanged[1].movement[0] = 0.6
    $missingKey = @($wave | ConvertTo-Json -Depth 8 | ConvertFrom-Json); $missingKey[0].PSObject.Properties.Remove('hp')
    $extraKey = @($wave | ConvertTo-Json -Depth 8 | ConvertFrom-Json); $extraKey[0] | Add-Member -NotePropertyName extra -NotePropertyValue 1
    $keyCase = @($wave | ConvertTo-Json -Depth 8 | ConvertFrom-Json); $keyCase[0].PSObject.Properties.Remove('hp'); $keyCase[0] | Add-Member -NotePropertyName Hp -NotePropertyValue 70000
    $positionOrder = @($wave | ConvertTo-Json -Depth 8 | ConvertFrom-Json); $positionOrder[0].position = @(22.0,38.0,0.0)
    $negativeCases = @(
        @{name='nested position';expected=$wave;actual=$positionChanged},
        @{name='nested HP';expected=$wave;actual=$hpChanged},
        @{name='nested shoot';expected=$input;actual=$inputChanged},
        @{name='nested movement';expected=$input;actual=$movementChanged},
        @{name='missing nested key';expected=$wave;actual=$missingKey},
        @{name='extra nested key';expected=$wave;actual=$extraKey},
        @{name='ordinal key case';expected=$wave;actual=$keyCase},
        @{name='wave order';expected=$wave;actual=@($wave[1],$wave[0])},
        @{name='input order';expected=$input;actual=@($input[1],$input[0])},
        @{name='position axis order';expected=$wave;actual=$positionOrder},
        @{name='boolean numeric coercion';expected=@{shoot=$false};actual=@{shoot=0}},
        @{name='boolean string coercion';expected=@{shoot=$false};actual=@{shoot='false'}},
        @{name='numeric string coercion';expected=@{hp=70000};actual=@{hp='70000'}},
        @{name='string numeric coercion';expected=@{id='70000'};actual=@{id=70000}},
        @{name='ordinal string case';expected='Repair';actual='repair'},
        @{name='ordinal Unicode string';expected=([string][char]0xE9);actual=('e'+[char]0x301)},
        @{name='ordinal Unicode key';expected=@{([string][char]0xE9)=1};actual=@{('e'+[char]0x301)=1}},
        @{name='empty array versus null';expected=@{values=@()};actual=@{values=$null}},
        @{name='single array versus scalar';expected=@{values=@('Repair')};actual=@{values='Repair'}},
        @{name='nonfinite NaN';expected=0.0;actual=[double]::NaN},
        @{name='nonfinite infinity';expected=0.0;actual=[double]::PositiveInfinity},
        @{name='integer fractional change';expected=@{hp=70000};actual=@{hp=70000.000001}},
        @{name='frame fractional change';expected=@{firstFrame=0;endFrame=120};actual=@{firstFrame=0;endFrame=120.000001}},
        @{name='capture frame fractional change';expected=@(0,120);actual=@(0,120.000001)},
        @{name='changed high coordinate';expected=@{aim=@(9000.001,22.0,0.0)};actual=@{aim=@(9000.002,22.0,0.0)}}
    )
    foreach ($fixture in $negativeCases) {
        $rejected = $false
        try { Assert-ScenarioInstructionValue $fixture.expected $fixture.actual $fixture.name } catch { $rejected = $true }
        Assert-Scenario $rejected "Instruction comparison accepted altered fixture: $($fixture.name)."
    }
    Write-Host "PASS: instruction comparison shuffled object keys and $($negativeCases.Count) changed-value/type/shape/order fixtures."
}

function Assert-ScenarioSettings([string]$Id, $Requested, $Reported) {
    foreach ($field in @('scenario','seed','frames','playerStyle','playerHp','bossHp','enemyCount','initialProjectiles','room')) {
        Assert-Scenario ($null -ne $Reported.PSObject.Properties[$field] -and $Requested[$field] -ceq $Reported.$field) "Scenario $Id did not run the requested setting $field."
    }
    Assert-Scenario ($null -ne $Reported.PSObject.Properties['fixedDeltaTime'] -and [Math]::Abs([double]$Requested.fixedDeltaTime - [double]$Reported.fixedDeltaTime) -le 1e-8) "Scenario $Id did not use the requested fixed timestep."
    foreach ($field in @('upgrades','enemyWave','input','captureFrames')) {
        Assert-Scenario ($null -ne $Reported.PSObject.Properties[$field]) "Scenario $Id is missing requested instructions $field."
        Assert-ScenarioInstructionValue $Requested[$field] $Reported.$field ($Id + '.' + $field)
    }
}
function Compare-ScenarioSnapshots([string]$Id, [object[]]$Expected, [object[]]$Actual) {
    Assert-Scenario ($Expected.Count -eq $Actual.Count) "Repeat frame count differs: $Id"
    $exactFields = @('frame','sceneEpoch','playerHp','playerMaxHp','playerStyle','playerDead','bossHp','bossActive','bossDead','primaryAttacks','projectiles','playerProjectiles','enemyProjectiles','enemies','activeDrones','activeAttacks','minimumEnemyHp','bossShots','bossDashes','bossPhase','bossEncounterGeneration','visualCreates','visualReleases','visualConstantBuffers','visualHasResources','flow','expeditionPhase','visitedNodes','classId','roomId','nodeId')
    $floatFields = @('simulationTime','dissolveProgress')
    for ($index = 0; $index -lt $Actual.Count; ++$index) {
        foreach ($field in $exactFields) {
            Assert-Scenario ($null -ne $Expected[$index].PSObject.Properties[$field] -and $null -ne $Actual[$index].PSObject.Properties[$field]) "Missing observable field $field in $Id frame $($index+1)."
            Assert-Scenario ($Expected[$index].$field -ceq $Actual[$index].$field) "Non-reproducible $Id frame $($index+1) field $field : $($Expected[$index].$field) / $($Actual[$index].$field)"
        }
        foreach ($field in $floatFields) {
            Assert-Scenario ($null -ne $Expected[$index].PSObject.Properties[$field] -and $null -ne $Actual[$index].PSObject.Properties[$field]) "Missing float observable $field in $Id."
            $a = [double]$Expected[$index].$field; $b = [double]$Actual[$index].$field
            Assert-Scenario (![double]::IsNaN($a) -and ![double]::IsInfinity($a) -and ![double]::IsNaN($b) -and ![double]::IsInfinity($b) -and [Math]::Abs($a-$b) -le 1e-5) "Non-reproducible/nonfinite $Id frame $($index+1) float $field : $a / $b"
        }
        foreach ($field in @('playerPosition','bossPosition')) {
            Assert-Scenario (@($Expected[$index].$field).Count -eq 3 -and @($Actual[$index].$field).Count -eq 3) "Invalid position observable $field in $Id."
            for ($axis = 0; $axis -lt 3; ++$axis) {
                $a = [double]$Expected[$index].$field[$axis]; $b = [double]$Actual[$index].$field[$axis]
                Assert-Scenario (![double]::IsNaN($a) -and ![double]::IsInfinity($a) -and ![double]::IsNaN($b) -and ![double]::IsInfinity($b) -and [Math]::Abs($a-$b) -le 1e-5) "Non-reproducible/nonfinite $Id frame $($index+1) $field axis $axis : $a / $b"
            }
        }
    }
}

function Assert-ScenarioCaptures([string]$Id, $Settings, $Report, [object[]]$Snapshots, [string]$Directory, [DateTime]$Start) {
    Assert-Scenario (@($Report.captures).Count -eq @($Settings.captureFrames).Count) "Scenario $Id capture count mismatch."
    Assert-Scenario (@($Report.captures.snapshot.frame | Select-Object -Unique).Count -eq @($Settings.captureFrames).Count) "Scenario $Id has duplicate capture frames."
    foreach ($capture in $Report.captures) {
        Assert-Scenario ($capture.name -ceq ('frame_' + $capture.snapshot.frame) -and $capture.snapshot.frame -in $Settings.captureFrames) "Unexpected capture name/frame in $Id."
        foreach ($extension in @('png','json')) { Assert-FreshScenarioFile (Join-Path $Directory ($capture.name + '.' + $extension)) $Start }
        Assert-Scenario ((Get-Item -LiteralPath (Join-Path $Directory ($capture.name + '.png'))).Length -gt 512) "Empty capture in $Id."
        $metadata = Get-Content -LiteralPath (Join-Path $Directory ($capture.name + '.json')) -Raw -Encoding UTF8 | ConvertFrom-Json
        Assert-Scenario ($metadata.scenario -ceq $Id -and $metadata.frame -eq $capture.snapshot.frame -and $metadata.sceneEpoch -eq $capture.snapshot.sceneEpoch -and $metadata.seed -eq $Settings.seed) "Capture metadata does not identify the requested simulation state in $Id."
        Compare-ScenarioSnapshots $Id @($capture.snapshot) @($metadata.snapshot)
        if ($capture.snapshot.frame -gt 0) { Compare-ScenarioSnapshots $Id @($Snapshots[$capture.snapshot.frame-1]) @($metadata.snapshot) }
        Assert-Scenario (@($metadata.resolution).Count -eq 2 -and $metadata.resolution[0] -gt 0 -and $metadata.resolution[1] -gt 0 -and $metadata.gpu.adapter -and $metadata.queueTimestampFrequencyHz -gt 0) "Capture $Id lacks actual renderer metadata."
        Assert-Scenario (@($metadata.camera.position).Count -eq 3 -and @($metadata.camera.rotation).Count -eq 3 -and @($metadata.camera.viewProjection).Count -eq 4) "Capture $Id lacks actual camera metadata."
        foreach ($row in $metadata.camera.viewProjection) {
            Assert-Scenario (@($row).Count -eq 4) "Invalid camera matrix shape in $Id."
            foreach ($value in $row) { Assert-Scenario (![double]::IsNaN([double]$value) -and ![double]::IsInfinity([double]$value)) "Nonfinite camera matrix in $Id." }
        }
    }
    if ($Id -eq 'boss_death' -and @($Settings.captureFrames).Count -gt 0) {
        $states = @($Report.captures | Sort-Object { $_.snapshot.frame })
        Assert-Scenario ($states[0].snapshot.bossDead -and $states[0].snapshot.dissolveProgress -eq 0 -and $states[0].snapshot.visualHasResources) 'Boss death start capture is missing.'
        Assert-Scenario ($states[1].snapshot.dissolveProgress -gt 0.4 -and $states[1].snapshot.dissolveProgress -lt 0.65 -and $states[1].snapshot.visualHasResources) 'Boss death midpoint capture is missing.'
        Assert-Scenario ($states[2].snapshot.dissolveProgress -eq 1 -and !$states[2].snapshot.visualHasResources -and $states[2].snapshot.visualReleases -eq 1) 'Boss death terminal capture is missing.'
    }
}

function Invoke-ScenarioRuntime([string]$Label, $Settings, [string]$Directory, [int]$ExpectedExitCode = 0, [switch]$UseDefaultStartup) {
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $manifest = Join-Path $Directory 'settings.json'
    [IO.File]::WriteAllText($manifest, ($Settings | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
    [Environment]::SetEnvironmentVariable('CG2_GAMEPLAY_SCENARIO', $manifest, 'Process')
    Copy-ScenarioResources $scenarioConfigSnapshot (Join-Path $scenarioRuntimeResources 'configs')
    $start = [DateTime]::UtcNow
    $launch = @{ FilePath=$ExecutablePath; WorkingDirectory=$scenarioWorkingDirectory; WindowStyle='Hidden'; PassThru=$true }
    if (!$UseDefaultStartup) { $launch.ArgumentList = @('--project','resources/projects/tank_expedition.project.json') }
    $script:scenarioProcess = Start-Process @launch
    Write-Host "Scenario $Label started (PID $($script:scenarioProcess.Id))."
    while (!$script:scenarioProcess.WaitForExit(10000)) {
        if (([DateTime]::UtcNow - $start).TotalSeconds -gt $TimeoutSeconds) {
            Stop-Process -Id $script:scenarioProcess.Id -ErrorAction SilentlyContinue
            throw "Scenario $Label exceeded $TimeoutSeconds seconds. Evidence: $Directory"
        }
    }
    $exitCode = $script:scenarioProcess.ExitCode
    Assert-Scenario ($exitCode -eq $ExpectedExitCode) "Scenario $Label exited $exitCode (expected $ExpectedExitCode). Evidence: $Directory"
    $reportPath = Join-Path $Directory 'report.json'; $snapshotsPath = Join-Path $Directory 'snapshots.json'
    Assert-FreshScenarioFile $reportPath $start; Assert-FreshScenarioFile $snapshotsPath $start
    $report = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $snapshots = @(Get-Content -LiteralPath $snapshotsPath -Raw -Encoding UTF8 | ConvertFrom-Json)
    $script:scenarioProcess = $null
    [pscustomobject]@{ report=$report; snapshots=$snapshots; reportPath=$reportPath; start=$start; exitCode=$exitCode }
}

function Invoke-ScenarioConfigurationProbes {
    $directory = Join-Path $scenarioRunDirectory 'configuration_configured_input'
    $settings = New-ScenarioSettings 'shooter' $directory
    $settings.frames = 360; $settings.fixedDeltaTime = 1.0 / 120.0
    $settings.playerStyle = 0; $settings.playerHp = 12; $settings.room = 'outskirts'
    $settings.upgrades = @('Repair'); $settings.enemyCount = 1
    $settings.enemyWave = @([ordered]@{type='Charger';position=@(38.0,22.0,0.0);hp=70000})
    $settings.input = @(
        [ordered]@{firstFrame=0;endFrame=120;movement=@(0.0,0.0);aim=@(38.0,22.0,0.0);shoot=$false;dash=$false},
        [ordered]@{firstFrame=120;endFrame=240;movement=@(0.5,0.0);aim=@(38.0,22.0,0.0);shoot=$true;dash=$false},
        [ordered]@{firstFrame=240;endFrame=360;movement=@(0.0,0.0);aim=@(38.0,22.0,0.0);shoot=$false;dash=$false}
    )
    $settings.captureFrames = @(); if (!$SkipCapture) { $settings.captureFrames = @(0,120,240,360) }
    $run = Invoke-ScenarioRuntime 'configured input/HP/room/wave/upgrade/timestep' $settings $directory
    Assert-ScenarioSettings 'shooter' $settings $run.report.settings
    Assert-ScenarioBehavior 'shooter' $run.report $run.snapshots
    Assert-ScenarioCaptures 'shooter' $settings $run.report $run.snapshots $directory $run.start
    Assert-Scenario ($run.report.details.fixture.authoredRoom -ceq 'outskirts' -and $run.report.details.fixture.geometryPreserved -and $run.report.details.fixture.upgrades -contains 'Repair') 'Configured authored room/upgrade fixture is missing.'
    Assert-Scenario ($run.snapshots[0].enemies -eq 1 -and $run.snapshots[0].minimumEnemyHp -eq 70000 -and $run.snapshots[0].playerHp -eq 12 -and $run.snapshots[0].playerMaxHp -gt 120) 'Configured wave/HP or actual Repair maximum-HP effect is missing.'
    Assert-Scenario (@($run.snapshots | Where-Object { $_.playerHp -ne 12 }).Count -eq 0 -and [Math]::Abs($run.snapshots[-1].simulationTime-3.0) -le 1e-5) 'Configured HP/fixed simulation time was not retained.'
    Assert-Scenario (@($run.snapshots[0..119] | Where-Object { $_.primaryAttacks -gt 0 -or $_.playerProjectiles -gt 0 }).Count -eq 0) 'Explicit shoot=false segment emitted player attacks.'
    Assert-Scenario ($run.snapshots[239].primaryAttacks -gt 0 -and [Math]::Abs($run.snapshots[239].playerPosition[0]-$run.snapshots[119].playerPosition[0]) -gt 0.3) 'Explicit move/shoot segment did not affect real Player behavior.'
    Assert-Scenario ($run.snapshots[-1].primaryAttacks -eq $run.snapshots[239].primaryAttacks) 'Final explicit shoot=false segment emitted another primary attack.'
    $scenarioProbeResults.Add([ordered]@{probe='configured_input';completed=$true;expectedExit=0;report=$run.reportPath;frames=$run.snapshots.Count})
    Write-Host 'PASS: custom positive HP, authored room/wave, real Repair upgrade, scripted movement/shoot and 1/120 timestep.'

    foreach ($probe in @('unknown_room','unknown_upgrade','incompatible_room','incomplete_boss_death')) {
        $directory = Join-Path $scenarioRunDirectory ('configuration_' + $probe)
        $settings = New-ScenarioSettings $(if ($probe -eq 'incomplete_boss_death') {'boss_death'} else {'shooter'}) $directory
        $settings.captureFrames = @()
        switch ($probe) {
            'unknown_room' { $settings.room = 'missing_scenario_room' }
            'unknown_upgrade' { $settings.upgrades = @('MissingScenarioUpgrade') }
            'incompatible_room' { $settings.room = 'gatekeeper' }
            'incomplete_boss_death' { $settings.frames = 120 }
        }
        $run = Invoke-ScenarioRuntime $probe $settings $directory 9
        Assert-Scenario (!$run.report.completed -and @($run.report.errors).Count -gt 0 -and @($run.report.errors).Count -le 16 -and $run.snapshots.Count -le $settings.frames) "Probe $probe did not produce a bounded failure report."
        if ($probe -ne 'incomplete_boss_death') { Assert-Scenario ($run.snapshots.Count -eq 0) "Invalid fixture $probe advanced gameplay." }
        $scenarioProbeResults.Add([ordered]@{probe=$probe;completed=$true;expectedExit=9;report=$run.reportPath;frames=$run.snapshots.Count;reportedErrors=@($run.report.errors)})
        Write-Host "PASS: $probe rejected with exit9 and a bounded report."
    }
    $directory = Join-Path $scenarioRunDirectory 'configuration_wrong_startup'
    $settings = New-ScenarioSettings 'shooter' $directory
    $settings.captureFrames = @()
    $run = Invoke-ScenarioRuntime 'valid manifest with default TITLE startup' $settings $directory 9 -UseDefaultStartup
    Assert-Scenario (!$run.report.completed -and @($run.report.errors).Count -gt 0 -and @($run.report.errors).Count -le 16 -and $run.snapshots.Count -eq 0) 'Wrong startup did not produce an immediate bounded failure report.'
    $scenarioProbeResults.Add([ordered]@{probe='wrong_startup';completed=$true;expectedExit=9;report=$run.reportPath;frames=$run.snapshots.Count;reportedErrors=@($run.report.errors)})
    Write-Host 'PASS: a valid manifest with default TITLE startup is rejected with exit9 and a bounded report.'
    if (!$SkipCapture) {
        $directory = Join-Path $scenarioRunDirectory 'configuration_restart_capture_boundary'
        $settings = New-ScenarioSettings 'player_restart' $directory
        $settings.captureFrames = @(0,226,600)
        $run = Invoke-ScenarioRuntime 'restart capture zero/old-scene boundary/final frame' $settings $directory
        Assert-ScenarioSettings 'player_restart' $settings $run.report.settings
        Assert-ScenarioBehavior 'player_restart' $run.report $run.snapshots
        Assert-ScenarioCaptures 'player_restart' $settings $run.report $run.snapshots $directory $run.start
        Assert-Scenario ($run.report.captures[0].snapshot.sceneEpoch -eq 1 -and $run.report.captures[1].snapshot.sceneEpoch -eq 1 -and $run.report.captures[2].snapshot.sceneEpoch -ge 2) 'Capture ownership did not span the actual restart boundary.'
        $scenarioProbeResults.Add([ordered]@{probe='restart_capture_boundary';completed=$true;expectedExit=0;report=$run.reportPath;frames=$run.snapshots.Count})
        Write-Host 'PASS: exactly one frame0 capture, old-scene boundary capture and final frame capture across actual restart.'
    }
}

$scenarioEnvironment = @{
    CG2_GAMEPLAY_SCENARIO = $null
    CG2_NEON_BOSS_AUTOTEST = $null; CG2_TANK_COMBAT_AUTOTEST = $null; CG2_TANK_SPECIAL_AUTOTEST = $null
    CG2_TANK_EXPERIENCE_AUTOTEST = $null; CG2_TANK_MAP_AUTOTEST = $null; CG2_TANK_TUTORIAL_AUTOTEST = $null
    CG2_TANK_AUTOTEST = $null; CG2_TITLE_AUTOTEST = $null; CG2_STARTUP_AUTOTEST = $null; CG2_SUBMISSION_AUTOTEST = $null
    CG2_TANK_EXPEDITION_VARIANT = $null; CG2_EXPEDITION_TUTORIAL = $null
    CG2_PERF_DISABLED = $null; CG2_PERF_OVERLAY = '0'; CG2_PERF_CAPTURE_FRAMES = $null; CG2_PERF_CAPTURE_WARMUP = $null; CG2_PERF_CAPTURE_PATH = $null
    CG2_PERF_EXIT_AFTER_CAPTURE = $null; CG2_PERF_STRESS_TRAILS = $null; CG2_FRAME_LIMIT = '1'
}
$scenarioPrevious = @{}
$scenarioProcess = $null
$scenarioResults = [Collections.Generic.List[object]]::new()
$scenarioProbeResults = [Collections.Generic.List[object]]::new()
try {
    Test-ScenarioInstructionComparison
    foreach ($key in $scenarioEnvironment.Keys) {
        $scenarioPrevious[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $scenarioEnvironment[$key], 'Process')
    }
    foreach ($id in ($Scenario | Select-Object -Unique)) {
        $firstSnapshots = $null
        for ($repeat = 1; $repeat -le $Repeats; ++$repeat) {
            $directory = Join-Path $scenarioRunDirectory ($id + '_' + $repeat)
            $settings = New-ScenarioSettings $id $directory
            $run = Invoke-ScenarioRuntime "$id repeat $repeat/$Repeats" $settings $directory
            $report = $run.report; $snapshots = $run.snapshots; $reportPath = $run.reportPath
            Assert-ScenarioSettings $id $settings $report.settings
            Assert-ScenarioBehavior $id $report $snapshots
            Assert-ScenarioCaptures $id $settings $report $snapshots $directory $run.start
            if ($repeat -eq 1) { $firstSnapshots = $snapshots } else { Compare-ScenarioSnapshots $id $firstSnapshots $snapshots }
            $scenarioResults.Add([ordered]@{ scenario=$id; repeat=$repeat; directory=$directory; report=$reportPath; frames=$snapshots.Count; completed=$true; reproducibilityCompared=($repeat -gt 1) })
            Write-Host "PASS: $id repeat $repeat, $($snapshots.Count) real simulation frames, invariants and captures."
            $scenarioProcess = $null
        }
    }
    if ($IncludeConfigurationProbes) { Invoke-ScenarioConfigurationProbes }
    $summaryPath = Join-Path $scenarioRunDirectory 'suite.json'
    [IO.File]::WriteAllText($summaryPath, ([ordered]@{ completed=$true; executable=$ExecutablePath; executableSha256=$scenarioProvenance.executableSha256; workingDirectory=$scenarioWorkingDirectory; provenance=$scenarioProvenance; repeats=$Repeats; capturesEnabled=(!$SkipCapture); configurationProbesEnabled=[bool]$IncludeConfigurationProbes; floatTolerance=1e-5; results=$scenarioResults.ToArray(); configurationProbes=$scenarioProbeResults.ToArray() } | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
    Write-Host 'PASS: actual gameplay scenarios, terminal invariants and repeated observable state comparisons.'
    Write-Output $summaryPath
} catch {
    [IO.File]::WriteAllText((Join-Path $scenarioRunDirectory 'suite.json'), ([ordered]@{ completed=$false; executable=$ExecutablePath; executableSha256=$scenarioProvenance.executableSha256; workingDirectory=$scenarioWorkingDirectory; provenance=$scenarioProvenance; repeats=$Repeats; error=$_.Exception.Message; results=$scenarioResults.ToArray(); configurationProbes=$scenarioProbeResults.ToArray() } | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
    throw
} finally {
    if ($scenarioProcess -and !$scenarioProcess.HasExited) { Stop-Process -Id $scenarioProcess.Id -ErrorAction SilentlyContinue }
    foreach ($key in $scenarioPrevious.Keys) { [Environment]::SetEnvironmentVariable($key, $scenarioPrevious[$key], 'Process') }
}
