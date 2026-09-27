param([Parameter(Mandatory = $true)][string]$PackageDirectory)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'TankSubmissionPackage.ps1')
$tankSubmissionResult = Assert-TankSubmissionPackage $PackageDirectory
Write-Host "PASS: pristine package; runtime files, default title project, SHA256 manifest, no user progress/history ($($tankSubmissionResult.FileCount) files)."
$tankSubmissionResult
