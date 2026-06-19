import json
import math
import os
import bpy


SUPPORTED_TYPES = {"PlayerSpawn", "BossSpawn", "Enemy", "SpawnArea", "Obstacle", "Item"}

# Set this to an absolute path when you want to export somewhere else.
LEVEL_JSON_PATH = ""


def get_script_dir():
    text = getattr(getattr(bpy.context, "space_data", None), "text", None)
    if text and getattr(text, "filepath", ""):
        return os.path.dirname(bpy.path.abspath(text.filepath))

    for text in bpy.data.texts:
        if text.name.endswith("export_level_test.py") and getattr(text, "filepath", ""):
            return os.path.dirname(bpy.path.abspath(text.filepath))

    file_path = globals().get("__file__", "")
    if file_path:
        return os.path.dirname(os.path.abspath(file_path))

    return ""


def resolve_output_path():
    if LEVEL_JSON_PATH:
        return bpy.path.abspath(LEVEL_JSON_PATH)

    candidates = []
    script_dir = get_script_dir()
    if script_dir:
        candidates.append(os.path.join(script_dir, "..", "..", "resources", "levels", "level_test.json"))

    if bpy.data.filepath:
        blend_dir = os.path.dirname(bpy.data.filepath)
        candidates.append(os.path.join(blend_dir, "resources", "levels", "level_test.json"))
        candidates.append(os.path.join(blend_dir, "level_test.json"))

    candidates.append(os.path.join(os.getcwd(), "resources", "levels", "level_test.json"))
    candidates.append(os.path.join(os.getcwd(), "level_test.json"))

    for path in candidates:
        normalized = os.path.abspath(os.path.normpath(path))
        parent = os.path.dirname(normalized)
        if os.path.isdir(parent):
            return normalized

    raise FileNotFoundError(
        "Export directory was not found. Set LEVEL_JSON_PATH at the top of this script. "
        f"Checked: {', '.join(os.path.abspath(os.path.normpath(path)) for path in candidates)}"
    )


def infer_type_and_prefab(object_name):
    parts = object_name.split("_")
    object_type = parts[0] if parts else "Unknown"

    if object_type == "PlayerSpawn":
        return "PlayerSpawn", "Default"

    if object_type == "BossSpawn":
        return "BossSpawn", "Default"

    if object_type in {"Enemy", "SpawnArea", "Obstacle", "Item"}:
        prefab = parts[1] if len(parts) >= 2 and parts[1] else "Default"
        return object_type, prefab

    return "Unknown", "Default"


def vector_to_dict(vector):
    return {
        "x": float(vector.x),
        "y": float(vector.y),
        "z": float(vector.z),
    }


def custom_properties_to_dict(obj):
    result = {}
    for key in obj.keys():
        if key.startswith("_"):
            continue
        value = obj[key]
        if isinstance(value, (str, int, float, bool)) or value is None:
            result[key] = value
        elif isinstance(value, (list, tuple)):
            result[key] = list(value)
        else:
            result[key] = str(value)
    return result


def should_export(obj):
    if obj.name.startswith("Label_"):
        return False
    ignored_import_collections = {"Boss Phases", "Root Spawn Areas"}
    for collection in obj.users_collection:
        current = collection
        while current:
            if current.name in ignored_import_collections:
                return False
            parents = [candidate for candidate in bpy.data.collections if current.name in candidate.children]
            current = parents[0] if parents else None
    return obj.type in {"EMPTY", "MESH"}


def export_level():
    objects = []

    for obj in bpy.context.scene.objects:
        if not should_export(obj):
            continue

        object_type, prefab = infer_type_and_prefab(obj.name)
        if object_type not in SUPPORTED_TYPES:
            print(f"[LevelExport] skipped unsupported object name: {obj.name}")
            continue

        objects.append({
            "name": obj.name,
            "type": object_type,
            "prefab": prefab,
            "position": vector_to_dict(obj.location),
            "rotation": {
                "x": float(obj.rotation_euler.x),
                "y": float(obj.rotation_euler.y),
                "z": float(obj.rotation_euler.z),
            },
            "scale": vector_to_dict(obj.scale),
            "customProperties": custom_properties_to_dict(obj),
        })

    output_path = resolve_output_path()
    if os.path.exists(output_path):
        with open(output_path, "r", encoding="utf-8") as file:
            level = json.load(file)
    else:
        level = {}

    level["toolName"] = level.get("toolName", "Level AI-ditor")
    level["editorMode"] = level.get("editorMode", "ArenaBattle")
    level["levelName"] = level.get("levelName", "test_stage")
    level["balance"] = level.get("balance", {"defaultRandomSpawnEnabled": True})
    level["objects"] = objects
    level["spawnAreas"] = level.get("spawnAreas", [])
    level["bossPhases"] = level.get("bossPhases", [])

    with open(output_path, "w", encoding="utf-8") as file:
        json.dump(level, file, ensure_ascii=False, indent=2)

    print(f"[LevelExport] exported {len(objects)} objects: {output_path}")


if __name__ == "__main__":
    export_level()
