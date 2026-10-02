param(
    [string]$VisualStudioPath = '',
    [switch]$AddressSanitizer,
    [switch]$ConfigOnly,
    [string]$TestCase = ''
)
$ErrorActionPreference = 'Stop'
$configRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$configOutput = Join-Path $configRoot ('generated/player_class_config_tests' + $(if ($AddressSanitizer) { '_asan' } else { '' }))
New-Item -ItemType Directory -Path $configOutput -Force | Out-Null

# Compile the complete production Catalog translation unit and real config types.
# Only Player runtime wrappers and Editor blocks need the existing CPU adapters.
function Read-ConfigSource([string]$path) {
    Get-Content -LiteralPath (Join-Path $configRoot $path) -Raw -Encoding UTF8
}
function Read-ConfigBlock([string]$source, [string]$signature) {
    $start = $source.IndexOf($signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing production anchor: $signature" }
    $opening = $source.IndexOf('{', $start)
    $depth = 0
    for ($index = $opening; $index -lt $source.Length; ++$index) {
        if ($source[$index] -eq '{') { ++$depth }
        if ($source[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) { return $source.Substring($start, $index - $start + 1) }
        }
    }
    throw "Unclosed production block: $signature"
}
$configCpp = Read-ConfigSource 'project/game/player/actor/Player.cpp'
$configMethods = foreach ($signature in @(
    'bool Player::LoadPlayerClassConfigs(', 'bool Player::ReloadPlayerClassConfigs(',
    'Player::PlayerClassConfig Player::CreateDefaultClassConfig(', 'void Player::SavePlayerClassConfigs(',
    'const Player::PlayerClassConfig* Player::GetClassConfig(ClassType',
    'const Player::PlayerClassConfig* Player::GetClassConfig(const std::string&',
    'const Player::PlayerClassConfig* Player::GetCurrentClassConfig(',
    'Player::PlayerClassConfig* Player::GetMutableClassConfig('
)) {
    Read-ConfigBlock $configCpp $signature
}
$configEditor = Read-ConfigBlock $configCpp 'void Player::DrawPlayerClassEditor()'
$configBaselines = (Read-ConfigBlock $configEditor 'auto refreshEditorBaselines = [&]()') + ';'
$configUniqueId = (Read-ConfigBlock $configEditor 'auto makeUniqueId = [this]') + ';'
$configCreate = Read-ConfigBlock $configEditor 'if (ImGui::Button("新規作成"))'
$configClone = Read-ConfigBlock $configEditor 'if (ImGui::Button("複製"))'
$configDelete = Read-ConfigBlock $configEditor 'if (!isLegacyId && ImGui::Button("削除"))'
$configReload = Read-ConfigBlock $configEditor 'if (ImGui::Button("JSON再読み込み"))'
$configApply = Read-ConfigBlock $configEditor 'if (config && config->id == currentClassId_ && (rebuildBarrels || relayoutBarrels))'
$configUtf8 = [Text.UTF8Encoding]::new($false)
foreach ($entry in @{
    'player_class_config_methods.inc' = ($configMethods -join "`n")
    'player_class_config_baselines.inc' = $configBaselines
    'player_class_config_unique_id.inc' = $configUniqueId
    'player_class_config_create.inc' = $configCreate
    'player_class_config_clone.inc' = $configClone
    'player_class_config_delete.inc' = $configDelete
    'player_class_config_reload.inc' = $configReload
    'player_class_config_apply.inc' = $configApply
}.GetEnumerator()) {
    [IO.File]::WriteAllText((Join-Path $configOutput $entry.Key), $entry.Value, $configUtf8)
}
if (!$VisualStudioPath) {
    $configVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioPath = (& $configVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
}
if (!$VisualStudioPath) { throw 'Existing Visual Studio C++ installation not found.' }
if ($ConfigOnly -and $TestCase) { throw 'Choose ConfigOnly or TestCase, not both.' }
if ($TestCase -and $TestCase -notmatch '^[a-z0-9_]+$') { throw 'Invalid test case name.' }
$configEnvironment = @{
    CLASS_TEST_VS = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    CLASS_TEST_SOURCE = Join-Path $PSScriptRoot 'player_class_config_tests.cpp'
    CLASS_TEST_CATALOG = Join-Path $configRoot 'project/game/player/PlayerClassCatalog.cpp'
    CLASS_TEST_INCLUDE = $configOutput
    CLASS_TEST_PROJECT = Join-Path $configRoot 'project'
    CLASS_TEST_STRUCT = Join-Path $configRoot 'project/DirectX/engine/struct'
    CLASS_TEST_JSON = Join-Path $configRoot 'project/externals'
    CLASS_TEST_FLAGS = if ($AddressSanitizer) { '/Od /Zi /MD /fsanitize=address' } else { '/O2' }
    CLASS_TEST_ARGS = if ($ConfigOnly) { '--config-only' } elseif ($TestCase) { "--case $TestCase" } else { '' }
}
$configPrevious = @{}
foreach ($name in $configEnvironment.Keys) {
    $configPrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $configEnvironment[$name], 'Process')
}
$configBatch = @'
@echo off
call "%CLASS_TEST_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /UNDEBUG %CLASS_TEST_FLAGS% /I"%CLASS_TEST_INCLUDE%" /I"%CLASS_TEST_JSON%" /I"%CLASS_TEST_PROJECT%" /I"%CLASS_TEST_STRUCT%" /Fe:player_class_config_tests.exe /Fo:.\ "%CLASS_TEST_SOURCE%" "%CLASS_TEST_CATALOG%"
if errorlevel 1 exit /b %errorlevel%
player_class_config_tests.exe %CLASS_TEST_ARGS%
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $configOutput 'build_player_class_config_tests.cmd'), $configBatch, [Text.Encoding]::ASCII)
try {
    Push-Location -LiteralPath $configOutput
    try {
        & $env:ComSpec /d /c build_player_class_config_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Player class config regression failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $configPrevious.Keys) {
        [Environment]::SetEnvironmentVariable($name, $configPrevious[$name], 'Process')
    }
}
