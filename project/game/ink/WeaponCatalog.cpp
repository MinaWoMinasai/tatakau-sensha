#include "WeaponCatalog.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace ink {
const std::vector<TypedFloatField<ShooterWeaponParams>>& ShooterFloatFields() {
    using T = ShooterWeaponParams;
#define F(key, label, unit, group, lo, hi) {#key, label, unit, group, &T::key, lo, hi}
    static const std::vector<TypedFloatField<T>> fields = {
        F(repeatFrame,"連射間隔","F","射撃",1,120),
        F(inkConsume,"1発のインク消費","比率","射撃",0,1),
        F(inkRecoverStop,"射撃後の回復停止","秒","射撃",0,10),
        F(initialShotDelay,"人型からの初弾待ち","秒","射撃",0,5),
        F(swimInitialShotDelay,"イカからの初弾待ち","秒","射撃",0,5),
        F(postShotDelay,"射撃後の変身待ち","秒","射撃",0,5),
        F(projectileSpeed,"弾の初速","距離/秒","弾道",0.01f,500),
        F(projectileGravity,"自由落下の重力","距離/秒2","弾道",0,1000),
        F(straightFlightTime,"直進時間","秒","弾道",0,10),
        F(brakeInitialSpeed,"減速開始時の速度","距離/秒","弾道",0.01f,500),
        F(brakeAirResistance,"減速中の抵抗","比率/F","弾道",0,0.99f),
        F(brakeGravity,"減速中の重力","距離/秒2","弾道",0,1000),
        F(brakeToFreeSpeedXZ,"自由落下への水平速度","距離/秒","弾道",0,1000),
        F(brakeToFreeSpeedY,"自由落下への垂直速度","距離/秒","弾道",-1000,1000),
        F(freeAirResistance,"自由落下中の抵抗","比率/F","弾道",0,0.99f),
        F(effectiveRange,"射程の基準距離","距離","弾道",0.01f,60),
        F(projectileLifetime,"弾の最大寿命","秒","弾道",0.01f,10),
        F(playerHitRadius,"的への当たり半径","距離","ダメージ",0.001f,3),
        F(stageHitRadius,"地形への当たり半径","距離","弾道",0.001f,3),
        F(baseDamage,"最大ダメージ","HP","ダメージ",0,1000),
        F(minimumDamage,"最低ダメージ","HP","ダメージ",0,1000),
        F(damageFalloffStart,"減衰開始時間","秒","ダメージ",0,10),
        F(damageFalloffEnd,"減衰終了時間","秒","ダメージ",0,10),
        F(groundSpread,"地上の拡散角","度","拡散",0,89),
        F(jumpSpread,"空中の拡散角","度","拡散",0,89),
        F(accuracyBiasMinimum,"拡散の最小偏り","比率","拡散",0,1),
        F(accuracyBiasPerShot,"1発ごとの偏り増加","比率","拡散",0,1),
        F(accuracyBiasMaximum,"拡散の最大偏り","比率","拡散",0,1),
        F(jumpAccuracyBiasMaximum,"空中の最大偏り","比率","拡散",0,1),
        F(accuracyRecovery,"偏りの回復速度","比率/秒","拡散",0,60),
        F(accuracyRecoveryDelay,"偏りの回復開始待ち","秒","拡散",0,10),
        F(jumpAccuracyRecoveryStart,"空中精度の回復開始","秒","拡散",0,10),
        F(jumpAccuracyRecoveryEnd,"空中精度の回復終了","秒","拡散",0,10),
        F(moveSpeedWhileFiring,"射撃中の移動速度","距離/秒","射撃",0,30),
        F(sourcePaintDropletSpacing,"公開値・飛沫間隔","距離","参考",0.1f,50),
        F(sourcePaintDropletSpawnCount,"平均飛沫生成数","個/発","塗り",0,12),
        F(paintDropletSpacing,"飛沫の生成間隔","距離","塗り",0.1f,50),
        F(firstPaintDropletDistance,"最初の飛沫の距離","距離","塗り",0,50),
        F(paintDropletRadius,"飛沫の幅半径","距離","塗り",0.001f,10),
        F(nearestPaintDropletRadius,"足元飛沫の幅半径","距離","塗り",0.001f,10),
        F(impactPaintRadius,"近い着弾の幅半径","距離","塗り",0.001f,10),
        F(distantImpactPaintRadius,"遠い着弾の幅半径","距離","塗り",0.001f,10),
        F(sourceImpactDepthScaleMin,"着弾の最小伸び","倍","塗り",0.01f,5),
        F(sourceImpactDepthScaleMax,"着弾の最大伸び","倍","塗り",0.01f,5),
        F(sourceBreakFreeDepthScaleMin,"公開値・落下時の最小伸び","倍","参考",0.01f,5),
        F(sourceBreakFreeDepthScaleMax,"公開値・落下時の最大伸び","倍","参考",0.01f,5),
        F(sourceDepthAngleMin,"最小伸びになる入射角","度","塗り",0,89.9f),
        F(sourceDepthAngleMax,"最大伸びになる入射角","度","塗り",0,89.9f),
        F(sourceImpactWidthHalfMiddle,"公開値・中距離の幅半径","距離","参考",0.001f,10),
        F(sourceImpactDistanceMiddle,"公開値・中距離の境界","距離","参考",0,60),
        F(sourceDropletDepthMaxDropHeight,"公開値・最大伸びの落差","距離","参考",0,100),
        F(sourceDropletDepthMinDropHeight,"公開値・最小伸びの落差","距離","参考",0,100),
        F(impactCoreScale,"主着弾の中心幅","倍","CG2調整",0.01f,2),
        F(dropletCoreScale,"飛沫の中心幅","倍","CG2調整",0.01f,2),
        F(footCoreScale,"足元塗りの中心幅","倍","CG2調整",0.01f,2),
        F(scatterRadiusScale,"周囲の小粒の幅","倍","CG2調整",0,1),
        F(impactNormalDepthScale,"正対時の縦横比","倍","CG2調整",0.1f,5)
    };
#undef F
    return fields;
}
const std::vector<TypedIntField<ShooterWeaponParams>>& ShooterIntFields() {
    using T = ShooterWeaponParams;
    static const std::vector<TypedIntField<T>> fields = {
        {"paintDropletCount","飛沫数の上限","個","塗り",&T::paintDropletCount,0,12},
        {"sourcePaintDropletSplitCount","飛沫周期の分割数","段階","塗り",&T::sourcePaintDropletSplitCount,1,16},
        {"sourceForceNearestAddCount","公開値・最近傍の追加数","個","参考",&T::sourceForceNearestAddCount,0,12},
        {"footRescueEveryShots","足元救済までの最大射撃数","発","CG2調整",&T::footRescueEveryShots,1,12}
    };
    return fields;
}
const std::vector<TypedBoolField<ShooterWeaponParams>>& ShooterBoolFields() {
    static const std::vector<TypedBoolField<ShooterWeaponParams>> fields;
    return fields;
}
const std::vector<TypedFloatField<StringerWeaponParams>>& StringerFloatFields() {
    using T = StringerWeaponParams;
#define F(key, label, unit, group, lo, hi) {#key, label, unit, group, &T::key, lo, hi}
    static const std::vector<TypedFloatField<T>> fields = {
        F(minChargeTime,"最短の溜め時間","秒","溜め",0.001f,10),
        F(midChargeTime,"1段階の溜め時間","秒","溜め",0.001f,10),
        F(fullChargeTime,"フル溜め時間","秒","溜め",0.001f,10),
        F(minFreezeTime,"最短射撃後の硬直","秒","射撃",0,10),
        F(midFreezeTime,"1段階射撃後の硬直","秒","射撃",0,10),
        F(fullFreezeTime,"フル射撃後の硬直","秒","射撃",0,10),
        F(postShotDelay,"射撃後の変身待ち","秒","射撃",0,5),
        F(minInkConsume,"最短射撃の消費","比率","射撃",0,1),
        F(midInkConsume,"1段階射撃の消費","比率","射撃",0,1),
        F(fullInkConsume,"フル射撃の消費","比率","射撃",0,1),
        F(minDamage,"最短時の矢1本の威力","HP","ダメージ",0,1000),
        F(midDamage,"1段階の矢1本の威力","HP","ダメージ",0,1000),
        F(fullDamage,"フル時の矢1本の威力","HP","ダメージ",0,1000),
        F(minProjectileSpeed,"最短時の初速","距離/秒","弾道",0.01f,500),
        F(midProjectileSpeed,"1段階時の初速","距離/秒","弾道",0.01f,500),
        F(fullProjectileSpeed,"フル時の初速","距離/秒","弾道",0.01f,500),
        F(minSpreadDegrees,"最短時の矢の開き","度","弾道",0,89),
        F(midSpreadDegrees,"1段階時の矢の開き","度","弾道",0,89),
        F(fullSpreadDegrees,"フル時の矢の開き","度","弾道",0,89),
        F(arrowSpacing,"矢の発射位置の間隔","距離","弾道",0,3),
        F(straightFlightTime,"直進時間","秒","弾道",0,10),
        F(brakeFlightTime,"減速段階の時間","秒","弾道",0,10),
        F(brakeAirResistance,"減速中の抵抗","比率/F","弾道",0,0.99f),
        F(brakeGravity,"減速中の重力","距離/秒2","弾道",0,1000),
        F(brakeToFreeSpeedXZ,"公開値・自由落下への水平速度","距離/秒","参考",0,1000),
        F(brakeToFreeSpeedY,"公開値・自由落下への垂直速度","距離/秒","参考",-1000,1000),
        F(freeAirResistance,"自由落下中の抵抗","比率/F","弾道",0,0.99f),
        F(freeGravity,"自由落下中の重力","距離/秒2","弾道",0,1000),
        F(playerHitRadius,"的への当たり半径","距離","ダメージ",0.001f,3),
        F(stageHitRadius,"地形への当たり半径","距離","弾道",0.001f,3),
        F(minPaintRadius,"最短時の着弾塗り半径","距離","塗り",0.001f,10),
        F(midPaintRadius,"1段階の着弾塗り半径","距離","塗り",0.001f,10),
        F(fullPaintRadius,"フル時の着弾塗り半径","距離","塗り",0.001f,10),
        F(paintDropletRadius,"飛沫の幅半径","距離","塗り",0.001f,10),
        F(nearestPaintDropletRadius,"公開値・足元飛沫の幅半径","距離","参考",0.001f,10),
        F(sourceDropletDepthScaleMax,"公開値・飛沫の最大伸び","倍","参考",0.01f,5),
        F(sourceDropletInterval,"公開値・飛沫間隔","元の値","参考",0.1f,100),
        F(detonationTime,"着弾から爆発まで","秒","爆発",0.001f,10),
        F(explosionDamage,"爆風ダメージ","HP","爆発",0,1000),
        F(explosionDamageRadius,"爆風の当たり半径","距離","爆発",0.001f,10),
        F(explosionPaintRadius,"爆発の塗り半径","距離","爆発",0.001f,10),
        F(explosionOffset,"爆発位置の高さ補正","距離","爆発",0,3),
        F(moveSpeedWhileCharging,"溜め中の移動速度","距離/秒","溜め",0,30),
        F(inkRecoverStop,"射撃後の回復停止","秒","CG2調整",0,10),
        F(airborneChargeRate,"空中の溜め速度","倍","CG2調整",0.1f,5),
        F(projectileLifetime,"矢の最大寿命","秒","CG2調整",0.01f,10),
        F(impactCoreScale,"着弾の中心幅","倍","CG2調整",0.01f,2),
        F(dropletCoreScale,"飛沫の中心幅","倍","CG2調整",0.01f,2)
    };
#undef F
    return fields;
}
const std::vector<TypedIntField<StringerWeaponParams>>& StringerIntFields() {
    using T = StringerWeaponParams;
    static const std::vector<TypedIntField<T>> fields = {
        {"arrowCount","一度に放つ矢","本","射撃",&T::arrowCount,1,3},
        {"sourceDropletMaxCount","公開値・飛沫の最大数","個","参考",&T::sourceDropletMaxCount,0,12},
        {"sourceDropletSplitCount","公開値・飛沫の分割数","段階","参考",&T::sourceDropletSplitCount,1,32}
    };
    return fields;
}
const std::vector<TypedBoolField<StringerWeaponParams>>& StringerBoolFields() {
    using T = StringerWeaponParams;
    static const std::vector<TypedBoolField<T>> fields = {
        {"enableChargeKeep","チャージキープ（未対応）","","参考",&T::enableChargeKeep,false},
        {"explosiveAtMidCharge","1段階以上で着弾後に爆発","","爆発",&T::explosiveAtMidCharge,true}
    };
    return fields;
}

