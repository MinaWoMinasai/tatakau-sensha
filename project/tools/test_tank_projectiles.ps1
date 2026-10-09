param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$projectileRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$projectileOutput = Join-Path $projectileRoot 'generated/tank_projectile_tests'
New-Item -ItemType Directory -Path $projectileOutput -Force | Out-Null
function Read-ProjectileSource([string]$path) {
    Get-Content -LiteralPath (Join-Path $projectileRoot $path) -Raw
}
function Remove-ProjectileIncludes([string]$source) {
    [regex]::Replace($source, '(?m)^\s*#(?:include|pragma)[^\r\n]*\r?\n', '')
}
function Read-ProjectileMethod([string]$path, [string]$signature) {
    $source = Read-ProjectileSource $path
    if ($path -eq 'project/game/player/actor/Player.cpp') {
        # 実際の担当クラスの処理を既存CPUアダプターの自機状態へ接続する。
        $source = ((Read-ProjectileSource 'project/game/player/combat/PlayerWeapons.cpp') + "`n" +
            (Read-ProjectileSource 'project/game/player/progression/PlayerProgression.cpp') + "`n" + $source).
            Replace('PlayerWeapons::', 'Player::').Replace('PlayerProgression::', 'Player::').Replace('player_.', '').Replace('ui_->', '')
    }
    $start = $source.IndexOf($signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing production method: $signature" }
    $opening = $source.IndexOf('{', $start)
    $depth = 0
    for ($index = $opening; $index -lt $source.Length; ++$index) {
        if ($source[$index] -eq '{') { ++$depth }
        if ($source[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) { return $source.Substring($start, $index - $start + 1) }
        }
    }
    throw "Unclosed production method: $signature"
}
$projectileStruct = Read-ProjectileSource 'project/game/weapon/CombatTypes.h'
$projectileTypeStart = $projectileStruct.IndexOf('struct AttackParam {', [StringComparison]::Ordinal)
$projectileTypeEnd = $projectileStruct.Length
if ($projectileTypeStart -lt 0) { throw 'Projectile type anchors changed.' }
$projectileTypes = $projectileStruct.Substring($projectileTypeStart, $projectileTypeEnd - $projectileTypeStart) + "`n" +
    (Remove-ProjectileIncludes (Read-ProjectileSource 'project/game/collision/CollisionConfig.h')) + "`n" +
    (Remove-ProjectileIncludes (Read-ProjectileSource 'project/game/collision/Collider.h'))
[IO.File]::WriteAllText((Join-Path $projectileOutput 'projectile_types.inc'), $projectileTypes, [Text.UTF8Encoding]::new($false))
$projectileDeclarations = foreach ($path in @('project/game/player/actor/Bullet.h', 'project/game/player/actor/BulletManager.h',
    'project/game/player/actor/AttackController.h', 'project/game/collision/CollisionManager.h')) {
    Remove-ProjectileIncludes (Read-ProjectileSource $path)
}
[IO.File]::WriteAllText((Join-Path $projectileOutput 'projectile_declarations.inc'), ($projectileDeclarations -join "`n"), [Text.UTF8Encoding]::new($false))
# Compile the real production declarations and complete projectile/collision
# implementations. Only rendering, vector math and unrelated actor systems use
# small adapters; the growth, damage, lifetime and wall algorithms are unedited.
$projectileMethods = foreach ($path in @('project/game/player/actor/Bullet.cpp', 'project/game/player/actor/BulletManager.cpp',
    'project/game/player/actor/AttackController.cpp', 'project/game/collision/CollisionManager.cpp')) {
    Remove-ProjectileIncludes (Read-ProjectileSource $path)
}
$projectileMethods += foreach ($signature in @(
    'void Player::RefreshAdditiveArmaments(', 'void Player::ResetAdditionalAbilities(',
    'bool Player::TryStartSpinBlade(', 'std::vector<Player::DroneAbilityVisual> Player::GetDroneAbilityVisuals(',
    'uint32_t Player::NotifyDroneHit(', 'float Player::GetDroneTargetDamageScale(',
    'void Player::ArmWallSmash(', 'void Player::UpdateAdditionalAbilities('
)) {
    Read-ProjectileMethod 'project/game/player/actor/Player.cpp' $signature
}
$projectileMethods += Read-ProjectileMethod 'project/game/exp/ExpEnemy.cpp' 'bool ExpEnemy::TryReflectProjectile('
$projectileMethods += Read-ProjectileMethod 'project/game/player/actor/Stage.cpp' 'void Stage::ResolveBulletsCollision('
$projectileMethods += Read-ProjectileMethod 'project/game/player/actor/Player.cpp' 'void Player::ApplyRunProjectileRules('
foreach ($signature in @('bool Player::SetExpeditionCombatStyle(', 'int Player::GetExpeditionDroneLimit(',
    'void Player::EnsureExpeditionDrones(', 'void Player::ConfigureRunDrone(', 'float Player::GetRunFireIntervalScale(',
    'std::vector<RunEvolutionChoice> Player::GetRunAuthoredEvolutionChoices(', 'bool Player::ChooseRunAuthoredClass(',
    'void Player::ApplyCombatStyleBalance(', 'float Player::GetRunBaseReloadFrames(', 'void Player::RecalculateStatsFromBase(', 'void Player::SetRunModifiers(',
    'void Player::AttackRailCannon(', 'void Player::UpdateSpecialCombat(', 'cg2::Vector3 Player::GetRailChargeMuzzle(', 'std::vector<Player::SpecialCombatEvent> Player::ConsumeSpecialCombatEvents(')) {
    $projectileMethods += Read-ProjectileMethod 'project/game/player/actor/Player.cpp' $signature
}
foreach ($signature in @('void PlayerDrone::ConfigureRunAttack(', 'void PlayerDrone::Attack(')) {
    $projectileMethods += Read-ProjectileMethod 'project/game/player/actor/PlayerDrone.cpp' $signature
}
[IO.File]::WriteAllText((Join-Path $projectileOutput 'projectile_methods.inc'), ($projectileMethods -join "`n"), [Text.UTF8Encoding]::new($false))
if (!$VisualStudioPath) {
    $projectileVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $projectileVsWhere) {
        $VisualStudioPath = (& $projectileVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    }
}
if (!$VisualStudioPath) { throw 'Existing Visual Studio C++ installation not found.' }
$projectileEnvironment = @{
    PROJECTILE_TEST_VS = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    PROJECTILE_TEST_SOURCE = Join-Path $PSScriptRoot 'tank_projectile_tests.cpp'
    PROJECTILE_TEST_INCLUDE = $projectileOutput
}
$projectilePrevious = @{}
foreach ($name in $projectileEnvironment.Keys) {
    $projectilePrevious[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $projectileEnvironment[$name], 'Process')
}
$projectileBatch = @'
@echo off
call "%PROJECTILE_TEST_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%PROJECTILE_TEST_INCLUDE%" /Fe:tank_projectile_tests.exe /Fo:.\ "%PROJECTILE_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
tank_projectile_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $projectileOutput 'build_tank_projectile_tests.cmd'), $projectileBatch, [Text.Encoding]::ASCII)
try {
    Push-Location -LiteralPath $projectileOutput
    try {
        & $env:ComSpec /d /c build_tank_projectile_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Projectile regression failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $projectileEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $projectilePrevious[$name], 'Process')
    }
}
