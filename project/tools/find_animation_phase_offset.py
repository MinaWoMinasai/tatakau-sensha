import argparse
import math
import sys

import bpy


BONES = [
    "J_Bip_C_Hips", "J_Bip_C_Chest", "J_Bip_C_UpperChest",
    "J_Bip_L_UpperArm", "J_Bip_L_LowerArm", "J_Bip_L_Hand",
    "J_Bip_R_UpperArm", "J_Bip_R_LowerArm", "J_Bip_R_Hand",
    "J_Bip_L_UpperLeg", "J_Bip_L_LowerLeg", "J_Bip_L_Foot",
    "J_Bip_R_UpperLeg", "J_Bip_R_LowerLeg", "J_Bip_R_Foot",
]


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--source", default="Walk")
    parser.add_argument("--target", default="Run")
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    return parser.parse_args(argv)


def set_action_phase(scene, armature, action, phase):
    armature.animation_data.action = action
    start, end = action.frame_range
    frame = start + (end - start) * (phase % 1.0)
    whole = math.floor(frame)
    scene.frame_set(whole, subframe=frame - whole)


def capture_local_rotations(armature):
    return {name: armature.pose.bones[name].matrix_basis.to_quaternion().copy() for name in BONES}


def quaternion_angle(a, b):
    difference = a.rotation_difference(b).angle
    return min(difference, 2.0 * math.pi - difference)


def main():
    args = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=args.input)
    armature = max(
        (obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"),
        key=lambda obj: len(obj.data.bones),
    )
    armature.animation_data_create()
    source_action = next(action for action in bpy.data.actions if action.name == args.source)
    target_action = next(action for action in bpy.data.actions if action.name == args.target)
    scene = bpy.context.scene

    samples = 16
    source_poses = []
    for sample in range(samples):
        set_action_phase(scene, armature, source_action, sample / samples)
        source_poses.append(capture_local_rotations(armature))

    results = []
    candidates = 32
    for candidate in range(candidates):
        offset = candidate / candidates
        total = 0.0
        for sample in range(samples):
            phase = sample / samples
            set_action_phase(scene, armature, target_action, phase + offset)
            target_pose = capture_local_rotations(armature)
            total += sum(
                quaternion_angle(source_poses[sample][name], target_pose[name])
                for name in BONES
            )
        results.append((total / (samples * len(BONES)), offset))

    for error, offset in sorted(results)[:8]:
        print(
            "PHASE_CANDIDATE",
            "offset", round(offset, 6),
            "average_error_degrees", round(math.degrees(error), 4),
        )


if __name__ == "__main__":
    main()