namespace {
using Json = nlohmann::json;
constexpr size_t MaxEntries = 128;
constexpr size_t MaxBytes = 2 * 1024 * 1024;

bool Fail(std::string& error, const std::string& message) { error = message; return false; }
bool ValidId(const std::string& id) {
    return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}
bool ValidText(const std::string& value, size_t limit, bool allowEmpty) {
    if ((!allowEmpty && value.empty()) || value.size() > limit) return false;
    if (std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 0x20; })) return false;
    try { Json(value).dump(); return true; } catch (const Json::exception&) { return false; }
}
template<class T> bool ValidateFields(const T& params, const std::vector<TypedFloatField<T>>& floats,
    const std::vector<TypedIntField<T>>& ints, std::string& error) {
    for (const auto& field : floats) {
        const float value = params.*(field.member);
        if (!std::isfinite(value) || value < field.min || value > field.max)
            return Fail(error, std::string("params.") + field.key + ": " + field.label + "が許容範囲外です。");
    }
    for (const auto& field : ints) {
        const int value = params.*(field.member);
        if (value < field.min || value > field.max)
            return Fail(error, std::string("params.") + field.key + ": " + field.label + "が許容範囲外です。");
    }
    return true;
}
template<class T> Json WriteParams(const T& params, const std::vector<TypedFloatField<T>>& floats,
    const std::vector<TypedIntField<T>>& ints, const std::vector<TypedBoolField<T>>& bools) {
    Json result = Json::object();
    for (const auto& f : floats) result[f.key] = params.*(f.member);
    for (const auto& f : ints) result[f.key] = params.*(f.member);
    for (const auto& f : bools) result[f.key] = params.*(f.member);
    return result;
}
template<class T> void ReadParams(const Json& json, T& params, const std::vector<TypedFloatField<T>>& floats,
    const std::vector<TypedIntField<T>>& ints, const std::vector<TypedBoolField<T>>& bools) {
    if (!json.is_object()) throw std::runtime_error("params はオブジェクトが必要です。");
    std::set<std::string> known;
    for (const auto& f : floats) {
        known.insert(f.key);
        if (!json.contains(f.key)) continue;
        if (!json[f.key].is_number()) throw std::runtime_error(std::string("params.")+f.key+": 数値が必要です。");
        const double value = json[f.key].get<double>();
        if (!std::isfinite(value) || value < -std::numeric_limits<float>::max() || value > std::numeric_limits<float>::max())
            throw std::runtime_error(std::string("params.")+f.key+": 許容範囲外です。");
        // Compare in the destination type: decimal 0.1 must be accepted when
        // the declared float lower bound is 0.1f (a slightly larger double).
        const float converted = static_cast<float>(value);
        if (!std::isfinite(converted) || converted < f.min || converted > f.max)
            throw std::runtime_error(std::string("params.")+f.key+": 許容範囲外です。");
        params.*(f.member) = converted;
    }
    for (const auto& f : ints) {
        known.insert(f.key);
        if (!json.contains(f.key)) continue;
        if (!json[f.key].is_number_integer()) throw std::runtime_error(std::string("params.")+f.key+": 整数が必要です。");
        const double value = json[f.key].get<double>();
        if (value < f.min || value > f.max) throw std::runtime_error(std::string("params.")+f.key+": 許容範囲外です。");
        params.*(f.member) = static_cast<int>(value);
    }
    for (const auto& f : bools) {
        known.insert(f.key);
        if (!json.contains(f.key)) continue;
        if (!json[f.key].is_boolean()) throw std::runtime_error(std::string("params.")+f.key+": true または false が必要です。");
        params.*(f.member) = json[f.key].get<bool>();
    }
    for (const auto& item : json.items()) if (!known.count(item.key()))
        throw std::runtime_error("params." + item.key() + ": 未対応の項目です。");
}
void CheckKeys(const Json& json, std::initializer_list<const char*> keys) {
    std::set<std::string> known(keys.begin(), keys.end());
    for (const auto& item : json.items()) if (!known.count(item.key()))
        throw std::runtime_error(item.key() + ": 未対応の項目です。");
}
std::string RequiredText(const Json& json, const char* key) {
    if (!json.contains(key) || !json[key].is_string()) throw std::runtime_error(std::string(key)+": 文字列が必要です。");
    return json[key].get<std::string>();
}
bool WriteAtomic(const std::filesystem::path& target, const std::string& bytes, std::string& error) {
    static std::atomic<unsigned long> sequence{0};
    std::filesystem::path temporary = target;
#ifdef _WIN32
    temporary += L".tmp_" + std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(++sequence);
#else
    temporary += ".tmp_" + std::to_string(++sequence);
#endif
    auto cleanup = [&]() { std::error_code ignored; std::filesystem::remove(temporary, ignored); };
    std::error_code ec;
    if (!target.parent_path().empty()) std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) return Fail(error, "保存先フォルダーを作成できません: " + ec.message());
