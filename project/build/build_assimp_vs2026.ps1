param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDir,
    [string]$BuildDir = "",
    [string]$VsRoot = "C:\Program Files\Microsoft Visual Studio\18\Community"
)

$ErrorActionPreference = "Stop"

$source = (Resolve-Path -LiteralPath $SourceDir).Path
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$vendorRoot = Join-Path $projectRoot "externals\assimp"
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $env:TEMP "cg2-assimp-vs2026"
}

$cmake = Join-Path $VsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninja = Join-Path $VsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$vcvars = Join-Path $VsRoot "VC\Auxiliary\Build\vcvarsall.bat"
foreach ($required in @($cmake, $ninja, $vcvars)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Visual Studio 2026 build tool was not found: $required"
    }
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$configureArgs = @(
    "-S", $source,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=$ninja",
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

function Invoke-VsCMake([string[]]$Arguments) {
    $quoted = $Arguments | ForEach-Object { '"' + ($_ -replace '"', '\"') + '"' }
    $command = 'call "{0}" x64 && "{1}" {2}' -f $vcvars, $cmake, ($quoted -join ' ')
    & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE"
    }
}

Invoke-VsCMake $configureArgs
Invoke-VsCMake @("--build", $BuildDir, "--parallel")

$includeTarget = Join-Path $vendorRoot "include\assimp"
$libTarget = Join-Path $vendorRoot "lib"
$runtimeTarget = Join-Path $vendorRoot "runtime"
New-Item -ItemType Directory -Force -Path $includeTarget, $libTarget, $runtimeTarget | Out-Null

Copy-Item -Path (Join-Path $source "include\assimp\*") -Destination $includeTarget -Recurse -Force
Copy-Item -LiteralPath (Join-Path $BuildDir "include\assimp\config.h") -Destination $includeTarget -Force
Copy-Item -LiteralPath (Join-Path $BuildDir "include\assimp\revision.h") -Destination $includeTarget -Force
Copy-Item -LiteralPath (Join-Path $BuildDir "lib\assimp-vc145-mt.lib") -Destination $libTarget -Force
Copy-Item -LiteralPath (Join-Path $BuildDir "bin\assimp-vc145-mt.dll") -Destination $runtimeTarget -Force
Copy-Item -LiteralPath (Join-Path $source "LICENSE") -Destination (Join-Path $vendorRoot "LICENSE.txt") -Force

Write-Host "Assimp was rebuilt and copied to $vendorRoot"
