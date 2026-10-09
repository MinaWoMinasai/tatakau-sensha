param(
    [ValidateSet('All','Unit','Source','Rendering','Runtime','Packaging')][string]$Phase = 'All',
    [switch]$InventoryOnly,
    [switch]$NoBuild,
    [switch]$SkipHardware,
    [switch]$SkipAddressSanitizer,
    [string[]]$TestName = @(),
    [string]$OutputDirectory = '',
    [string]$VisualStudioPath = '',
    [string]$PythonPath = 'python',
    [string]$PlatformToolset = ''
)
$ErrorActionPreference = 'Stop'
$validationRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$validationGenerated = Join-Path $validationRepo 'generated'
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $validationGenerated ('repository-validation/' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [Guid]::NewGuid().ToString('N').Substring(0,8))
}
$validationOutput = [IO.Path]::GetFullPath($OutputDirectory)
if (!$validationOutput.StartsWith($validationGenerated + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Validation output must be a new directory below this repository generated/ directory.'
}
if ((Test-Path -LiteralPath $validationOutput) -and @(Get-ChildItem -LiteralPath $validationOutput -Force).Count) {
    throw 'Use a new output directory so old results cannot be mistaken for this run.'
}
New-Item -ItemType Directory -Path $validationOutput -Force | Out-Null

# Explicit membership makes additions visible during review. Unknown tests fail
# inventory rather than silently disappearing from the final validation suite.
$validationGroups = @{
    Unit = @(
        'audio_runtime','boss_gameplay_lifecycle','frame_pacer','generated_texture_cache',
        'neon_boss_visual','neon_contour_geometry','neon_dissolve','neon_feature_masks','neon_preview_animations',
        'neon_projectile_geometry','neon_quality_data','neon_skinned_model',
        'player_class_config','rival_boss_combat','shader_disk_cache','startup_trace',
        'tank_additional_abilities','tank_collisions','tank_enemy_combat',
        'tank_expedition_content','tank_expedition_map','tank_expedition_rooms',
        'tank_expedition_tutorial','tank_expedition','tank_presentation','tank_projectiles',
        'tank_reward_cards','tank_reward_pool','tank_run','tank_trails',
        'combat_presentation','neon_preview_lifecycle','player_derived_stats',
        'player_drone_lifecycle','player_movement','gameplay_scenario_session',
        'neon_depth_combat','neon_depth_presentation','neon_depth_config','neon_depth_runtime_settings',
        'neon_windmill'
    )
    Source = @('developer_tools_profile','neon_particle_shader_contract')
    Rendering = @('bloom_pipeline','neon_skinned_pipeline')
    Runtime = @('neon_boss_runtime','tank_combat_runtime','tank_expedition_map_runtime',
        'tank_experience_runtime','tank_special_runtime','tank_tutorial_runtime','title_demo',
        'gameplay_scenarios','gameplay_scenario_release','neon_depth_runtime')
    Packaging = @('tank_submission_packaging','tank_submission','tank_submission_runtime')
}
$validationPython = @{
    'test_neon_bloom_comparison.py' = 'Unit'
    'test_neon_bloom_source_contract.py' = 'Source'
    'test_gameplay_boundaries.py' = 'Source'
    'test_neon_dissolve_comparison_fixtures.py' = 'Unit'
    'test_neon_showcase_comparison_fixtures.py' = 'Unit'
    'test_neon_showcase_summary.py' = 'Unit'
    'test_neon_quality_masks.py' = 'Unit wrapper'
    'test_neon_dissolve_comparison.py' = 'Fixture validator'
    'test_neon_showcase_comparison.py' = 'Fixture validator'
}
$validationInventory = @(foreach ($file in Get-ChildItem -LiteralPath $PSScriptRoot -Filter 'test*.ps1' | Sort-Object Name) {
    if ($file.FullName -eq $PSCommandPath) { continue }
    $tokens = $null; $parseErrors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile($file.FullName,[ref]$tokens,[ref]$parseErrors)
    if (@($parseErrors).Count) { throw "Invalid PowerShell syntax: $($file.Name): $parseErrors" }
    $stem = $file.BaseName -replace '^test_', ''
    $categories = @($validationGroups.Keys | Where-Object { $validationGroups[$_] -contains $stem })
    if ($categories.Count -ne 1) { throw "Test must have exactly one execution category: $($file.Name)" }
    $text = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    $evidence = @()
    if ($stem -in @('boss_gameplay_lifecycle','player_class_config','tank_collisions','tank_enemy_combat',
        'tank_projectiles','tank_reward_pool','tank_trails','neon_preview_lifecycle','player_drone_lifecycle')) {
        $evidence += 'Production source adapter'
    }
    if ($categories[0] -eq 'Unit' -and $stem -match '^(tank_|player_|boss_|rival_|combat_|gameplay_)') { $evidence += 'Gameplay unit' }
    if ($stem -in @('combat_presentation','player_derived_stats','player_movement','gameplay_scenario_session')) { $evidence += 'Pure component policy' }
    if ($stem -in @('audio_runtime','generated_texture_cache','shader_disk_cache')) { $evidence += 'OS integration' }
    if ($text -match 'resources/|resources\\') { $evidence += 'Real asset integration' }
    if ($stem -eq 'gameplay_scenarios' -or $text -match 'Start-Process.*CG2|Start-Process.*Exe|Start-Process.*exe') { $evidence += 'Actual application runtime' }
    [pscustomobject]@{ name=$file.Name; path=$file.FullName; category=$categories[0]; evidence=$evidence;
        parameters=@($ast.ParamBlock.Parameters | ForEach-Object { $_.Name.VariablePath.UserPath });
        sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
})
$validationInventory += @(foreach ($file in Get-ChildItem -LiteralPath $PSScriptRoot -Filter 'test*.py' | Sort-Object Name) {
    if (!$validationPython.ContainsKey($file.Name)) { throw "Unclassified Python test: $($file.Name)" }
    $kind=$validationPython[$file.Name]
    $evidence=switch ($kind) {
        'Unit' { 'Synthetic / source fixture' }
        'Source' { 'Source contract' }
        'Unit wrapper' { 'Executed by PowerShell wrapper' }
        'Fixture validator' { 'Input validator covered by fixture suite' }
    }
    [pscustomobject]@{ name=$file.Name; path=$file.FullName; category=$validationPython[$file.Name];
        evidence=@($evidence); parameters=@(); sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
})
$validationInventory | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $validationOutput 'test-inventory.json') -Encoding utf8
foreach ($name in $TestName) {
    if ($name -notin $validationInventory.name) { throw "Unknown test interface: $name" }
    $selected=$validationInventory | Where-Object {$_.name -eq $name}
    if ($selected.category -ne $Phase) { throw "Test $name belongs to $($selected.category), not $Phase. Wrapper/validator interfaces run through their registered suite." }
}
if ($TestName.Count -and $Phase -in @('All','Packaging')) { throw 'Use a focused phase with TestName. All and Packaging run complete groups.' }
$validationState = [ordered]@{
    startedUtc=[DateTime]::UtcNow.ToString('o'); phase=$Phase; inventoryOnly=[bool]$InventoryOnly;
    buildsRequested=(!$NoBuild -and !$InventoryOnly -and $Phase -in @('All','Source','Runtime','Packaging'));
    hardwareRequested=(!$SkipHardware -and $Phase -in @('All','Rendering') -and !$InventoryOnly);
    addressSanitizerRequested=(!$SkipAddressSanitizer -and $Phase -in @('All','Unit') -and !$InventoryOnly -and
        (!$TestName.Count -or 'test_player_class_config.ps1' -in $TestName));
    repository=$validationRepo; branch=(& git -C $validationRepo branch --show-current);
    head=(& git -C $validationRepo rev-parse HEAD); status=@(& git -C $validationRepo status --short);
    testCount=$validationInventory.Count; selectedTests=$TestName; results=@()
    runnerSha256=(Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
}
function Save-ValidationState {
    $validationState | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $validationOutput 'results.json') -Encoding utf8
}
Save-ValidationState
if ($InventoryOnly) { Write-Output "PASS: classified $($validationInventory.Count) test interfaces. $validationOutput"; exit 0 }

function Get-ValidationInputManifest {
    foreach ($path in & git -C $validationRepo ls-files --cached --others --exclude-standard | Sort-Object -Unique) {
        if ([IO.Path]::GetExtension($path) -notin @('.cpp','.h','.hlsl','.hlsli','.ps1','.py','.json','.vcxproj','.props','.targets','.sln','.yml','.yaml')) { continue }
        $absolute=Join-Path $validationRepo $path
        if (Test-Path -LiteralPath $absolute -PathType Leaf) {
            [pscustomobject]@{path=$path;sha256=(Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash}
        }
    }
}
$validationInputs=@(Get-ValidationInputManifest)
$validationInputs | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $validationOutput 'source-input-manifest.json') -Encoding utf8

$validationShell = (Get-Process -Id $PID).Path
if (!$VisualStudioPath) {
    $validationWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $validationWhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1)
}
$validationMsbuild = Join-Path $VisualStudioPath 'MSBuild/Current/Bin/MSBuild.exe'
if (!(Test-Path -LiteralPath $validationMsbuild)) { throw 'Existing Visual Studio MSBuild not found.' }
$validationResults = [Collections.Generic.List[object]]::new()
function Invoke-ValidationCase([string]$Name,[string]$Executable,[string[]]$Arguments) {
    $log = Join-Path $validationOutput ($Name + '.log')
    $started = [DateTime]::UtcNow
    Write-Output "START $Name"
    & $Executable @Arguments *> $log
    $code = $LASTEXITCODE
    $entry = [pscustomobject]@{ name=$Name; executable=$Executable; arguments=$Arguments;
        startedUtc=$started.ToString('o'); durationSeconds=([DateTime]::UtcNow-$started).TotalSeconds;
        exitCode=$code; passed=($code -eq 0); log=$log }
    $validationResults.Add($entry)
    $validationState.results=@($validationResults.ToArray())
    Save-ValidationState
    Write-Output "END $Name exit=$code"
}
function Invoke-ValidationScript($Test,[string]$Suffix='',[string[]]$Extra=@()) {
    $name = ($Test.name -replace '\.ps1$','') + $Suffix
    $arguments = @('-NoProfile','-File',$Test.path)
    if ($Test.parameters -contains 'VisualStudioPath') { $arguments += @('-VisualStudioPath',$VisualStudioPath) }
    if ($Test.parameters -contains 'PythonPath') { $arguments += @('-PythonPath',$PythonPath) }
    # Apple reference artwork is local and ignored; the shared suite checks the
    # deterministic motion without requiring those images. The wrapper also
    # supports explicit local GPU verification with its normal invocation.
    if ($Test.name -eq 'test_neon_windmill.ps1') { $arguments += '-CpuOnly' }
    if ($Test.name -eq 'test_gameplay_scenarios.ps1') {
        if ($Test.parameters -notcontains 'IncludeConfigurationProbes') { throw 'The Scenario configuration-probe suite is not ready.' }
        $arguments += '-IncludeConfigurationProbes'
    }
    if ($Test.parameters -contains 'OutputDirectory') { $arguments += @('-OutputDirectory',(Join-Path $validationOutput ('artifacts/'+$name))) }
    $previousTrace=[Environment]::GetEnvironmentVariable('CG2_STARTUP_TRACE_PATH','Process')
    try {
        if ($Test.category -eq 'Runtime') {
            [Environment]::SetEnvironmentVariable('CG2_STARTUP_TRACE_PATH',(Join-Path $validationOutput ($name+'-startup-trace.json')),'Process')
        }
        Invoke-ValidationCase $name $validationShell ($arguments + $Extra)
    } finally { [Environment]::SetEnvironmentVariable('CG2_STARTUP_TRACE_PATH',$previousTrace,'Process') }
    $runtimeFolders = @{
        test_neon_boss_runtime='neon_boss_gameplay'; test_tank_combat_runtime='combat_validation';
        test_tank_special_runtime='special_validation'; test_tank_experience_runtime='experience_validation';
        test_tank_expedition_map_runtime='expedition_map'; test_tank_tutorial_runtime='tank_expedition/tutorial_validation';
        test_title_demo='title_demo'
    }
    $stem=$Test.name -replace '\.ps1$',''
    if ($runtimeFolders.ContainsKey($stem)) {
        $source=Join-Path $validationRepo ('project/generated/'+$runtimeFolders[$stem])
        $target=Join-Path $validationOutput ('artifacts/'+$name)
        $started=[DateTime]::Parse($validationResults[-1].startedUtc).ToUniversalTime()
        $copied=@(if (Test-Path -LiteralPath $source) {
            foreach ($file in Get-ChildItem -LiteralPath $source -File -Recurse | Where-Object {$_.LastWriteTimeUtc -ge $started}) {
                $relative=$file.FullName.Substring($source.Length).TrimStart([char[]]'\/')
                $destination=Join-Path $target $relative
                New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
                Copy-Item -LiteralPath $file.FullName -Destination $destination
                [pscustomobject]@{path=$destination;sha256=(Get-FileHash -LiteralPath $destination).Hash}
            }
        })
        $validationResults[-1] | Add-Member -NotePropertyName artifacts -NotePropertyValue $copied
        Save-ValidationState
    }
}
# Child scripts own their environment restoration. The driver clears ambient
# test/performance flags once so a user's shell cannot select another fixture.
$validationEnvironment = @{}
foreach ($entry in Get-ChildItem Env: | Where-Object {$_.Name -like 'CG2_*'}) {
    $validationEnvironment[$entry.Name]=$entry.Value
    [Environment]::SetEnvironmentVariable($entry.Name,$null,'Process')
}
try {
    if ($validationState.buildsRequested) {
        foreach ($configuration in @('Development','Release')) {
            $arguments=@((Join-Path $validationRepo 'project/CG2.sln'),'/m',"/p:Configuration=$configuration",'/p:Platform=x64')
            if ($PlatformToolset) { $arguments += "/p:PlatformToolset=$PlatformToolset" }
            Invoke-ValidationCase ($configuration.ToLowerInvariant()+'-build') $validationMsbuild $arguments
        }
        if (@($validationResults | Where-Object {!$_.passed}).Count) { throw 'Build failed; runtime and package tests must not run stale executables.' }
    }
    foreach ($test in $validationInventory) {
        if ($test.name -notlike '*.ps1' -or $test.category -eq 'Packaging') { continue }
        if ($Phase -ne 'All' -and $test.category -ne $Phase) { continue }
        if ($TestName.Count -and $test.name -notin $TestName) { continue }
        Invoke-ValidationScript $test
        if ($test.name -eq 'test_tank_experience_runtime.ps1') {
            foreach ($style in @('Shooter','Drone')) { Invoke-ValidationScript $test ('-'+$style) @('-Style',$style) }
        }
        if ($test.category -eq 'Rendering' -and !$SkipHardware) { Invoke-ValidationScript $test '-hardware' @('-Hardware') }
        if ($test.name -eq 'test_player_class_config.ps1' -and !$SkipAddressSanitizer) { Invoke-ValidationScript $test '-asan' @('-AddressSanitizer') }
    }
    foreach ($test in $validationInventory | Where-Object {$_.name -like '*.py' -and $_.category -in @('Source','Unit')}) {
        if ($Phase -ne 'All' -and $test.category -ne $Phase) { continue }
        if ($TestName.Count -and $test.name -notin $TestName) { continue }
        Invoke-ValidationCase ($test.name -replace '\.py$','') $PythonPath @($test.path)
    }
    if ($Phase -eq 'All') {
        $profile=$validationInventory | Where-Object {$_.name -eq 'test_developer_tools_profile.ps1'}
        Invoke-ValidationScript $profile '-runtime-traces' @(
            '-DevelopmentTrace',(Join-Path $validationOutput 'test_neon_boss_runtime-startup-trace.json'),
            '-ReleaseTrace',(Join-Path $validationOutput 'test_tank_combat_runtime-startup-trace.json'))
    }
    if ($Phase -in @('All','Packaging')) {
        $fixture=$validationInventory | Where-Object {$_.name -eq 'test_tank_submission_packaging.ps1'}
        Invoke-ValidationScript $fixture
        $pristine=Join-Path $validationOutput 'pristine-package'
        Invoke-ValidationCase 'package_tank_submission' $validationShell @('-NoProfile','-File',(Join-Path $PSScriptRoot 'package_tank_submission.ps1'),'-OutputDirectory',$pristine)
        $audit=$validationInventory | Where-Object {$_.name -eq 'test_tank_submission.ps1'}
        Invoke-ValidationScript $audit '' @('-PackageDirectory',$pristine)
        if (@($validationResults | Where-Object {$_.name -in @('package_tank_submission','test_tank_submission') -and !$_.passed}).Count -eq 0) {
            $played=Join-Path $validationOutput 'package-runtime-copy'
            Copy-Item -LiteralPath $pristine -Destination $played -Recurse
            $walkthrough=$validationInventory | Where-Object {$_.name -eq 'test_tank_submission_runtime.ps1'}
            Invoke-ValidationScript $walkthrough '' @('-PackageDirectory',$played)
        }
    }
} catch {
    $validationState.aborted=$_.Exception.Message
    throw
} finally {
    foreach ($entry in Get-ChildItem Env: | Where-Object {$_.Name -like 'CG2_*'}) { [Environment]::SetEnvironmentVariable($entry.Name,$null,'Process') }
    foreach ($name in $validationEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$validationEnvironment[$name],'Process') }
    $validationState.completedUtc=[DateTime]::UtcNow.ToString('o')
    Save-ValidationState
}
$validationFailures=@($validationResults | Where-Object {!$_.passed})
$validationFinalInputs=@(Get-ValidationInputManifest)
$validationInitialByPath=@{}; $validationFinalByPath=@{}
foreach ($inputFile in $validationInputs) { $validationInitialByPath[$inputFile.path]=$inputFile.sha256 }
foreach ($inputFile in $validationFinalInputs) { $validationFinalByPath[$inputFile.path]=$inputFile.sha256 }
$validationInputChanges=@(foreach ($path in @($validationInitialByPath.Keys+$validationFinalByPath.Keys | Sort-Object -Unique)) {
    if ($validationInitialByPath[$path] -ne $validationFinalByPath[$path]) { $path }
})
$validationState.sourceChangesDuringRun=$validationInputChanges
$validationState.failedCases=$validationFailures.Count
$validationState.passed=($validationResults.Count -gt 0 -and $validationFailures.Count -eq 0 -and $validationInputChanges.Count -eq 0)
Save-ValidationState
Write-Output "RESULT: $($validationResults.Count) cases; $($validationFailures.Count) failed. $validationOutput"
if ($validationInputChanges.Count) { Write-Output ('Source inputs changed during validation: '+($validationInputChanges -join ', ')) }
if (!$validationState.passed) { exit 1 }
