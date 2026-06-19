import json
import os
import math
import bpy


# Set this to an absolute path when Blender cannot find the file automatically.
LEVEL_JSON_PATH = ""

COLLECTION_NAME = "Level AI-ditor Import"
CREATE_LABELS = True


TYPE_COLORS = {
    "PlayerSpawn": (0.35, 1.0, 0.25, 1.0),
    "BossSpawn": (1.0, 0.18, 0.25, 1.0),
    "Enemy": (1.0, 0.8, 0.2, 1.0),
    "SpawnArea": (0.2, 0.65, 1.0, 0.35),
    "Obstacle": (0.55, 1.0, 0.25, 1.0),
    "DamageBlock": (1.0, 0.15, 0.15, 1.0),
    "Item": (0.65, 1.0, 0.9, 1.0),
}


def get_script_dir():
    text = getattr(getattr(bpy.context, "space_data", None), "text", None)
    if text and getattr(text, "filepath", ""):
        return os.path.dirname(bpy.path.abspath(text.filepath))

    for text in bpy.data.texts:
        if text.name.endswith("import_level_test.py") and getattr(text, "filepath", ""):
            return os.path.dirname(bpy.path.abspath(text.filepath))

    file_path = globals().get("__file__", "")
    if file_path:
        return os.path.dirname(os.path.abspath(file_path))

    return ""


def resolve_level_path():
    if LEVEL_JSON_PATH:
        return bpy.path.abspath(LEVEL_JSON_PATH)

    candidates = []
    script_dir = get_script_dir()
    if script_dir:
        candidates.append(os.path.join(script_dir, "..", "..", "resources", "levels", "level_test.json"))
        candidates.append(os.path.join(script_dir, "level_test.json"))

    if bpy.data.filepath:
        blend_dir = os.path.dirname(bpy.data.filepath)
        candidates.append(os.path.join(blend_dir, "level_test.json"))
        candidates.append(os.path.join(blend_dir, "resources", "levels", "level_test.json"))

    candidates.append(os.path.join(os.getcwd(), "resources", "levels", "level_test.json"))
    candidates.append(os.path.join(os.getcwd(), "level_test.json"))

    for path in candidates:
        normalized = os.path.abspath(os.path.normpath(path))
        if os.path.exists(normalized):
            return normalized

    raise FileNotFoundError(
        "level_test.json was not found. Set LEVEL_JSON_PATH at the top of this script. "
        f"Checked: {', '.join(os.path.abspath(os.path.normpath(path)) for path in candidates)}"
    )


def get_or_create_material(name, color):
    material = bpy.data.materials.get(name)
    if material is None:
        material = bpy.data.materials.new(name)
        material.diffuse_color = color
        material.use_nodes = True
        bsdf = material.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = color
            bsdf.inputs["Alpha"].default_value = color[3]
        material.blend_method = "BLEND"
        material.use_screen_refraction = True
    return material


def clear_import_collection():
    old = bpy.data.collections.get(COLLECTION_NAME)
    if old:
        for obj in list(old.objects):
            bpy.data.objects.remove(obj, do_unlink=True)
        for child in list(old.children):
            for obj in list(child.objects):
                bpy.data.objects.remove(obj, do_unlink=True)
            bpy.data.collections.remove(child)
        bpy.data.collections.remove(old)


def make_collection(name, parent=None):
    collection = bpy.data.collections.new(name)
    if parent is None:
        bpy.context.scene.collection.children.link(collection)
    else:
        parent.children.link(collection)
    return collection


def unlink_from_scene_roots(obj):
    for collection in list(obj.users_collection):
        collection.objects.unlink(obj)


def link_object(obj, collection):
    collection.objects.link(obj)


def vector3(data, fallback=(0.0, 0.0, 0.0)):
    if not isinstance(data, dict):
        return fallback
    return (
        float(data.get("x", fallback[0])),
        float(data.get("y", fallback[1])),
        float(data.get("z", fallback[2])),
    )


def apply_transform(obj, level_object, position_key="position", scale_key="scale"):
    obj.location = vector3(level_object.get(position_key), (0.0, 0.0, 0.0))
    obj.rotation_euler = vector3(level_object.get("rotation"), (0.0, 0.0, 0.0))
    obj.scale = vector3(level_object.get(scale_key), (1.0, 1.0, 1.0))


def apply_custom_properties(obj, level_object):
    obj["levelType"] = level_object.get("type", "")
    obj["prefab"] = level_object.get("prefab", "Default")
    obj["sourceName"] = level_object.get("name", obj.name)
    custom = level_object.get("customProperties", {})
    if isinstance(custom, dict):
        for key, value in custom.items():
            if isinstance(value, (str, int, float, bool)) or value is None:
                obj[key] = value
            else:
                obj[key] = json.dumps(value, ensure_ascii=False)


def add_label(text, location, collection):
    if not CREATE_LABELS:
        return None
    bpy.ops.object.text_add(location=(location[0], location[1], location[2] + 1.2))
    label = bpy.context.object
    label.name = f"Label_{text}"
    label.data.body = text
    label.data.size = 0.45
    label.data.align_x = "CENTER"
    label.data.align_y = "CENTER"
    unlink_from_scene_roots(label)
    link_object(label, collection)
    return label


