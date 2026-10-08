param([string]$DxcPath = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$particleRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$particleOutput = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else {
    Join-Path $particleRepo 'generated/neon_particle_radiance/shader_contract'
}
if (!$DxcPath) {
    $particleSdkBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
    $particleDxcCandidates = @(Get-ChildItem -LiteralPath $particleSdkBin -Directory |
        Sort-Object { try { [version]$_.Name } catch { [version]'0.0' } } -Descending |
        ForEach-Object { Join-Path $_.FullName 'x64/dxc.exe' } |
        Where-Object { Test-Path -LiteralPath $_ })
    if ($particleDxcCandidates.Count) { $DxcPath = $particleDxcCandidates[0] }
}
if (!$DxcPath -or !(Test-Path -LiteralPath $DxcPath)) { throw 'DXC was not found. Pass -DxcPath with an installed x64 dxc.exe.' }
New-Item -ItemType Directory -Path $particleOutput -Force | Out-Null
$particleShaders = @(
    @{ name='ModelParticle.VS.hlsl'; profile='vs_6_0' },
    @{ name='ModelParticle.PS.hlsl'; profile='ps_6_0' },
    @{ name='ModelParticle.Scene.PS.hlsl'; profile='ps_6_0' },
    @{ name='ParticleUpdate.CS.hlsl'; profile='cs_6_0' },
    @{ name='ParticleEmit.CS.hlsl'; profile='cs_6_0' },
    @{ name='ParticleEmitBatch.CS.hlsl'; profile='cs_6_0' },
    @{ name='ParticleInitialize.CS.hlsl'; profile='cs_6_0' }
)
$particleReflection = @{}
$particleHashes = @{}
foreach ($particleShader in $particleShaders) {
    $particleSource = Join-Path $particleRepo ('project/resources/shaders/' + $particleShader.name)
    $particleBinary = Join-Path $particleOutput ($particleShader.name + '.dxil')
    & $DxcPath $particleSource -E main -T $particleShader.profile -Zpr -WX -Fo $particleBinary
    if ($LASTEXITCODE -ne 0) { throw ('Shader compilation failed: ' + $particleShader.name) }
    $particleDump = (& $DxcPath -dumpbin $particleBinary) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw ('Shader reflection failed: ' + $particleShader.name) }
    $particleReflection[$particleShader.name] = $particleDump
    $particleHashes[$particleShader.name] = (Get-FileHash -LiteralPath $particleSource -Algorithm SHA256).Hash
}
$particleChecks = [Collections.Generic.List[string]]::new()
function Assert-ParticleReflection([string]$Shader, [string]$Pattern, [string]$Label) {
    if ($particleReflection[$Shader] -notmatch $Pattern) { throw ('ABI check failed: ' + $Label) }
    $particleChecks.Add($Label)
}
foreach ($particlePixel in @('ModelParticle.PS.hlsl', 'ModelParticle.Scene.PS.hlsl')) {
    Assert-ParticleReflection $particlePixel 'float proceduralContour;\s*; Offset:\s*28\b' ($particlePixel + ': procedural selector keeps Material byte 28')
    Assert-ParticleReflection $particlePixel 'row_major float4x4 uvTransform;\s*; Offset:\s*32\b' ($particlePixel + ': UV transform remains row-major at byte 32')
    Assert-ParticleReflection $particlePixel 'float shininess;\s*; Offset:\s*96\b' ($particlePixel + ': generic material shininess remains at byte 96')
    Assert-ParticleReflection $particlePixel 'gMaterial\s+cbuffer\s+NA\s+NA\s+CB\d+\s+cb0\b' ($particlePixel + ': Material binding remains b0')
    Assert-ParticleReflection $particlePixel 'gTexture\s+texture\s+f32\s+2d\s+T\d+\s+t0\b' ($particlePixel + ': texture binding remains t0')
}
Assert-ParticleReflection 'ModelParticle.VS.hlsl' 'WorldInverseTranspose;\s*; Offset:\s*128\b' 'VS normal matrix remains at byte 128'
Assert-ParticleReflection 'ModelParticle.VS.hlsl' '\$Element;\s*; Offset:\s*0 Size:\s*208\b' 'VS instancing stride remains 208 bytes'
Assert-ParticleReflection 'ModelParticle.VS.hlsl' 'gTransformationMatrices\s+texture\s+struct\s+r/o\s+T\d+\s+t1\b' 'VS instancing binding remains t1'
foreach ($particleCompute in @('ParticleUpdate.CS.hlsl', 'ParticleEmit.CS.hlsl', 'ParticleEmitBatch.CS.hlsl', 'ParticleInitialize.CS.hlsl')) {
    Assert-ParticleReflection $particleCompute 'float padding1;\s*; Offset:\s*108\b' ($particleCompute + ': neon flag keeps existing byte 108 slot')
    Assert-ParticleReflection $particleCompute 'float padding2;\s*; Offset:\s*124\b' ($particleCompute + ': combat silhouette keeps existing byte 124 slot')
    Assert-ParticleReflection $particleCompute '\$Element;\s*; Offset:\s*0 Size:\s*128\b' ($particleCompute + ': particle stride remains 128 bytes')
}
Assert-ParticleReflection 'ParticleUpdate.CS.hlsl' '\$Element;\s*; Offset:\s*0 Size:\s*208\b' 'CS render-data stride matches VS 208 bytes'
Assert-ParticleReflection 'ParticleEmit.CS.hlsl' 'float2 padding;\s*; Offset:\s*120\b' 'GPU emitter request retains 128-byte layout and flag slot at byte 120'
$particleReport = [ordered]@{
    passed = $true
    scope = 'DXC compilation and reflected shader ABI; game pixels and CPU/GPU lifetime behavior require runtime validation'
    shaders = $particleShaders
    checks = @($particleChecks)
    sourceSha256 = $particleHashes
}
$particleReport | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $particleOutput 'shader_contract.json') -Encoding utf8
Write-Output ('PASS: ' + $particleShaders.Count + ' DXC shaders and ' + $particleChecks.Count + ' reflected ABI checks')
