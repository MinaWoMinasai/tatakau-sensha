"""Validate raw OFF/Legacy/Quality captures and run CPU/source guard regressions.

No GPU or application launch occurs. Synthetic fixtures validate the validator;
source checks do not prove runtime rendering or visual quality. Pass a real
comparison directory to validate engine PNG/JSON evidence without retouching it.
"""

import argparse
import copy
import json
import re
import tempfile
import unittest
from pathlib import Path

import test_neon_showcase_comparison as quality
from test_neon_showcase_comparison_fixtures import synthetic_png


ROOT = quality.ROOT
CASES = (("bloom_off", 0), ("legacy_bloom", 1), ("quality_bloom", 2))
BLOOM_FIELDS = ("mode", "softKnee", "scatter", "radius", "gain", "toneMappingMode")


def fixed_settings(metadata):
    fixed = copy.deepcopy(metadata)
    fixed.pop("bloomComparison", None)
    fixed["bloom"].pop("mode")
    return fixed


def validate_comparison(directory, before_json=None, after_json=None, repo=ROOT):
    directory = Path(directory)
    quality.require((before_json is None) == (after_json is None),
                    "Provide both before and after captures to verify restoration")
    model = repo / "project" / quality.MODEL
    quality.require(model.is_file() and quality.digest(model) == quality.MODEL_SHA,
                    "Original GLB hash differs")
    result = {"directory": str(directory), "modelSha256": quality.MODEL_SHA,
              "method": "Raw PNG/JSON identities and fixed recorded state; no retouching, no visual-quality score",
              "captures": []}
    reference = None
    for index, (label, mode) in enumerate(CASES):
        stem = directory / f"{index:02d}_{label}"
        metadata = quality.load_metadata(stem.with_suffix(".json"))
        png = quality.png_info(stem.with_suffix(".png"))
        quality.require(metadata["resolution"] == [png["width"], png["height"]], "PNG resolution differs")
        quality.require(metadata.get("bloomComparison") == {
            "case": index, "label": label, "count": 3, "samePausedPose": True,
            "sameCameraAndExposure": True, "sameDissolveProgress": True, "restoreModeAfterCapture": True},
            "Bloom comparison case contract differs")
        bloom = metadata["bloom"]
        quality.require(all(field in bloom for field in BLOOM_FIELDS), "Bloom settings are incomplete")
        quality.require(type(bloom["mode"]) is int and bloom["mode"] == mode, "Unexpected mode")
        quality.require(0 <= bloom["softKnee"] <= 1 and 0 <= bloom["scatter"] <= .9
                        and .25 <= bloom["radius"] <= 2 and 0 <= bloom["gain"] <= 4,
                        "Invalid new Bloom settings")
        quality.require(bloom["toneMappingMode"] in (0, 1, 2), "Unknown tone mapping")
        quality.require(bloom["diagnostic"] == 0 and bloom["taa"] is False and bloom["grayscale"] is False,
                        "Comparison must save Final color without TAA/menu grayscale")
        quality.require(metadata["animation"]["paused"] is True and metadata["camera"]["orbit"] is False,
                        "Pose/camera must be paused")
        if "dissolve" in metadata:
            quality.require(metadata["dissolve"]["playing"] is False, "Dissolve cannot progress during the batch")
        fixed = fixed_settings(metadata)
        if reference is None:
            reference = fixed
        quality.require(fixed == reference, "Bloom comparison changed non-mode settings: " + label)
        result["captures"].append({"case": index, "label": label, "png": png,
                                   "json": str(stem.with_suffix(".json")), "sources": quality.sources(metadata, repo)})
    result["fixedSettings"] = reference
    result["restoration"] = {"verified": False, "reason": "Provide original and restored raw captures; a UI intent flag is insufficient."}
    if before_json is not None:
        before, after = quality.load_metadata(Path(before_json)), quality.load_metadata(Path(after_json))
        quality.require(before == after, "Original settings were not restored exactly")
        before_png, after_png = quality.png_info(Path(before_json).with_suffix(".png")), quality.png_info(Path(after_json).with_suffix(".png"))
        quality.require(before_png["sha256"] == after_png["sha256"], "Original raw output was not restored")
        result["restoration"] = {"verified": True, "before": before_png, "after": after_png}
    result["pass"] = True
    return result


def function_body(source, signature):
    offset = source.index(signature)
    begin = source.index("{", offset)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin + 1:end - 1]


