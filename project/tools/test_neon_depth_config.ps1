#requires -Version 7.0
param(
    [string]$VisualStudioPath = '',
    [string]$OutputDirectory = '',
    [ValidateRange(5,600)][int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$depthConfigRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$depthConfigProject = Join-Path $depthConfigRepo 'project'
$depthConfigOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else {
    Join-Path $depthConfigRepo 'generated/neon_depth_config_tests'
}
$depthConfigSource = Join-Path $PSScriptRoot 'neon_depth_config_tests.cpp'
$depthConfigMath = Join-Path $depthConfigProject 'DirectX/engine/calc/Calculation.cpp'
$depthConfigEasing = Join-Path $depthConfigProject 'DirectX/engine/easing/Easing.cpp'
foreach ($depthConfigRequired in @($depthConfigSource,$depthConfigMath,$depthConfigEasing,
        (Join-Path $depthConfigProject 'game/enemy/actor/NeonDepthConfig.h'))) {
    if (!(Test-Path -LiteralPath $depthConfigRequired -PathType Leaf)) {
        throw "Missing Depth config test input: $depthConfigRequired"
    }
}
$depthConfigCandidates = [Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $depthConfigCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $depthConfigCandidates.Add($env:VSINSTALLDIR) }
$depthConfigWhereCommand = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$depthConfigWhere = if ($depthConfigWhereCommand) { $depthConfigWhereCommand.Source } else {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
}
if (Test-Path -LiteralPath $depthConfigWhere -PathType Leaf) {
    $depthConfigFound = & $depthConfigWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $depthConfigFound) { $depthConfigCandidates.Add(([string]$depthConfigFound).Trim()) }
}
$depthConfigDevCmd = $null
foreach ($depthConfigCandidate in $depthConfigCandidates) {
    $depthConfigPossible = Join-Path $depthConfigCandidate 'Common7/Tools/VsDevCmd.bat'
    if (Test-Path -LiteralPath $depthConfigPossible -PathType Leaf) { $depthConfigDevCmd = $depthConfigPossible; break }
}
if (!$depthConfigDevCmd) { throw 'Visual Studio C++ tools were not found. Pass -VisualStudioPath with an existing C++ installation.' }
New-Item -ItemType Directory -Path $depthConfigOutput -Force | Out-Null
$depthConfigBatch = @'
@echo off
call "%NEON_DEPTH_CONFIG_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MD /Gy /Gw /UNDEBUG /I"%NEON_DEPTH_CONFIG_PROJECT%" /I"%NEON_DEPTH_CONFIG_PROJECT%\DirectX\engine\calc" /I"%NEON_DEPTH_CONFIG_PROJECT%\DirectX\engine\easing" /I"%NEON_DEPTH_CONFIG_PROJECT%\DirectX\engine\struct" /I"%NEON_DEPTH_CONFIG_PROJECT%\DirectX\engine\3d" /external:I"%NEON_DEPTH_CONFIG_PROJECT%\externals" /external:W0 /Fe:neon_depth_config_tests.exe /Fo:.\ "%NEON_DEPTH_CONFIG_SOURCE%" "%NEON_DEPTH_CONFIG_MATH%" "%NEON_DEPTH_CONFIG_EASING%" /link /OPT:REF /OPT:ICF
if errorlevel 1 exit /b %errorlevel%
neon_depth_config_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $depthConfigOutput 'build_neon_depth_config_tests.cmd'),$depthConfigBatch,[Text.Encoding]::ASCII)
$depthConfigStart = [Diagnostics.ProcessStartInfo]::new()
$depthConfigStart.FileName = $env:ComSpec
$depthConfigStart.Arguments = '/d /s /c "build_neon_depth_config_tests.cmd"'
$depthConfigStart.WorkingDirectory = $depthConfigOutput
$depthConfigStart.UseShellExecute = $false
$depthConfigStart.CreateNoWindow = $true
$depthConfigStart.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
$depthConfigStart.RedirectStandardOutput = $true
$depthConfigStart.RedirectStandardError = $true
$depthConfigStart.Environment['NEON_DEPTH_CONFIG_DEV_CMD'] = $depthConfigDevCmd
$depthConfigStart.Environment['NEON_DEPTH_CONFIG_PROJECT'] = $depthConfigProject
$depthConfigStart.Environment['NEON_DEPTH_CONFIG_SOURCE'] = $depthConfigSource
$depthConfigStart.Environment['NEON_DEPTH_CONFIG_MATH'] = $depthConfigMath
$depthConfigStart.Environment['NEON_DEPTH_CONFIG_EASING'] = $depthConfigEasing
$depthConfigProcess = [Diagnostics.Process]::new()
$depthConfigProcess.StartInfo = $depthConfigStart
$depthConfigTimedOut = $false
$depthConfigStarted = $false
try {
    if (!$depthConfigProcess.Start()) { throw 'Failed to start the Depth config test process.' }
    $depthConfigStarted = $true
    $depthConfigStdout = $depthConfigProcess.StandardOutput.ReadToEndAsync()
    $depthConfigStderr = $depthConfigProcess.StandardError.ReadToEndAsync()
    $depthConfigTimer = [Diagnostics.Stopwatch]::StartNew()
    while (!$depthConfigProcess.WaitForExit(250)) {
        if ($depthConfigTimer.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
            $depthConfigTimedOut = $true
            $depthConfigProcess.Kill($true)
            if (!$depthConfigProcess.WaitForExit(5000)) { throw 'Depth config test process did not stop after its timeout.' }
            break
        }
    }
    if (![Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($depthConfigStdout,$depthConfigStderr),5000)) {
        throw 'Depth config test output did not close within the bounded wait.'
    }
    $depthConfigOutText = $depthConfigStdout.GetAwaiter().GetResult()
    $depthConfigErrText = $depthConfigStderr.GetAwaiter().GetResult()
    [IO.File]::WriteAllText((Join-Path $depthConfigOutput 'stdout.log'),$depthConfigOutText,[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $depthConfigOutput 'stderr.log'),$depthConfigErrText,[Text.UTF8Encoding]::new($false))
    if ($depthConfigOutText) { Write-Host $depthConfigOutText.TrimEnd() }
    if ($depthConfigErrText) { Write-Host $depthConfigErrText.TrimEnd() }
    if ($depthConfigTimedOut) { throw "Depth config tests exceeded ${TimeoutSeconds}s. Logs: $depthConfigOutput" }
    if ($depthConfigProcess.ExitCode -ne 0) {
        throw "Depth config tests failed: $($depthConfigProcess.ExitCode). Logs: $depthConfigOutput"
    }
} finally {
    if ($depthConfigStarted -and !$depthConfigProcess.HasExited) { $depthConfigProcess.Kill($true) }
    $depthConfigProcess.Dispose()
}
Write-Host "Neon Depth config schema, boundaries and transactional reload tests passed: $depthConfigOutput"
