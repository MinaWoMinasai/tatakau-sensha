"""Render submission PNGs from the actual scene and enemy class relationships.

Requires Pillow. Run from any directory; outputs go to docs/source-review-unit1.
"""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parents[2] / "docs" / "source-review-unit1"
FONT = Path("C:/Windows/Fonts/meiryo.ttc")
INK = "#172333"


class Diagram:
    def __init__(self, title, subtitle, height=1430):
        self.image = Image.new("RGB", (2200, height), "white")
        self.draw = ImageDraw.Draw(self.image)
        self.height = height
        self.text((70, 30), title, 49)
        self.text((70, 100), subtitle, 28)

    def text(self, point, text, size=29, color=INK):
        self.draw.text(point, text, font=ImageFont.truetype(str(FONT), size), fill=color)

    def box(self, rect, name, lines, abstract=False):
        x, y, w, h = rect
        self.draw.rectangle((x, y, x + w, y + h), fill="white", outline=INK, width=3)
        self.draw.rectangle((x + 2, y + 2, x + w - 2, y + 65),
                            fill="#eaf1fa" if abstract else "#f1f4f7")
        self.draw.line((x, y + 67, x + w, y + 67), fill=INK, width=2)
        name_font = ImageFont.truetype(str(FONT), 34)
        while self.draw.textlength(name, font=name_font) > w - 30:
            name_font = ImageFont.truetype(str(FONT), name_font.size - 1)
        self.draw.text((x + 15, y + 10), name, font=name_font, fill=INK)
        for i, line in enumerate(lines):
            font = ImageFont.truetype(str(FONT), 28)
            while self.draw.textlength(line, font=font) >= w - 26:
                font = ImageFont.truetype(str(FONT), font.size - 1)
            assert font.size >= 20, (name, line)
            assert self.draw.textlength(line, font=font) < w - 26, (name, line)
            assert y + 80 + i * 38 + 34 <= y + h, (name, h)
            self.draw.text((x + 13, y + 79 + i * 38), line, font=font, fill=INK)

    def path(self, points, kind="line"):
        pts = list(points)
        if kind == "composition":
            p, q = pts[:2]
            dx, dy = q[0] - p[0], q[1] - p[1]
            length = math.hypot(dx, dy)
            ux, uy = dx / length, dy / length
            px, py = -uy, ux
            diamond = [p, (p[0] + ux * 19 + px * 11, p[1] + uy * 19 + py * 11),
                       (p[0] + ux * 38, p[1] + uy * 38),
                       (p[0] + ux * 19 - px * 11, p[1] + uy * 19 - py * 11)]
            self.draw.polygon(diamond, fill=INK)
            pts[0] = diamond[2]
        self.draw.line(pts, fill=INK, width=3)
        if kind in ("inheritance", "dependency"):
            p, q = pts[-2:]
            dx, dy = q[0] - p[0], q[1] - p[1]
            length = math.hypot(dx, dy)
            ux, uy = dx / length, dy / length
            px, py = -uy, ux
            triangle = [q, (q[0] - ux * 30 + px * 17, q[1] - uy * 30 + py * 17),
                        (q[0] - ux * 30 - px * 17, q[1] - uy * 30 - py * 17)]
            if kind == "inheritance":
                self.draw.polygon(triangle, fill="white", outline=INK, width=3)
            else:
                self.draw.line((triangle[1], q, triangle[2]), fill=INK, width=3)

    def legend(self, y):
        self.path([(80, y), (235, y)], "inheritance")
        self.text((270, y - 25), "継承：白抜き三角形は基底クラス側", 29)
        self.path([(1150, y), (1310, y)], "composition")
        self.text((1340, y - 25), "コンポジション：黒菱形は所有者側", 29)
        self.text((80, y + 40), "+ public   /   - private     メンバは説明に必要なものだけを抜粋", 27)

    def save(self, filename):
        OUT.mkdir(parents=True, exist_ok=True)
        path = OUT / filename
        self.image.save(path, dpi=(160, 160))
        print(path)


