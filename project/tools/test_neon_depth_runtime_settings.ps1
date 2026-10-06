# Draft, NOT COMPILED / NOT RUN. Actual parser/session CPU contracts only.
param([string]$VisualStudioPath='',[string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$depthUnitRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!(Test-Path -LiteralPath (Join-Path $depthUnitRepo 'project/CG2.sln'))) {throw 'Promote this draft into project/tools before use.'}
$depthUnitProject=Join-Path $depthUnitRepo 'project'
if (!$OutputDirectory) {$OutputDirectory=Join-Path $depthUnitRepo ('generated/neon-boss-depth/runtime-settings/'+[Guid]::NewGuid().ToString('N'))}
$depthUnitOutput=[IO.Path]::GetFullPath($OutputDirectory)
if (!$depthUnitOutput.StartsWith((Join-Path $depthUnitRepo 'generated')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw 'Depth unit evidence must stay in workspace generated/.'}
New-Item -ItemType Directory -Path $depthUnitOutput -Force | Out-Null
if (!$VisualStudioPath) {
    $depthUnitWhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath=(& $depthUnitWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$depthUnitDev=Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $depthUnitDev)) {throw 'Visual Studio C++ tools are required.'}
$depthUnitBatch=@'
@echo off
call "%DEPTH_UNIT_DEV%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MT /UNDEBUG /DCG2_DEVELOPER_TOOLS=1 /I"%DEPTH_UNIT_PROJECT%" /I"%DEPTH_UNIT_PROJECT%\externals" /I"%DEPTH_UNIT_PROJECT%\DirectX\engine\commom" /Fe:neon_depth_runtime_settings_tests.exe /Fo:.\ "%DEPTH_UNIT_PROJECT%\tools\neon_depth_runtime_settings_tests.cpp" user32.lib
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $depthUnitOutput 'build.cmd'),$depthUnitBatch,[Text.Encoding]::ASCII)
$depthUnitEnv=@{DEPTH_UNIT_DEV=$depthUnitDev;DEPTH_UNIT_PROJECT=$depthUnitProject;CG2_GAMEPLAY_SCENARIO=$null}
$depthUnitPrevious=@{}
try {
    foreach($key in $depthUnitEnv.Keys) {$depthUnitPrevious[$key]=[Environment]::GetEnvironmentVariable($key,'Process');[Environment]::SetEnvironmentVariable($key,$depthUnitEnv[$key],'Process')}
    Push-Location -LiteralPath $depthUnitOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if($LASTEXITCODE -ne 0) {throw "Depth parser/session build failed: $LASTEXITCODE"}
        $depthUnitExe=Join-Path $depthUnitOutput 'neon_depth_runtime_settings_tests.exe'
        & $depthUnitExe
        if($LASTEXITCODE -ne 0) {throw 'Depth strict parser/config bounds failed.'}
        foreach($mode in @('row_valid','row_gap','row_nan','row_oversize','draw_duplicate','title_notify','title_missing')) {
            $depthUnitDir=Join-Path $depthUnitOutput ('cpu_'+$mode+'_'+[Guid]::NewGuid().ToString('N'))
            $depthUnitManifest=@{scenario=$(if($mode -like 'title_*'){'neon_depth_lifecycle'}else{'neon_depth_cycles'})
                frames=120;enemyCount=0;captureFrames=@();depthFixture=@{probe=$(if($mode -like 'title_*'){'title_return'}else{'none'})};outputDirectory=$depthUnitDir}
            $depthUnitSettings=Join-Path $depthUnitOutput 'cpu-settings.json'
            [IO.File]::WriteAllText($depthUnitSettings,($depthUnitManifest|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
            [Environment]::SetEnvironmentVariable('CG2_GAMEPLAY_SCENARIO',$depthUnitSettings,'Process')
            & $depthUnitExe $mode
            if($LASTEXITCODE -ne 0) {throw "Actual CPU Depth Session contract failed: $mode"}
            $depthUnitReport=Get-Content -LiteralPath (Join-Path $depthUnitDir 'report.json') -Raw|ConvertFrom-Json
            $depthUnitSuccess=$mode -in @('row_valid','title_notify')
            if($depthUnitReport.completed -ne $depthUnitSuccess -or !$depthUnitReport.details.cpuDepthContract.injectedRows -or
                $depthUnitReport.details.cpuDepthContract.actualActorsProduced -or $depthUnitReport.details.cpuDepthContract.actualSceneInitializationProduced) {throw "CPU proof/completion flags wrong: $mode"}
            if($mode -eq 'title_notify' -and ($depthUnitReport.frameCount -ne 3 -or !$depthUnitReport.depth.earlyTitleCompletion)) {throw 'Bounded Title notify count contract changed.'}
        }
        Write-Host 'PASS: actual Depth settings/session parser contracts; synthetic CPU rows are explicitly marked.'
    } finally {Pop-Location}
} finally {foreach($key in $depthUnitPrevious.Keys) {[Environment]::SetEnvironmentVariable($key,$depthUnitPrevious[$key],'Process')}}
