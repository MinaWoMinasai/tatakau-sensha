param([string]$OutputDirectory, [string]$PreparedTextCacheDirectory)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'TankSubmissionPackage.ps1')
$tankSubmissionRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $tankSubmissionRoot ('generated/submission/TatakauSensha_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
}
$tankSubmissionResult = New-TankSubmissionPackage $tankSubmissionRoot $OutputDirectory $PreparedTextCacheDirectory
Write-Host "PASS: Release submission created; $($tankSubmissionResult.FileCount) files; prepared text PNGs: $($tankSubmissionResult.PreparedTextCount); no tutorial progress/logs/generated history."
$tankSubmissionResult
