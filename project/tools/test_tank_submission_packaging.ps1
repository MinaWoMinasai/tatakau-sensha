$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'TankSubmissionPackage.ps1')
$tankPackageTestRoot = Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))) ('generated/submission_tests/' + [Guid]::NewGuid().ToString('N'))
$tankPackageFixture = Join-Path $tankPackageTestRoot 'source'

function Write-Fixture([string]$RelativePath, [string]$Text = 'fixture') {
    $target = Join-Path $tankPackageFixture $RelativePath
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    Set-Content -LiteralPath $target -Value $Text -Encoding UTF8
}
function Assert-True([bool]$Condition, [string]$Message) {
    if (!$Condition) { throw $Message }
}
function Assert-Throws([scriptblock]$Action, [string]$Message) {
    $failed = $false
    try { & $Action | Out-Null } catch { $failed = $true }
    Assert-True $failed $Message
}

foreach ($name in @('CG2.exe', 'dxcompiler.dll', 'dxil.dll', 'assimp-vc145-mt.dll', 'CG2.pdb', 'DirectXTex.lib')) {
    Write-Fixture "generated/outputs/Release/$name"
}
$fixtureHash = (Get-FileHash -LiteralPath (Join-Path $tankPackageFixture 'generated/outputs/Release/CG2.exe')).Hash
$safeBuildProfile = @{ configuration = 'Release'; developerTools = $false; sha256 = $fixtureHash } | ConvertTo-Json
Write-Fixture 'generated/outputs/Release/CG2.build.json' $safeBuildProfile
Write-Fixture 'docs/submission-README.md' '# たたかうせんしゃ'
Write-Fixture 'project/externals/assimp/LICENSE.txt' 'Assimp notice'
Write-Fixture 'project/externals/imgui/LICENSE.txt' 'ImGui notice'
foreach ($notice in Get-TankSubmissionNoticePaths) { Write-Fixture $notice "Notice fixture: $notice" }
Write-Fixture 'project/resources/projects/tank_game.project.json' '{"schemaVersion":1,"projectName":"たたかうせんしゃ","startupScene":"TITLE","gameModule":"builtin","resourceRoot":"resources"}'
Write-Fixture 'project/resources/projects/default.project.json' '{"original":"untouched"}'
foreach ($name in @('expedition_content', 'tankExpeditionBalance', 'expedition_map')) {
    Write-Fixture "project/resources/configs/$name.json" '{}'
}
Write-Fixture 'project/resources/shaders/common.hlsli'
Write-Fixture 'project/resources/shaders/main.hlsl'
Write-Fixture 'project/resources/models/tank.obj'
$sharedTankAssets = @('player3D.obj', 'player3D.mtl', 'ground.obj', 'ground.mtl', 'cube.obj', 'cube.mtl', 'white512x512.png')
foreach ($name in $sharedTankAssets) { Write-Fixture "project/resources/$name" 'Shared Tank runtime asset' }
$runtimeAudio = @('audio/tank_expedition/shot.wav', 'audio/tank_expedition/music_base.wav', 'bulletShoot.mp3')
foreach ($name in $runtimeAudio) { Write-Fixture "project/resources/$name" 'Shared runtime audio' }
Write-Fixture 'project/resources/audio/tank_expedition/preview.wav' 'Listening preview for source documentation'
$retiredAssets = @('Player_Mixamo.fbx', 'BGM_shining_star.mp3', 'models/player/testModel.glb',
    'models/player/animations/Idle.fbx', 'archives/source.zip', 'archives/source.7z', 'archives/source.rar') + @(Get-TankSubmissionRetiredLabResourcePaths)
