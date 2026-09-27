param(
    [string]$VisualStudioPath = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$tankExpRepoDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$tankExpOutputDir = if ($OutputDirectory) {
    [System.IO.Path]::GetFullPath($OutputDirectory)
} else {
    Join-Path $tankExpRepoDir 'generated\tank_enemy_combat_tests'
}
$tankExpCandidates = [System.Collections.Generic.List[string]]::new()
if ($VisualStudioPath) { $tankExpCandidates.Add($VisualStudioPath) }
if ($env:VSINSTALLDIR) { $tankExpCandidates.Add($env:VSINSTALLDIR) }
$tankExpVsWhereCommand = Get-Command vswhere.exe -ErrorAction SilentlyContinue
$tankExpVsWhere = if ($tankExpVsWhereCommand) { $tankExpVsWhereCommand.Source } else {
    Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
}
if (Test-Path -LiteralPath $tankExpVsWhere) {
    $tankExpFound = & $tankExpVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -eq 0 -and $tankExpFound) { $tankExpCandidates.Add(([string]$tankExpFound).Trim()) }
}
$tankExpDevCmd = $null
foreach ($tankExpCandidate in $tankExpCandidates) {
    $tankExpPossible = Join-Path $tankExpCandidate 'Common7\Tools\VsDevCmd.bat'
    if (Test-Path -LiteralPath $tankExpPossible) { $tankExpDevCmd = $tankExpPossible; break }
}
if (!$tankExpDevCmd) {
    throw 'Visual Studio C++ tools were not found. Pass -VisualStudioPath with an existing C++ installation directory.'
}

New-Item -ItemType Directory -Path $tankExpOutputDir -Force | Out-Null
# Exercise the production AI update and damage callbacks with graphics-free
# adapters, in addition to the pure timing/navigation contracts.
function Read-GuardMethod([string]$signature, [string]$relativePath='project/game/exp/ExpEnemy.cpp') {
    $guardSource = Get-Content -LiteralPath (Join-Path $tankExpRepoDir $relativePath) -Raw
    $guardStart = $guardSource.IndexOf($signature, [StringComparison]::Ordinal)
    if ($guardStart -lt 0) { throw "Missing guard production method: $signature" }
    $guardOpen = $guardSource.IndexOf('{', $guardStart)
    $guardDepth = 0
    for ($guardIndex = $guardOpen; $guardIndex -lt $guardSource.Length; ++$guardIndex) {
        if ($guardSource[$guardIndex] -eq '{') { ++$guardDepth }
        if ($guardSource[$guardIndex] -eq '}') {
            --$guardDepth
            if ($guardDepth -eq 0) { return $guardSource.Substring($guardStart, $guardIndex - $guardStart + 1) }
        }
    }
    throw "Unclosed guard method: $signature"
}
function Read-GuardDeclarations([string]$relativePath) {
    $guardSource = Get-Content -LiteralPath (Join-Path $tankExpRepoDir $relativePath) -Raw
    [regex]::Replace($guardSource, '(?m)^\s*#(?:include|pragma)[^\r\n]*\r?\n', '').Replace('private:', 'public:')
}
$guardEncoding = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $tankExpOutputDir 'guard_collider.inc'), (Read-GuardDeclarations 'project/game/collision/Collider.h'), $guardEncoding)
[IO.File]::WriteAllText((Join-Path $tankExpOutputDir 'guard_enemy_declarations.inc'), (Read-GuardDeclarations 'project/game/exp/ExpEnemy.h'), $guardEncoding)
$guardMethods = foreach ($signature in @('void ExpEnemy::Initialize(', 'void ExpEnemy::ApplyTypeParams(',
    'void ExpEnemy::ResetMagazine(', 'void ExpEnemy::RefreshCollisionMask(', 'Vector3 ExpEnemy::ClipCombatRay(',
    'void ExpEnemy::UpdateExpeditionCombat(', 'void ExpEnemy::OnCollision(', 'bool ExpEnemy::TakeDamageFromPlayer(',
    'bool ExpEnemy::TakeDirectionalDamage(', 'uint32_t ExpEnemy::ResolveShieldDamage(',
    'bool ExpEnemy::ApplyDamage(', 'void ExpEnemy::TriggerDamageFeedback(', 'void ExpEnemy::ConfigureSummonedUnit(',
    'bool ExpEnemy::TryReflectProjectile(', 'void ExpEnemy::ApplyKnockback(', 'bool ExpEnemy::MoveCombatActor(')) { Read-GuardMethod $signature }
[IO.File]::WriteAllText((Join-Path $tankExpOutputDir 'guard_enemy_methods.inc'), ($guardMethods -join "`n"), $guardEncoding)
$guardManagerMethods = foreach ($signature in @('void EnemyManager::DismissOrphanedSummons(', 'void EnemyManager::UpdateSummonedUnits(')) {
    Read-GuardMethod $signature 'project/game/exp/EnemyManager.cpp'
}
[IO.File]::WriteAllText((Join-Path $tankExpOutputDir 'guard_manager_methods.inc'), ($guardManagerMethods -join "`n"), $guardEncoding)
$tankExpBuildCmd = Join-Path $tankExpOutputDir 'build_tank_enemy_combat_tests.cmd'
# Pass Unicode source paths through process environment variables.
$tankExpBatch = @'
@echo off
call "%TANK_EXP_TEST_VS_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /Fe:tank_enemy_combat_tests.exe /Fo:.\ "%TANK_EXP_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
tank_enemy_combat_tests.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%TANK_GUARD_TEST_INCLUDE%" /Fe:tank_guard_integration_tests.exe /Fo:.\ "%TANK_GUARD_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
tank_guard_integration_tests.exe
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText($tankExpBuildCmd, $tankExpBatch, [System.Text.Encoding]::ASCII)
$tankExpEnvironment = @{
    TANK_EXP_TEST_VS_DEV_CMD = $tankExpDevCmd
    TANK_EXP_TEST_SOURCE = Join-Path $PSScriptRoot 'tank_enemy_combat_tests.cpp'
    TANK_GUARD_TEST_SOURCE = Join-Path $PSScriptRoot 'tank_guard_integration_tests.cpp'
    TANK_GUARD_TEST_INCLUDE = $tankExpOutputDir
}
$tankExpPreviousEnvironment = @{}
foreach ($tankExpName in $tankExpEnvironment.Keys) {
    $tankExpPreviousEnvironment[$tankExpName] = [Environment]::GetEnvironmentVariable($tankExpName, 'Process')
    [Environment]::SetEnvironmentVariable($tankExpName, $tankExpEnvironment[$tankExpName], 'Process')
}
try {
    Push-Location -LiteralPath $tankExpOutputDir
    try {
        & $env:ComSpec /d /c build_tank_enemy_combat_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Tank enemy combat build/tests failed with exit code $LASTEXITCODE." }
    } finally { Pop-Location }
} finally {
    foreach ($tankExpName in $tankExpEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($tankExpName, $tankExpPreviousEnvironment[$tankExpName], 'Process')
    }
}
Write-Host "Tank enemy combat test suite passed. Executable: $tankExpOutputDir\tank_enemy_combat_tests.exe"
