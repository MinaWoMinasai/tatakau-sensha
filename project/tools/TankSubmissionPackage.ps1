# Shared, read-only package policy. Dot-source from the packaging and audit scripts.
Set-StrictMode -Version Latest

function Get-TankSubmissionNoticePaths {
    # Keep repository-relative paths so supplemental license links also work in packages.
    return @('COPYRIGHT.md', 'THIRD_PARTY_NOTICES.md',
        'docs/third-party/README.md', 'docs/third-party/DirectXTex-LICENSE.txt',
        'docs/third-party/nlohmann-json-LICENSE.MIT', 'docs/third-party/Konva-LICENSE.txt',
        'docs/third-party/Abseil-LICENSE.txt', 'docs/third-party/stb-LICENSE.txt',
        'docs/third-party/RapidJSON-LICENSE.txt', 'docs/third-party/zlib-LICENSE.txt',
        'docs/third-party/Hedley-CC0-1.0.txt')
}

function Test-TankSubmissionPreparedTextPath([string]$RelativePath) {
    return $RelativePath.Replace('\', '/') -cmatch '^resources/generated/text/text_[0-9a-f]{16}\.png$'
}

function Assert-TankSubmissionPng([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $signature = [byte[]]::new(8)
        if ($stream.Read($signature, 0, 8) -ne 8 -or [BitConverter]::ToString($signature) -ne '89-50-4E-47-0D-0A-1A-0A') {
            throw "Prepared text cache is not a PNG image: $Path"
        }
    } finally { $stream.Dispose() }
}

function Get-TankSubmissionPreparedTextFiles([string]$Directory, [string]$AuthorResources) {
    $prepared = [IO.Path]::GetFullPath($Directory).TrimEnd('\', '/')
    $author = [IO.Path]::GetFullPath($AuthorResources).TrimEnd('\', '/')
    if (($prepared + '\').StartsWith($author + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Author resource caches cannot be shipped. Prepare text images from a fresh isolated staging launch.'
    }
    $files = @(Get-TankSubmissionFiles $prepared)
    if (!$files.Count) { throw 'Prepared text cache contains no PNG files.' }
    foreach ($file in $files) {
        $relative = Get-TankSubmissionRelativePath $prepared $file.FullName
        if (!(Test-TankSubmissionPreparedTextPath ('resources/generated/text/' + $relative))) {
            throw "Only direct text_<16 hex digits>.png files are allowed in the prepared cache: $relative"
        }
        Assert-TankSubmissionPng $file.FullName
    }
    return $files
}

function Get-TankSubmissionRetiredLabResourcePaths {
    # Exact retired Lab assets only; shared Tank/Engine resources stay eligible.
    return @(
        'projects/graphics_lab.project.json',
        'projects/vfx_lab.project.json',
        'shaders/VfxLabBackground.VS.hlsl',
        'shaders/VfxLabBackground.PS.hlsl',
        'UnderwaterCausticsAtlas.png',
        'UnderwaterCausticsDeepBroadAtlas.png',
        'models/player/testModel_animated.glb',
        'models/human/walk.gltf',
        'models/human/walk.bin',
        'models/human/white.png',
        'TestBlock.obj',
        'TestBlock.mtl',
        'material_tests/TestBlock_albedo.png',
        'material_tests/TestBlock_normal.png',
        'material_tests/TestBlock_roughness.png',
        'material_tests/TestBlock_metallic.png',
        'material_tests/TestBlock_ao.png',
        'graphicsSand.obj',
        'graphicsSand.mtl',
        'graphicsBeach.obj',
        'graphicsBeach.mtl',
        'graphicsOcean.obj',
        'graphicsOcean.mtl',
        'graphicsWater.obj',
        'graphicsWater.mtl'
    )
}

function Test-TankSubmissionExcludedPath([string]$RelativePath, [bool]$AllowPreparedTextCache = $false) {
    $path = $RelativePath.Replace('\', '/')
    if ($AllowPreparedTextCache -and (Test-TankSubmissionPreparedTextPath $path)) { return $false }
    # Private source assets may still exist locally; never copy them back into a public package.
    # Accept both a resources-relative source path and a package-relative destination path.
    $resourcePath = $path -replace '^resources/', ''
    if ($resourcePath -in (Get-TankSubmissionRetiredLabResourcePaths)) { return $true }
    if ($resourcePath -match '^(Player_Mixamo\.fbx|BGM_shining_star\.mp3|models/player/testModel\.glb)$' -or
        $resourcePath -match '^models/player/animations/[^/]+\.fbx$') { return $true }
    return $path -match '(^|/)(generated|logs|Dumps|\.git|\.vs|\.deps|vcpkg_installed)(/|$)' -or
        $path -match '(^|/)expedition_user\.json($|\.)' -or
        $path -match '(^|/)(imgui\.ini|startup_trace\.json)$' -or
        $path -match '\.(log|tmp|pdb|ilk|py|ps1|obj\.ilk|dmp|bak|zip|7z|rar)$'
}

function Get-TankSubmissionFiles([string]$Root) {
    # Do not follow junctions/symlinks into other workspaces or personal folders.
    $rootItem = Get-Item -LiteralPath $Root -Force
    if ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point is not allowed: $Root" }
    $pending = [Collections.Generic.Queue[string]]::new()
    $pending.Enqueue($rootItem.FullName)
    while ($pending.Count -gt 0) {
        foreach ($item in Get-ChildItem -LiteralPath $pending.Dequeue() -Force) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point is not allowed: $($item.FullName)" }
            if ($item.PSIsContainer) { $pending.Enqueue($item.FullName) } else { $item }
        }
    }
}

function Get-TankSubmissionRelativePath([string]$Root, [string]$Path) {
    $prefix = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $full = [IO.Path]::GetFullPath($Path)
    if (!$full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "File is outside package/source root: $Path" }
    return $full.Substring($prefix.Length).Replace('\', '/')
}

function Assert-TankSubmissionBuild([string]$BuildDirectory) {
    $profile = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CG2.build.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($profile.configuration -ne 'Release' -or $profile.developerTools -isnot [bool] -or $profile.developerTools) {
        throw 'Submission requires Release with CG2DeveloperTools=false. Rebuild before packaging.'
    }
    if ($profile.sha256 -ne (Get-FileHash -LiteralPath (Join-Path $BuildDirectory 'CG2.exe') -Algorithm SHA256).Hash) {
        throw 'Build profile does not match CG2.exe. Rebuild before packaging.'
    }
}

function Assert-TankSubmissionPackage([string]$PackageDirectory) {
    $package = [IO.Path]::GetFullPath($PackageDirectory)
    Assert-TankSubmissionBuild $package
    $files = @(Get-TankSubmissionFiles $package)
    $manifest = Get-Content -LiteralPath (Join-Path $package 'submission_manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $hasPreparedText = $null -ne $manifest.PSObject.Properties['preparedTextCache']
    if ($hasPreparedText -and ($manifest.preparedTextCache.kind -ne 'clean-staging-text-png' -or $manifest.preparedTextCache.fileCount -lt 1)) {
        throw 'Invalid prepared text cache declaration.'
    }
    $preparedTextCount = 0
    $paths = @{}
    foreach ($file in $files) {
        $relative = Get-TankSubmissionRelativePath $package $file.FullName
        if (Test-TankSubmissionExcludedPath $relative $hasPreparedText) { throw "Local history/development file in submission: $relative" }
        if (Test-TankSubmissionPreparedTextPath $relative) { Assert-TankSubmissionPng $file.FullName; ++$preparedTextCount }
        $paths[$relative] = $file
    }
    if ($hasPreparedText -and $preparedTextCount -ne $manifest.preparedTextCache.fileCount) { throw 'Prepared text cache file count does not match manifest.' }
    foreach ($notice in Get-TankSubmissionNoticePaths) {
        if (!$paths.ContainsKey($notice)) { throw "Required copyright/license notice is missing: $notice" }
    }
    foreach ($required in @('CG2.exe', 'dxcompiler.dll', 'dxil.dll', 'README.md', 'licenses/assimp-LICENSE.txt', 'licenses/imgui-LICENSE.txt',
            'resources/projects/default.project.json', 'resources/projects/tank_game.project.json',
            'resources/configs/expedition_content.json', 'resources/configs/tankExpeditionBalance.json',
            'resources/configs/expedition_map.json', 'submission_manifest.json')) {
        if (!$paths.ContainsKey($required)) { throw "Required runtime file is missing: $required" }
    }
    if (!@($paths.Keys | Where-Object { $_ -match '^assimp-vc\d+-mt\.dll$' }).Count) { throw 'Assimp runtime DLL is missing.' }
    if (!@($paths.Keys | Where-Object { $_ -match '^resources/shaders/.+\.hlsl$' }).Count) { throw 'Runtime shader sources are missing.' }
    $project = Get-Content -LiteralPath (Join-Path $package 'resources/projects/default.project.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($project.startupScene -ne 'TITLE' -or $project.gameModule -ne 'builtin' -or $project.resourceRoot -ne 'resources') {
        throw 'Default launch must use the ordinary tank title and package-local resources.'
    }
    if ((Get-FileHash -LiteralPath (Join-Path $package 'resources/projects/default.project.json')).Hash -ne
        (Get-FileHash -LiteralPath (Join-Path $package 'resources/projects/tank_game.project.json')).Hash) {
        throw 'Default project does not match the tank game project.'
    }
    if ($manifest.schemaVersion -ne 1 -or $manifest.configuration -ne 'Release' -or $manifest.tutorialState -ne 'fresh') {
        throw 'Invalid submission manifest.'
    }
    $listed = @{}
    foreach ($entry in $manifest.files) {
        $relative = [string]$entry.path
        if ($relative -match '(^/|^[A-Za-z]:|\\|(^|/)\.\.(/|$))' -or $listed.ContainsKey($relative)) {
            throw "Invalid/duplicate manifest path: $relative"
        }
        $listed[$relative] = $true
        if (!$paths.ContainsKey($relative) -or (Get-FileHash -LiteralPath $paths[$relative].FullName -Algorithm SHA256).Hash -ne $entry.sha256) {
            throw "Missing or modified package file: $relative"
        }
    }
    if ($listed.Count -ne $paths.Count - 1) { throw 'Package has files absent from its manifest.' }
    return [pscustomobject]@{ PackageDirectory = $package; FileCount = $paths.Count; TutorialState = 'fresh'; PreparedTextCount = $preparedTextCount; Bytes = ($files | Measure-Object Length -Sum).Sum }
}

function New-TankSubmissionPackage([string]$SourceRoot, [string]$OutputDirectory, [string]$PreparedTextCacheDirectory) {
    $source = [IO.Path]::GetFullPath($SourceRoot).TrimEnd('\', '/')
    $destination = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\', '/')
    $resources = Join-Path $source 'project/resources'
    $build = Join-Path $source 'generated/outputs/Release'
    foreach ($protected in @($resources, $build, (Join-Path $source 'project/tools'), (Join-Path $source 'docs'))) {
        if (($destination + '\').StartsWith(([IO.Path]::GetFullPath($protected).TrimEnd('\') + '\'), [StringComparison]::OrdinalIgnoreCase)) {
            throw "Output must not be inside a source directory: $destination"
        }
    }
    if (Test-Path -LiteralPath $destination) { throw "Output already exists; choose a new directory: $destination" }
    Assert-TankSubmissionBuild $build
    $runtime = @('CG2.exe', 'CG2.build.json', 'dxcompiler.dll', 'dxil.dll')
    $assimp = @(Get-ChildItem -LiteralPath $build -File -Filter 'assimp-vc*-mt.dll')
    if (!$assimp.Count) { throw "Assimp runtime DLL is missing in $build" }
    $runtime += @($assimp.Name)
    $required = @($runtime | ForEach-Object { Join-Path $build $_ }) + @(
        (Join-Path $source 'docs/submission-README.md'),
        (Join-Path $source 'project/externals/assimp/LICENSE.txt'),
        (Join-Path $source 'project/externals/imgui/LICENSE.txt'),
        (Join-Path $resources 'projects/tank_game.project.json'),
        (Join-Path $resources 'configs/expedition_content.json'),
        (Join-Path $resources 'configs/tankExpeditionBalance.json'),
        (Join-Path $resources 'configs/expedition_map.json'))
    $required += @(Get-TankSubmissionNoticePaths | ForEach-Object { Join-Path $source $_ })
    foreach ($file in $required) { if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing source file: $file" } }
    $preparedTextFiles = @()
    if ($PreparedTextCacheDirectory) {
        $preparedTextFiles = @(Get-TankSubmissionPreparedTextFiles $PreparedTextCacheDirectory $resources)
        $preparedRoot = [IO.Path]::GetFullPath($PreparedTextCacheDirectory).TrimEnd('\', '/')
        if (($destination + '\').StartsWith($preparedRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Output must not be inside the prepared text cache.'
        }
    }
    # Author caches are always excluded; only explicitly prepared text PNGs are added below.
    $resourceFiles = @(Get-TankSubmissionFiles $resources | Where-Object {
        !(Test-TankSubmissionExcludedPath (Get-TankSubmissionRelativePath $resources $_.FullName))
    })
    if (!@($resourceFiles | Where-Object { $_.Extension -eq '.hlsl' }).Count) { throw 'Shader sources are missing.' }
    $sourceProject = Get-Content -LiteralPath (Join-Path $resources 'projects/tank_game.project.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($sourceProject.startupScene -ne 'TITLE' -or $sourceProject.gameModule -ne 'builtin' -or $sourceProject.resourceRoot -ne 'resources') {
        throw 'The source tank game project is not an ordinary title launch.'
    }
    New-Item -ItemType Directory -Path $destination -ErrorAction Stop | Out-Null
    foreach ($name in $runtime) { Copy-Item -LiteralPath (Join-Path $build $name) -Destination (Join-Path $destination $name) }
    foreach ($file in $resourceFiles) {
        $relative = Get-TankSubmissionRelativePath $resources $file.FullName
        $target = Join-Path $destination ('resources/' + $relative)
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $target
    }
    if ($preparedTextFiles.Count) {
        $textDestination = Join-Path $destination 'resources/generated/text'
        [IO.Directory]::CreateDirectory($textDestination) | Out-Null
        foreach ($file in $preparedTextFiles) { Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $textDestination $file.Name) }
    }
    # Only the package's default project is changed. The author's project is untouched.
    Copy-Item -LiteralPath (Join-Path $resources 'projects/tank_game.project.json') -Destination (Join-Path $destination 'resources/projects/default.project.json')
    Copy-Item -LiteralPath (Join-Path $source 'docs/submission-README.md') -Destination (Join-Path $destination 'README.md')
    [IO.Directory]::CreateDirectory((Join-Path $destination 'licenses')) | Out-Null
    Copy-Item -LiteralPath (Join-Path $source 'project/externals/assimp/LICENSE.txt') -Destination (Join-Path $destination 'licenses/assimp-LICENSE.txt')
    Copy-Item -LiteralPath (Join-Path $source 'project/externals/imgui/LICENSE.txt') -Destination (Join-Path $destination 'licenses/imgui-LICENSE.txt')
    foreach ($notice in Get-TankSubmissionNoticePaths) {
        $noticeTarget = Join-Path $destination $notice
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($noticeTarget)) | Out-Null
        Copy-Item -LiteralPath (Join-Path $source $notice) -Destination $noticeTarget
    }
    $entries = @(Get-TankSubmissionFiles $destination | Sort-Object FullName | ForEach-Object {
        [ordered]@{ path = Get-TankSubmissionRelativePath $destination $_.FullName; sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
    $manifest = [ordered]@{ schemaVersion = 1; configuration = 'Release'; tutorialState = 'fresh'; files = $entries }
    if ($preparedTextFiles.Count) { $manifest.preparedTextCache = [ordered]@{ kind = 'clean-staging-text-png'; fileCount = $preparedTextFiles.Count } }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destination 'submission_manifest.json') -Encoding UTF8
    return Assert-TankSubmissionPackage $destination
}
