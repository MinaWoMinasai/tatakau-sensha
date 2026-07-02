import argparse
import math
import sys

import bpy


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--bone", required=True)
    parser.add_argument("--clip")
    parser.add_argument("--fbx", action="store_true")
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    return parser.parse_args(argv)


def main():
    args = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if args.fbx:
        bpy.ops.import_scene.fbx(filepath=args.input, automatic_bone_orientation=False)
    else:
        bpy.ops.import_scene.gltf(filepath=args.input)
    armature = max(
        (obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"),
        key=lambda obj: len(obj.data.bones),
    )
    if args.clip:
        armature.animation_data_create()
        armature.animation_data.action = next(a for a in bpy.data.actions if a.name == args.clip)
    action = armature.animation_data.action
    start = action.frame_range[0]
    end = action.frame_range[1]
    rest_world = armature.matrix_world.to_quaternion() @ armature.data.bones[args.bone].matrix_local.to_quaternion()
    print("ROOT_ROTATION", args.input, args.clip or action.name, "frames", tuple(action.frame_range))
    for step in range(9):
        frame = start + (end - start) * step / 8.0
        whole = math.floor(frame)
        bpy.context.scene.frame_set(whole, subframe=frame - whole)
        pose_world = armature.matrix_world.to_quaternion() @ armature.pose.bones[args.bone].matrix.to_quaternion()
        delta = pose_world @ rest_world.inverted()
        angle = min(delta.angle, 2.0 * math.pi - delta.angle)
        axis = delta.axis
        print(
            "SAMPLE",
            round(frame, 3),
            "angle_deg", round(math.degrees(angle), 3),
            "axis", tuple(round(value, 3) for value in axis),
        )


if __name__ == "__main__":
    main()
