param(
    [string]$VisualStudioPath = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$depthVisualRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$depthVisualProject = Join-Path $depthVisualRepo 'project'
$depthVisualOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else {
    Join-Path $depthVisualRepo 'generated/neon_depth_presentation_tests'
}
$depthVisualCandidates = [Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $depthVisualCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $depthVisualCandidates.Add($env:VSINSTALLDIR) }
$depthVisualWhereCommand = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$depthVisualWhere = if ($depthVisualWhereCommand) { $depthVisualWhereCommand.Source } else {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
}
if (Test-Path -LiteralPath $depthVisualWhere) {
    $depthVisualFound = & $depthVisualWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $depthVisualFound) { $depthVisualCandidates.Add(([string]$depthVisualFound).Trim()) }
}
$depthVisualDevCmd = $null
foreach ($depthVisualCandidate in $depthVisualCandidates) {
    $depthVisualPossible = Join-Path $depthVisualCandidate 'Common7/Tools/VsDevCmd.bat'
    if (Test-Path -LiteralPath $depthVisualPossible) { $depthVisualDevCmd = $depthVisualPossible; break }
}
if (!$depthVisualDevCmd) { throw 'Visual Studio C++ tools were not found. Pass -VisualStudioPath with an existing C++ installation.' }
$depthVisualAssimp = if (Test-Path -LiteralPath (Join-Path $depthVisualProject 'externals/assimp/lib/assimp-vc145-mt.lib')) {
    'assimp-vc145-mt'
} else { 'assimp-vc143-mt' }
$depthVisualSuites = @(
    @{ Name='neon_depth_placement_tests'; Math=$true; Skeleton=$false },
    @{ Name='neon_depth_presentation_tests'; Math=$true; Skeleton=$true },
    @{ Name='neon_depth_visual_state_tests'; Math=$false; Skeleton=$false },
    @{ Name='neon_depth_effect_geometry_tests'; Math=$true; Skeleton=$false }
)
$depthVisualBatch = @'
@echo off
call "%NEON_DEPTH_VISUAL_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MD /Gy /Gw /UNDEBUG /I"%NEON_DEPTH_VISUAL_PROJECT%" /I"%NEON_DEPTH_VISUAL_PROJECT%\DirectX\engine\calc" /I"%NEON_DEPTH_VISUAL_PROJECT%\DirectX\engine\easing" /I"%NEON_DEPTH_VISUAL_PROJECT%\DirectX\engine\struct" /I"%NEON_DEPTH_VISUAL_PROJECT%\DirectX\engine\3d" /external:I"%NEON_DEPTH_VISUAL_PROJECT%\externals\assimp\include" /external:W0 /Fe:tests.exe /Fo:.\ "%NEON_DEPTH_VISUAL_SOURCE%" %NEON_DEPTH_VISUAL_EXTRA% /link /OPT:REF /OPT:ICF %NEON_DEPTH_VISUAL_LINK%
if errorlevel 1 exit /b %errorlevel%
tests.exe
exit /b %errorlevel%
'@
$depthVisualSettings = @{
    NEON_DEPTH_VISUAL_DEV_CMD=$depthVisualDevCmd
    NEON_DEPTH_VISUAL_PROJECT=$depthVisualProject
    NEON_DEPTH_VISUAL_SOURCE=''
    NEON_DEPTH_VISUAL_EXTRA=''
    NEON_DEPTH_VISUAL_LINK=''
}
$depthVisualPrevious = @{}
try {
    foreach ($name in $depthVisualSettings.Keys) {
        $depthVisualPrevious[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
        [Environment]::SetEnvironmentVariable($name,$depthVisualSettings[$name],'Process')
    }
    foreach ($depthVisualSuite in $depthVisualSuites) {
        $depthVisualSource = Join-Path $PSScriptRoot ($depthVisualSuite.Name + '.cpp')
        if (!(Test-Path -LiteralPath $depthVisualSource)) { throw "Missing Depth suite source: $depthVisualSource" }
        $depthVisualSuiteOutput = Join-Path $depthVisualOutput $depthVisualSuite.Name
        New-Item -ItemType Directory -Path $depthVisualSuiteOutput -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $depthVisualSuiteOutput 'build.cmd'),$depthVisualBatch,[Text.Encoding]::ASCII)
        $depthVisualExtra = [Collections.Generic.List[string]]::new()
        if ($depthVisualSuite.Math) {
            $depthVisualExtra.Add('"' + (Join-Path $depthVisualProject 'DirectX/engine/calc/Calculation.cpp') + '"')
            $depthVisualExtra.Add('"' + (Join-Path $depthVisualProject 'DirectX/engine/easing/Easing.cpp') + '"')
        }
        $depthVisualLink = ''
        if ($depthVisualSuite.Skeleton) {
            $depthVisualExtra.Add('"' + (Join-Path $depthVisualProject 'DirectX/engine/3d/Skeleton.cpp') + '"')
            $depthVisualExtra.Add('"' + (Join-Path $depthVisualProject 'DirectX/engine/3d/Animation.cpp') + '"')
            $depthVisualLink = '/LIBPATH:"' + (Join-Path $depthVisualProject 'externals/assimp/lib') + '" ' + $depthVisualAssimp + '.lib'
            $depthVisualRuntime = Join-Path $depthVisualProject ('externals/assimp/runtime/' + $depthVisualAssimp + '.dll')
            if (Test-Path -LiteralPath $depthVisualRuntime) { Copy-Item -LiteralPath $depthVisualRuntime -Destination $depthVisualSuiteOutput -Force }
        }
        [Environment]::SetEnvironmentVariable('NEON_DEPTH_VISUAL_SOURCE',$depthVisualSource,'Process')
        [Environment]::SetEnvironmentVariable('NEON_DEPTH_VISUAL_EXTRA',($depthVisualExtra -join ' '),'Process')
        [Environment]::SetEnvironmentVariable('NEON_DEPTH_VISUAL_LINK',$depthVisualLink,'Process')
        Push-Location -LiteralPath $depthVisualSuiteOutput
        try {
            & $env:ComSpec /d /c build.cmd
            if ($LASTEXITCODE -ne 0) { throw "Depth presentation suite $($depthVisualSuite.Name) failed: $LASTEXITCODE" }
        } finally { Pop-Location }
    }
} finally {
    foreach ($name in $depthVisualPrevious.Keys) { [Environment]::SetEnvironmentVariable($name,$depthVisualPrevious[$name],'Process') }
}
Write-Host "Neon Depth placement, presentation, state and effect geometry suites passed: $depthVisualOutput"
