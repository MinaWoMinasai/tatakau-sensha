param([string]$VisualStudioPath = '', [string]$PythonPath = 'python', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$maskRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$maskProject = Join-Path $maskRepo 'project'
$maskOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $maskRepo 'generated/neon_feature_mask_data_tests' }
$maskRuntime = Join-Path $maskProject 'resources/models/neon_hologram/line_masks'
$maskTexLib = Join-Path $maskRepo 'generated/outputs/Development/DirectXTex.lib'
if (!(Test-Path -LiteralPath $maskTexLib)) { throw 'Build Development x64 first to create DirectXTex.lib.' }
if (!$VisualStudioPath) {
    $maskWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $maskWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$maskDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $maskDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $maskOutput -Force | Out-Null
& $PythonPath (Join-Path $PSScriptRoot 'generate_neon_feature_masks.py') --verify
if ($LASTEXITCODE -ne 0) { throw "Mask deterministic regeneration / authoring-data validation failed: $LASTEXITCODE" }
$maskFixtures = @'
from pathlib import Path
import struct
import sys
from PIL import Image

source, output = map(Path, sys.argv[1:])
for name in ("face_candidate", "bangs_candidate"):
    data = (source / (name + ".png")).read_bytes()
    offset = 8
    while offset < len(data):
        count, kind = struct.unpack_from(">I4s", data, offset)
        assert kind not in (b"sRGB", b"gAMA", b"iCCP"), "Coverage PNG unexpectedly carries color conversion metadata"
        offset += count + 12
    with Image.open(source / (name + ".png")) as image:
        assert image.mode == "RGBA" and image.size == (1024, 1024)
        (output / (name + ".rgba")).write_bytes(image.tobytes())
for name, alpha in (("midvalue_a255", 255), ("midvalue_a64", 64)):
    image = Image.new("RGBA", (4, 4), (128, 64, 0, alpha))
    image.save(output / (name + ".png"))
    (output / (name + ".rgba")).write_bytes(image.tobytes())
image = Image.new("RGBA", (4, 4))
for y in range(4):
    for x in range(4):
        image.putpixel((x, y), (255 if (x + y) % 2 else 0, 128, 0, 255))
image.save(output / "linear_checker.png")
(output / "linear_checker.rgba").write_bytes(image.tobytes())
print("PASS: independent Pillow RGBA goldens, midvalue/alpha/checker fixtures; runtime PNGs carry no gamma/SRGB/ICC tags")
'@
[IO.File]::WriteAllText((Join-Path $maskOutput 'write_fixtures.py'), $maskFixtures, [Text.UTF8Encoding]::new($false))
& $PythonPath (Join-Path $maskOutput 'write_fixtures.py') $maskRuntime $maskOutput
if ($LASTEXITCODE -ne 0) { throw "Mask fixture preparation failed: $LASTEXITCODE" }
$maskBatch = @'
@echo off
call "%MASK_DATA_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MT /I"%MASK_DATA_PROJECT%\externals\DirectXTex" /Fe:neon_feature_mask_data_tests.exe /Fo:.\ "%MASK_DATA_PROJECT%\tools\neon_feature_mask_data_tests.cpp" /link "%MASK_DATA_TEX_LIB%" dxgi.lib dxguid.lib ole32.lib windowscodecs.lib
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $maskOutput 'build.cmd'), $maskBatch, [Text.Encoding]::ASCII)
$maskSettings = @{ MASK_DATA_DEV_CMD=$maskDevCmd; MASK_DATA_PROJECT=$maskProject; MASK_DATA_TEX_LIB=$maskTexLib }
$maskPrevious = @{}
try {
    foreach ($name in $maskSettings.Keys) {
        $maskPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $maskSettings[$name], 'Process')
    }
    Push-Location -LiteralPath $maskOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Mask data test build failed: $LASTEXITCODE" }
        & (Join-Path $maskOutput 'neon_feature_mask_data_tests.exe') $maskRuntime $maskOutput
        if ($LASTEXITCODE -ne 0) { throw "Mask data test failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $maskPrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $maskPrevious[$name], 'Process') }
}
