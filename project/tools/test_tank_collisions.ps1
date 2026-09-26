param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$tankCollisionRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$tankCollisionOutput = Join-Path $tankCollisionRoot 'generated/tank_collision_tests'
New-Item -ItemType Directory -Path $tankCollisionOutput -Force | Out-Null

function Read-ProductionMethod([string]$relativePath, [string]$signature) {
    $source = Get-Content -LiteralPath (Join-Path $tankCollisionRoot $relativePath) -Raw
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
$tankCollisionMethods = @(
    (Read-ProductionMethod 'project/game/collision/CollisionManager.cpp' 'void CollisionManager::CheckCollisionPair('),
    (Read-ProductionMethod 'project/game/player/actor/Player.cpp' 'bool Player::TryDashImpact('),
    (Read-ProductionMethod 'project/game/exp/ExpEnemy.cpp' 'void ExpEnemy::ApplyKnockback('),
    (Read-ProductionMethod 'project/game/enemy/actor/Enemy.cpp' 'void Enemy::ApplyKnockback('),
    (Read-ProductionMethod 'project/game/player/actor/Bullet.cpp' 'void Bullet::OnCollision('),
    (Read-ProductionMethod 'project/game/player/actor/Bullet.cpp' 'bool Bullet::CanHitActor('),
    (Read-ProductionMethod 'project/game/player/actor/Bullet.cpp' 'void Bullet::ApplyBulletDurabilityDamage('),
    (Read-ProductionMethod 'project/game/exp/ExpEnemy.cpp' 'void ExpEnemy::OnCollision('),
    (Read-ProductionMethod 'project/game/exp/ExpEnemy.cpp' 'bool ExpEnemy::ApplyDamage('),
    (Read-ProductionMethod 'project/game/exp/ExpEnemy.cpp' 'bool ExpEnemy::TakeDamageFromEnemy('),
    (Read-ProductionMethod 'project/game/exp/ExpEnemy.cpp' 'bool ExpEnemy::TakeDamageFromPlayer('),
	(Read-ProductionMethod 'project/game/exp/ExpEnemy.cpp' 'bool ExpEnemy::TakeDirectionalDamage('),
	(Read-ProductionMethod 'project/game/exp/ExpEnemy.cpp' 'uint32_t ExpEnemy::ResolveShieldDamage('),
    (Read-ProductionMethod 'project/game/exp/EnemyManager.cpp' 'ExpEnemy* EnemyManager::FindNearestEnemy('),
    (Read-ProductionMethod 'project/game/exp/EnemyManager.cpp' 'ExpEnemy* EnemyManager::FindNearestRunResource('),
    (Read-ProductionMethod 'project/game/enemy/actor/Enemy.cpp' 'void Enemy::RegisterExpEnemyKill('),
    (Read-ProductionMethod 'project/game/enemy/actor/Enemy.cpp' 'void Enemy::HealFromFeeding('),
    (Read-ProductionMethod 'project/game/enemy/actor/Enemy.cpp' 'void Enemy::AdvanceFeedingLevel('),
    (Read-ProductionMethod 'project/game/enemy/actor/Enemy.cpp' 'void Enemy::RegisterRunResourceClaim('),
    (Read-ProductionMethod 'project/game/enemy/actor/Enemy.cpp' 'Vector3 Enemy::ResolveMoveTargetPosition('),
    (Read-ProductionMethod 'project/game/enemy/actor/Enemy.cpp' 'void Enemy::SetRunEncounterEnabled('),
    (Read-ProductionMethod 'project/game/enemy/actor/Enemy.cpp' 'void Enemy::ResetRunEncounter('),
    (Read-ProductionMethod 'project/game/enemy/actor/Enemy.cpp' 'void Enemy::SetPrototypeMaxHp('),
    (Read-ProductionMethod 'project/game/exp/EnemyManager.cpp' 'void EnemyManager::ClearRunActors('),
    (Read-ProductionMethod 'project/game/exp/EnemyManager.cpp' 'void EnemyManager::ClearLevelData('),
    (Read-ProductionMethod 'project/game/player/actor/BulletManager.cpp' 'void BulletManager::ClearAll('),
    (Read-ProductionMethod 'project/game/player/actor/Stage.cpp' 'bool Stage::LoadRunMap(')
)
[IO.File]::WriteAllText((Join-Path $tankCollisionOutput 'tank_collision_methods.inc'), ($tankCollisionMethods -join "`n`n"), [Text.UTF8Encoding]::new($false))
if (!$VisualStudioPath) {
    $tankCollisionVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $tankCollisionVsWhere) {
        $VisualStudioPath = (& $tankCollisionVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
    }
}
if (!$VisualStudioPath) { throw 'Existing Visual Studio C++ installation not found.' }
$tankCollisionEnv = @{
    TANK_COLLISION_VS = Join-Path $VisualStudioPath 'Common7/Tools/VsDevCmd.bat'
    TANK_COLLISION_SOURCE = Join-Path $PSScriptRoot 'tank_collision_tests.cpp'
    TANK_COLLISION_INCLUDE = $tankCollisionOutput
}
$tankCollisionPrevious = @{}
foreach ($name in $tankCollisionEnv.Keys) {
    $tankCollisionPrevious[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
    [Environment]::SetEnvironmentVariable($name,$tankCollisionEnv[$name],'Process')
}
$tankCollisionBatch = @'
@echo off
call "%TANK_COLLISION_VS%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /utf-8 /EHsc /W4 /O2 /I"%TANK_COLLISION_INCLUDE%" /Fe:tank_collision_tests.exe /Fo:.\ "%TANK_COLLISION_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
tank_collision_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $tankCollisionOutput 'build_tank_collision_tests.cmd'),$tankCollisionBatch,[Text.Encoding]::ASCII)
try {
    Push-Location -LiteralPath $tankCollisionOutput
    try {
        & $env:ComSpec /d /c build_tank_collision_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Collision regression failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    foreach ($name in $tankCollisionEnv.Keys) { [Environment]::SetEnvironmentVariable($name,$tankCollisionPrevious[$name],'Process') }
}
