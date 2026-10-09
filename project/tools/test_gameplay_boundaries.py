"""シーンの薄さ・担当の分離・Visual Studio登録を検査する。"""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

PROJECT = Path(__file__).resolve().parents[1]


class GameplayBoundaries(unittest.TestCase):
    def test_scene_stays_small(self):
        for name in ("GameScene.cpp", "GameScene.h"):
            source = (PROJECT / "game/scene" / name).read_text(encoding="utf-8-sig")
            self.assertLessEqual(len(source.splitlines()), 250, name)
        self.assertEqual({p.name for p in (PROJECT / "game/scene").glob("GameScene.*")},
                         {"GameScene.cpp", "GameScene.h"})

    def test_feature_implementations_do_not_depend_on_scene(self):
        for directory in ("session", "run/session", "editor/session", "render/session", "effects/session", "ui/session", "debug/session"):
            for path in (PROJECT / "game" / directory).rglob("*"):
                if path.suffix not in (".h", ".cpp"):
                    continue
                self.assertNotIn('#include "GameScene.h"', path.read_text(encoding="utf-8-sig"), str(path))

    def test_player_ui_state_has_its_own_owner(self):
        actor = (PROJECT / "game/player/actor/Player.h").read_text(encoding="utf-8-sig")
        ui = (PROJECT / "game/player/ui/PlayerUiState.h").read_text(encoding="utf-8-sig")
        for field in ("upgradeHudBackdropSprite_", "evolutionCircuitTankButtons_", "encyclopedia_"):
            self.assertNotIn(field, actor)
            self.assertIn(field, ui)
        self.assertLessEqual(len((PROJECT / "game/player/actor/Player.cpp").read_text(encoding="utf-8-sig").splitlines()), 2000)

    def test_visual_studio_registers_feature_sources(self):
        ns = {"ms": "http://schemas.microsoft.com/developer/msbuild/2003"}
        for filename in ("CG2_testPro.vcxproj", "CG2_testPro.vcxproj.filters"):
            xml = ET.parse(PROJECT / filename)
            for tag, suffix in (("ClCompile", ".cpp"), ("ClInclude", ".h")):
                paths = [n.attrib["Include"].replace("\\", "/") for n in xml.findall(f".//ms:{tag}", ns) if "Include" in n.attrib]
                self.assertEqual(len(paths), len(set(paths)), filename + ": duplicate registration")
                self.assertFalse(any(p.startswith("game/scene/GameScene.") and p not in
                                     ("game/scene/GameScene.cpp", "game/scene/GameScene.h") for p in paths))
                for directory in ("session", "run/session", "render/session", "render/settings", "effects/session", "ui/session",
                                  "editor/session", "debug/session", "demo", "encounter", "player/ui", "player/editor",
                                  "player/combat", "player/progression"):
                    for path in (PROJECT / "game" / directory).rglob("*" + suffix):
                        self.assertIn(path.relative_to(PROJECT).as_posix(), paths, str(path))


if __name__ == "__main__":
    unittest.main(verbosity=2)
