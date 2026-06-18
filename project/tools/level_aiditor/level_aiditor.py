from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk


SUPPORTED_TYPES = {
    "PlayerSpawn": {"Default"},
    "BossSpawn": {"Default"},
    "Enemy": {"Default", "Basic", "Square", "Triangle", "Pentagon", "Shooter"},
    "SpawnArea": {"Default", "Basic", "Square", "Triangle", "Pentagon", "Shooter"},
    "Obstacle": {"Wall", "DamageBlock"},
    "Item": {"Default", "Heal", "Power", "Exp"},
}


def find_project_root() -> Path:
    path = Path(__file__).resolve()
    for parent in path.parents:
        if (parent / "CG2_testPro.vcxproj").exists():
            return parent
    return path.parents[2]


PROJECT_ROOT = find_project_root()
DEFAULT_LEVEL = PROJECT_ROOT / "resources" / "levels" / "level_test.json"
DEFAULT_HANDOFF = PROJECT_ROOT / "resources" / "levels" / "ai_balance_handoff.md"


@dataclass
class ValidationIssue:
    severity: str
    message: str


def vector(obj: dict, key: str) -> dict:
    value = obj.get(key, {})
    return value if isinstance(value, dict) else {}


def number(value, fallback: float = 0.0) -> float:
    return float(value) if isinstance(value, (int, float)) else fallback


def distance_xy(a: dict, b: dict) -> float:
    ax = number(a.get("x"))
    ay = number(a.get("y"))
    bx = number(b.get("x"))
    by = number(b.get("y"))
    return math.hypot(ax - bx, ay - by)


def object_label(obj: dict) -> str:
    name = obj.get("name", "<unnamed>")
    obj_type = obj.get("type", "<no type>")
    prefab = obj.get("prefab", "<no prefab>")
    return f"{name}  [{obj_type}/{prefab}]"


class LevelAIDitorApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("Level AI-ditor External")
        self.root.geometry("1180x760")

        self.level_path = DEFAULT_LEVEL
        self.level_data: dict = {}
        self.balance_paths: dict[str, tuple[str, ...]] = {}
        self.selected_balance_key = tk.StringVar()
        self.selected_balance_value = tk.StringVar()
        self.status_text = tk.StringVar(value="Ready")

        self._build_ui()
        self.load_level(self.level_path)

    def _build_ui(self) -> None:
        toolbar = ttk.Frame(self.root, padding=6)
        toolbar.pack(fill=tk.X)

        ttk.Button(toolbar, text="Open JSON", command=self.open_level).pack(side=tk.LEFT)
        ttk.Button(toolbar, text="Save JSON", command=self.save_level).pack(side=tk.LEFT, padx=(6, 0))
        ttk.Button(toolbar, text="Validate", command=self.refresh_all).pack(side=tk.LEFT, padx=(6, 0))
        ttk.Button(toolbar, text="AI Handoff MD", command=self.write_ai_handoff).pack(side=tk.LEFT, padx=(6, 0))
        ttk.Label(toolbar, textvariable=self.status_text).pack(side=tk.LEFT, padx=12)

        main = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        main.pack(fill=tk.BOTH, expand=True)

        left = ttk.PanedWindow(main, orient=tk.VERTICAL)
        main.add(left, weight=1)

        object_frame = ttk.LabelFrame(left, text="Level Objects / Spawn Areas / Boss Phases", padding=6)
        left.add(object_frame, weight=2)
        self.object_tree = ttk.Treeview(object_frame, columns=("type", "prefab", "pos"), show="tree headings")
        self.object_tree.heading("#0", text="Name")
        self.object_tree.heading("type", text="Type")
        self.object_tree.heading("prefab", text="Prefab")
        self.object_tree.heading("pos", text="Position / Trigger")
        self.object_tree.column("#0", width=260)
        self.object_tree.column("type", width=90)
        self.object_tree.column("prefab", width=90)
        self.object_tree.column("pos", width=160)
        self.object_tree.pack(fill=tk.BOTH, expand=True)

        validation_frame = ttk.LabelFrame(left, text="Validation Report", padding=6)
        left.add(validation_frame, weight=1)
        self.validation_list = tk.Listbox(validation_frame)
        self.validation_list.pack(fill=tk.BOTH, expand=True)

        right = ttk.PanedWindow(main, orient=tk.VERTICAL)
        main.add(right, weight=2)

        balance_frame = ttk.LabelFrame(right, text="Balance Editor", padding=6)
        right.add(balance_frame, weight=1)

        self.balance_tree = ttk.Treeview(balance_frame, columns=("path", "value"), show="headings")
        self.balance_tree.heading("path", text="balance path")
        self.balance_tree.heading("value", text="value")
        self.balance_tree.column("path", width=360)
        self.balance_tree.column("value", width=160)
        self.balance_tree.bind("<<TreeviewSelect>>", self.on_balance_select)
        self.balance_tree.pack(fill=tk.BOTH, expand=True)

        edit_row = ttk.Frame(balance_frame)
        edit_row.pack(fill=tk.X, pady=(6, 0))
        ttk.Label(edit_row, textvariable=self.selected_balance_key, width=44).pack(side=tk.LEFT)
        ttk.Entry(edit_row, textvariable=self.selected_balance_value).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(edit_row, text="Apply Value", command=self.apply_balance_value).pack(side=tk.LEFT, padx=(6, 0))

        json_frame = ttk.LabelFrame(right, text="Raw JSON", padding=6)
        right.add(json_frame, weight=2)
        self.json_text = tk.Text(json_frame, wrap=tk.NONE, undo=True)
        self.json_text.pack(fill=tk.BOTH, expand=True)
        raw_buttons = ttk.Frame(json_frame)
        raw_buttons.pack(fill=tk.X, pady=(6, 0))
        ttk.Button(raw_buttons, text="Apply Raw JSON To Editor", command=self.apply_raw_json).pack(side=tk.LEFT)
        ttk.Button(raw_buttons, text="Format Raw JSON", command=self.refresh_json_text).pack(side=tk.LEFT, padx=(6, 0))

    def open_level(self) -> None:
        path = filedialog.askopenfilename(
            title="Open level JSON",
            initialdir=str(self.level_path.parent),
            filetypes=(("JSON files", "*.json"), ("All files", "*.*")),
        )
        if path:
            self.load_level(Path(path))

    def load_level(self, path: Path) -> None:
        try:
            self.level_data = json.loads(path.read_text(encoding="utf-8"))
            self.level_path = path
        except Exception as exc:
            messagebox.showerror("Load failed", str(exc))
            return
        self.status_text.set(f"Loaded: {self.level_path}")
        self.refresh_all()

    def save_level(self) -> None:
        if not self.apply_raw_json(show_success=False):
            return
        try:
            self.level_path.write_text(json.dumps(self.level_data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        except Exception as exc:
            messagebox.showerror("Save failed", str(exc))
            return
        self.status_text.set(f"Saved: {self.level_path}")
        self.refresh_all()

    def refresh_all(self) -> None:
        self.refresh_object_tree()
        self.refresh_balance_tree()
        self.refresh_validation()
        self.refresh_json_text()

    def refresh_object_tree(self) -> None:
        self.object_tree.delete(*self.object_tree.get_children())

        root_objects = self.object_tree.insert("", tk.END, text="objects", values=("", "", ""))
        for obj in self.level_data.get("objects", []):
            if not isinstance(obj, dict):
                continue
            pos = vector(obj, "position")
            pos_text = f"{pos.get('x', 0)}, {pos.get('y', 0)}, {pos.get('z', 0)}"
            self.object_tree.insert(
                root_objects,
                tk.END,
                text=obj.get("name", "<unnamed>"),
                values=(obj.get("type", ""), obj.get("prefab", ""), pos_text),
            )

        root_areas = self.object_tree.insert("", tk.END, text="spawnAreas", values=("", "", ""))
        for area in self.level_data.get("spawnAreas", []):
            if not isinstance(area, dict):
                continue
            center = vector(area, "center")
            size = vector(area, "size")
            pos_text = f"center {center.get('x', 0)}, {center.get('y', 0)} / size {size.get('x', 0)}x{size.get('y', 0)}"
            self.object_tree.insert(root_areas, tk.END, text=area.get("name", "<unnamed>"), values=("SpawnArea", area.get("prefab", ""), pos_text))

        root_phases = self.object_tree.insert("", tk.END, text="bossPhases", values=("", "", ""))
        for phase in self.level_data.get("bossPhases", []):
            if not isinstance(phase, dict):
                continue
            phase_id = self.object_tree.insert(
                root_phases,
                tk.END,
                text=phase.get("name", "<unnamed>"),
                values=("BossPhase", "", f"HP <= {phase.get('startHpRate', 1.0)}"),
            )
            for obj in phase.get("objects", []):
                if not isinstance(obj, dict):
                    continue
                pos = vector(obj, "position")
                pos_text = f"{pos.get('x', 0)}, {pos.get('y', 0)}, {pos.get('z', 0)}"
                self.object_tree.insert(phase_id, tk.END, text=obj.get("name", "<unnamed>"), values=(obj.get("type", ""), obj.get("prefab", ""), pos_text))

        for item in self.object_tree.get_children():
            self.object_tree.item(item, open=True)

    def refresh_balance_tree(self) -> None:
        self.balance_tree.delete(*self.balance_tree.get_children())
        self.balance_paths.clear()
        balance = self.level_data.get("balance", {})
        if not isinstance(balance, dict):
            return

        def walk(prefix: tuple[str, ...], value) -> None:
            if isinstance(value, dict):
                for key, child in value.items():
                    walk((*prefix, str(key)), child)
                return
            if isinstance(value, (str, int, float, bool)) or value is None:
                path_text = ".".join(prefix)
                item = self.balance_tree.insert("", tk.END, values=(path_text, json.dumps(value, ensure_ascii=False)))
                self.balance_paths[item] = prefix

        walk((), balance)

    def on_balance_select(self, _event=None) -> None:
        selection = self.balance_tree.selection()
        if not selection:
            return
        item = selection[0]
        values = self.balance_tree.item(item, "values")
        if len(values) >= 2:
            self.selected_balance_key.set(values[0])
            self.selected_balance_value.set(values[1])

    def apply_balance_value(self) -> None:
        selection = self.balance_tree.selection()
        if not selection:
            return
        path = self.balance_paths.get(selection[0])
        if not path:
            return
        try:
            new_value = json.loads(self.selected_balance_value.get())
        except json.JSONDecodeError:
            new_value = self.selected_balance_value.get()

        balance = self.level_data.setdefault("balance", {})
        if not isinstance(balance, dict):
            balance = {}
            self.level_data["balance"] = balance

        current = balance
        for key in path[:-1]:
            current = current.setdefault(key, {})
        current[path[-1]] = new_value
        self.refresh_all()
        self.status_text.set(f"Applied balance value: {'.'.join(path)}")

    def apply_raw_json(self, show_success: bool = True) -> bool:
        raw = self.json_text.get("1.0", tk.END)
        try:
            self.level_data = json.loads(raw)
        except json.JSONDecodeError as exc:
            messagebox.showerror("JSON parse failed", str(exc))
            return False
        if show_success:
            self.status_text.set("Applied raw JSON to editor.")
            self.refresh_all()
        return True

    def refresh_json_text(self) -> None:
        self.json_text.delete("1.0", tk.END)
        self.json_text.insert("1.0", json.dumps(self.level_data, ensure_ascii=False, indent=2))

    def validate_level(self) -> list[ValidationIssue]:
        issues: list[ValidationIssue] = []
        data = self.level_data
        if not isinstance(data, dict):
            return [ValidationIssue("ERROR", "Root JSON must be an object.")]

        objects = [obj for obj in data.get("objects", []) if isinstance(obj, dict)]
        spawn_areas = [area for area in data.get("spawnAreas", []) if isinstance(area, dict)]
        phases = [phase for phase in data.get("bossPhases", []) if isinstance(phase, dict)]

        players = [obj for obj in objects if obj.get("type") == "PlayerSpawn"]
        bosses = [obj for obj in objects if obj.get("type") == "BossSpawn"]
        if len(players) != 1:
            issues.append(ValidationIssue("ERROR", f"PlayerSpawn count should be 1, found {len(players)}."))
        if len(bosses) != 1:
            issues.append(ValidationIssue("ERROR", f"BossSpawn count should be 1, found {len(bosses)}."))
        if players and bosses:
            dist = distance_xy(vector(players[0], "position"), vector(bosses[0], "position"))
            if dist < 12.0:
                issues.append(ValidationIssue("WARN", f"PlayerSpawn and BossSpawn are close: {dist:.1f}."))

        all_level_objects = list(objects)
        for phase in phases:
            all_level_objects.extend(obj for obj in phase.get("objects", []) if isinstance(obj, dict))

        for obj in all_level_objects:
            obj_type = obj.get("type", "")
            prefab = obj.get("prefab", "Default")
            if obj_type not in SUPPORTED_TYPES:
                issues.append(ValidationIssue("ERROR", f"Unsupported type: {object_label(obj)}"))
                continue
            if prefab not in SUPPORTED_TYPES[obj_type]:
                issues.append(ValidationIssue("WARN", f"Unsupported prefab: {object_label(obj)}"))
            if obj_type == "SpawnArea":
                props = obj.get("customProperties", {})
                if isinstance(props, dict):
                    max_alive = props.get("maxAlive", 0)
                    interval = props.get("spawnInterval", 999)
                    if prefab == "Shooter" and isinstance(max_alive, (int, float)) and max_alive > 3:
                        issues.append(ValidationIssue("WARN", f"Shooter SpawnArea maxAlive is high: {object_label(obj)}"))
                    if isinstance(interval, (int, float)) and interval < 1.0:
                        issues.append(ValidationIssue("WARN", f"Spawn interval may be too fast: {object_label(obj)}"))

        for area in spawn_areas:
            prefab = area.get("prefab", "Basic")
            if prefab not in SUPPORTED_TYPES["SpawnArea"]:
                issues.append(ValidationIssue("WARN", f"Unsupported spawnAreas prefab: {area.get('name', '<unnamed>')} [{prefab}]"))
            if prefab == "Shooter" and number(area.get("maxAlive")) > 3:
                issues.append(ValidationIssue("WARN", f"Shooter spawnAreas maxAlive is high: {area.get('name', '<unnamed>')}"))

        last_rate = 2.0
        for phase in phases:
            rate = number(phase.get("startHpRate"), 1.0)
            if not 0.0 <= rate <= 1.0:
                issues.append(ValidationIssue("ERROR", f"Boss phase startHpRate out of range: {phase.get('name', '<unnamed>')}"))
            if rate > last_rate:
                issues.append(ValidationIssue("INFO", f"Boss phases are not sorted high-to-low near: {phase.get('name', '<unnamed>')}"))
            last_rate = rate

        balance = data.get("balance", {})
        if not isinstance(balance, dict):
            issues.append(ValidationIssue("WARN", "balance should be an object."))
        else:
            player = balance.get("player", {})
            if isinstance(player, dict) and number(player.get("maxHp")) <= 0:
                issues.append(ValidationIssue("ERROR", "balance.player.maxHp must be positive."))
            damage = balance.get("damage", {})
            if isinstance(damage, dict) and number(damage.get("damageBlock")) > number(player.get("maxHp"), 1000) * 0.35:
                issues.append(ValidationIssue("WARN", "DamageBlock damage is more than 35% of player maxHp."))

        if not issues:
            issues.append(ValidationIssue("OK", "No validation issues found."))
        return issues

    def refresh_validation(self) -> None:
        self.validation_list.delete(0, tk.END)
        for issue in self.validate_level():
            self.validation_list.insert(tk.END, f"[{issue.severity}] {issue.message}")

    def write_ai_handoff(self) -> None:
        issues = self.validate_level()
        balance = self.level_data.get("balance", {})
        summary = {
            "objects": len(self.level_data.get("objects", [])) if isinstance(self.level_data.get("objects"), list) else 0,
            "spawnAreas": len(self.level_data.get("spawnAreas", [])) if isinstance(self.level_data.get("spawnAreas"), list) else 0,
            "bossPhases": len(self.level_data.get("bossPhases", [])) if isinstance(self.level_data.get("bossPhases"), list) else 0,
        }
        text = [
            "# Level AI-ditor AI Handoff",
            "",
            "This file was generated by the external Level AI-ditor tool.",
            "",
            "## Summary",
            "",
            f"- level file: `{self.level_path}`",
            f"- objects: {summary['objects']}",
            f"- spawnAreas: {summary['spawnAreas']}",
            f"- bossPhases: {summary['bossPhases']}",
            "",
            "## Validation",
            "",
        ]
        text.extend(f"- [{issue.severity}] {issue.message}" for issue in issues)
        text.extend([
            "",
            "## Current Balance",
            "",
            "```json",
            json.dumps(balance, ensure_ascii=False, indent=2),
            "```",
            "",
            "## Request Template",
            "",
            "- Current play feel:",
            "- Problem to solve:",
            "- Make stronger:",
            "- Make weaker:",
            "- Keep this experience:",
            "",
            "## Rules For AI",
            "",
            "- Prefer editing `resources/levels/level_test.json` instead of changing C++ constants.",
            "- Use `balance` for HP, damage, default boss attacks, and enemy-system tuning.",
            "- Use `bossPhases[].customProperties.bossAttack` for phase-specific boss attacks.",
            "- Keep JSON valid.",
        ])
        try:
            DEFAULT_HANDOFF.write_text("\n".join(text) + "\n", encoding="utf-8")
        except Exception as exc:
            messagebox.showerror("AI handoff failed", str(exc))
            return
        self.status_text.set(f"Wrote: {DEFAULT_HANDOFF}")
        messagebox.showinfo("AI handoff", f"Wrote:\n{DEFAULT_HANDOFF}")


def main() -> None:
    root = tk.Tk()
    LevelAIDitorApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
