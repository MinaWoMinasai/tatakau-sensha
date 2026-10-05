param(
    [string]$ExecutablePath = '',
    [string]$OutputDirectory = '',
    [ValidateRange(30,180)][int]$TimeoutSeconds = 90
)
$ErrorActionPreference='Stop'
$releaseScenarioRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$ExecutablePath) { $ExecutablePath=Join-Path $releaseScenarioRepo 'generated/outputs/Release/CG2.exe' }
if (!$OutputDirectory) { $OutputDirectory=Join-Path $releaseScenarioRepo ('generated/scenario-release-smoke/'+[DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)) }
$ExecutablePath=[IO.Path]::GetFullPath($ExecutablePath)
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
$releaseScenarioGenerated=Join-Path $releaseScenarioRepo 'generated'
if (!$OutputDirectory.StartsWith($releaseScenarioGenerated+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Release smoke evidence must stay below repository generated/.' }
if (!(Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) { throw 'Build default Release x64 first.' }
if ((Test-Path -LiteralPath $OutputDirectory) -and @(Get-ChildItem -LiteralPath $OutputDirectory -Force).Count) { throw 'Use a new output directory so stale evidence cannot pass.' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$releaseScenarioWorking=Join-Path $OutputDirectory 'runtime-project'
$releaseScenarioResources=Join-Path $releaseScenarioWorking 'resources'
function Copy-ReleaseScenarioResources([string]$Source,[string]$Destination) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($entry in Get-ChildItem -LiteralPath $Source) {
        if ($entry.PSIsContainer) {
            if ($entry.Name -ne 'generated') { Copy-ReleaseScenarioResources $entry.FullName (Join-Path $Destination $entry.Name) }
        } else {
            Copy-Item -LiteralPath $entry.FullName -Destination (Join-Path $Destination $entry.Name)
        }
    }
}
Copy-ReleaseScenarioResources (Join-Path $releaseScenarioRepo 'project/resources') $releaseScenarioResources
@(foreach ($file in Get-ChildItem -LiteralPath $releaseScenarioResources -Recurse -File) {
    [ordered]@{path=$file.FullName.Substring($releaseScenarioWorking.Length+1);sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
}) | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'runtime-assets-manifest.json') -Encoding utf8
$releaseScenarioSaved=@{}; $releaseScenarioProcess=$null
foreach ($entry in Get-ChildItem Env: | Where-Object {$_.Name -like 'CG2_*'}) {
    $releaseScenarioSaved[$entry.Name]=$entry.Value
    [Environment]::SetEnvironmentVariable($entry.Name,$null,'Process')
}
$releaseScenarioResults=[Collections.Generic.List[object]]::new()
try {
    foreach ($kind in @('nonexistent','malformed')) {
        $directory=Join-Path $OutputDirectory $kind
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        $manifest=Join-Path $directory 'manifest.json'
        $reportDirectory=Join-Path $directory 'must-not-be-created'
        if ($kind -eq 'malformed') {
            # A Developer process rejects this invalid scenario. Release must
            # never parse it or create its requested output directory.
            [IO.File]::WriteAllText($manifest,(@{scenario='not-a-scenario';outputDirectory=$reportDirectory} | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
        }
        $tracePath=Join-Path $directory 'startup-trace.json'
        $env:CG2_GAMEPLAY_SCENARIO=$manifest
        $env:CG2_STARTUP_AUTOTEST='1'
        $env:CG2_STARTUP_TRACE_PATH=$tracePath
        $env:CG2_FRAME_LIMIT='1'
        $started=[DateTime]::UtcNow
        # No arguments: normal project discovery starts the ordinary Title.
        $releaseScenarioProcess=Start-Process -FilePath $ExecutablePath -WorkingDirectory $releaseScenarioWorking -WindowStyle Hidden -PassThru
        Write-Output "START Release scenario exclusion: $kind"
        while (!$releaseScenarioProcess.WaitForExit(10000)) {
            if (([DateTime]::UtcNow-$started).TotalSeconds -gt $TimeoutSeconds) { throw "Release startup timeout: $kind" }
        }
        if ($releaseScenarioProcess.ExitCode -ne 0) { throw "Release unexpectedly consumed $kind scenario settings: exit $($releaseScenarioProcess.ExitCode)." }
        if (!(Test-Path -LiteralPath $tracePath) -or (Get-Item -LiteralPath $tracePath).LastWriteTimeUtc -lt $started) { throw 'No fresh normal Release startup trace.' }
        $trace=Get-Content -LiteralPath $tracePath -Raw -Encoding UTF8 | ConvertFrom-Json
        foreach ($key in @('build.developer_tools','ui.imgui_initialized','ui.runtime_profiler_allowed')) {
            $counter=$trace.counters.PSObject.Properties[$key]
            if ($null -eq $counter -or $counter.Value -ne 0) { throw "Release Developer UI/API exclusion failed: $key" }
        }
        $events=@($trace.events.name)
        if ('first_frame.TITLE' -notin $events -or 'first_frame.TANK_EXPEDITION' -notin $events) { throw 'Release did not complete the existing Title→Expedition startup validation.' }
        if (Test-Path -LiteralPath $reportDirectory) { throw 'Release created a Developer scenario output directory.' }
        $releaseScenarioResults.Add([ordered]@{case=$kind;passed=$true;exitCode=$releaseScenarioProcess.ExitCode;
            startedUtc=$started.ToString('o');durationSeconds=([DateTime]::UtcNow-$started).TotalSeconds;
            trace=$tracePath;scenarioEnvironment=$manifest;forbiddenReportDirectory=$reportDirectory;
            titlePresented=$true;expeditionPresented=$true;developerTools=0;imgui=0;profiler=0})
        Write-Output "PASS: Release ignores $kind Scenario environment; ordinary Title→Expedition, Developer/UI/profiler all OFF."
        $releaseScenarioProcess=$null
    }
    [ordered]@{completed=$true;executable=$ExecutablePath;sha256=(Get-FileHash -LiteralPath $ExecutablePath).Hash;results=$releaseScenarioResults.ToArray()} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'validation.json') -Encoding utf8
} finally {
    if ($releaseScenarioProcess -and !$releaseScenarioProcess.HasExited) { Stop-Process -Id $releaseScenarioProcess.Id -ErrorAction SilentlyContinue }
    foreach ($entry in Get-ChildItem Env: | Where-Object {$_.Name -like 'CG2_*'}) { [Environment]::SetEnvironmentVariable($entry.Name,$null,'Process') }
    foreach ($name in $releaseScenarioSaved.Keys) { [Environment]::SetEnvironmentVariable($name,$releaseScenarioSaved[$name],'Process') }
}
