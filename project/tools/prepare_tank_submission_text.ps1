param(
    [string]$OutputDirectory,
    [ValidateRange(240, 1800)][int]$TimeoutSeconds = 360
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'TankSubmissionPackage.ps1')
$tankPreparationRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $tankPreparationRepo ('generated/submission_text_preparation/' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
}
$tankPreparationRoot = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/')
foreach ($tankProtected in @('project/resources', 'generated/outputs', 'project/tools', 'docs')) {
    $tankProtectedPath = [IO.Path]::GetFullPath((Join-Path $tankPreparationRepo $tankProtected)).TrimEnd('\', '/')
    if (($tankPreparationRoot + '\').StartsWith($tankProtectedPath + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Preparation output must not be inside source assets, build outputs, tools, or documentation.'
    }
}
if (Test-Path -LiteralPath $tankPreparationRoot) { throw 'Preparation output already exists; choose a new directory.' }
$tankStage = Join-Path $tankPreparationRoot 'staging'
$tankPreparedText = Join-Path $tankPreparationRoot 'text'
# This creates a fresh package without any prepared cache or author history.
New-TankSubmissionPackage $tankPreparationRepo $tankStage | Out-Null
$tankTutorial = Join-Path $tankStage 'resources/configs/expedition_user.json'
if (Test-Path -LiteralPath $tankTutorial) { throw 'Fresh staging unexpectedly contains tutorial progress.' }

$tankPreviousEnvironment = @{}
foreach ($entry in [Environment]::GetEnvironmentVariables('Process').GetEnumerator()) {
    if ([string]$entry.Key -like 'CG2_*') { $tankPreviousEnvironment[[string]$entry.Key] = [string]$entry.Value }
}
$tankPreparationProcess = $null
$tankPreparationWatch = [Diagnostics.Stopwatch]::StartNew()
try {
    foreach ($name in $tankPreviousEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $null, 'Process') }
    # The normal startup probe advances Title -> initial map, then exits. No tutorial/combat fixture runs.
    [Environment]::SetEnvironmentVariable('CG2_STARTUP_AUTOTEST', '1', 'Process')
    $tankPreparationProcess = Start-Process -FilePath (Join-Path $tankStage 'CG2.exe') -WorkingDirectory $tankStage -WindowStyle Hidden -PassThru
    if (!$tankPreparationProcess.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $tankPreparationProcess.Id -ErrorAction SilentlyContinue
        throw "Text preparation exceeded $TimeoutSeconds seconds; isolated staging is preserved for inspection."
    }
    if ($tankPreparationProcess.ExitCode -ne 0) { throw "Text preparation failed with exit code $($tankPreparationProcess.ExitCode)." }
} finally {
    $tankPreparationWatch.Stop()
    foreach ($name in @([Environment]::GetEnvironmentVariables('Process').Keys | Where-Object { [string]$_ -like 'CG2_*' })) {
        [Environment]::SetEnvironmentVariable([string]$name, $null, 'Process')
    }
    foreach ($name in $tankPreviousEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $tankPreviousEnvironment[$name], 'Process') }
}
if (Test-Path -LiteralPath $tankTutorial) { throw 'Startup preparation wrote tutorial progress; no text cache was exported.' }
if (Test-Path -LiteralPath ($tankTutorial + '.tmp')) { throw 'Startup preparation left tutorial progress temporary data; no cache was exported.' }
$tankTracePath = Join-Path $tankStage 'generated/startup_trace.json'
$tankTrace = Get-Content -LiteralPath $tankTracePath -Raw -Encoding UTF8 | ConvertFrom-Json
if (!@($tankTrace.events | Where-Object { $_.name -eq 'first_frame.TITLE' }).Count -or
    !@($tankTrace.events | Where-Object { $_.name -eq 'first_frame.TANK_EXPEDITION' }).Count) {
    throw 'Startup preparation did not reach both the title and expedition map.'
}
$tankTextFiles = @(Get-TankSubmissionPreparedTextFiles (Join-Path $tankStage 'resources/generated/text') (Join-Path $tankPreparationRepo 'project/resources'))
New-Item -ItemType Directory -Path $tankPreparedText | Out-Null
foreach ($file in $tankTextFiles) { Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $tankPreparedText $file.Name) }
$tankTextManifest = [ordered]@{
    schemaVersion = 1
    kind = 'clean-staging-text-png'
    source = 'fresh Release package; CG2_STARTUP_AUTOTEST only; title to initial expedition map'
    tutorialProgressAbsent = $true
    elapsedSeconds = [math]::Round($tankPreparationWatch.Elapsed.TotalSeconds, 3)
    executableSha256 = (Get-FileHash -LiteralPath (Join-Path $tankStage 'CG2.exe') -Algorithm SHA256).Hash
    fontSha256 = (Get-FileHash -LiteralPath (Join-Path $tankStage 'resources/fonts/ZenMaruGothic-Bold.ttf') -Algorithm SHA256).Hash
    files = @($tankTextFiles | Sort-Object Name | ForEach-Object {
        [ordered]@{ path = 'text/' + $_.Name; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
}
# Preparation logs/metadata stay outside the PNG-only directory and never enter the final package.
$tankTextManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $tankPreparationRoot 'preparation.json') -Encoding UTF8
Write-Host "PASS: prepared $($tankTextFiles.Count) text PNGs from a fresh staging launch; tutorial progress absent."
[pscustomobject]@{ PreparedTextCacheDirectory = $tankPreparedText; StagingDirectory = $tankStage; FileCount = $tankTextFiles.Count; PreparationRecord = (Join-Path $tankPreparationRoot 'preparation.json') }
