param([ValidateSet('Development','Debug','Release')][string]$Configuration='Development', [switch]$Demo, [switch]$Validate, [switch]$Fidelity, [switch]$Weapons, [switch]$Feel, [switch]$Settings)
$ErrorActionPreference='Stop'
$inkProjectDir=[System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$inkRepoDir=[System.IO.Path]::GetFullPath((Join-Path $inkProjectDir '..'))
$inkExe=Join-Path $inkRepoDir "generated\outputs\$Configuration\CG2.exe"
# The existing solution maps its Debug selection to the Development project.
if ($Configuration -eq 'Debug' -and !(Test-Path -LiteralPath $inkExe)) {
    $inkExe=Join-Path $inkRepoDir 'generated\outputs\Development\CG2.exe'
}
if (!(Test-Path -LiteralPath $inkExe)) { throw "Build project/CG2.sln ($Configuration | x64) first." }
$inkPreviousDemo=$env:CG2_INK_AUTOTEST
$inkPreviousSettings=$env:CG2_INK_SETTINGS
try {
    if ($Demo) { $env:CG2_INK_AUTOTEST='1' }
    if ($Validate) { $env:CG2_INK_AUTOTEST='2' }
    if ($Fidelity) { $env:CG2_INK_AUTOTEST='3' }
    if ($Weapons) { $env:CG2_INK_AUTOTEST='4' }
    if ($Feel) { $env:CG2_INK_AUTOTEST='5' }
    if ($Settings) { $env:CG2_INK_SETTINGS='1' }
    Start-Process -FilePath $inkExe -WorkingDirectory $inkProjectDir -ArgumentList '--project','resources/projects/ink_shooter.project.json'
} finally {
    $env:CG2_INK_AUTOTEST=$inkPreviousDemo
    $env:CG2_INK_SETTINGS=$inkPreviousSettings
}
