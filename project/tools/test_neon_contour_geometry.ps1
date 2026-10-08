param([string]$VisualStudioPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$contourRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (!$OutputDirectory) { $OutputDirectory = Join-Path $contourRoot 'generated/neon_contour_geometry_tests' }
$contourOutput = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($contourOutput) | Out-Null
$contourHeader = Get-Content -LiteralPath (Join-Path $contourRoot 'project/DirectX/engine/commom/NeonGridRenderer.h') -Raw
$contourSource = Get-Content -LiteralPath (Join-Path $contourRoot 'project/DirectX/engine/commom/NeonGridRenderer.cpp') -Raw
# Retain the production CPU geometry verbatim; only GPU resource / draw entry
# points are omitted, because this suite must run without a Device or game.
foreach ($method in @('Initialize', 'DrawAll', 'DrawRange', 'DrawRangeSolid')) {
    $match = [regex]::Match($contourSource, "(?m)^void NeonGridRenderer::$method\(")
    if (!$match.Success) { throw "Missing production method: $method" }
    $open = $contourSource.IndexOf('{', $match.Index)
    $depth = 1
    $end = $open + 1
    while ($depth -gt 0 -and $end -lt $contourSource.Length) {
        if ($contourSource[$end] -eq '{') { $depth++ }
        if ($contourSource[$end] -eq '}') { $depth-- }
        $end++
    }
    if ($depth -ne 0) { throw "Unbalanced production method: $method" }
    $contourSource = $contourSource.Remove($match.Index, $end - $match.Index)
}
$contourProduction = [regex]::Replace(($contourHeader + "`n" + $contourSource), '(?m)^\s*#(?:include|pragma)[^\r\n]*\r?\n', '')
[IO.File]::WriteAllText((Join-Path $contourOutput 'neon_contour_production.inc'), $contourProduction, [Text.UTF8Encoding]::new($false))
if (!$VisualStudioPath) {
    $contourVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $contourVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
}
$contourEnvironment = @{
    CONTOUR_TEST_DEV_CMD = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    CONTOUR_TEST_SOURCE = Join-Path $PSScriptRoot 'neon_contour_geometry_tests.cpp'
    CONTOUR_TEST_INCLUDE = $contourOutput
}
$contourBatch = @'
@echo off
call "%CONTOUR_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%CONTOUR_TEST_INCLUDE%" /Fe:neon_contour_geometry_tests.exe /Fo:.\ "%CONTOUR_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
neon_contour_geometry_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $contourOutput 'build_cpu_tests.cmd'), $contourBatch, [Text.Encoding]::ASCII)
$contourPrevious = @{}
try {
    foreach ($key in $contourEnvironment.Keys) {
        $contourPrevious[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $contourEnvironment[$key], 'Process')
    }
    Push-Location -LiteralPath $contourOutput
    try {
        & $env:ComSpec /d /c build_cpu_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Contour geometry CPU tests failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($key in $contourPrevious.Keys) { [Environment]::SetEnvironmentVariable($key, $contourPrevious[$key], 'Process') }
}
