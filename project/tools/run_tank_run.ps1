param(
    [ValidateSet('Development', 'Debug', 'Release')][string]$Configuration = 'Development',
    [switch]$Validate
)
$ErrorActionPreference = 'Stop'
$tankRunProjectDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$tankRunRepoDir = [System.IO.Path]::GetFullPath((Join-Path $tankRunProjectDir '..'))
$tankRunExe = Join-Path $tankRunRepoDir "generated\outputs\$Configuration\CG2.exe"
# The existing solution maps its Debug selection to the Development project.
if ($Configuration -eq 'Debug' -and !(Test-Path -LiteralPath $tankRunExe)) {
    $tankRunExe = Join-Path $tankRunRepoDir 'generated\outputs\Development\CG2.exe'
}
if (!(Test-Path -LiteralPath $tankRunExe)) {
    throw "Build project/CG2.sln ($Configuration | x64) first."
}

$tankRunPreviousAutotest = [Environment]::GetEnvironmentVariable('CG2_TANK_AUTOTEST', 'Process')
try {
    # A normal launch always stays interactive, even if the calling shell has
    # inherited a validation flag from another session.
    [Environment]::SetEnvironmentVariable('CG2_TANK_AUTOTEST', $(if ($Validate) { '1' } else { $null }), 'Process')
    $tankRunStartOptions = @{
        FilePath = $tankRunExe
        WorkingDirectory = $tankRunProjectDir
        ArgumentList = @('--project', 'resources/projects/tank_run.project.json')
        PassThru = $true
    }
    if ($Validate) { $tankRunStartOptions.WindowStyle = 'Hidden' }
    # The ordinary launch is the interactive prototype requested by the user.
    $tankRunProcess = Start-Process @tankRunStartOptions
    Write-Host "Core clash started (PID $($tankRunProcess.Id), $Configuration)."
} finally {
    [Environment]::SetEnvironmentVariable('CG2_TANK_AUTOTEST', $tankRunPreviousAutotest, 'Process')
}