#ifdef _WIN32
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return Fail(error, "一時ファイルを作成できません。Windowsエラー " + std::to_string(GetLastError()));
    DWORD written = 0;
    const bool writeOk = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != 0 && written == bytes.size();
    const bool flushOk = writeOk && FlushFileBuffers(file) != 0;
    const bool closeOk = CloseHandle(file) != 0;
    if (!writeOk || !flushOk || !closeOk) { cleanup(); return Fail(error, "書き込みに失敗しました。元のJSONを保持します。"); }
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto code = GetLastError(); cleanup();
        return Fail(error, "JSONを置換できません。元のJSONを保持します。Windowsエラー " + std::to_string(code));
    }
#else
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); file.flush();
    const bool writeOk = file.good(); file.close();
    if (!writeOk || file.fail()) { cleanup(); return Fail(error, "書き込みに失敗しました。元のJSONを保持します。"); }
    std::filesystem::rename(temporary, target, ec);
    if (ec) { cleanup(); return Fail(error, "JSONを置換できません: " + ec.message()); }
#endif
    return true;
}
} // namespace

bool ValidateWeapon(const WeaponDefinition& d, std::string& error) {
    error.clear();
    if (!ValidId(d.id)) return Fail(error, "id: 1〜64文字の半角英小文字・数字・_ を使用してください。");
    if (!ValidText(d.displayNameJa, 240, false)) return Fail(error, "displayNameJa: 空でないUTF-8の表示名が必要です（240バイト以内）。");
    if (!ValidText(d.reference, 4096, true)) return Fail(error, "reference: UTF-8の参考情報が必要です（4096バイト以内）。");
    if (d.type == WeaponClass::Shooter) {
        const auto& p = d.shooter;
        if (!ValidateFields(p, ShooterFloatFields(), ShooterIntFields(), error)) return false;
        if (p.minimumDamage > p.baseDamage) return Fail(error,"params.minimumDamage: 最低威力は最大威力以下にしてください。");
        if (p.damageFalloffStart > p.damageFalloffEnd) return Fail(error,"params.damageFalloffEnd: 減衰終了は開始以降にしてください。");
        if (p.accuracyBiasMinimum > p.accuracyBiasMaximum) return Fail(error,"params.accuracyBiasMaximum: 最大偏りは最小偏り以上にしてください。");
        if (p.jumpAccuracyRecoveryStart > p.jumpAccuracyRecoveryEnd) return Fail(error,"params.jumpAccuracyRecoveryEnd: 精度回復終了は開始以降にしてください。");
        if (p.sourceImpactDepthScaleMin > p.sourceImpactDepthScaleMax || p.sourceBreakFreeDepthScaleMin > p.sourceBreakFreeDepthScaleMax)
            return Fail(error,"params: 最小伸びは最大伸び以下にしてください。");
        if (p.sourceDepthAngleMax > p.sourceDepthAngleMin) return Fail(error,"params.sourceDepthAngleMin: 最小伸びの入射角は最大伸びの入射角以上にしてください。");
        if (p.sourceDropletDepthMaxDropHeight > p.sourceDropletDepthMinDropHeight) return Fail(error,"params: 最大伸びの落差は最小伸びの落差以下にしてください。");
    } else if (d.type == WeaponClass::Stringer) {
        const auto& p = d.stringer;
        if (!ValidateFields(p, StringerFloatFields(), StringerIntFields(), error)) return false;
        if (!(p.minChargeTime < p.midChargeTime && p.midChargeTime < p.fullChargeTime))
            return Fail(error,"params: 溜め時間は 最短 < 1段階 < フル の順にしてください。");
        if (p.minInkConsume > p.midInkConsume || p.midInkConsume > p.fullInkConsume)
            return Fail(error,"params: インク消費は 最短 <= 1段階 <= フル の順にしてください。");
        if (p.enableChargeKeep) return Fail(error,"params.enableChargeKeep: チャージキープ未対応です。false を指定してください。");
    } else return Fail(error,"class: 未対応のブキクラスです。");
    return true;
}

