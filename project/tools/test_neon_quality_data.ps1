param([string]$VisualStudioPath = '', [string]$PythonPath = 'python', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$qualityRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$qualityProject = Join-Path $qualityRepo 'project'
$qualityOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $qualityRepo 'generated/neon_quality_data_tests' }
$qualityRuntime = Join-Path $qualityProject 'resources/models/neon_hologram/line_masks/quality'
$qualityTexLib = Join-Path $qualityRepo 'generated/outputs/Development/DirectXTex.lib'
if (!(Test-Path -LiteralPath $qualityTexLib)) { throw 'Build Development x64 first to create DirectXTex.lib.' }
if (!$VisualStudioPath) {
    $qualityWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $qualityWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$qualityDevCmd = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
if (!(Test-Path -LiteralPath $qualityDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
New-Item -ItemType Directory -Path $qualityOutput -Force | Out-Null
& $PythonPath (Join-Path $PSScriptRoot 'generate_neon_quality_masks.py') --verify
if ($LASTEXITCODE -ne 0) { throw "Quality regeneration failed: $LASTEXITCODE" }
& $PythonPath (Join-Path $PSScriptRoot 'test_neon_quality_masks.py')
if ($LASTEXITCODE -ne 0) { throw "Quality numerical data tests failed: $LASTEXITCODE" }
$qualityFixtures = @'
from pathlib import Path
import json
import sys
import numpy as np
from PIL import Image
source, output, tools = map(Path, sys.argv[1:])
sys.path.insert(0,str(tools))
from generate_neon_quality_masks import make_mask, png_chunks
config=json.loads((source/'authoring.json').read_text(encoding='utf-8'))
for version in config['versions']:
    for mask in version['masks']:
        stem=mask['id']+'_'+version['id']
        _,_,distance=make_mask(mask,config)
        for kind in ['coverage','sdf']:
            name=stem+'_'+kind
            assert not any(c in (b'sRGB',b'gAMA',b'iCCP') for c in png_chunks((source/(name+'.png')).read_bytes()))
            with Image.open(source/(name+'.png')) as im:
                assert im.mode=='RGBA' and im.size==(1024,1024)
                (output/(name+'.rgba')).write_bytes(im.tobytes())
            if kind=='sdf':
                (output/(name+'.distance.f32')).write_bytes(np.asarray(distance,dtype='<f4').tobytes())
for name in ['linear_midvalues','linear_checker']:
    im=Image.new('RGBA',(4,4),(128,64,192,255))
    if name=='linear_checker':
        for y in range(4):
            for x in range(4):im.putpixel((x,y),(255 if (x+y)%2 else 0,64,192,255))
    im.save(output/(name+'.png'))
    (output/(name+'.rgba')).write_bytes(im.tobytes())
print('PASS: independent Pillow all-channel byte goldens and analytic source distance references')
'@
[IO.File]::WriteAllText((Join-Path $qualityOutput 'write_fixtures.py'), $qualityFixtures, [Text.UTF8Encoding]::new($false))
& $PythonPath (Join-Path $qualityOutput 'write_fixtures.py') $qualityRuntime $qualityOutput $PSScriptRoot
if ($LASTEXITCODE -ne 0) { throw "Quality fixture preparation failed: $LASTEXITCODE" }
$qualityBatch = @'
@echo off
call "%QUALITY_DATA_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /MT /I"%QUALITY_DATA_PROJECT%\externals\DirectXTex" /Fe:neon_quality_mask_data_tests.exe /Fo:.\ "%QUALITY_DATA_PROJECT%\tools\neon_quality_mask_data_tests.cpp" /link "%QUALITY_DATA_TEX_LIB%" dxgi.lib dxguid.lib ole32.lib windowscodecs.lib
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $qualityOutput 'build.cmd'), $qualityBatch, [Text.Encoding]::ASCII)
$qualitySettings = @{ QUALITY_DATA_DEV_CMD=$qualityDevCmd; QUALITY_DATA_PROJECT=$qualityProject; QUALITY_DATA_TEX_LIB=$qualityTexLib }
$qualityPrevious = @{}
try {
    foreach ($name in $qualitySettings.Keys) {
        $qualityPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $qualitySettings[$name], 'Process')
    }
    Push-Location -LiteralPath $qualityOutput
    try {
        & $env:ComSpec /d /c build.cmd
        if ($LASTEXITCODE -ne 0) { throw "Quality data test build failed: $LASTEXITCODE" }
        & (Join-Path $qualityOutput 'neon_quality_mask_data_tests.exe') $qualityRuntime $qualityOutput
        if ($LASTEXITCODE -ne 0) { throw "Quality data test failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $qualityPrevious.Keys) { [Environment]::SetEnvironmentVariable($name, $qualityPrevious[$name], 'Process') }
}
