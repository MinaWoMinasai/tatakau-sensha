import argparse
import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retarget_mixamo_to_vroid import BONE_MAP, find_armature


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--action", required=True)
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    return parser.parse_args(argv)


def angle_degrees(quaternion):
    angle = quaternion.angle
    return math.degrees(min(angle, 2.0 * math.pi - angle))


def sample_source(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    armature = find_armature(list(bpy.context.scene.objects))
    action = armature.animation_data.action
    start, end = map(lambda value: int(round(value)), action.frame_range)
    rest = {name: armature.data.bones[name].matrix_local.to_quaternion() for name in BONE_MAP}
    object_rotation = armature.matrix_world.to_quaternion()
    result = {name: [] for name in BONE_MAP}
    for frame in range(start, end + 1):
        bpy.context.scene.frame_set(frame)
        for name in BONE_MAP:
            pose = armature.pose.bones[name].matrix.to_quaternion()
            result[name].append((object_rotation @ pose) @ (object_rotation @ rest[name]).inverted())
    return result


def sample_target(path, action_name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=path)
    armature = find_armature(list(bpy.context.scene.objects))
    action = bpy.data.actions[action_name]
    armature.animation_data.action = action
    start, end = map(lambda value: int(round(value)), action.frame_range)
    rest = {
        name: armature.data.bones[name].matrix_local.to_quaternion()
        for name in BONE_MAP.values()
    }
    object_rotation = armature.matrix_world.to_quaternion()
    result = {name: [] for name in BONE_MAP.values()}
    for frame in range(start, end + 1):
        bpy.context.scene.frame_set(frame)
        for name in BONE_MAP.values():
            pose = armature.pose.bones[name].matrix.to_quaternion()
            result[name].append((object_rotation @ pose) @ (object_rotation @ rest[name]).inverted())
    return result


def main():
    args = parse_args()
    source = sample_source(args.source)
    target = sample_target(args.target, args.action)
    print("bone,source_motion_deg,target_motion_deg,error_deg")
    for source_name, target_name in BONE_MAP.items():
        count = min(len(source[source_name]), len(target[target_name]))
        source_motion = max(angle_degrees(value) for value in source[source_name][:count])
        target_motion = max(angle_degrees(value) for value in target[target_name][:count])
        error = max(
            angle_degrees(source[source_name][index].rotation_difference(target[target_name][index]))
            for index in range(count)
        )
        print(f"{target_name},{source_motion:.3f},{target_motion:.3f},{error:.3f}")


if __name__ == "__main__":
    main()
