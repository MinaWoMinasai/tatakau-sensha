param(
    [string]$AssimpVersion = "v5.4.3",
    [string]$WorkDir = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $projectRoot "..")).Path
$vendorRoot = Join-Path $projectRoot "externals\assimp"
$assimpLibDir = Join-Path $vendorRoot "lib"
$assimpRuntimeDir = Join-Path $vendorRoot "runtime"

if ([string]::IsNullOrWhiteSpace($WorkDir)) {
    $WorkDir = Join-Path $projectRoot ".deps\assimp"
}

$existingLib = Get-ChildItem -LiteralPath $assimpLibDir -Filter "assimp-vc*-mt.lib" -ErrorAction SilentlyContinue | Select-Object -First 1
$existingDll = Get-ChildItem -LiteralPath $assimpRuntimeDir -Filter "assimp-vc*-mt.dll" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $Force -and $existingLib -and $existingDll) {
    Write-Host "Assimp local binaries already exist."
    Write-Host "  $($existingLib.FullName)"
    Write-Host "  $($existingDll.FullName)"
    exit 0
}

# Visual Studio installations with the C++ tools, newest first (vswhere ships with every VS 2017+ installer),
# followed by the fixed VS 2022 locations used before.
function Get-VisualStudioRoots {
    $roots = @()
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -sort -property installationPath
        if ($LASTEXITCODE -eq 0 -and $found) {
            $roots += @($found)
        }
    }
    foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
        $roots += "C:\Program Files\Microsoft Visual Studio\2022\$edition"
    }
    return @($roots | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_) } | Select-Object -Unique)
}

$visualStudioRoots = Get-VisualStudioRoots

