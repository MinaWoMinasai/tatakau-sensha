import json
import math
import os
import bpy


SUPPORTED_TYPES = {"PlayerSpawn", "Enemy", "Obstacle", "Item"}


def infer_type_and_prefab(object_name):
    parts = object_name.split("_")
    object_type = parts[0] if parts else "Unknown"

    if object_type == "PlayerSpawn":
        return "PlayerSpawn", "Default"

    if object_type in {"Enemy", "Obstacle", "Item"}:
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

    level = {
        "levelName": "test_stage",
        "objects": objects,
    }

    if bpy.data.filepath:
        output_dir = os.path.dirname(bpy.data.filepath)
    else:
        output_dir = os.path.expanduser("~/Desktop")

    output_path = os.path.join(output_dir, "level_test.json")
    with open(output_path, "w", encoding="utf-8") as file:
        json.dump(level, file, ensure_ascii=False, indent=2)

    print(f"[LevelExport] exported {len(objects)} objects: {output_path}")


if __name__ == "__main__":
    export_level()