foreach ($name in $retiredAssets) { Write-Fixture "project/resources/$name" 'Private local input' }
Write-Fixture 'project/resources/fonts/OFL.txt' 'Font notice'
foreach ($name in @('configs/expedition_user.json', 'configs/expedition_user.json.tmp', 'configs/expedition_user.json.backup',
        'generated/text/text_123.png', 'generated/cache.json', 'logs/local.log', 'Dumps/crash.dmp',
        'imgui.ini', 'configs/local.tmp', 'audio/generate.py')) {
    Write-Fixture "project/resources/$name" '{"tutorialCompleted":true,"authorHistory":"must stay private"}'
}
# Reject a tool-enabled Release and a stale build profile before creating output.
Write-Fixture 'generated/outputs/Release/CG2.build.json' (@{ configuration = 'Release'; developerTools = $true; sha256 = $fixtureHash } | ConvertTo-Json)
Assert-Throws { New-TankSubmissionPackage $tankPackageFixture (Join-Path $tankPackageTestRoot 'tools_enabled') } 'Developer-enabled Release was accepted.'
Write-Fixture 'generated/outputs/Release/CG2.build.json' (@{ configuration = 'Release'; developerTools = $false; sha256 = 'stale' } | ConvertTo-Json)
Assert-Throws { New-TankSubmissionPackage $tankPackageFixture (Join-Path $tankPackageTestRoot 'stale_profile') } 'Stale build profile was accepted.'
Write-Fixture 'generated/outputs/Release/CG2.build.json' $safeBuildProfile
$before = @{}
Get-TankSubmissionFiles $tankPackageFixture | ForEach-Object { $before[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName).Hash }
$cleanOutput = Join-Path $tankPackageTestRoot 'clean'
$result = New-TankSubmissionPackage $tankPackageFixture $cleanOutput
Assert-True ($result.TutorialState -eq 'fresh') 'Missing progress file must produce a fresh package.'
Assert-True (Test-Path -LiteralPath (Join-Path $cleanOutput 'resources/models/tank.obj')) 'Runtime model .obj was incorrectly excluded.'
foreach ($name in $sharedTankAssets) {
    Assert-True (Test-Path -LiteralPath (Join-Path $cleanOutput "resources/$name")) "Runtime asset was incorrectly excluded: $name"
}
foreach ($name in $runtimeAudio) {
    Assert-True (Test-Path -LiteralPath (Join-Path $cleanOutput "resources/$name")) "Runtime audio was incorrectly excluded: $name"
}
Assert-True (!(Test-Path -LiteralPath (Join-Path $cleanOutput 'resources/audio/tank_expedition/preview.wav'))) 'Listening preview was copied into the runtime package.'
Assert-True (Test-TankSubmissionExcludedPath 'resources/audio/tank_expedition/preview.wav') 'Package verification permits the listening preview.'
Assert-True (Test-TankSubmissionExcludedPath 'audio/tank_expedition/preview.wav') 'Source copy permits the listening preview.'
foreach ($name in $retiredAssets) {
    Assert-True (!(Test-Path -LiteralPath (Join-Path $cleanOutput "resources/$name"))) "Retired/local asset was copied: $name"
    Assert-True (Test-TankSubmissionExcludedPath "resources/$name") "Package verification permits a retired/local asset: $name"
}
Assert-True (Test-Path -LiteralPath (Join-Path $cleanOutput 'resources/shaders/common.hlsli')) 'Shader include was not copied.'
Assert-True (Test-Path -LiteralPath (Join-Path $cleanOutput 'resources/fonts/OFL.txt')) 'Asset notice was not preserved.'
foreach ($notice in @('COPYRIGHT.md', 'THIRD_PARTY_NOTICES.md', 'docs/third-party/DirectXTex-LICENSE.txt',
        'docs/third-party/nlohmann-json-LICENSE.MIT', 'docs/third-party/Abseil-LICENSE.txt', 'docs/third-party/stb-LICENSE.txt',
        'docs/third-party/Hedley-CC0-1.0.txt')) {
    Assert-True ((Get-FileHash -LiteralPath (Join-Path $cleanOutput $notice)).Hash -eq
        (Get-FileHash -LiteralPath (Join-Path $tankPackageFixture $notice)).Hash) "Notice not copied intact: $notice"
}
# An incomplete notice set must fail before a package directory is created.
$noticePath = Join-Path $tankPackageFixture 'COPYRIGHT.md'
$noticeText = [IO.File]::ReadAllText($noticePath)
try {
    Remove-Item -LiteralPath $noticePath
    $missingNoticeOutput = Join-Path $tankPackageTestRoot 'missing_notice'
    Assert-Throws { New-TankSubmissionPackage $tankPackageFixture $missingNoticeOutput } 'Missing copyright notice was accepted.'
    Assert-True (!(Test-Path -LiteralPath $missingNoticeOutput)) 'Incomplete package directory was created.'
} finally { [IO.File]::WriteAllText($noticePath, $noticeText) }
Assert-True (!(Test-Path -LiteralPath (Join-Path $cleanOutput 'resources/generated'))) 'Generated author cache was copied.'
Assert-True (!(Test-Path -LiteralPath (Join-Path $cleanOutput 'CG2.pdb'))) 'Debug symbols were copied.'
Assert-True (!(Test-Path -LiteralPath (Join-Path $cleanOutput 'DirectXTex.lib'))) 'Development library was copied.'
foreach ($path in $before.Keys) {
    Assert-True ((Get-FileHash -LiteralPath $path).Hash -eq $before[$path]) "Author source changed: $path"
}
$manifestText = Get-Content -LiteralPath (Join-Path $cleanOutput 'submission_manifest.json') -Raw
Assert-True (!$manifestText.Contains($tankPackageFixture)) 'Manifest leaked a local absolute path.'
Assert-Throws { New-TankSubmissionPackage $tankPackageFixture $cleanOutput } 'Existing output was overwritten.'
Assert-Throws { New-TankSubmissionPackage $tankPackageFixture (Join-Path $tankPackageFixture 'project/resources/nested') } 'Output inside resources was allowed.'
Assert-True (!(Test-Path -LiteralPath (Join-Path $tankPackageFixture 'project/resources/nested'))) 'Rejected output was created.'

