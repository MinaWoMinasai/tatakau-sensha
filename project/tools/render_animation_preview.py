import argparse
import math
import os
import sys

import bpy
from mathutils import Vector


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--action")
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    return parser.parse_args(argv)


def point_camera(camera, target):
    camera.rotation_euler = (Vector(target) - camera.location).to_track_quat("-Z", "Y").to_euler()


def main():
    args = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=args.input)
    os.makedirs(args.output, exist_ok=True)

    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.name not in {"Body", "Face", "Hair"}:
            obj.hide_render = True

    armature = max(
        (obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"),
        key=lambda obj: len(obj.data.bones),
    )
    armature.animation_data_create()

    bpy.ops.object.camera_add(location=(0.0, -4.2, 1.1))
    camera = bpy.context.object
    camera.data.lens = 55
    point_camera(camera, (0.0, 0.0, 0.9))
    bpy.context.scene.camera = camera

    bpy.ops.object.light_add(type="AREA", location=(1.5, -2.5, 4.0))
    key_light = bpy.context.object
    key_light.data.energy = 900.0
    key_light.data.shape = "DISK"
    key_light.data.size = 5.0
    point_camera(key_light, (0.0, 0.0, 1.0))

    bpy.ops.object.light_add(type="AREA", location=(-2.5, 1.0, 2.0))
    fill_light = bpy.context.object
    fill_light.data.energy = 500.0
    fill_light.data.size = 4.0
    point_camera(fill_light, (0.0, 0.0, 1.0))

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 512
    scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.world = bpy.data.worlds.new("PreviewWorld")
    scene.world.color = (0.06, 0.06, 0.08)

    for action in sorted(bpy.data.actions, key=lambda item: item.name):
        if args.action and action.name != args.action:
            continue
        armature.animation_data.action = action
        start = int(round(action.frame_range[0]))
        end = int(round(action.frame_range[1]))
        frames = {
            f"phase{sample}": start + (end - start) * sample // 8
            for sample in range(8)
        }
        for label, frame in frames.items():
            scene.frame_set(frame)
            scene.render.filepath = os.path.join(args.output, f"{action.name}_{label}.png")
            bpy.ops.render.render(write_still=True)
            print(f"RENDERED {action.name} {label} frame={frame}")


if __name__ == "__main__":
    main()
