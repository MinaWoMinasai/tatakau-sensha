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
DEFAULT_DESIGN_BRIEF = PROJECT_ROOT / "resources" / "levels" / "ai_design_brief.md"
DEFAULT_TANK_DICTIONARY = PROJECT_ROOT / "resources" / "levels" / "tank_dictionary.json"


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


def spawn_area_to_object(area: dict) -> dict:
    return {
        "name": area.get("name", "SpawnArea"),
        "type": "SpawnArea",
        "prefab": area.get("prefab", "Basic"),
        "position": area.get("center", {"x": 0.0, "y": 0.0, "z": 0.0}),
        "rotation": {"x": 0.0, "y": 0.0, "z": 0.0},
        "scale": area.get("size", {"x": 10.0, "y": 10.0, "z": 1.0}),
        "customProperties": {
            "spawnInterval": area.get("spawnInterval", 2.0),
            "maxAlive": area.get("maxAlive", 8),
            "hp": area.get("hp", -1),
            "enabled": area.get("enabled", True),
            **(area.get("customProperties", {}) if isinstance(area.get("customProperties"), dict) else {}),
        },
    }


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
        self.preview_phase_objects = tk.BooleanVar(value=True)
        self.preview_labels = tk.BooleanVar(value=True)
        self.plan_difficulty = tk.StringVar(value="Normal")
        self.plan_stage_style = tk.StringVar(value="Central duel arena")
        self.plan_experience = tk.StringVar(value="Dodge bullets while farming EXP enemies")
        self.plan_tank_focus = tk.StringVar(value="NormalTank, SniperTank, RushTank")
        self.plan_notes = tk.StringVar(value="Keep the neon readability and leave escape routes.")

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

        right = ttk.Notebook(main)
        main.add(right, weight=2)

        preview_frame = ttk.Frame(right, padding=6)
        right.add(preview_frame, text="Arena Preview")
        preview_toolbar = ttk.Frame(preview_frame)
        preview_toolbar.pack(fill=tk.X)
        ttk.Checkbutton(preview_toolbar, text="Show boss phase objects", variable=self.preview_phase_objects, command=self.refresh_preview).pack(side=tk.LEFT)
        ttk.Checkbutton(preview_toolbar, text="Labels", variable=self.preview_labels, command=self.refresh_preview).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(preview_toolbar, text="Refresh Preview", command=self.refresh_preview).pack(side=tk.LEFT, padx=(8, 0))
        self.preview_canvas = tk.Canvas(preview_frame, bg="#05070d", highlightthickness=0)
        self.preview_canvas.pack(fill=tk.BOTH, expand=True, pady=(6, 0))
        self.preview_canvas.bind("<Configure>", lambda _event: self.refresh_preview())

        plan_frame = ttk.Frame(right, padding=8)
        right.add(plan_frame, text="AI Plan")
        self._build_ai_plan_ui(plan_frame)

        balance_frame = ttk.Frame(right, padding=6)
        right.add(balance_frame, text="Balance")

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

        json_frame = ttk.Frame(right, padding=6)
        right.add(json_frame, text="Raw JSON")
        self.json_text = tk.Text(json_frame, wrap=tk.NONE, undo=True)
        self.json_text.pack(fill=tk.BOTH, expand=True)
        raw_buttons = ttk.Frame(json_frame)
        raw_buttons.pack(fill=tk.X, pady=(6, 0))
        ttk.Button(raw_buttons, text="Apply Raw JSON To Editor", command=self.apply_raw_json).pack(side=tk.LEFT)
        ttk.Button(raw_buttons, text="Format Raw JSON", command=self.refresh_json_text).pack(side=tk.LEFT, padx=(6, 0))

    def _build_ai_plan_ui(self, parent: ttk.Frame) -> None:
        top = ttk.LabelFrame(parent, text="Production Intent", padding=8)
        top.pack(fill=tk.X)

        rows = [
            ("Difficulty", self.plan_difficulty, ("Easy", "Normal", "Hard", "Bullet Hell")),
            ("Stage style", self.plan_stage_style, ("Central duel arena", "Outer-ring chase", "Maze pressure", "Open farming field")),
            ("Target feel", self.plan_experience, ("Dodge bullets while farming EXP enemies", "Tank duel focus", "High mobility rush", "Survival pressure")),
            ("Tank focus", self.plan_tank_focus, ("NormalTank, SniperTank, RushTank", "HeavyTank, SpreadTank", "MineTank, SupportTank", "New tank ideas")),
        ]
        for row, (label, variable, values) in enumerate(rows):
            ttk.Label(top, text=label, width=14).grid(row=row, column=0, sticky=tk.W, pady=2)
            combo = ttk.Combobox(top, textvariable=variable, values=values)
            combo.grid(row=row, column=1, sticky=tk.EW, pady=2)
        top.columnconfigure(1, weight=1)

        note_frame = ttk.LabelFrame(parent, text="Notes For AI", padding=8)
        note_frame.pack(fill=tk.X, pady=(8, 0))
        ttk.Entry(note_frame, textvariable=self.plan_notes).pack(fill=tk.X)

        buttons = ttk.Frame(parent)
        buttons.pack(fill=tk.X, pady=(8, 0))
        ttk.Button(buttons, text="Write AI Design Brief MD", command=self.write_ai_design_brief).pack(side=tk.LEFT)
        ttk.Button(buttons, text="Write Tank Dictionary JSON", command=self.write_tank_dictionary).pack(side=tk.LEFT, padx=(8, 0))

        prompt_frame = ttk.LabelFrame(parent, text="Prompt Preview", padding=6)
        prompt_frame.pack(fill=tk.BOTH, expand=True, pady=(8, 0))
        self.plan_preview = tk.Text(prompt_frame, wrap=tk.WORD, height=18)
        self.plan_preview.pack(fill=tk.BOTH, expand=True)
        for variable in (self.plan_difficulty, self.plan_stage_style, self.plan_experience, self.plan_tank_focus, self.plan_notes):
            variable.trace_add("write", lambda *_args: self.refresh_plan_preview())

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
        self.refresh_preview()
        self.refresh_plan_preview()
        self.refresh_json_text()

    def collect_preview_objects(self) -> list[tuple[dict, str]]:
        items: list[tuple[dict, str]] = []
        for obj in self.level_data.get("objects", []):
            if isinstance(obj, dict):
                items.append((obj, "base"))
        for area in self.level_data.get("spawnAreas", []):
            if isinstance(area, dict):
                items.append((spawn_area_to_object(area), "spawnAreas"))
        if self.preview_phase_objects.get():
            for phase in self.level_data.get("bossPhases", []):
                if not isinstance(phase, dict):
                    continue
                phase_name = phase.get("name", "phase")
                for obj in phase.get("objects", []):
                    if isinstance(obj, dict):
                        items.append((obj, str(phase_name)))
        return items

    def refresh_preview(self) -> None:
        if not hasattr(self, "preview_canvas"):
            return
        canvas = self.preview_canvas
        canvas.delete("all")
        width = max(canvas.winfo_width(), 320)
        height = max(canvas.winfo_height(), 240)

        objects = self.collect_preview_objects()
        points: list[tuple[float, float]] = []
        for obj, _source in objects:
            pos = vector(obj, "position")
            scale = vector(obj, "scale")
            x = number(pos.get("x"))
            y = number(pos.get("y"))
            sx = max(number(scale.get("x"), 1.0), 1.0)
            sy = max(number(scale.get("y"), 1.0), 1.0)
            points.extend([(x - sx * 0.5, y - sy * 0.5), (x + sx * 0.5, y + sy * 0.5)])

        if not points:
            canvas.create_text(width / 2, height / 2, text="Open a level JSON to preview.", fill="#9fb8cf")
            return

        min_x = min(x for x, _y in points)
        max_x = max(x for x, _y in points)
        min_y = min(y for _x, y in points)
        max_y = max(y for _x, y in points)
        pad_world = 6.0
        min_x -= pad_world
        max_x += pad_world
        min_y -= pad_world
        max_y += pad_world
        world_w = max(max_x - min_x, 1.0)
        world_h = max(max_y - min_y, 1.0)
        margin = 28
        scale = min((width - margin * 2) / world_w, (height - margin * 2) / world_h)

        def to_screen(x: float, y: float) -> tuple[float, float]:
            sx = margin + (x - min_x) * scale
            sy = height - margin - (y - min_y) * scale
            return sx, sy

        for gx in range(math.floor(min_x), math.ceil(max_x) + 1, 2):
            x0, y0 = to_screen(gx, min_y)
            x1, y1 = to_screen(gx, max_y)
            color = "#163044" if gx % 10 else "#275d7d"
            canvas.create_line(x0, y0, x1, y1, fill=color)
        for gy in range(math.floor(min_y), math.ceil(max_y) + 1, 2):
            x0, y0 = to_screen(min_x, gy)
            x1, y1 = to_screen(max_x, gy)
            color = "#163044" if gy % 10 else "#275d7d"
            canvas.create_line(x0, y0, x1, y1, fill=color)

        for obj, source in objects:
            self.draw_preview_object(canvas, obj, source, to_screen, scale)

        canvas.create_text(10, 10, anchor=tk.NW, text=f"{len(objects)} objects / grid preview", fill="#d6f7ff")

    def draw_preview_object(self, canvas: tk.Canvas, obj: dict, source: str, to_screen, scale: float) -> None:
        obj_type = obj.get("type", "")
        prefab = obj.get("prefab", "Default")
        pos = vector(obj, "position")
        obj_scale = vector(obj, "scale")
        x = number(pos.get("x"))
        y = number(pos.get("y"))
        sx = max(number(obj_scale.get("x"), 1.0), 1.0)
        sy = max(number(obj_scale.get("y"), 1.0), 1.0)
        cx, cy = to_screen(x, y)
        phase_dash = (3, 3) if source not in {"base", "spawnAreas"} else None

        colors = {
            "PlayerSpawn": ("#dfff96", "#f5ffd5"),
            "BossSpawn": ("#ff7fb2", "#ffd4e6"),
            "Enemy": ("#f9ff7a", "#ffffff"),
            "SpawnArea": ("#49b7ff", "#a7e2ff"),
            "Obstacle": ("#58ff55", "#d8ffd6"),
            "Item": ("#82ffe2", "#e1fff7"),
        }
        outline, fill = colors.get(obj_type, ("#c9d6df", "#ffffff"))
        if prefab == "DamageBlock":
            outline, fill = "#ff384d", "#ff9aa6"
        if source == "spawnAreas":
            outline, fill = "#27d4ff", "#073344"
        dash_option = {"dash": phase_dash} if phase_dash else {}

        if obj_type in {"Obstacle", "SpawnArea"}:
            x0, y0 = to_screen(x - sx * 0.5, y - sy * 0.5)
            x1, y1 = to_screen(x + sx * 0.5, y + sy * 0.5)
            stipple = "gray25" if obj_type == "SpawnArea" else ""
            canvas.create_rectangle(x0, y0, x1, y1, outline=outline, fill=fill if obj_type == "SpawnArea" else "", width=3, stipple=stipple, **dash_option)
        elif obj_type == "Enemy" and prefab in {"Triangle", "Pentagon"}:
            radius = max(7, 0.55 * scale)
            sides = 3 if prefab == "Triangle" else 5
            points = []
            for index in range(sides):
                angle = -math.pi / 2 + math.tau * index / sides
                points.extend([cx + math.cos(angle) * radius, cy + math.sin(angle) * radius])
            canvas.create_polygon(points, outline=outline, fill="", width=3, **dash_option)
        elif obj_type == "Enemy":
            radius = max(7, 0.55 * scale)
            canvas.create_rectangle(cx - radius, cy - radius, cx + radius, cy + radius, outline=outline, width=3, **dash_option)
        else:
            radius = max(7, (1.0 if obj_type == "BossSpawn" else 0.65) * scale)
            canvas.create_oval(cx - radius, cy - radius, cx + radius, cy + radius, outline=outline, width=3, **dash_option)

        if self.preview_labels.get():
            label = obj.get("name", obj_type)
            canvas.create_text(cx + 8, cy - 8, anchor=tk.SW, text=label, fill="#d8efff", font=("Segoe UI", 8))

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

    def build_design_brief_text(self) -> str:
        issues = self.validate_level()
        summary = {
            "levelName": self.level_data.get("levelName", ""),
            "objects": len(self.level_data.get("objects", [])) if isinstance(self.level_data.get("objects"), list) else 0,
            "spawnAreas": len(self.level_data.get("spawnAreas", [])) if isinstance(self.level_data.get("spawnAreas"), list) else 0,
            "bossPhases": len(self.level_data.get("bossPhases", [])) if isinstance(self.level_data.get("bossPhases"), list) else 0,
        }
        balance = self.level_data.get("balance", {})
        objects = self.level_data.get("objects", [])
        boss_phases = self.level_data.get("bossPhases", [])
        text = [
            "# Neon Arena Director AI Design Brief",
            "",
            "This file is generated by Level AI-ditor External.",
            "Use it to ask Codex or ChatGPT for broad stage, tank, enemy, and balance ideas before hand-tuning.",
            "",
            "## Human Intent",
            "",
            f"- Difficulty: {self.plan_difficulty.get()}",
            f"- Stage style: {self.plan_stage_style.get()}",
            f"- Target feel: {self.plan_experience.get()}",
            f"- Tank focus: {self.plan_tank_focus.get()}",
            f"- Notes: {self.plan_notes.get()}",
            "",
            "## Current Level Summary",
            "",
            f"- level file: `{self.level_path}`",
            f"- level name: `{summary['levelName']}`",
            f"- objects: {summary['objects']}",
            f"- root spawn areas: {summary['spawnAreas']}",
            f"- boss phases: {summary['bossPhases']}",
            "",
            "## Validation",
            "",
        ]
        text.extend(f"- [{issue.severity}] {issue.message}" for issue in issues)
        text.extend([
            "",
            "## Current Objects",
            "",
            "```json",
            json.dumps(objects, ensure_ascii=False, indent=2),
            "```",
            "",
            "## Current Boss Phases",
            "",
            "```json",
            json.dumps(boss_phases, ensure_ascii=False, indent=2),
            "```",
            "",
            "## Current Balance",
            "",
            "```json",
            json.dumps(balance, ensure_ascii=False, indent=2),
            "```",
            "",
            "## Request For AI",
            "",
            "Please propose a small, playable improvement plan for this neon tank arena.",
            "Prioritize broad ideas first, then provide JSON edits only where needed.",
            "",
            "Include:",
            "",
            "- 3 stage layout ideas",
            "- 3 tank or enemy type ideas that fit `resources/levels/tank_dictionary.json`",
            "- Suggested spawnArea and bossPhase changes",
            "- Balance changes with short reasons",
            "- Risks that a human designer should review manually",
            "",
            "## Editing Rules",
            "",
            "- Keep `level_test.json` valid JSON.",
            "- Prefer changing `resources/levels/level_test.json` instead of C++ constants.",
            "- Use `objects`, `spawnAreas`, `bossPhases`, and `balance` as the main edit targets.",
            "- Do not fully block escape routes with DamageBlock or Wall objects.",
            "- Keep PlayerSpawn and BossSpawn readable on the preview map.",
            "- Treat AI output as a proposal; the human designer will do final tuning.",
        ])
        return "\n".join(text) + "\n"

    def refresh_plan_preview(self) -> None:
        if not hasattr(self, "plan_preview"):
            return
        self.plan_preview.delete("1.0", tk.END)
        self.plan_preview.insert("1.0", self.build_design_brief_text())

    def write_ai_design_brief(self) -> None:
        try:
            DEFAULT_DESIGN_BRIEF.write_text(self.build_design_brief_text(), encoding="utf-8")
        except Exception as exc:
            messagebox.showerror("AI design brief failed", str(exc))
            return
        self.status_text.set(f"Wrote: {DEFAULT_DESIGN_BRIEF}")
        messagebox.showinfo("AI design brief", f"Wrote:\n{DEFAULT_DESIGN_BRIEF}")

    def build_tank_dictionary(self) -> dict:
        return {
            "toolName": "Neon Arena Director",
            "purpose": "AI and human designers use this dictionary to discuss tank and enemy roles before editing level JSON or C++.",
            "tankTypes": {
                "NormalTank": {
                    "role": "standard",
                    "strengths": ["balanced movement", "balanced reload", "easy to place in most stages"],
                    "weaknesses": ["no strong identity"],
                    "levelUse": "baseline player or enemy tank idea",
                    "balanceHints": {"hp": "medium", "moveSpeed": "medium", "reload": "medium", "bulletSpeed": "medium"},
                },
                "SniperTank": {
                    "role": "long range pressure",
                    "strengths": ["fast bullets", "high single-shot threat", "forces lateral dodging"],
                    "weaknesses": ["low fire rate", "weak at close range"],
                    "levelUse": "place on open lanes, not near PlayerSpawn",
                    "balanceHints": {"hp": "low-medium", "moveSpeed": "low", "reload": "slow", "bulletSpeed": "high"},
                },
                "RushTank": {
                    "role": "close range chase",
                    "strengths": ["high movement", "creates panic and repositioning"],
                    "weaknesses": ["low durability", "can feel unfair in narrow maps"],
                    "levelUse": "use with wide escape routes and low maxAlive",
                    "balanceHints": {"hp": "low", "moveSpeed": "high", "reload": "medium", "bulletSpeed": "medium"},
                },
                "HeavyTank": {
                    "role": "slow pressure wall",
                    "strengths": ["high durability", "strong contact pressure", "anchors a phase"],
                    "weaknesses": ["slow", "easy to kite in open spaces"],
                    "levelUse": "good for bossPhase objects or late-game pressure",
                    "balanceHints": {"hp": "high", "moveSpeed": "low", "reload": "slow", "bulletSpeed": "medium"},
                },
                "SpreadTank": {
                    "role": "area denial shooter",
                    "strengths": ["wide bullet coverage", "good against direct approaches"],
                    "weaknesses": ["can clutter the screen", "needs lower damage per bullet"],
                    "levelUse": "use sparingly near red hazard zones",
                    "balanceHints": {"hp": "medium", "moveSpeed": "low-medium", "reload": "medium", "bulletCount": "high", "damage": "low-medium"},
                },
                "MineTank": {
                    "role": "trap setter",
                    "strengths": ["controls routes", "pairs well with chase enemies"],
                    "weaknesses": ["requires clear visual readability", "too many traps feel unfair"],
                    "levelUse": "use in outer-ring or maze-pressure stages",
                    "balanceHints": {"hp": "medium", "moveSpeed": "low", "spawnInterval": "slow", "damage": "medium"},
                },
            },
            "aiRules": [
                "Suggest tank ideas as roles first, then numeric values.",
                "Keep readability high in neon scenes with many bloom effects.",
                "Avoid combining narrow corridors, high rush speed, and high contact damage.",
                "When adding a strong tank type, reduce maxAlive or increase spawnInterval.",
                "Use the external preview to check escape routes before final tuning.",
            ],
        }

    def write_tank_dictionary(self) -> None:
        try:
            DEFAULT_TANK_DICTIONARY.write_text(json.dumps(self.build_tank_dictionary(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        except Exception as exc:
            messagebox.showerror("Tank dictionary failed", str(exc))
            return
        self.status_text.set(f"Wrote: {DEFAULT_TANK_DICTIONARY}")
        messagebox.showinfo("Tank dictionary", f"Wrote:\n{DEFAULT_TANK_DICTIONARY}")

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
