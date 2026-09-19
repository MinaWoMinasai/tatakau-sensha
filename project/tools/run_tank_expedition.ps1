param(
    [ValidateSet('Development', 'Debug', 'Release')][string]$Configuration = 'Development',
    [switch]$Validate,
    [ValidateRange(0, 5)][int]$Variant = 0,
    [switch]$Wait
)
$ErrorActionPreference = 'Stop'
$tankExpProjectDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$tankExpRepoDir = [System.IO.Path]::GetFullPath((Join-Path $tankExpProjectDir '..'))
$tankExpExe = Join-Path $tankExpRepoDir "generated\outputs\$Configuration\CG2.exe"
# The solution maps its Debug selection to the Development project.
if ($Configuration -eq 'Debug' -and !(Test-Path -LiteralPath $tankExpExe)) {
    $tankExpExe = Join-Path $tankExpRepoDir 'generated\outputs\Development\CG2.exe'
}
if (!(Test-Path -LiteralPath $tankExpExe)) {
    throw "Build project/CG2.sln ($Configuration | x64) first."
}

$tankExpPreviousEnvironment = @{
    CG2_TANK_AUTOTEST = [Environment]::GetEnvironmentVariable('CG2_TANK_AUTOTEST', 'Process')
    CG2_TANK_EXPEDITION_VARIANT = [Environment]::GetEnvironmentVariable('CG2_TANK_EXPEDITION_VARIANT', 'Process')
}
try {
    # Ordinary launches are interactive even when a parent shell has test flags.
    [Environment]::SetEnvironmentVariable('CG2_TANK_AUTOTEST', $(if ($Validate) { '1' } else { $null }), 'Process')
    [Environment]::SetEnvironmentVariable('CG2_TANK_EXPEDITION_VARIANT', $(if ($Validate) { [string]$Variant } else { $null }), 'Process')
    $tankExpStartOptions = @{
        FilePath = $tankExpExe
        WorkingDirectory = $tankExpProjectDir
        ArgumentList = @('--project', 'resources/projects/tank_expedition.project.json')
        PassThru = $true
    }
    $tankExpStartOptions.WindowStyle = $(if ($Validate) { 'Hidden' } else { 'Normal' })
    # This ordinary launch is the user-requested interactive prototype.
    $tankExpLaunchTime = [DateTime]::UtcNow
    $tankExpProcess = Start-Process @tankExpStartOptions
    Write-Host "Branching expedition started (PID $($tankExpProcess.Id), $Configuration, validation: $($Validate.IsPresent), variant: $Variant)."
    if ($Wait) {
        if (!$Validate) { $tankExpProcess.WaitForExit(); return }
        if (!$tankExpProcess.WaitForExit(180000)) {
            Stop-Process -Id $tankExpProcess.Id
            throw 'Expedition validation exceeded 180 seconds.'
        }
        if ($Validate) {
            $tankExpResultFile = Join-Path $tankExpProjectDir "generated/tank_expedition/variant_$Variant/validation.json"
            if ($tankExpProcess.ExitCode -ne 0 -or !(Test-Path -LiteralPath $tankExpResultFile) -or
                (Get-Item -LiteralPath $tankExpResultFile).LastWriteTimeUtc -lt $tankExpLaunchTime) {
                throw 'Validation did not produce a fresh successful process result.'
            }
            $tankExpResult = Get-Content -LiteralPath $tankExpResultFile -Raw -Encoding UTF8 | ConvertFrom-Json
            $tankExpRankTotal = ($tankExpResult.maintenanceRanks | Measure-Object -Sum).Sum
            if (!$tankExpResult.completed -or $tankExpResult.validationErrors -ne 0 -or $tankExpResult.rooms -ne 5 -or
                $tankExpRankTotal -ne 4 -or $tankExpResult.maintenanceRemaining -ne 0 -or $tankExpResult.audioLoaded -ne 9 -or !$tankExpResult.musicPlaying) {
                throw "Expedition runtime validation failed: $tankExpResultFile"
            }
            Write-Host "PASS variant $Variant : $($tankExpResult.class), four maintenance points, five rooms, audio loaded."
        }
    }
} finally {
    foreach ($tankExpVariable in $tankExpPreviousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($tankExpVariable, $tankExpPreviousEnvironment[$tankExpVariable], 'Process')
    }
}