def scenes():
    d = Diagram("単元1 UMLクラス図 シーン管理とFactory Method",
                "実装対応：game/scene と game/runtime   ／   IScene と AbstractSceneFactory は抽象クラス")
    d.box((760, 170, 680, 260), "SceneManager", [
        "- currentScene_: unique_ptr<IScene>",
        "- sceneFactory_: unique_ptr<AbstractSceneFactory>",
        "+ Update() / Draw()", "+ SetSceneFactory(factory): bool"])
    d.box((100, 560, 760, 275), "IScene  {abstract}", [
        "+ virtual ~IScene()", "+ Initialize() / Update() / Draw() {abstract}",
        "+ IsFinished(): bool {abstract}", "+ GetNextSceneName(): string"], True)
    d.box((1160, 560, 880, 230), "AbstractSceneFactory  {abstract}", [
        "+ virtual ~AbstractSceneFactory()",
        "+ CreateScene(name): unique_ptr<IScene> {abstract}",
        "+ ContainsScene(name): bool {abstract}"], True)
    d.box((100, 1010, 420, 195), "TitleScene", [
        "+ Update() / Draw()", "+ GetNextSceneName()"])
    d.box((570, 1010, 420, 195), "GameScene", [
        "+ Update() / Draw()", "+ GetNextSceneName()"])
    d.box((1120, 1010, 580, 195), "SceneFactory", [
        "- registry_: SceneRegistry", "+ CreateScene(name)"])
    d.box((1810, 1010, 330, 195), "SceneRegistry", [
        "+ Register(name)", "+ Create(name)"])
    d.path([(890, 430), (890, 485), (480, 485), (480, 560)], "composition")
    d.text((110, 435), "currentScene_  0..1", 28)
    d.path([(1290, 430), (1290, 485), (1600, 485), (1600, 560)], "composition")
    d.text((1510, 435), "sceneFactory_  0..1", 28)
    d.path([(310, 1010), (310, 925), (480, 925), (480, 835)], "inheritance")
    d.path([(780, 1010), (780, 925), (480, 925)])
    d.path([(1410, 1010), (1410, 900), (1600, 900), (1600, 790)], "inheritance")
    d.path([(1700, 1120), (1810, 1120)], "composition")
    d.text((1720, 1080), "1", 26)
    d.text((1080, 1225), "registry_ は値メンバ。Factoryと一緒に破棄される。", 27)
    d.legend(1320)
    d.save("01-05-scene-factory.png")


def enemy_states():
    d = Diagram("単元1 UMLクラス図 敵のStateパターン",
                "実装対応：ExpEnemyCombatCycle.h   ／   各Stateは同クラスのprivateな入れ子クラス", 1510)
    d.box((80, 200, 530, 205), "ExpEnemy", [
        "- combatCycle_: ExpEnemyCombatCycle", "+ Update(stage, deltaTime)"])
    d.box((940, 190, 930, 280), "ExpEnemyCombatCycle", [
        "- phase_: ExpEnemyCombatPhase / - remaining_: float",
        "+ Advance(dt, canTrack): bool", "- GetState(phase): const State&",
        "- Enter(phase, duration) / - Elapse(dt)"])
    d.box((800, 670, 1250, 200), "ExpEnemyCombatCycle::State  {abstract}", [
        "+ virtual ~State()",
        "+ Update(cycle, dt, canTrack): bool {abstract}"], True)
    d.path([(610, 300), (940, 300)], "composition")
    d.text((655, 242), "combatCycle_  1", 27)
    # Base-reference dispatch is a dependency; states are shared immutable
    # objects, not owned by each cycle, so this relationship is NOT a diamond.
    for start in range(470, 640, 18):
        d.draw.line((1405, start, 1405, start + 9), fill=INK, width=3)
    d.path([(1405, 640), (1405, 670)], "dependency")
    d.text((1460, 525), "const State& 経由でUpdateを呼ぶ", 28)
    positions = [80, 510, 940, 1370, 1800]
    names = ["CooldownState", "TrackingState", "LockedState", "ActiveState", "RecoveryState"]
    for x, name in zip(positions, names):
        d.box((x, 1070, 370, 160), name, ["+ Update(...) override"])
        d.path([(x + 185, 1070), (x + 185, 955), (1425, 955)])
    d.path([(1425, 955), (1425, 870)], "inheritance")
    d.text((80, 1260), "各StateのUpdateが次状態を決定する。タイマーはContextごとに保持する。", 29)
    d.text((80, 1305), "ExpEnemyMagazineCycle・BladeCycle・RivalBossCombat・DroneMissionも同じ構造。遷移時のnewは不要。", 26)
    d.legend(1400)
    d.save("01-05-enemy-state.png")


if __name__ == "__main__":
    scenes()
    enemy_states()