def game_freeze_body(update):
    # Check the current two independent freeze modes and title exclusion, while
    # tolerating formatting and the order of the OR operands.
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", update, flags=re.S)
    flag = r"(developerBloomFreeze_|neonBossDeveloperFreeze_)"
    guard = re.search(r"\bif\s*\(\s*\(\s*" + flag + r"\s*\|\|\s*" + flag
                      + r"\s*\)\s*&&\s*!\s*titleDemo_\s*\)\s*\{", code)
    quality.require(guard is not None and set(guard.groups()) == {
        "developerBloomFreeze_", "neonBossDeveloperFreeze_"},
        "Either comparison freeze must stop gameplay outside the title demo")
    gameplay = re.search(r"\b(?:UpdateTitleDemo|UpdateTankRun)\s*\(", code)
    quality.require(gameplay is not None and guard.start() < gameplay.start(),
                    "Comparison freeze must precede gameplay updates")
    frozen = function_body(code, guard.group())
    quality.require(re.search(r"(?:^|[;}])\s*return\s*;\s*$", frozen) is not None,
                    "Frozen update must end with an unconditional early return")
    quality.require(re.search(r"\b(?:UpdateTitleDemo|UpdateTankRun)\s*\(", frozen) is None,
                    "Frozen update must not advance gameplay")
    return frozen


def validate_game_comparison(files):
    quality.require(len(files) == 3, "Provide game OFF, Legacy, Quality JSONs in that order")
    result = {"method": "Manual frozen gameplay captures; exact recorded camera/actor/trail/appearance/post metadata; no retouching or visual-quality score",
              "captures": []}
    reference = None
    for mode, file in enumerate(files):
        file = Path(file)
        metadata = json.loads(file.read_text(encoding="utf-8-sig"))
        quality.finite(metadata, str(file))
        quality.require(metadata.get("build") == "Development" and metadata.get("scene") == "GameScene",
                        "Game capture is not from Development GameScene")
        quality.require(metadata.get("comparisonFreeze") is True and metadata.get("globalPostAvailable") is True,
                        "Game must be frozen and actual composite parameters recorded")
        quality.require(metadata.get("samePoseBatch") is False, "Game capture must identify its manual comparison method")
        post = metadata["globalPost"]
        quality.require(metadata["bloomOverrideMode"] == mode and post["mode"] == mode, "Actual global Bloom mode differs")
        quality.require(post["taa"] == 0 and post["jitter"] == 0, "Frozen game comparison must disable temporal accumulation/jitter")
        png = quality.png_info(file.with_suffix(".png"))
        quality.require(metadata["resolution"] == [png["width"], png["height"]], "PNG resolution differs")
        fixed = copy.deepcopy(metadata)
        fixed.pop("bloomOverrideMode")
        fixed["globalPost"].pop("mode")
        if reference is None:
            reference = fixed
        quality.require(fixed == reference, "Frozen game comparison changed camera/source/appearance/exposure/post settings")
        result["captures"].append({"mode": mode, "png": png, "json": str(file)})
    result["fixedSettings"] = reference
    result["pass"] = True
    return result


class PreviewSourceContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.preview = (ROOT / "project/game/debug/NeonSkinnedPreview.cpp").read_text(encoding="utf-8")
        cls.game = (ROOT / "project/game/scene/GameScene.cpp").read_text(encoding="utf-8")

    def test_fixed_pose_guards_and_only_post_mode_changes(self):
        start = function_body(self.preview, "void NeonSkinnedPreview::StartBloomComparison()")
        for guard in ("!model_->IsAnimationPaused()", "showcaseOrbit_", "dissolve_.IsPlaying()",
                      "IsCaptureActive()", "showcaseCapture_.IsBusy()", "showcaseSequence_", "dissolveSequence_",
                      "showcaseComparisonActive_", "dissolveComparisonActive_"):
            self.assertIn(guard, start)
        for forbidden in ("SelectAnimation", "SeekCurrentAnimation", "SetAnimationPlaying", "SelectQualityCandidate",
                          "UpdateShowcaseCamera", "ApplyRecommended", "params_ =", "TriggerDissolve", "ResetDissolve"):
            self.assertNotIn(forbidden, start)
        finish = function_body(self.preview, "void NeonSkinnedPreview::FinishBloomComparison(")
        self.assertIn("showcaseBloomMode_ = bloomComparisonOriginalMode_", finish)
        self.assertIn("showcaseDiagnostic_ = bloomComparisonOriginalDiagnostic_", finish)
        self.assertIn("FinishBloomComparison", function_body(self.preview, "void NeonSkinnedPreview::LeaveShowcase()"))

    def test_recommended_bloom_presentation_only_changes_bloom_settings(self):
        preset = function_body(self.preview, "void NeonSkinnedPreview::ApplyRecommendedBloomPresentation()")
        expected = {"Mode": "2", "Threshold": "0.65f", "SoftKnee": "0.5f",
                    "Scatter": "0.55f", "Radius": "1.0f", "Gain": "0.8f"}
        assignments = dict(re.findall(r"showcaseBloom(\w+)_\s*=\s*([^;]+);", preset))
        self.assertEqual(assignments, expected)
        self.assertEqual(len(re.findall(r"\w+\s*=", preset)), len(expected))
        self.assertIn('Button("Recommended Bloom presentation")', self.preview)
        header = (ROOT / "project/game/debug/NeonSkinnedPreview.h").read_text(encoding="utf-8")
        self.assertIn("showcaseBloomScatter_ = 0.55f", header)
        self.assertIn("showcaseBloomRadius_ = 1.0f", header)
        self.assertIn("showcaseBloomGain_ = 0.8f", header)

    def test_global_visible_preset_is_used_at_initialize_and_reset_without_changing_generic_defaults(self):
        bloom = (ROOT / "project/DirectX/engine/postEffect/Bloom.cpp").read_text(encoding="utf-8")
        preset = function_body(bloom, "void ApplyVisibleNeonBloomPreset(BloomParam& param)")
        assignments = dict(re.findall(r"param\.(\w+)\s*=\s*([^;]+);", preset))
        self.assertEqual(assignments, {"bloomMode": "2", "threshold": "0.65f", "bloomSoftKnee": "0.5f",
                                       "bloomScatter": "0.55f", "bloomRadius": "0.9f", "bloomGain": "0.55f"})
        self.assertIn("ApplyVisibleNeonBloomPreset(bloomParam_)", function_body(bloom, "void Bloom::Initialize("))
        update = function_body(bloom, "void Bloom::Update()")
        self.assertRegex(update, r'Button\("Reset"\)\)\s*\{\s*ApplyVisibleNeonBloomPreset\(bloomParam_\)')
        self.assertIn('Button("Recommended visible neon Bloom")', update)
        generic = (ROOT / "project/DirectX/engine/struct/Struct.h").read_text(encoding="utf-8")
        for field, value in (("bloomScatter", "0.3"), ("bloomRadius", "0.75"), ("bloomGain", "0.15")):
            self.assertRegex(generic, rf"float {field}\s*=\s*{re.escape(value)}f")

    def test_metadata_and_actual_pre_imgui_fence_capture_remain(self):
        metadata = function_body(self.preview, "nlohmann::json NeonSkinnedPreview::MakeShowcaseMetadata() const")
        for key in BLOOM_FIELDS:
            self.assertIn('"' + key + '"', metadata)
        capture = (ROOT / "project/game/debug/NeonShowcaseCapture.cpp").read_text(encoding="utf-8")
        self.assertIn("GetCompletedValue() < fence_", capture)
        self.assertIn("CopyTextureRegion", capture)
        game_draw = (ROOT / "project/game/scene/Game.cpp").read_text(encoding="utf-8")
        self.assertLess(game_draw.index("RecordDeveloperFrame"), game_draw.index("ImGui_ImplDX12_RenderDrawData"))
        self.assertIn("StartCapture(path.generic_string(),300,60)", self.preview)

    def test_category_gain_is_distinct_finite_persistent_and_no_automatic_save(self):
        write = function_body(self.game, "nlohmann::json WriteBloomParamJson(")
        read = function_body(self.game, "void ReadBloomParamJson(")
        for field in ("bloomMode", "bloomSoftKnee", "bloomScatter", "bloomRadius", "bloomGain"):
            self.assertIn('"' + field + '"', write)
            self.assertIn('"' + field + '"', read)
        self.assertIn("std::isfinite(value)", read)
        for name, gain, scatter, radius in (
                ("playerPost", .38, .5, .9), ("enemyPost", .38, .5, .9), ("expEnemyPost", .38, .5, .9),
                ("objectBloomPost", .3, .5, .9), ("stagePost", .3, .5, .9), ("gridPost", .48, .55, .9),
                ("bulletTrailPost", 1.2, .55, 1.0), ("particlePost", .65, .5, .9)):
            for field, expected in (("bloomGain", gain), ("bloomScatter", scatter), ("bloomRadius", radius)):
                values = re.findall(rf"{name}\.{field}\s*=\s*([0-9.]+)f", self.game)
                self.assertEqual([float(value) for value in values], [expected], name + "." + field)
        for field in ("bloomSoftKnee", "bloomScatter", "bloomRadius", "bloomGain"):
            self.assertIn(f'readBloomSetting("{field}", param.{field},', read)
        self.assertNotIn("SaveGamePostEffectConfig", function_body(self.game, "void GameScene::Initialize()"))

    def test_release_and_title_do_not_activate_preview_or_comparison(self):
        header = (ROOT / "project/game/debug/NeonSkinnedPreview.h").read_text(encoding="utf-8")
        self.assertIn("#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)", header)
        self.assertIn("bool enabled_ = false", header)
        state = function_body(self.game, "IScene::DeveloperShowcaseState GameScene::GetDeveloperShowcaseState()")
        self.assertIn("#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)", state)
        self.assertIn("else if (!titleDemo_)", state)
        scene_header = (ROOT / "project/game/scene/GameScene.h").read_text(encoding="utf-8")
        self.assertIn("developerBloomComparisonMode_ = -1", scene_header)
        text = (ROOT / "project/game/ui/NeonTextEffect.cpp").read_text(encoding="utf-8")
        self.assertIn("param.bloomGain = qualityGain", text)
        self.assertIn("EndCaptureBloomOnlyToBackBuffer", text)

    def test_game_freeze_keeps_draw_and_actual_fence_capture_metadata(self):
        update = function_body(self.game, "void GameScene::Update()")
        self.assertIn("developerGameCapture_.Resolve", update)
        frozen = game_freeze_body(update)
        self.assertIn("DrawGameSceneDebugImGui()", frozen)
        self.assertIn("Update(0.0f)", frozen)
        record = function_body(self.game, "void GameScene::RecordDeveloperFrame(")
        self.assertIn("MakeDeveloperGameCaptureMetadata(dx)", record)
        self.assertIn("developerGameCapture_.Record(dx)", record)
        metadata = function_body(self.game, "nlohmann::json GameScene::MakeDeveloperGameCaptureMetadata(")
        for key in ("globalPostAvailable", "comparisonFreeze", "sourcePositions", "sourceCounts", "localCategories", "visualAppearance"):
            self.assertIn('"' + key + '"', metadata)
        self.assertIn("developerCompositeParams_", metadata)
        self.assertIn("1.0f // Quality/OFF preserve projectile cores", self.game)

    def test_game_freeze_source_rejects_missing_gates_or_gameplay(self):
        guard = "if ((developerBloomFreeze_ || neonBossDeveloperFreeze_) && !titleDemo_)"
        fixture = guard + " { DrawGameSceneDebugImGui(); preview->Update(0.0f); return; }\nUpdateTitleDemo(dt); UpdateTankRun(dt);"
        self.assertIn("return;", game_freeze_body(fixture))
        self.assertIn("return;", game_freeze_body(fixture.replace(
            "developerBloomFreeze_ || neonBossDeveloperFreeze_",
            "neonBossDeveloperFreeze_\n || developerBloomFreeze_")))
        invalid = (
            fixture.replace(" || neonBossDeveloperFreeze_", ""),
            fixture.replace("developerBloomFreeze_", "neonBossDeveloperFreeze_"),
            fixture.replace(" || ", " && "),
            fixture.replace(" && !titleDemo_", ""),
            fixture.replace("return;", ""),
            fixture.replace("return;", "if (paused) return;"),
            fixture.replace("return;", "if (paused) { return; }"),
            fixture.replace("return;", "UpdateTankRun(dt); return;"),
            "UpdateTankRun(dt);\n" + fixture,
        )
        for source in invalid:
            with self.subTest(source=source), self.assertRaises(ValueError):
                game_freeze_body(source)