$tamperOutput = Join-Path $tankPackageTestRoot 'tampered'
New-TankSubmissionPackage $tankPackageFixture $tamperOutput | Out-Null
Set-Content -LiteralPath (Join-Path $tamperOutput 'CG2.exe') -Value 'modified after packaging'
Assert-Throws { Assert-TankSubmissionPackage $tamperOutput } 'Modified executable was accepted.'
$historyOutput = Join-Path $tankPackageTestRoot 'with_history'
New-TankSubmissionPackage $tankPackageFixture $historyOutput | Out-Null
Set-Content -LiteralPath (Join-Path $historyOutput 'resources/configs/expedition_user.json') -Value '{"tutorialCompleted":true}'
Assert-Throws { Assert-TankSubmissionPackage $historyOutput } 'Tutorial history was accepted.'
$extraOutput = Join-Path $tankPackageTestRoot 'with_extra'
New-TankSubmissionPackage $tankPackageFixture $extraOutput | Out-Null
Set-Content -LiteralPath (Join-Path $extraOutput 'personal.json') -Value '{}'
Assert-Throws { Assert-TankSubmissionPackage $extraOutput } 'Unlisted file was accepted.'

# Only an explicitly prepared, flat PNG directory may bypass the generated exclusion.
$preparedDirectory = Join-Path $tankPackageTestRoot 'prepared_text'
[IO.Directory]::CreateDirectory($preparedDirectory) | Out-Null
$pngName = 'text_0123456789abcdef.png'
$pngBytes = [Convert]::FromBase64String('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==')
[IO.File]::WriteAllBytes((Join-Path $preparedDirectory $pngName), $pngBytes)
$preparedOutput = Join-Path $tankPackageTestRoot 'with_prepared_text'
$preparedResult = New-TankSubmissionPackage $tankPackageFixture $preparedOutput $preparedDirectory
Assert-True ($preparedResult.PreparedTextCount -eq 1 -and $preparedResult.TutorialState -eq 'fresh') 'Prepared text must not change tutorial state.'
$copiedPng = Join-Path $preparedOutput ('resources/generated/text/' + $pngName)
Assert-True ((Get-FileHash -LiteralPath $copiedPng).Hash -eq (Get-FileHash -LiteralPath (Join-Path $preparedDirectory $pngName)).Hash) 'Prepared PNG was modified.'
Assert-True (!(Test-Path -LiteralPath (Join-Path $preparedOutput 'resources/generated/text/text_123.png'))) 'Author-generated text leaked into prepared package.'
$preparedManifest = Get-Content -LiteralPath (Join-Path $preparedOutput 'submission_manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
Assert-True ($preparedManifest.preparedTextCache.kind -eq 'clean-staging-text-png' -and $preparedManifest.preparedTextCache.fileCount -eq 1) 'Prepared cache is not declared in manifest.'
Assert-Throws { New-TankSubmissionPackage $tankPackageFixture (Join-Path $tankPackageTestRoot 'author_cache_rejected') (Join-Path $tankPackageFixture 'project/resources/generated/text') } 'Direct author cache was accepted.'
Assert-Throws { New-TankSubmissionPackage $tankPackageFixture (Join-Path $preparedDirectory 'nested_output') $preparedDirectory } 'Output inside prepared cache was accepted.'
Set-Content -LiteralPath (Join-Path $preparedOutput 'resources/generated/other.json') -Value '{}'
Assert-Throws { Assert-TankSubmissionPackage $preparedOutput } 'A prepared text declaration allowed general generated history.'

$invalidCache = Join-Path $tankPackageTestRoot 'invalid_cache'
[IO.Directory]::CreateDirectory($invalidCache) | Out-Null
Set-Content -LiteralPath (Join-Path $invalidCache $pngName) -Value 'Not a PNG'
Assert-Throws { New-TankSubmissionPackage $tankPackageFixture (Join-Path $tankPackageTestRoot 'invalid_png_output') $invalidCache } 'Fake PNG was accepted.'
Assert-True (!(Test-Path -LiteralPath (Join-Path $tankPackageTestRoot 'invalid_png_output'))) 'Invalid cache created output before validation.'
[IO.File]::WriteAllBytes((Join-Path $invalidCache $pngName), $pngBytes)
Set-Content -LiteralPath (Join-Path $invalidCache 'expedition_user.json') -Value '{"tutorialCompleted":true}'
Assert-Throws { New-TankSubmissionPackage $tankPackageFixture (Join-Path $tankPackageTestRoot 'mixed_cache_output') $invalidCache } 'Non-PNG history in prepared directory was accepted.'

$undeclaredOutput = Join-Path $tankPackageTestRoot 'undeclared_text'
New-TankSubmissionPackage $tankPackageFixture $undeclaredOutput $preparedDirectory | Out-Null
$undeclaredManifestPath = Join-Path $undeclaredOutput 'submission_manifest.json'
$undeclaredManifest = Get-Content -LiteralPath $undeclaredManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$undeclaredManifest.PSObject.Properties.Remove('preparedTextCache')
$undeclaredManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $undeclaredManifestPath -Encoding UTF8
Assert-Throws { Assert-TankSubmissionPackage $undeclaredOutput } 'Undeclared generated text was accepted.'

# Missing dependency failure must occur before any output directory is created.
$missingFixture = Join-Path $tankPackageTestRoot 'missing_source'
[IO.Directory]::CreateDirectory((Join-Path $missingFixture 'generated/outputs/Release')) | Out-Null
$missingOutput = Join-Path $tankPackageTestRoot 'missing_output'
Assert-Throws { New-TankSubmissionPackage $missingFixture $missingOutput } 'Missing dependency was accepted.'
Assert-True (!(Test-Path -LiteralPath $missingOutput)) 'Dependency failure created an incomplete output.'
Write-Host 'PASS: source preservation, shared Tank runtime assets, retired Lab asset exclusion, fresh tutorial state, runtime dependencies, prepared text PNG allowlist/manifest, author/invalid/mixed cache rejection, history exclusion, protected output, tamper detection.'
Write-Host "Fixtures retained: $tankPackageTestRoot"