def create_empty(level_object, collection, display_type, size, material_key):
    bpy.ops.object.empty_add(type=display_type, location=vector3(level_object.get("position")))
    obj = bpy.context.object
    obj.name = level_object.get("name", "Empty")
    obj.empty_display_size = size
    obj.rotation_euler = vector3(level_object.get("rotation"), (0.0, 0.0, 0.0))
    apply_custom_properties(obj, level_object)
    unlink_from_scene_roots(obj)
    link_object(obj, collection)
    add_label(obj.name, obj.location, collection)
    return obj


def create_cube(level_object, collection, material, display_wire=False):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=vector3(level_object.get("position")))
    obj = bpy.context.object
    obj.name = level_object.get("name", "Cube")
    apply_transform(obj, level_object)
    obj.data.materials.append(material)
    if display_wire:
        obj.display_type = "WIRE"
        obj.show_wire = True
        obj.show_in_front = True
    apply_custom_properties(obj, level_object)
    unlink_from_scene_roots(obj)
    link_object(obj, collection)
    add_label(obj.name, obj.location, collection)
    return obj


def create_enemy(level_object, collection):
    prefab = level_object.get("prefab", "Basic")
    material = get_or_create_material("LevelAI_Enemy", TYPE_COLORS["Enemy"])
    location = vector3(level_object.get("position"))

    if prefab == "Triangle":
        bpy.ops.mesh.primitive_cone_add(vertices=3, radius1=0.8, depth=0.35, location=location, rotation=(math.radians(90), 0, 0))
    elif prefab == "Pentagon":
        bpy.ops.mesh.primitive_cone_add(vertices=5, radius1=0.9, depth=0.35, location=location, rotation=(math.radians(90), 0, 0))
    elif prefab == "Shooter":
        bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.8, location=location)
    else:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=location)

    obj = bpy.context.object
    obj.name = level_object.get("name", "Enemy")
    apply_transform(obj, level_object)
    obj.data.materials.append(material)
    apply_custom_properties(obj, level_object)
    unlink_from_scene_roots(obj)
    link_object(obj, collection)
    add_label(obj.name, obj.location, collection)
    return obj


def create_level_object(level_object, collection):
    obj_type = level_object.get("type", "")
    prefab = level_object.get("prefab", "Default")

    if obj_type == "PlayerSpawn":
        return create_empty(level_object, collection, "SPHERE", 1.4, "PlayerSpawn")
    if obj_type == "BossSpawn":
        return create_empty(level_object, collection, "SPHERE", 2.2, "BossSpawn")
    if obj_type == "Enemy":
        return create_enemy(level_object, collection)
    if obj_type == "SpawnArea":
        material = get_or_create_material("LevelAI_SpawnArea", TYPE_COLORS["SpawnArea"])
        return create_cube(level_object, collection, material, display_wire=True)
    if obj_type == "Obstacle":
        color_key = "DamageBlock" if prefab == "DamageBlock" else "Obstacle"
        material = get_or_create_material(f"LevelAI_{color_key}", TYPE_COLORS[color_key])
        return create_cube(level_object, collection, material, display_wire=False)
    if obj_type == "Item":
        material = get_or_create_material("LevelAI_Item", TYPE_COLORS["Item"])
        bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6, radius=0.55, location=vector3(level_object.get("position")))
        obj = bpy.context.object
        obj.name = level_object.get("name", "Item")
        apply_transform(obj, level_object)
        obj.data.materials.append(material)
        apply_custom_properties(obj, level_object)
        unlink_from_scene_roots(obj)
        link_object(obj, collection)
        add_label(obj.name, obj.location, collection)
        return obj

    print(f"[LevelImport] skipped unsupported type: {obj_type} ({level_object.get('name', '')})")
    return None


def spawn_area_to_object(area):
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
        },
    }


def import_level():
    level_path = resolve_level_path()
    with open(level_path, "r", encoding="utf-8") as file:
        level = json.load(file)

    clear_import_collection()
    root_collection = make_collection(COLLECTION_NAME)
    base_collection = make_collection("Base Objects", root_collection)
    spawn_area_collection = make_collection("Root Spawn Areas", root_collection)
    phase_collection = make_collection("Boss Phases", root_collection)

    created = 0
    for level_object in level.get("objects", []):
        if isinstance(level_object, dict) and create_level_object(level_object, base_collection):
            created += 1

    for area in level.get("spawnAreas", []):
        if isinstance(area, dict) and create_level_object(spawn_area_to_object(area), spawn_area_collection):
            created += 1

    for phase in level.get("bossPhases", []):
        if not isinstance(phase, dict):
            continue
        phase_name = phase.get("name", "Phase")
        phase_child = make_collection(phase_name, phase_collection)
        for level_object in phase.get("objects", []):
            if isinstance(level_object, dict) and create_level_object(level_object, phase_child):
                created += 1

    print(f"[LevelImport] imported {created} objects from: {level_path}")
    print("[LevelImport] collection:", COLLECTION_NAME)


if __name__ == "__main__":
    import_level()