class ValidatorFixtures(unittest.TestCase):
    def setUp(self):
        generated = ROOT / "generated/bloom_presentation_tests"
        generated.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="comparison_fixture_", dir=generated)
        self.directory = Path(self.temp.name)
        self.files = []
        for index, (label, mode) in enumerate(CASES):
            metadata = {"schemaVersion": 1, "model": quality.MODEL, "expectedModelSha256": quality.MODEL_SHA,
                        "build": "Development", "resolution": [2, 2], "mode": "Neon",
                        "animation": {"clip": "Preview_Idle", "time": .4, "paused": True},
                        "camera": {"position": [0, 1, -2], "orbit": False}, "submeshes": [],
                        "bloom": {"mode": mode, "softKnee": .5, "scatter": .55, "radius": 1.0, "gain": .8,
                                  "toneMappingMode": 1, "diagnostic": 0, "taa": False, "grayscale": False, "exposure": .75},
                        "bloomComparison": {"case": index, "label": label, "count": 3, "samePausedPose": True,
                                            "sameCameraAndExposure": True, "sameDissolveProgress": True, "restoreModeAfterCapture": True}}
            path = self.directory / f"{index:02d}_{label}.json"
            path.write_text(json.dumps(metadata), encoding="utf-8")
            path.with_suffix(".png").write_bytes(synthetic_png(red=80 + index))
            self.files.append(path)

    def tearDown(self):
        self.temp.cleanup()

    def mutate(self, key, value):
        metadata = json.loads(self.files[2].read_text(encoding="utf-8"))
        metadata[key[0]][key[1]] = value
        self.files[2].write_text(json.dumps(metadata), encoding="utf-8")

    def test_synthetic_valid_batch_is_labelled_as_metadata_evidence(self):
        result = validate_comparison(self.directory)
        self.assertTrue(result["pass"])
        self.assertFalse(result["restoration"]["verified"])
        self.assertIn("no visual-quality score", result["method"])

    def test_reject_changed_pose_camera_exposure_and_tone(self):
        for key, value in ((["animation", "time"], .8), (["camera", "orbit"], True),
                           (["bloom", "exposure"], .8), (["bloom", "toneMappingMode"], 2)):
            original = self.files[2].read_text(encoding="utf-8")
            self.mutate(key, value)
            with self.assertRaises(ValueError):
                validate_comparison(self.directory)
            self.files[2].write_text(original, encoding="utf-8")

    def test_reject_nonfinite_or_invalid_settings(self):
        for key, value in (("gain", float("nan")), ("radius", 0), ("softKnee", 2), ("mode", 1)):
            original = self.files[2].read_text(encoding="utf-8")
            self.mutate(["bloom", key], value)
            with self.assertRaises(ValueError):
                validate_comparison(self.directory)
            self.files[2].write_text(original, encoding="utf-8")

    def test_restore_requires_both_and_byte_identical_original(self):
        with self.assertRaises(ValueError):
            validate_comparison(self.directory, before_json=self.files[2])
        self.assertTrue(validate_comparison(self.directory, self.files[2], self.files[2])["restoration"]["verified"])
        altered = self.directory / "altered.json"
        altered.write_bytes(self.files[2].read_bytes())
        altered.with_suffix(".png").write_bytes(synthetic_png(red=1))
        with self.assertRaises(ValueError):
            validate_comparison(self.directory, self.files[2], altered)

    def test_manual_game_validator_rejects_live_or_mismatched_sources(self):
        game_files = []
        for mode in range(3):
            path = self.directory / f"game_{mode}.json"
            value = {"build": "Development", "scene": "GameScene", "comparisonFreeze": True,
                     "globalPostAvailable": True, "samePoseBatch": False, "bloomOverrideMode": mode,
                     "globalPost": {"mode": mode, "taa": 0, "jitter": 0, "exposure": 1},
                     "resolution": [2, 2], "sourcePositions": {"bullets": [[1, 2, 0]]}}
            path.write_text(json.dumps(value), encoding="utf-8")
            path.with_suffix(".png").write_bytes(synthetic_png(red=80 + mode))
            game_files.append(path)
        self.assertTrue(validate_game_comparison(game_files)["pass"])
        original = game_files[2].read_text(encoding="utf-8")
        for field, value in (("comparisonFreeze", False), ("globalPostAvailable", False),
                             ("sourcePositions", {"bullets": [[4, 5, 0]]})):
            altered = json.loads(original)
            altered[field] = value
            game_files[2].write_text(json.dumps(altered), encoding="utf-8")
            with self.assertRaises(ValueError):
                validate_game_comparison(game_files)
        game_files[2].write_text(original, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", type=Path)
    parser.add_argument("--before-json", type=Path)
    parser.add_argument("--after-json", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--game-json", nargs=3, type=Path, metavar=("OFF", "LEGACY", "QUALITY"))
    args = parser.parse_args()
    if args.directory is None and args.game_json is None:
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromModule(__import__(__name__)))
        raise SystemExit(not result.wasSuccessful())
    result = validate_game_comparison(args.game_json) if args.game_json else validate_comparison(args.directory, args.before_json, args.after_json)
    text = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text)


if __name__ == "__main__":
    main()
