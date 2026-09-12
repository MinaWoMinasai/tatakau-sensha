#include "InkShooterScene.h"
#include "Object3dCommon.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
namespace {
const char* StateName(ink::PlayerState state) {
    return state == ink::PlayerState::WallSwim ? "壁泳ぎ（潜伏）" :
        state == ink::PlayerState::Swim ? "イカ（潜伏）" :
        state == ink::PlayerState::Squid ? "イカ（露出）" : "人型";
}
void Explain(const char* text) {
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
}
#endif

void InkShooterScene::DrawDebug() {
#ifdef USE_IMGUI
    if (!debug_) return;
    // Anchor inside the live viewport. Widget IDs remain independent of labels.
    const auto* viewport = ImGui::GetMainViewport();
    const float margin = (std::min)(12.0f, (std::min)(viewport->WorkSize.x, viewport->WorkSize.y) * 0.02f);
    const ImVec2 size = {
        (std::min)(450.0f, (std::max)(1.0f, viewport->WorkSize.x - margin * 2)),
        (std::min)(790.0f, (std::max)(1.0f, viewport->WorkSize.y - margin * 2))
    };
    ImGui::SetNextWindowPos({viewport->WorkPos.x + viewport->WorkSize.x - margin, viewport->WorkPos.y + margin}, ImGuiCond_Always, {1, 0});
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    if (ImGui::Begin("インクシューターの調整 / F1###InkShooterDebug", &debug_,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushTextWrapPos(0.0f);
        if (ImGui::Button("操作に戻る###ResumePlay", {-1, 0})) debug_ = false;
        ImGui::TextWrapped("WASD：移動　左クリック：射撃\nShift：イカ変身／自色で潜伏\nSpace：ジャンプ　壁の W/S：上下\n壁の A/D：左右　F1：調整\nTab：マウス切替　Esc：終了");
        audio_.DrawEditor();
        if(ImGui::CollapsingHeader("イカの慣性###SquidCarry")) {
            ImGui::TextWrapped("自インクから未塗装へ出たときに速度を引き継ぎ、徐々に減速します。数値はCG2の操作感調整です。");
            ImGui::SliderFloat("余韻の減速###CarryDeceleration",&simulation_.movement.drySquidCarryDeceleration,2.0f,30.0f,"%.1f 単位/秒²");
            ImGui::SliderFloat("余韻の最大時間###CarryTime",&simulation_.movement.drySquidCarryMaxTime,0.05f,1.0f,"%.2f 秒");
            ImGui::SliderFloat("離す・逆方向の制動###CarryBrake",&simulation_.movement.drySquidCarryBrakeMultiplier,1.0f,5.0f,"%.1f 倍");
            ImGui::Text("現在の速度 %.2f / 余韻 %.2f 秒",simulation_.Player().speed,simulation_.DrySquidCarryRemaining());
            ImGui::TextWrapped("この3項目は今回の試し撃ちだけに反映します。");
        }
        DrawWeaponEditor();
        const auto& p = simulation_.Player();
        auto& w = simulation_.weapon;
        auto& movement = simulation_.movement;
        ImGui::Separator();
        ImGui::Text("状態：%s / %s", StateName(p.state), p.grounded ? "接地" : p.state == ink::PlayerState::WallSwim ? "壁に接触" : "空中");
        Explain("イカへの変身は、乾いた床・空中・インク切れでも可能です。自分のインクに触れて潜ったときだけ、高速移動と高速補給へ切り替わります。");
        ImGui::Text("接触するインク：%s", p.onOwnInk ? "自分の色" : p.onEnemyInk ? "相手の色" : "なし");
        char tankText[64];
        std::snprintf(tankText, sizeof(tankText), "インク残量 %.1f%%", p.ink * 100.0f);
        ImGui::ProgressBar(p.ink, {-1, 0}, tankText);
        Explain("自分のインクに潜ると速く補給できます。人型・露出イカは通常の補給速度です。射撃後は回復停止時間が経過してから再開します。残量0でも変身できます。");
        ImGui::Text("描画 %.1f FPS / 固定更新 120 Hz", fps_);
        ImGui::PushItemWidth((std::max)(80.0f, ImGui::GetContentRegionAvail().x * 0.48f));
        if (ImGui::CollapsingHeader("カメラ・マウス###CameraControls", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("マウス感度###MouseSensitivity", &sensitivity_, 0.0005f, 0.008f, "%.4f");
            Explain("値が大きいほど少ないマウス移動で大きく視点が動きます。");
            ImGui::Checkbox("マウスの上下を反転###InvertMousePitch", &invertPitch_);
            ImGui::SliderFloat("カメラ距離###CameraDistance", &cameraDistance_, 2, 7, "%.2f");
            ImGui::SliderFloat("注視点の高さ###CameraHeight", &cameraHeight_, 0.7f, 2.2f, "%.2f");
            ImGui::SliderFloat("右肩へのずれ###ShoulderOffset", &shoulderOffset_, 0, 1.1f, "%.2f");
            ImGui::SliderAngle("縦の視野角###VerticalFieldOfView", &fovY_, 40, 80, "%.1f 度");
            ImGui::SliderFloat("移動への追従###CameraFollow", &followSharpness_, 5, 45, "%.1f");
            Explain("値が大きいほどプレイヤーへ素早く追従します。照準から銃口への弾道補正は維持します。");
            ImGui::Text("射撃の反動演出：%.2f", visualKick_);
            Explain("現在の反動演出の強さです。弾の拡散や連射間隔とは別に、射撃に合わせて減衰します。");
        }
        if (ImGui::CollapsingHeader("塗りの形・境界###PaintAppearance")) {
            if(simulation_.ActiveWeaponClass()==ink::WeaponClass::Stringer) {
                ImGui::TextWrapped("ストリンガーの塗りは、上のブキ編集から調整して保存できます。");
                ImGui::Checkbox("塗りの境界をなめらかに###SmoothStringerEdges", &wetEdges_);
            } else {
            ImGui::TextWrapped("飛沫は1・2個の予算を交互に割り当て、8段階で落下位置を変えます。壁や床へ先に当たると個数が減ります。周期の順序と揺らぎはCG2の近似です。");
            Explain("原作の公開値は生成数1.5、分割数8です。小数の丸め方と周期処理は公開されていないため、原作そのものの乱数処理とはしていません。");
            ImGui::Checkbox("塗りの境界をなめらかに###SmoothPaintEdges", &wetEdges_);
            Explain("境界を画面上でなめらかに見せます。CPUとGPUの塗り範囲・所有者判定は共通です。");
            ImGui::TextWrapped("塗りの幅・飛沫・足元塗りは、上の「ブキの数値を編集」で調整して保存できます。");
            }
        }
        if (ImGui::CollapsingHeader("シューター基準・共通移動###GameplayReference")) {
            ImGui::TextWrapped("スプラシューター参考 / Ver.11.3.0\n以下は現在の値です。初期値には調査した基準値を使用しています。");
            ImGui::Text("連射間隔：%.1f F / %.3f 秒", w.repeatFrame, w.FireInterval());
            Explain("原作の1Fは1/60秒です。基準は6F、0.1秒ごと、毎秒10発。描画FPSや120Hzの固定更新とは別です。");
            ImGui::Text("消費：%.2f%% / 発　回復停止：%.3f 秒", w.inkConsume * 100, w.inkRecoverStop);
            Explain("基準は1発0.92％の消費、最後の射撃から20Fの回復停止です。100％のタンクから補給なしで108発撃てます。");
            ImGui::Text("弾の初速：%.2f　重力：%.2f", w.projectileSpeed, w.projectileGravity);
            ImGui::Text("拡散角：地上 %.2f 度 / ジャンプ %.2f 度", w.groundSpread, w.jumpSpread);
            ImGui::Text("歩行 %.2f / 射撃歩行 %.2f / 遊泳 %.2f", movement.humanSpeed, w.moveSpeedWhileFiring, movement.swimSpeed);
            Explain("速度は1秒あたりのCG2ワールド距離です。公開データの距離に0.5の縮尺を適用しています。実世界のメートルではありません。");
            ImGui::Text("補給：通常 %.1f%% / 秒 / 潜伏 %.1f%% / 秒", movement.humanInkRecovery * 100, movement.swimInkRecovery * 100);
            Explain("通常は人型と露出イカ、潜伏は自色インク内の床・壁泳ぎです。露出状態への通常補給の適用はCG2側の近似条件です。");
            ImGui::Text("未塗装でのイカ移動：%.2f / 秒", movement.drySquidSpeed);
            Explain("未塗装や塗れない床での低速移動です。現在はCG2独自の調整値を使用し、原作の確定値とはしていません。空中では飛び出しの水平速度を引き継ぎます。");
            ImGui::Text("相手インク：移動 %.2f / 射撃移動 %.2f", movement.enemyInkSpeed, movement.enemyInkShotSpeed);
            ImGui::Text("相手インク：ジャンプ初速 %.2f", movement.enemyInkJumpSpeed);
            Explain("相手の色へ接地すると移動とジャンプを制限します。基準値は1130の能力補正なしの人型データです。露出イカへの同じ減速の適用はCG2の近似です。");
            if (ImGui::TreeNode("基準値を変える実験###AdvancedGameplayTuning")) {
                static bool editGameplay = false;
                ImGui::Checkbox("実験用の数値変更を有効にする###EnableGameplayEditing", &editGameplay);
                ImGui::TextWrapped("変更すると公開値を基準とした比較から外れます。再起動すると初期値へ戻ります。");
                ImGui::BeginDisabled(!editGameplay);
                ImGui::TextWrapped("ブキの数値は上の「ブキを選ぶ・作る」で編集・保存できます。以下は共通移動の一時調整です。");
                ImGui::SliderFloat("人型の移動速度###HumanSpeed", &movement.humanSpeed, 1, 6, "%.2f");
                ImGui::SliderFloat("遊泳の移動速度###SwimSpeed", &movement.swimSpeed, 2, 12, "%.2f");
                ImGui::SliderFloat("未塗装のイカ速度###DrySquidSpeed", &movement.drySquidSpeed, 0.2f, 2.0f, "%.2f");
                Explain("CG2独自の低速移動の調整です。自インクの遊泳速度は変更しません。");
                ImGui::SliderFloat("相手インクの速度###EnemyInkSpeed", &movement.enemyInkSpeed, 0.1f, 1.5f, "%.2f");
                ImGui::SliderFloat("同・射撃移動速度###EnemyInkShotSpeed", &movement.enemyInkShotSpeed, 0.05f, 1.0f, "%.2f");
                ImGui::SliderFloat("同・ジャンプ初速###EnemyInkJumpSpeed", &movement.enemyInkJumpSpeed, 0.5f, 5.0f, "%.2f");
                if (ImGui::Button("共通移動を初期値へ###RestoreWeaponDefaults")) {
                    movement = ink::MovementParams{};
                }
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
        }
        if (ImGui::CollapsingHeader("ダメージ確認用の的###DamageDummy")) {
            const auto& dummy = simulation_.Dummy();
            ImGui::Text("的の体力：%.1f / 100", dummy.hp);
            ImGui::Text("直近のダメージ：%.1f", dummy.lastDamage);
            ImGui::Text("命中した弾の飛距離：%.2f", dummy.lastHitDistance);
            Explain("距離はCG2ワールド単位です。シューターの減衰は飛翔時間、ストリンガーの直撃は発射時のチャージ量で決まります。爆風には別の判定があります。");
            if (dummy.lastShotsToKill) ImGui::Text("直近の撃破に必要だった弾：%u 発", dummy.lastShotsToKill);
            else ImGui::TextUnformatted("直近の撃破に必要だった弾：未計測");
            ImGui::Text("今回の命中：%u 発　撃破回数：%u", dummy.hits, dummy.kills);
            if (dummy.resetRemaining > 0) ImGui::Text("的の復活まで：%.1f 秒", dummy.resetRemaining);
            ImGui::TextWrapped(simulation_.ActiveWeaponClass()==ink::WeaponClass::Stringer?
                "基準：矢1本30～35、最大チャージの3本直撃で105。爆風は1個30です。撃破までの表示は命中した矢・爆風の個数です。":
                "基準：近距離36、減衰後18ダメージ。近距離では3発で体力100を超えます。");
        }
        if (ImGui::CollapsingHeader("状態・負荷の詳細###RuntimeStatistics")) {
            ImGui::Text("接地：%s　速度：%.2f", p.grounded ? "はい" : "いいえ", p.speed);
            ImGui::Text("位置：X %.2f / Y %.2f / Z %.2f", p.position.x, p.position.y, p.position.z);
            ImGui::Text("現在の拡散角：%.2f 度", p.accuracy);
            ImGui::Text("回復停止の残り：%.3f 秒", simulation_.InkRecoveryDelay());
            ImGui::Text("飛行中：主弾 %zu / 飛沫 %zu", simulation_.Projectiles().size(), simulation_.Droplets().size());
            ImGui::Text("累計：発射 %llu / 塗り %zu", static_cast<unsigned long long>(simulation_.ShotsFired()), simulation_.StampCount());
            ImGui::Text("直近のGPU塗り更新：%u 個", paint_.GetLastStampCount());
            ImGui::Text("GPU処理：塗り更新 %.3f ms", paint_.GetLastPaintGpuMs());
            Explain("直近に完了したフレームの塗りマスク更新にかかったGPU時間です。初回の計測前は0を表示します。");
            ImGui::Text("GPU処理：床・壁の描画 %.3f ms", paint_.GetLastSurfaceGpuMs());
            Explain("塗装面を描く処理のGPU時間です。画面全体やインク弾の描画時間は含みません。");
            ImGui::Text("描画中のインク粒子：%u 個", liquid_.GetParticleCount());
            ImGui::TextWrapped("ジャイロは未対応です。今回はマウス操作での再現度を優先しています。");
        }
        if (ImGui::CollapsingHeader("GPUの塗りマスク###GpuPaintAtlas")) {
            ImGui::TextWrapped("1面512×512、最大16面。\n塗りマスク：GPU 16 MiB / CPU 4 MiB");
            const auto handle = Object3dCommon::GetInstance()->GetSrvManager()->GetGPUDescriptorHandle(paint_.GetMaskSrvIndex());
            const float previewSize = (std::min)(320.0f, (std::max)(1.0f, ImGui::GetContentRegionAvail().x));
            ImGui::Image(static_cast<ImTextureID>(handle.ptr), {previewSize, previewSize});
            Explain("床・坂・壁を4×4に並べた塗り情報です。各面はCPUもGPUも512×512で、同じ塗り形状と所有者情報を更新します。色のある場所が塗装済みです。メモリ表示は塗りマスク本体のみです。");
        }
        ImGui::PopItemWidth();
        ImGui::Separator();
        if (ImGui::Button("ステージをリセット / R###ResetStage", {-1, 0})) Reset();
        if (ImGui::Button("操作デモを再生 / F9###RunDemo", {-1, 0})) StartReplay();
        if (ImGui::Button("比較画像を保存###CaptureComparison", {-1, 0})) RequestCapture("manual");
        Explain("調整パネルを含まないゲーム画面を保存します。比較用のPNG画像として利用できます。");
        if (ImGui::Button("壁登り用の道を塗る（確認用）###PaintClimbLane", {-1, 0})) {
            const auto& surfaces = simulation_.Surfaces();
            for (uint32_t n = 0; n < static_cast<uint32_t>(surfaces.size()); ++n) {
                const auto& s = surfaces[n];
                if (!s.inkable) continue;
                for (float y = 0; y < s.height; y += 0.6f) {
                    for (float x = 0; x < s.width; x += 0.6f) {
                        const auto world = s.origin + s.u * x + s.v * y;
                        if (std::abs(world.x) < 1.2f && world.z > -9 && world.z < 15)
                            simulation_.Paint({n, x, y, 0.6f, 0.6f, 0, 1});
                    }
                }
            }
        }
        Explain("テストステージの中央に塗り済みの道を作ります。壁泳ぎの動作確認に使う補助ボタンです。");
        if (ImGui::Button("相手インクの試験帯を置く###PaintEnemyTestLane", {-1, 0}))
            simulation_.Paint({0,15,9,1.5f,2.4f,0,2});
        Explain("開始位置の少し先に相手色の塗りを置きます。移動・ジャンプの低下と、自分の射撃による塗り返しを試せます。Rで消せます。");
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
    if (!debug_) SetCaptured(true);
#endif
}
