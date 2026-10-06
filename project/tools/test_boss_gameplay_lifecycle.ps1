param(
    [string]$VisualStudioPath = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$tankExpRepoDir = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$tankExpOutputDir = if ($OutputDirectory) {
    [System.IO.Path]::GetFullPath($OutputDirectory)
} else {
    Join-Path $tankExpRepoDir 'generated\boss_gameplay_lifecycle_tests'
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
function Read-BossMethod([string]$signature) {
    $bossSource = Get-Content -LiteralPath (Join-Path $tankExpRepoDir 'project/game/enemy/actor/Enemy.cpp') -Raw
    $bossStart = $bossSource.IndexOf($signature, [StringComparison]::Ordinal)
    if ($bossStart -lt 0) { throw "Missing production boss method: $signature" }
    $bossOpen = $bossSource.IndexOf('{', $bossStart)
    $bossDepth = 0
    for ($bossIndex = $bossOpen; $bossIndex -lt $bossSource.Length; ++$bossIndex) {
        if ($bossSource[$bossIndex] -eq '{') { ++$bossDepth }
        if ($bossSource[$bossIndex] -eq '}') {
            --$bossDepth
            if ($bossDepth -eq 0) { return $bossSource.Substring($bossStart, $bossIndex - $bossStart + 1) }
        }
    }
    throw "Unclosed production boss method: $signature"
}
function Read-BossDeclarations([string]$path) {
    $bossSource = Get-Content -LiteralPath (Join-Path $tankExpRepoDir $path) -Raw
    [regex]::Replace($bossSource, '(?m)^\s*#(?:include|pragma|define)[^\r\n]*\r?\n', '').Replace('private:', 'public:')
}
$bossEncoding = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $tankExpOutputDir 'boss_collider.inc'), (Read-BossDeclarations 'project/game/collision/Collider.h'), $bossEncoding)
[IO.File]::WriteAllText((Join-Path $tankExpOutputDir 'boss_enemy_declarations.inc'), (Read-BossDeclarations 'project/game/enemy/actor/Enemy.h'), $bossEncoding)
$bossMethods = foreach ($signature in @('void Enemy::Fire(', 'void Enemy::ShotgunFire(', 'cg2::Vector3 Enemy::GetWorldPosition(',
    'void Enemy::OnCollision(', 'void Enemy::TakeDamage(', 'void Enemy::ApplyKnockback(', 'void Enemy::Die(',
    'void Enemy::SpawnParticles(', 'void Enemy::Update(', 'void Enemy::Move(', 'void Enemy::UpdateDefeatPresentation(',
    'void Enemy::UpdateParticles(', 'void Enemy::TriggerDamageFeedback(', 'bool Enemy::isFinished(',
    'void Enemy::RegisterExpEnemyKill(', 'void Enemy::HealFromFeeding(', 'void Enemy::AdvanceFeedingLevel(',
    'void Enemy::SetEnemyProgressConfig(', 'void Enemy::SetRunEncounterEnabled(', 'void Enemy::ResetRunEncounter(',
    'void Enemy::SetPrototypeMaxHp(', 'Enemy::RivalCombatStatus Enemy::GetRivalCombatStatus(')) { Read-BossMethod $signature }
[IO.File]::WriteAllText((Join-Path $tankExpOutputDir 'boss_enemy_methods.inc'), ($bossMethods -join "`n"), $bossEncoding)
$tankExpBuildCmd = Join-Path $tankExpOutputDir 'build_boss_gameplay_lifecycle_tests.cmd'
# Pass Unicode source paths through process environment variables.
$tankExpBatch = @'
@echo off
call "%TANK_EXP_TEST_VS_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"%BOSS_LIFECYCLE_INCLUDE%" /I"%BOSS_LIFECYCLE_PROJECT%" /Fe:boss_gameplay_lifecycle_tests.exe /Fo:.\ "%TANK_EXP_TEST_SOURCE%"
if errorlevel 1 exit /b %errorlevel%
boss_gameplay_lifecycle_tests.exe
exit /b %errorlevel%
'@
[System.IO.File]::WriteAllText($tankExpBuildCmd, $tankExpBatch, [System.Text.Encoding]::ASCII)
$tankExpEnvironment = @{
    TANK_EXP_TEST_VS_DEV_CMD = $tankExpDevCmd
    BOSS_LIFECYCLE_INCLUDE = $tankExpOutputDir
    BOSS_LIFECYCLE_PROJECT = Join-Path $tankExpRepoDir 'project'
    TANK_EXP_TEST_SOURCE = Join-Path $PSScriptRoot 'boss_gameplay_lifecycle_tests.cpp'
}
$tankExpPreviousEnvironment = @{}
foreach ($tankExpName in $tankExpEnvironment.Keys) {
    $tankExpPreviousEnvironment[$tankExpName] = [Environment]::GetEnvironmentVariable($tankExpName, 'Process')
    [Environment]::SetEnvironmentVariable($tankExpName, $tankExpEnvironment[$tankExpName], 'Process')
}
try {
    Push-Location -LiteralPath $tankExpOutputDir
    try {
        & $env:ComSpec /d /c build_boss_gameplay_lifecycle_tests.cmd
        if ($LASTEXITCODE -ne 0) { throw "Boss gameplay lifecycle build/tests failed with exit code $LASTEXITCODE." }
    } finally { Pop-Location }
} finally {
    foreach ($tankExpName in $tankExpEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($tankExpName, $tankExpPreviousEnvironment[$tankExpName], 'Process')
    }
}
Write-Host "Boss gameplay lifecycle test suite passed. Executable: $tankExpOutputDir\boss_gameplay_lifecycle_tests.exe"
