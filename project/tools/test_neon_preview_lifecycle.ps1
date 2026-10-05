param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$previewRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$previewOutput = Join-Path $previewRepo 'generated/neon_preview_lifecycle_tests'
New-Item -ItemType Directory -Path $previewOutput -Force | Out-Null
if (!$VisualStudioPath) {
    $previewVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $previewVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$previewDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $previewDevCmd)) { throw 'Visual Studio C++ tools are required.' }
$previewSource = Get-Content -LiteralPath (Join-Path $previewRepo 'project/game/debug/NeonSkinnedPreview.cpp') -Raw
function Read-PreviewMethod([string]$signature) {
    $begin = $previewSource.IndexOf($signature, [StringComparison]::Ordinal)
    if ($begin -lt 0) { throw "Missing production Preview method: $signature" }
    $open = $previewSource.IndexOf('{', $begin)
    $depth = 0
    for ($i = $open; $i -lt $previewSource.Length; ++$i) {
        if ($previewSource[$i] -eq '{') { ++$depth }
        if ($previewSource[$i] -eq '}') {
            --$depth
            if ($depth -eq 0) { return $previewSource.Substring($begin, $i - $begin + 1) }
        }
    }
    throw "Unclosed production Preview method: $signature"
}
$previewDestructor = Read-PreviewMethod 'NeonSkinnedPreview::~NeonSkinnedPreview()'
$previewLoad = Read-PreviewMethod 'void NeonSkinnedPreview::Load()'
$previewCatch = $previewLoad.LastIndexOf('catch (const std::exception& error)', [StringComparison]::Ordinal)
if ($previewCatch -lt 0) { throw 'Preview Load must preserve a recoverable failure path.' }
$previewCatchOpen = $previewLoad.IndexOf('{', $previewCatch)
$previewCatchBody = $previewLoad.Substring($previewCatchOpen, $previewLoad.LastIndexOf('}') - $previewCatchOpen).Trim()
$previewFailure = "void NeonSkinnedPreview::FailLoadForTest(const std::exception& error) $previewCatchBody"
$previewRelease = Read-PreviewMethod 'void NeonSkinnedPreview::ReleaseResources()'
[IO.File]::WriteAllText((Join-Path $previewOutput 'preview_owner_methods.inc'), "$previewDestructor`n$previewRelease`n$previewFailure", [Text.UTF8Encoding]::new($false))
$previewBatch = @'
@echo off
call "%PREVIEW_OWNER_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%PREVIEW_OWNER_INCLUDE%" /Fe:neon_preview_lifecycle_tests.exe /Fo:.\ "%PREVIEW_OWNER_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
neon_preview_lifecycle_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $previewOutput 'build.cmd'), $previewBatch, [Text.Encoding]::ASCII)
$previewSettings = @{ PREVIEW_OWNER_DEV_CMD=$previewDevCmd; PREVIEW_OWNER_INCLUDE=$previewOutput; PREVIEW_OWNER_SOURCE=(Join-Path $PSScriptRoot 'neon_preview_lifecycle_tests.cpp') }
$previewPrevious = @{}
try {
    foreach ($name in $previewSettings.Keys) {
        $previewPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $previewSettings[$name], 'Process')
    }
    Push-Location -LiteralPath $previewOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Preview owner lifecycle build/test failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $previewPrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $previewPrevious[$name], 'Process') }
}
