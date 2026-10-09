"""分離された実装を、既存の描画契約検査で読み取るための入力。"""
from pathlib import Path
import re

PROJECT = Path(__file__).resolve().parents[1]


def gameplay_source() -> str:
    paths = (
        "game/session/SessionBootstrap.cpp",
        "game/session/CombatFramePipeline.cpp",
        "game/session/GameplayQueries.cpp",
        "game/render/session/GameplayRenderer.cpp",
        "game/editor/session/GameplaySettings.cpp",
        "game/editor/session/GameplayEditor.cpp",
        "game/session/GameplayHelpers.h",
    )
    source = "\n".join((PROJECT / path).read_text(encoding="utf-8-sig") for path in paths)
    # 契約は資源への操作を調べる。共有状態の配置名には依存させない。
    return re.sub(r"world_\.(?:demo|run|resources|combat|presentation|validation)\.", "", source)
