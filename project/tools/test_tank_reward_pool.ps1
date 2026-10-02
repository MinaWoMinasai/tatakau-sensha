param([string]$VisualStudioPath = '')
$ErrorActionPreference = 'Stop'
$poolRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$poolOutput = Join-Path $poolRepo 'generated\tank_reward_pool_tests'
New-Item -ItemType Directory -Path $poolOutput -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $poolOutput 'Struct.h'),
    '#pragma once' + "`n" + 'namespace cg2 { struct Vector2 {float x,y;}; struct Vector4 {float x,y,z,w;}; }',
    [Text.UTF8Encoding]::new($false))
$poolSource = [IO.File]::ReadAllText((Join-Path $poolRepo 'project\game\ui\TankRewardCard.cpp'))
$poolStart = $poolSource.IndexOf('void TankRewardCard::BuildFrame()')
$poolEnd = $poolSource.IndexOf('void TankRewardCard::Tank(', $poolStart)
if ($poolStart -lt 0 -or $poolEnd -lt $poolStart) { throw 'Production card frame function was not found.' }
# Compile the real frame builder against counters, so adding ornaments to the
# production function also checks the prewarmed pool budget without a GPU.
$poolHarness = @'
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include "ColorMath.h"
using namespace cg2;
constexpr float kPi=3.14159265359f;
Vector4 RarityColor(int,float=0) {return {1,1,1,1};}
struct TankRewardCard {
    struct Model {bool styleChoice=false;int rarity=0;} model_;
    Vector2 center_{248,425},size_{368,330};Vector4 accent_{1,1,1,1};
    float acquireTime_=-1,hoverBlend_=0,visualTime_=0;int solids=0,glows=0;
    bool IsAcquireAnimating()const{return acquireTime_>=0&&acquireTime_<0.62f;}
    void Rect(Vector2,Vector2,Vector4,float=0){++solids;}
    void Line(Vector2,Vector2,float,Vector4){++solids;}
    void Glow(Vector2,Vector2,Vector4,float=0){++glows;}
    void BuildFrame();
};
'@
$poolHarness += "`n" + $poolSource.Substring($poolStart, $poolEnd-$poolStart) + "`n"
$poolHarness += @'
int main() {
    int maxSolid=0,maxGlow=0,samples=0;
    for(bool styleChoice:{false,true})for(int rarity=0;rarity<=4;++rarity)
    for(float hover:{0.0f,.01f,.03f,.06f,.5f,1.0f})
    for(float acquire:{-1.0f,0.0f,.01f,.16f,.31f,.61f,.62f})for(int frame=0;frame<120;++frame) {
        TankRewardCard card;card.model_.styleChoice=styleChoice;card.model_.rarity=rarity;
        card.hoverBlend_=hover;card.acquireTime_=acquire;card.visualTime_=frame/12.0f;
        card.BuildFrame();++card.solids; // HDR preview's single background rectangle.
        assert(card.solids<=96&&card.glows<=32);
        maxSolid=(std::max)(maxSolid,card.solids);maxGlow=(std::max)(maxGlow,card.glows);++samples;
    }
    std::cout<<"PASS "<<samples<<" real-frame samples; maximum "<<maxSolid<<"/96 solids, "<<maxGlow<<"/32 glows\n";
}
'@
[IO.File]::WriteAllText((Join-Path $poolOutput 'pool_tests.cpp'), $poolHarness, [Text.UTF8Encoding]::new($false))
if (!$VisualStudioPath) {
    $poolWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $VisualStudioPath = (& $poolWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
}
$poolDevCmd = Join-Path $VisualStudioPath 'Common7\Tools\VsDevCmd.bat'
if (!(Test-Path -LiteralPath $poolDevCmd)) { throw 'Visual Studio C++ tools were not found.' }
$poolBatch = @'
@echo off
call "%POOL_TEST_DEV_CMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /O2 /UNDEBUG /I"." /I"%POOL_TEST_UI_DIR%" pool_tests.cpp /Fe:pool_tests.exe
if errorlevel 1 exit /b %errorlevel%
pool_tests.exe
exit /b %errorlevel%
'@
[IO.File]::WriteAllText((Join-Path $poolOutput 'build.cmd'), $poolBatch, [Text.Encoding]::ASCII)
$poolPrevious = [Environment]::GetEnvironmentVariable('POOL_TEST_DEV_CMD','Process')
$poolPreviousUi = [Environment]::GetEnvironmentVariable('POOL_TEST_UI_DIR','Process')
[Environment]::SetEnvironmentVariable('POOL_TEST_DEV_CMD',$poolDevCmd,'Process')
[Environment]::SetEnvironmentVariable('POOL_TEST_UI_DIR',(Join-Path $poolRepo 'project/game/ui'),'Process')
try {
    Push-Location -LiteralPath $poolOutput
    try { & $env:ComSpec /d /c build.cmd; if ($LASTEXITCODE -ne 0) { throw "Card pool tests failed: $LASTEXITCODE" } }
    finally { Pop-Location }
} finally {
    [Environment]::SetEnvironmentVariable('POOL_TEST_DEV_CMD',$poolPrevious,'Process')
    [Environment]::SetEnvironmentVariable('POOL_TEST_UI_DIR',$poolPreviousUi,'Process')
}
