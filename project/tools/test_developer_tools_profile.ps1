param([string]$ReleaseTrace = '', [string]$DevelopmentTrace = '')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = (& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1)
$msbuild = Join-Path $vs 'MSBuild/Current/Bin/MSBuild.exe'
$project = Join-Path $repo 'project/CG2_testPro.vcxproj'
foreach ($case in @(
    @{ config='Release'; override=''; enabled=$false; ui='' },
    @{ config='Development'; override=''; enabled=$true; ui='USE_IMGUI;' },
    @{ config='Debug'; override=''; enabled=$true; ui='USE_IMGUI;' },
    @{ config='Release'; override='true'; enabled=$true; ui='USE_RUNTIME_PROFILER;' },
    @{ config='Development'; override='false'; enabled=$false; ui='' }
)) {
    $arguments = @($project, "/p:Configuration=$($case.config)", '/p:Platform=x64', '-getProperty:CG2DeveloperTools,CG2DeveloperToolsValue,CG2DeveloperUiDefines')
    if ($case.override) { $arguments += "/p:CG2DeveloperTools=$($case.override)" }
    $output = & $msbuild @arguments
    if ($LASTEXITCODE -ne 0) { throw 'MSBuild profile evaluation failed.' }
    $values = ($output | Out-String | ConvertFrom-Json).Properties
    if ($values.CG2DeveloperTools -ne $case.enabled.ToString().ToLowerInvariant() -or
        [int]$values.CG2DeveloperToolsValue -ne [int]$case.enabled -or $values.CG2DeveloperUiDefines -cne $case.ui) {
        throw "Wrong developer profile: $($case.config)/$($case.override)"
    }
}
foreach ($config in @('Release', 'Development')) {
    $build = Join-Path $repo "generated/outputs/$config"
    $profile = Get-Content -LiteralPath (Join-Path $build 'CG2.build.json') -Raw | ConvertFrom-Json
    $enabled = $config -eq 'Development'
    if ($profile.configuration -ne $config -or $profile.developerTools -isnot [bool] -or $profile.developerTools -ne $enabled -or
        $profile.sha256 -ne (Get-FileHash -LiteralPath (Join-Path $build 'CG2.exe')).Hash) {
        throw "Build $config with its default developer-tools setting before testing."
    }
    $path = if ($enabled) { $DevelopmentTrace } else { $ReleaseTrace }
    if ($path) {
        $trace = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        foreach ($key in @('build.developer_tools','ui.imgui_initialized','ui.runtime_profiler_allowed')) {
            $property = $trace.counters.PSObject.Properties[$key]
            if ($null -eq $property -or $property.Value -ne [int]$enabled) { throw "$config trace disagrees: $key" }
        }
    }
}
Write-Host 'PASS: five build-profile combinations, executable/profile SHA256, supplied runtime UI counters.'
