param([string]$Executable = '', [string]$VisualStudioPath = '', [switch]$CpuOnly,
    [switch]$Quality, [switch]$RecordMotion, [switch]$MeasureGpu, [string]$PythonPath = 'python')
$ErrorActionPreference = 'Stop'
$windmillRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$windmillOutput = Join-Path $windmillRoot 'generated/neon_windmill'
New-Item -ItemType Directory -Force -Path $windmillOutput | Out-Null
if (!$VisualStudioPath) {
    $windmillWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $windmillWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$windmillBatch = @'
@echo off
call "%WINDMILL_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /I"..\..\project" /Fe:motion_tests.exe /Fo:motion_tests.obj "..\..\project\tools\neon_windmill_motion_tests.cpp"
if errorlevel 1 exit /b %errorlevel%
motion_tests.exe
'@
[IO.File]::WriteAllText((Join-Path $windmillOutput 'build-motion-tests.cmd'), $windmillBatch, [Text.Encoding]::ASCII)
$windmillPriorDev = $env:WINDMILL_DEV_CMD
try {
    $env:WINDMILL_DEV_CMD = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    Push-Location -LiteralPath $windmillOutput
    try { & $env:ComSpec /d /c build-motion-tests.cmd; if ($LASTEXITCODE) { throw 'Windmill motion tests failed.' } }
    finally { Pop-Location }
} finally { $env:WINDMILL_DEV_CMD = $windmillPriorDev }
if ($CpuOnly) { return }
if (!$Executable) { $Executable = Join-Path $windmillRoot 'generated/outputs/Development/CG2.exe' }
if (!(Test-Path -LiteralPath $Executable)) { throw 'Build Development/x64 before the GPU test.' }
if (!(Test-Path -LiteralPath (Join-Path $windmillOutput 'iphone_atlas.png'))) { throw 'Prepare the iPhone atlas before the GPU test.' }
$windmillPriorAuto = $env:CG2_WINDMILL_AUTOTEST
$windmillPriorLimit = $env:CG2_FRAME_LIMIT
try {
    $env:CG2_WINDMILL_AUTOTEST = '1'; $env:CG2_FRAME_LIMIT = '0'
    $windmillStarted = [DateTime]::UtcNow
    $windmillProcess = Start-Process -FilePath $Executable -WorkingDirectory (Join-Path $windmillRoot 'project') `
        -ArgumentList '--project resources/projects/neon_windmill.project.json' -WindowStyle Hidden -PassThru
    if (!$windmillProcess.WaitForExit(45000)) {
        $windmillProcess.Kill()
        throw 'The launched windmill verification process timed out after 45 seconds.'
    }
    if ($windmillProcess.ExitCode -ne 0) { throw "Windmill runtime returned $($windmillProcess.ExitCode)." }
    $windmillReportPath = Join-Path $windmillOutput 'runtime_report.json'
    if (!(Test-Path -LiteralPath $windmillReportPath) -or (Get-Item -LiteralPath $windmillReportPath).LastWriteTimeUtc -lt $windmillStarted) {
        throw 'No fresh windmill runtime report was produced. Use a Development build.'
    }
    $windmillReport = Get-Content -LiteralPath $windmillReportPath -Raw | ConvertFrom-Json
    if (!$windmillReport.completed -or $windmillReport.errors.Count -or $windmillReport.captures.Count -ne 6) { throw 'The six GPU captures did not complete.' }
    foreach ($windmillName in $windmillReport.captures) {
        foreach ($windmillExtension in @('png','json')) {
            $windmillCapturePath = Join-Path $windmillOutput "$windmillName.$windmillExtension"
            if (!(Test-Path -LiteralPath $windmillCapturePath) -or (Get-Item -LiteralPath $windmillCapturePath).LastWriteTimeUtc -lt $windmillStarted) {
                throw "Missing fresh capture: $windmillName.$windmillExtension"
            }
        }
        $windmillMetadata = Get-Content (Join-Path $windmillOutput "$windmillName.json") -Raw | ConvertFrom-Json
        if ($windmillMetadata.emojiQuads -ne 5 -or $windmillMetadata.billboard -or $windmillMetadata.drawCalls -ne 3 -or
            $windmillMetadata.vertices -ge $windmillReport.maxVertices -or $windmillMetadata.post.temporal -ne 0) { throw "Invalid plate/render state: $windmillName" }
    }
    $windmillOff = Get-Content (Join-Path $windmillOutput 'orbit_off.json') -Raw | ConvertFrom-Json
    $windmillOn = Get-Content (Join-Path $windmillOutput 'orbit_on.json') -Raw | ConvertFrom-Json
    if ($windmillOff.timeSeconds -ne $windmillOn.timeSeconds -or ($windmillOff.camera -join ',') -ne ($windmillOn.camera -join ',') -or
        $windmillOff.post.bloomMode -ne 0 -or $windmillOn.post.bloomMode -ne 2 -or $windmillOff.post.exposure -ne $windmillOn.post.exposure) {
        throw 'Bloom comparison did not use the same pose, camera and exposure.'
    }
    $windmillRecognized = Get-Content (Join-Path $windmillOutput 'recognized_on.json') -Raw | ConvertFrom-Json
    if ($windmillRecognized.recognition -ne 1) { throw 'Recognition state was not rendered.' }
    Write-Host 'PASS: six actual GPU captures, fixed 5 plates, same-state Bloom comparison, side view, lock and recognized glyphs.'
} finally {
    $env:CG2_WINDMILL_AUTOTEST = $windmillPriorAuto
    $env:CG2_FRAME_LIMIT = $windmillPriorLimit
}

if ($Quality) {
    $windmillQuality = Join-Path $windmillRoot 'generated/neon_windmill_quality'
    $windmillRuns = @('comparison')
    if ($RecordMotion) { $windmillRuns += 'motion' }
    if ($MeasureGpu) { $windmillRuns += @('perf0','perf1','perf2') }
    $windmillRunPrior = $env:CG2_WINDMILL_RUN
    $windmillOutputPrior = $env:CG2_WINDMILL_OUTPUT
    $windmillAutoPrior = $env:CG2_WINDMILL_AUTOTEST
    $windmillLimitPrior = $env:CG2_FRAME_LIMIT
    try {
        foreach ($windmillRun in $windmillRuns) {
            $env:CG2_WINDMILL_AUTOTEST = '1'; $env:CG2_FRAME_LIMIT = '0'
            $env:CG2_WINDMILL_RUN = $windmillRun
            $windmillRunDirectory = Join-Path $windmillQuality $windmillRun
            $env:CG2_WINDMILL_OUTPUT = $windmillRunDirectory
            $windmillStarted = [DateTime]::UtcNow
            $windmillProcess = Start-Process -FilePath $Executable -WorkingDirectory (Join-Path $windmillRoot 'project') `
                -ArgumentList '--project resources/projects/neon_windmill.project.json' -WindowStyle Hidden -PassThru
            for ($windmillWait = 0; $windmillWait -lt 8 -and !$windmillProcess.HasExited; ++$windmillWait) {
                $null = $windmillProcess.WaitForExit(30000)
            }
            if (!$windmillProcess.HasExited) { $windmillProcess.Kill(); throw "$windmillRun timed out." }
            if ($windmillProcess.ExitCode -ne 0) { throw "$windmillRun returned $($windmillProcess.ExitCode)." }
            $windmillRunReport = Join-Path $windmillRunDirectory 'runtime_report.json'
            if (!(Test-Path $windmillRunReport) -or (Get-Item $windmillRunReport).LastWriteTimeUtc -lt $windmillStarted) {
                throw "$windmillRun produced no fresh report."
            }
            $windmillRunData = Get-Content $windmillRunReport -Raw | ConvertFrom-Json
            if (!$windmillRunData.completed -or $windmillRunData.errors.Count) { throw "$windmillRun incomplete." }
            foreach ($windmillName in $windmillRunData.captures) {
                foreach ($windmillExtension in @('png','json')) {
                    $windmillPath = Join-Path $windmillRunDirectory "$windmillName.$windmillExtension"
                    if (!(Test-Path $windmillPath) -or (Get-Item $windmillPath).LastWriteTimeUtc -lt $windmillStarted) {
                        throw "$windmillRun missing fresh $windmillName.$windmillExtension"
                    }
                }
            }
            Write-Host "PASS: actual DX12 $windmillRun run."
        }
        & $PythonPath -X utf8 (Join-Path $PSScriptRoot 'validate_windmill_quality.py') $windmillQuality
        if ($LASTEXITCODE) { throw 'Windmill quality evidence validation failed.' }
    } finally {
        $env:CG2_WINDMILL_RUN = $windmillRunPrior; $env:CG2_WINDMILL_OUTPUT = $windmillOutputPrior
        $env:CG2_WINDMILL_AUTOTEST = $windmillAutoPrior; $env:CG2_FRAME_LIMIT = $windmillLimitPrior
    }
}