WeaponCatalog WeaponCatalog::Defaults() {
    WeaponCatalog result;
    WeaponDefinition shooter;
    shooter.id="splattershot"; shooter.displayNameJa="スプラシューター参考";
    shooter.reference="Splatoon 3 11.3.0 / Leanny 1130 / 塗りの形と周期はCG2近似";
    WeaponDefinition stringer;
    stringer.id="tri_stringer"; stringer.displayNameJa="トライストリンガー参考"; stringer.type=WeaponClass::Stringer;
    stringer.reference="Splatoon 3 11.3.0 / Leanny 1130 / 弾道・塗り・爆発演出にはCG2近似を含む";
    result.entries_={shooter,stringer}; result.defaultId_=shooter.id;
    return result;
}
const WeaponDefinition* WeaponCatalog::Find(const std::string& id) const {
    const auto it=std::find_if(entries_.begin(),entries_.end(),[&](const auto& d){return d.id==id;});
    return it==entries_.end()?nullptr:&*it;
}
bool WeaponCatalog::Upsert(const WeaponDefinition& definition, std::string& error) {
    if (!ValidateWeapon(definition,error)) return false;
    const auto it=std::find_if(entries_.begin(),entries_.end(),[&](const auto& d){return d.id==definition.id;});
    if (it==entries_.end()) {
        if (entries_.size()>=MaxEntries) return Fail(error,"ブキの登録上限128件に達しました。");
        entries_.push_back(definition);
    } else *it=definition;
    if(defaultId_.empty()) defaultId_=definition.id;
    error.clear(); return true;
}
std::string WeaponCatalog::Clone(const std::string& sourceId,const std::string& newName,std::string& error) {
    const auto* source=Find(sourceId);
    if(!source) { Fail(error,"複製元のブキが見つかりません。"); return {}; }
    WeaponDefinition copy=*source;
    copy.displayNameJa=newName.empty()?source->displayNameJa+"のコピー":newName;
    const std::string prefix=source->type==WeaponClass::Shooter?"custom_shooter_":"custom_stringer_";
    for(size_t n=1;n<=MaxEntries+1;++n) {
        copy.id=prefix+std::to_string(n);
        if(!Find(copy.id)) { if(Upsert(copy,error)) return copy.id; return {}; }
    }
    Fail(error,"複製用のIDを作成できません。"); return {};
}
bool WeaponCatalog::Load(const std::filesystem::path& path,std::string& error) {
    error.clear();
    try {
        std::ifstream file(path,std::ios::binary);
        if(!file) return Fail(error,"JSONを開けません。現在の設定を維持します。");
        file.seekg(0,std::ios::end); const auto length=file.tellg();
        if(length<0 || length>static_cast<std::streamoff>(MaxBytes)) return Fail(error,"JSONは2MiB以内にしてください。現在の設定を維持します。");
        file.seekg(0);
        std::set<std::string> objectKeys[32];
        const auto callback=[&](int depth,Json::parse_event_t event,Json& parsed) {
            if(depth<0 || depth>=32) throw std::runtime_error("JSONの入れ子が深すぎます。");
            if(event==Json::parse_event_t::object_start) objectKeys[depth].clear();
            if(event==Json::parse_event_t::key && (depth==0 || !objectKeys[depth-1].insert(parsed.get<std::string>()).second))
                throw std::runtime_error("JSONの同じオブジェクトに重複キーがあります。");
            return true;
        };
        const Json root=Json::parse(file,callback);
        if(!root.is_object()) throw std::runtime_error("ルートはオブジェクトが必要です。");
        CheckKeys(root,{"schemaVersion","defaultWeaponId","weapons"});
        if(!root.contains("schemaVersion") || !root["schemaVersion"].is_number_integer() || root["schemaVersion"]!=1)
            throw std::runtime_error("schemaVersion: 対応している版は整数の1だけです。");
        if(!root.contains("weapons") || !root["weapons"].is_array() || root["weapons"].empty() || root["weapons"].size()>MaxEntries)
            throw std::runtime_error("weapons: 1〜128件の配列が必要です。");
        WeaponCatalog candidate;
        const std::string defaultId=RequiredText(root,"defaultWeaponId");
        size_t index=0;
        for(const auto& item:root["weapons"]) {
            try {
                if(!item.is_object()) throw std::runtime_error("ブキ定義はオブジェクトが必要です。");
                CheckKeys(item,{"id","displayNameJa","class","reference","params"});
                WeaponDefinition d;
                d.id=RequiredText(item,"id"); d.displayNameJa=RequiredText(item,"displayNameJa");
                if(item.contains("reference")) d.reference=RequiredText(item,"reference");
                const auto type=RequiredText(item,"class");
                if(type=="shooter") d.type=WeaponClass::Shooter;
                else if(type=="stringer") d.type=WeaponClass::Stringer;
                else throw std::runtime_error("class: 未対応のブキクラスです。");
                if(!item.contains("params")) throw std::runtime_error("params が必要です。");
                if(d.type==WeaponClass::Shooter) ReadParams(item["params"],d.shooter,ShooterFloatFields(),ShooterIntFields(),ShooterBoolFields());
                else ReadParams(item["params"],d.stringer,StringerFloatFields(),StringerIntFields(),StringerBoolFields());
                if(candidate.Find(d.id)) throw std::runtime_error("id: ブキIDが重複しています。");
                std::string validation;
                if(!candidate.Upsert(d,validation)) throw std::runtime_error(validation);
            } catch(const std::exception& e) { throw std::runtime_error("weapons["+std::to_string(index)+"]."+e.what()); }
            ++index;
        }
        if(!candidate.Find(defaultId)) throw std::runtime_error("defaultWeaponId: 対応するブキがありません。");
        candidate.defaultId_=defaultId;
        entries_.swap(candidate.entries_); defaultId_.swap(candidate.defaultId_);
        return true;
    } catch(const std::exception& e) { return Fail(error,std::string("読込に失敗しました。現在の設定を維持します。 ")+e.what()); }
}
bool WeaponCatalog::Save(const std::filesystem::path& path,std::string& error) const {
    error.clear();
    try {
        if(entries_.empty() || entries_.size()>MaxEntries || !Find(defaultId_)) return Fail(error,"保存できません。有効なブキと初期選択が必要です。");
        Json root={{"schemaVersion",1},{"defaultWeaponId",defaultId_},{"weapons",Json::array()}};
        std::set<std::string> ids;
        for(const auto& d:entries_) {
            if(!ValidateWeapon(d,error)) return false;
            if(!ids.insert(d.id).second) return Fail(error,"保存できません。ブキIDが重複しています。");
            Json item={{"id",d.id},{"displayNameJa",d.displayNameJa},{"class",d.type==WeaponClass::Shooter?"shooter":"stringer"},{"reference",d.reference}};
            item["params"]=d.type==WeaponClass::Shooter?WriteParams(d.shooter,ShooterFloatFields(),ShooterIntFields(),ShooterBoolFields()):
                WriteParams(d.stringer,StringerFloatFields(),StringerIntFields(),StringerBoolFields());
            root["weapons"].push_back(std::move(item));
        }
        const std::string bytes=root.dump(2)+'\n';
        if(bytes.size()>MaxBytes) return Fail(error,"保存するJSONが2MiBを超えています。");
        return WriteAtomic(path,bytes,error);
    } catch(const std::exception& e) { return Fail(error,std::string("保存に失敗しました。未保存の編集を保持します。 ")+e.what()); }
}
} // namespace ink
