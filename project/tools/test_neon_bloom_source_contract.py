"""CPU-only integration wiring checks; actual pixel behavior is tested on D3D12.

This supplements the GPU tests with the GameScene capture-resolution contract.
It does not claim that source inspection proves visual sharpness or GPU state.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "project"


def source(relative: str) -> str:
    return (PROJECT / relative).read_text(encoding="utf-8-sig")


def compact(value: str) -> str:
    value = re.sub(r"/\*.*?\*/|//[^\n]*", "", value, flags=re.S)
    return re.sub(r"\s+", "", value)


def function(value: str, signature: str) -> str:
    start = value.index(signature)
    opening = value.index("{", start)
    level = 1
    for position in range(opening + 1, len(value)):
        level += (value[position] == "{") - (value[position] == "}")
        if level == 0:
            return compact(value[opening + 1:position])
    raise AssertionError(f"Unbalanced function: {signature}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "generated/neon_bloom/tests/model_animation/source_contract.json")
    args = parser.parse_args()
    files = {
        "objectHeader": "DirectX/engine/postEffect/ObjectPostEffect.h",
        "objectSource": "DirectX/engine/postEffect/ObjectPostEffect.cpp",
        "constantBuffer": "DirectX/engine/postEffect/BloomConstantBuffer.cpp",
        "pyramid": "DirectX/engine/postEffect/BloomPyramid.cpp",
        "outline": "resources/shaders/ObjectPostOutlineAdd.PS.hlsl",
        "bloomAdd": "resources/shaders/ObjectPostBloomAdd.PS.hlsl",
        "common": "resources/shaders/PostEffectCommon.hlsli",
        "game": "game/scene/GameScene.cpp",
        "bullet": "game/player/actor/Bullet.cpp",
        "projectileHead": "game/render/NeonProjectileRenderer.cpp",
        "winApp": "DirectX/engine/commom/WinApp.h",
    }
    text = {key: source(path) for key, path in files.items()}
    checks: list[str] = []

    def require(condition: bool, label: str) -> None:
        assert condition, label
        checks.append(label)

    header = compact(text["objectHeader"])
    require("floatrenderScale=1.0f,floatqualityCaptureScale=0.0f" in header,
            "Existing callers default to their original capture scale")
    init = function(text["objectSource"], "void ObjectPostEffect::Initialize(")
    require("qualityCaptureScale>0" in init and ":safeScale" in init,
            "Unspecified quality scale uses the legacy source dimensions")
    require("if(qualityWidth_!=renderWidth_||qualityHeight_!=renderHeight_){qualityObjectRT_=std::make_unique<RenderTexture>()" in init,
            "Default equal dimensions do not allocate a duplicate quality capture/pyramid")
    select = function(text["objectSource"], "void ObjectPostEffect::SelectCaptureTarget()")
    require("qualityObjectRT_&&BloomPyramid::EffectiveMode(param_)!=1" in select,
            "Legacy selects original source; OFF/Quality/Light select optional full-resolution source")
    require("activeWidth_=useQualitySource?qualityWidth_:renderWidth_" in select and
            "activeHeight_=useQualitySource?qualityHeight_:renderHeight_" in select,
            "Active viewport dimensions follow the selected source")
    for name in ("BeginCapture()", "BeginCaptureWithCurrentDepth()"):
        body = function(text["objectSource"], "void ObjectPostEffect::" + name)
        require("SelectCaptureTarget()" in body and "SetViewport(activeWidth_,activeHeight_)" in body,
                name + " binds the selected source at its native resolution")
    finish = function(text["objectSource"], "void ObjectPostEffect::FinishCapture(")
    require("DrawObjectComposite(activeCaptureRT_->GetGPUHandle()" in finish and
            "DrawObjectOutlineAdd(activeCaptureRT_->GetGPUHandle()" in finish,
            "Sharp composition uses the selected unfiltered source")
    require("if(drawParam.bloomMode!=1&&drawParam.outlineWidth<=0&&drawParam.outlineBloomIntensity<=0)" in finish and
            "drawUnshiftedBloom();return;" in finish,
            "New CompositeAndAdd no-outline path adds filtered glow rather than duplicating its sharp core")
    normal_add = function(text["objectSource"], "const auto drawUnshiftedBloom =")
    require("BloomParambloomAddParam=drawParam" in normal_add and
            "bloomAddParam.boxBlurRadius=0.0f" in normal_add and
            "bloomAddParam.fullScreenBoxBlurBlend=0.0f" in normal_add and
            "cb_->Update(bloomAddParam);postEffect_->DrawObjectBloomAdd" in normal_add,
            "Normal bloom addition uses an immutable zero-UV-offset copy of the draw parameters")
    require("if(mode==FinishMode::BloomOnly){drawUnshiftedBloom();return;}" in finish and
            finish.count("drawUnshiftedBloom();") == 2,
            "BloomOnly and new CompositeAndAdd fallback both use unshifted bloom")
    cached = function(text["objectSource"], "void ObjectPostEffect::DrawCachedBloom(")
    require("drawParam.boxBlurRadius=uvOffset.x" in cached and
            "drawParam.fullScreenBoxBlurBlend=uvOffset.y" in cached and
            "cb_->Update(drawParam);postEffect_->DrawObjectBloomAdd" in cached,
            "Cached bloom preserves explicit per-draw UV offsets in its own immutable snapshot")
    require("input.uv-float2(boxBlurRadius,fullScreenBoxBlurBlend)" in compact(text["bloomAdd"]),
            "Object bloom-add shader offset matches the tested C++ snapshot contract")
    draw = function(text["objectSource"], "BloomParam ObjectPostEffect::MakeDrawParam(")
    require("if(drawParam.bloomMode==0)drawParam.outlineBloomIntensity=0" in draw,
            "OFF removes only glow, retaining the source and outline")
    outline = compact(text["outline"])
    require("?objectColor:float3(0.0f,0.0f,0.0f)" in outline and
            "float3addColor=baseAdd+bloomColor+outlineColor*addMask" in outline,
            "Additive-only zero-glow rendering retains original core pixels")
    common = function(text["common"], "float BloomCompositeGain()")
    require("bloomMode==0?0.0f" in common,
            "OFF contributes zero filtered bloom gain")
    for name in ("BeginCapture()", "BeginCaptureWithCurrentDepth()", "FinishCapture(", "RenderBloom("):
        signature = "uint32_t ObjectPostEffect::" if name == "RenderBloom(" else "void ObjectPostEffect::"
        body = function(text["objectSource"], signature + name)
        require("make_unique" not in body and "CreateCommittedResource" not in body,
                name + " does not allocate or resize capture textures")
    game = compact(text["game"])
    gpu_snapshot = function(text["constantBuffer"], "void BloomConstantBuffer::Update(")
    require('param.bloomGain=readBloomSetting("bloomGain",param.bloomGain,0,4)' in game and
            "safe.bloomGain=finiteRange(safe.bloomGain,0.15f,4.0f)" in gpu_snapshot,
            "Persisted category bloom gain and GPU snapshot use the same maximum of four")
    require(re.search(r"bulletTrailPostEffect_->Initialize\([^;]*,nullptr,0\.5f,1\.0f\)", game) is not None,
            "Game projectile capture preserves Legacy 0.5 scale and selects Quality 1.0 scale")
    require("kClientWidth=1280" in compact(text["winApp"]) and "kClientHeight=720" in compact(text["winApp"]),
            "Current fixed scene is 1280x720: legacy capture640x360, quality capture1280x720")
    require(function(text["bullet"], "void Bullet::Draw()") == "",
            "Ordinary projectile heads remain a separate presentation batch; Bullet::Draw is unchanged")
    initialize = function(text["game"], "void GameScene::Initialize()")
    require("neonProjectileRenderer_=std::make_unique<NeonProjectileRenderer>()" in initialize and
            "neonProjectileRenderer_->Initialize(" in initialize,
            "Game initializes its independent projectile-head renderer once")
    scene_draw = function(text["game"], "void GameScene::DrawPostEffect3D()")
    prepare = "neonProjectileRenderer_->BeginFrame(bulletManager_->GetBulletPtrs(),forward)"
    require(scene_draw.count(prepare) == 1 and
            scene_draw.index(prepare) < scene_draw.index("if(useBulletTrailPost&&hasTrailContent)"),
            "Head geometry prepares once before choosing captured or direct Trail drawing")
    require("hasTrailContent=bulletManager_->GetBulletCount()!=0||bulletManager_->HasDrawableTrails()" in scene_draw,
            "A first-frame bullet head activates the capture even before its trail has history")
    capture_branch = function(text["game"], 'profile("Trail Post", true,')
    direct_branch = function(text["game"], 'profile("Trail Draw", false,')
    for name, branch in (("captured", capture_branch), ("direct", direct_branch)):
        require(branch.count("neonProjectileRenderer_->Draw(vp)") == 1 and
                branch.index("bulletManager_->DrawTrails(vp)") < branch.index("neonProjectileRenderer_->Draw(vp)"),
                name + " Trail pass draws the head batch once beside existing ribbons")
        require(branch.index("neonProjectileRenderer_->Draw(vp)") <
                branch.index("Object3dCommon::GetInstance()->PreDraw(cg2::kNormal)"),
                name + " Trail pass rebinds the following normal renderer state")
    require(capture_branch.index("BeginCapture()") < capture_branch.index("neonProjectileRenderer_->Draw(vp)") <
            capture_branch.index("EndCaptureAdditiveOnly()"),
            "Projectile heads use the existing HDR Trail bloom capture")

    model_path = "project/resources/models/neon_hologram/AvatarSample_B.glb"
    current_model = (ROOT / model_path).read_bytes()
    head_model = subprocess.check_output(["git", "show", "HEAD:" + model_path], cwd=ROOT)
    model_hash = hashlib.sha256(current_model).hexdigest().upper()
    require(model_hash == hashlib.sha256(head_model).hexdigest().upper(),
            "AvatarSample_B bytes match unchanged HEAD model")
    report = {
        "passed": True, "scope": "CPU source wiring and model integrity; GPU pixel checks are separate",
        "checks": checks, "modelSha256": model_hash,
        "sourceSha256": {key: hashlib.sha256((PROJECT / path).read_bytes()).hexdigest() for key, path in files.items()},
        "dimensionsFromCurrentSourceContract": {"legacyCapture": [640, 360], "legacyBloom": [320, 180],
                                                "qualityCapture": [1280, 720], "qualityBloom": [640, 360]},
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: {len(checks)} CPU source-contract checks; GLB {model_hash}")
    print(args.output)


if __name__ == "__main__":
    main()
