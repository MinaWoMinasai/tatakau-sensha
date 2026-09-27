param([ValidateSet('Development','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$titleProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$titleRoot=[IO.Path]::GetFullPath((Join-Path $titleProject '..'))
$titleExe=Join-Path $titleRoot "generated/outputs/$Configuration/CG2.exe"
if(!(Test-Path -LiteralPath $titleExe)) {throw "Build $Configuration first."}
$titleVariables=@('CG2_TITLE_AUTOTEST','CG2_TANK_AUTOTEST','CG2_TANK_TUTORIAL_AUTOTEST','CG2_TANK_MAP_AUTOTEST','CG2_TANK_COMBAT_AUTOTEST','CG2_TANK_EXPERIENCE_AUTOTEST','CG2_TANK_SPECIAL_AUTOTEST','CG2_PERF_EXIT_AFTER_CAPTURE','CG2_PERF_STRESS_TRAILS','CG2_FRAME_LIMIT')
$titleSaved=@{}
foreach($name in $titleVariables) {$titleSaved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    foreach($name in $titleVariables) {[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:CG2_TITLE_AUTOTEST='1'
    $titleStarted=[DateTime]::UtcNow
    $titleProcess=Start-Process -FilePath $titleExe -WorkingDirectory $titleProject -ArgumentList @('--project','resources/projects/tank_game.project.json') -WindowStyle Hidden -PassThru
    Write-Host "Title demo verification started, PID $($titleProcess.Id). Four real combat showcases plus a fresh game transition."
    if(!$titleProcess.WaitForExit(300000)) {
        Stop-Process -Id $titleProcess.Id
        throw 'Title demo exceeded 300 seconds.'
    }
    $titleResult=Join-Path $titleProject 'generated/title_demo/validation.json'
    if(!(Test-Path -LiteralPath $titleResult) -or (Get-Item -LiteralPath $titleResult).LastWriteTimeUtc -lt $titleStarted) {
        throw 'No fresh title demo result was written.'
    }
    $titleReport=Get-Content -LiteralPath $titleResult -Raw -Encoding UTF8 | ConvertFrom-Json
    if($titleProcess.ExitCode -ne 0 -or !$titleReport.completed) {throw "Title demo failed: $titleResult"}
    if(!$titleReport.sceneFadeCaptured -or $titleReport.maxBurstAge -gt 0.701) {throw 'Demo scene fade or death-effect lifetime regression.'}
    foreach($shot in @('stage_0','stage_1','stage_2','stage_3','new_game','scene_fade_out','scene_fade_in')) {
        $titleImage=Join-Path $titleProject "generated/title_demo/$shot.png"
        if(!(Test-Path -LiteralPath $titleImage) -or (Get-Item -LiteralPath $titleImage).LastWriteTimeUtc -lt $titleStarted) {
            throw "Missing fresh screenshot: $shot"
        }
    }
    Write-Host "PASS: 4 stages, $($titleReport.projectileEmissionSamples) projectile emission samples, $($titleReport.kills) real kills, $($titleReport.dashes) dashes, $($titleReport.rewards) rewards, $($titleReport.routes) routes, frozen fade, fresh playable expedition."
} finally {
    foreach($name in $titleVariables) {[Environment]::SetEnvironmentVariable($name,$titleSaved[$name],'Process')}
}