function Find-InVisualStudio([string]$RelativePath) {
    foreach ($root in $visualStudioRoots) {
        $candidate = Join-Path $root $RelativePath
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return ""
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakePath = if ($cmakeCommand) { $cmakeCommand.Source } else { "" }
if ([string]::IsNullOrWhiteSpace($cmakePath)) {
    $cmakePath = Find-InVisualStudio "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}
if ([string]::IsNullOrWhiteSpace($cmakePath)) {
    throw "CMake was not found. Install Visual Studio 2022 or 2026 with C++ CMake tools, or add cmake.exe to PATH."
}

$ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
$ninjaPath = if ($ninjaCommand) { $ninjaCommand.Source } else { "" }
if ([string]::IsNullOrWhiteSpace($ninjaPath)) {
    $ninjaPath = Find-InVisualStudio "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
}
if ([string]::IsNullOrWhiteSpace($ninjaPath)) {
    throw "Ninja was not found. Install Visual Studio 2022 or 2026 with C++ CMake tools, or add ninja.exe to PATH."
}

$vcvarsPath = Find-InVisualStudio "VC\Auxiliary\Build\vcvarsall.bat"
if ([string]::IsNullOrWhiteSpace($vcvarsPath)) {
    throw "Visual Studio vcvarsall.bat was not found. Install Visual Studio 2022 or 2026 with Desktop development with C++."
}
Write-Host "Using Visual Studio tools:"
Write-Host "  $vcvarsPath"
Write-Host "  $cmakePath"

function Invoke-VsCMake([string[]]$Arguments) {
    $quotedArgs = $Arguments | ForEach-Object { '"' + ($_ -replace '"', '\"') + '"' }
    $command = 'call "{0}" x64 && "{1}" {2}' -f $vcvarsPath, $cmakePath, ($quotedArgs -join ' ')
    & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "CMake command failed with exit code $LASTEXITCODE."
    }
}

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
$zipPath = Join-Path $WorkDir "assimp-$AssimpVersion.zip"
$sourceDir = Join-Path $WorkDir "source"
$buildDir = Join-Path $WorkDir "build"

if ($Force) {
    Remove-Item -LiteralPath $sourceDir -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $buildDir -Recurse -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path -LiteralPath $zipPath)) {
    $url = "https://github.com/assimp/assimp/archive/refs/tags/$AssimpVersion.zip"
    Write-Host "Downloading Assimp $AssimpVersion from $url"
    Invoke-WebRequest -Uri $url -OutFile $zipPath
}

if (-not (Test-Path -LiteralPath $sourceDir)) {
    $extractRoot = Join-Path $WorkDir "extract"
    Remove-Item -LiteralPath $extractRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
    Expand-Archive -LiteralPath $zipPath -DestinationPath $extractRoot -Force
    $expanded = Get-ChildItem -LiteralPath $extractRoot -Directory | Select-Object -First 1
    if (-not $expanded) {
        throw "Failed to extract Assimp source archive."
    }
    Move-Item -LiteralPath $expanded.FullName -Destination $sourceDir -Force
    Remove-Item -LiteralPath $extractRoot -Recurse -Force -ErrorAction SilentlyContinue
}

$configureArgs = @(
    "-S", $sourceDir,
    "-B", $buildDir,
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=$ninjaPath",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DBUILD_SHARED_LIBS=ON",
    "-DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF",
    "-DASSIMP_BUILD_GLTF_IMPORTER=ON",
    "-DASSIMP_BUILD_ZLIB=ON",
    "-DASSIMP_BUILD_DRACO=OFF",
    "-DASSIMP_NO_EXPORT=ON",
    "-DASSIMP_BUILD_TESTS=OFF",
    "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF",
    "-DASSIMP_BUILD_SAMPLES=OFF",
    "-DASSIMP_INSTALL=OFF",
    "-DASSIMP_IGNORE_GIT_HASH=ON",
    "-DUSE_STATIC_CRT=ON"
)

Write-Host "Configuring Assimp..."
Invoke-VsCMake $configureArgs

Write-Host "Building Assimp..."
Invoke-VsCMake @("--build", $buildDir, "--parallel")

$builtLib = Get-ChildItem -LiteralPath $buildDir -Recurse -Filter "assimp-vc*-mt.lib" | Select-Object -First 1
$builtDll = Get-ChildItem -LiteralPath $buildDir -Recurse -Filter "assimp-vc*-mt.dll" | Select-Object -First 1
if (-not $builtLib -or -not $builtDll) {
    throw "Assimp build finished, but assimp-vc*-mt.lib or assimp-vc*-mt.dll was not found under $buildDir."
}

New-Item -ItemType Directory -Force -Path $assimpLibDir, $assimpRuntimeDir | Out-Null
Copy-Item -LiteralPath $builtLib.FullName -Destination $assimpLibDir -Force
Copy-Item -LiteralPath $builtDll.FullName -Destination $assimpRuntimeDir -Force

$includeTarget = Join-Path $vendorRoot "include\assimp"
# The headers are committed. Overwriting them would only change line endings (and revision.h) in a fresh clone,
# so copy them only when they are missing, or when -Force asks for a refresh.
if ($Force -or -not (Test-Path -LiteralPath (Join-Path $includeTarget "version.h"))) {
    New-Item -ItemType Directory -Force -Path $includeTarget | Out-Null
    Copy-Item -Path (Join-Path $sourceDir "include\assimp\*") -Destination $includeTarget -Recurse -Force
    $generatedConfig = Get-ChildItem -LiteralPath $buildDir -Recurse -Filter "config.h" | Where-Object { $_.FullName -match "\\include\\assimp\\config\.h$" } | Select-Object -First 1
    $generatedRevision = Get-ChildItem -LiteralPath $buildDir -Recurse -Filter "revision.h" | Where-Object { $_.FullName -match "\\include\\assimp\\revision\.h$" } | Select-Object -First 1
    if ($generatedConfig) {
        Copy-Item -LiteralPath $generatedConfig.FullName -Destination $includeTarget -Force
    }
    if ($generatedRevision) {
        Copy-Item -LiteralPath $generatedRevision.FullName -Destination $includeTarget -Force
    }
} else {
    Write-Host "Assimp headers already exist; kept the committed copy."
}

Write-Host "Assimp local binaries are ready:"
Write-Host "  $(Join-Path $assimpLibDir $builtLib.Name)"
Write-Host "  $(Join-Path $assimpRuntimeDir $builtDll.Name)"
