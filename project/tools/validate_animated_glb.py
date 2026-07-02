import argparse
import math
import sys

import bpy
from mathutils import Vector


LOCOMOTION_BONES = {
    "J_Bip_C_Hips", "J_Bip_C_Spine", "J_Bip_C_Chest", "J_Bip_C_UpperChest",
    "J_Bip_C_Neck", "J_Bip_C_Head",
    "J_Bip_L_Shoulder", "J_Bip_L_UpperArm", "J_Bip_L_LowerArm", "J_Bip_L_Hand",
    "J_Bip_R_Shoulder", "J_Bip_R_UpperArm", "J_Bip_R_LowerArm", "J_Bip_R_Hand",
    "J_Bip_L_UpperLeg", "J_Bip_L_LowerLeg", "J_Bip_L_Foot", "J_Bip_L_ToeBase",
    "J_Bip_R_UpperLeg", "J_Bip_R_LowerLeg", "J_Bip_R_Foot", "J_Bip_R_ToeBase",
}


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    return parser.parse_args(argv)


def curve_variation(curve):
    values = [point.co.y for point in curve.keyframe_points]
    return max(values) - min(values) if values else 0.0


def scene_bounds():
    depsgraph = bpy.context.evaluated_depsgraph_get()
    points = []
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        evaluated = obj.evaluated_get(depsgraph)
        points.extend(evaluated.matrix_world @ Vector(corner) for corner in evaluated.bound_box)
    minimum = Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
    maximum = Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
    return maximum - minimum


def set_exact_frame(frame):
    whole = math.floor(frame)
    bpy.context.scene.frame_set(whole, subframe=frame - whole)


def main():
    args = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=args.input)
    armature = max(
        (obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"),
        key=lambda obj: len(obj.data.bones),
    )
    armature.animation_data_create()

    failed = False
    for action in sorted(bpy.data.actions, key=lambda item: item.name):
        moving_child_locations = [
            curve
            for curve in action.fcurves
            if curve.data_path.endswith("location")
            and "J_Bip_C_Hips" not in curve.data_path
            and curve_variation(curve) > 1.0e-5
        ]
        animated_scales = [
            curve
            for curve in action.fcurves
            if curve.data_path.endswith("scale") and curve_variation(curve) > 1.0e-5
        ]
        armature.animation_data.action = action
        start = int(round(action.frame_range[0]))
        end = int(round(action.frame_range[1]))
        set_exact_frame(action.frame_range[0])
        start_rotations = {
            bone.name: bone.matrix.to_quaternion().copy()
            for bone in armature.pose.bones
            if bone.name in LOCOMOTION_BONES
        }
        set_exact_frame(action.frame_range[1])
        seam_angles = {
            bone.name: min(
                start_rotations[bone.name].rotation_difference(bone.matrix.to_quaternion()).angle,
                2.0 * __import__('math').pi - start_rotations[bone.name].rotation_difference(bone.matrix.to_quaternion()).angle,
            )
            for bone in armature.pose.bones
            if bone.name in start_rotations
        }
        worst_seams = sorted(seam_angles.items(), key=lambda item: item[1], reverse=True)[:5]
        sample_frames = sorted({start, (start + end) // 2, end})
        dimensions = []
        for frame in sample_frames:
            bpy.context.scene.frame_set(frame)
            dimensions.append(tuple(round(value, 4) for value in scene_bounds()))
        print(
            f"VALIDATE {action.name}: frames={action.frame_range[:]}, "
            f"moving_child_locations={len(moving_child_locations)}, "
            f"animated_scales={len(animated_scales)}, dimensions={dimensions}, "
            f"loop_seam_degrees={[(name, round(__import__('math').degrees(angle), 2)) for name, angle in worst_seams]}"
        )
        failed |= bool(moving_child_locations or animated_scales)

    if failed:
        raise RuntimeError("Animated child translation or scale was found")


if __name__ == "__main__":
    main()
