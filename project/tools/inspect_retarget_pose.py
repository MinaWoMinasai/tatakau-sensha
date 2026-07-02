import argparse
import sys

import bpy


SOURCE_BONES = [
    "mixamorig:Hips", "mixamorig:Spine", "mixamorig:Spine1", "mixamorig:Spine2",
    "mixamorig:LeftArm", "mixamorig:LeftForeArm", "mixamorig:LeftHand",
    "mixamorig:RightArm", "mixamorig:RightForeArm", "mixamorig:RightHand",
    "mixamorig:LeftUpLeg", "mixamorig:LeftLeg", "mixamorig:LeftFoot",
    "mixamorig:RightUpLeg", "mixamorig:RightLeg", "mixamorig:RightFoot",
]

TARGET_BONES = [
    "J_Bip_C_Hips", "J_Bip_C_Spine", "J_Bip_C_Chest", "J_Bip_C_UpperChest",
    "J_Bip_L_UpperArm", "J_Bip_L_LowerArm", "J_Bip_L_Hand",
    "J_Bip_R_UpperArm", "J_Bip_R_LowerArm", "J_Bip_R_Hand",
    "J_Bip_L_UpperLeg", "J_Bip_L_LowerLeg", "J_Bip_L_Foot",
    "J_Bip_R_UpperLeg", "J_Bip_R_LowerLeg", "J_Bip_R_Foot",
]


def args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True)
    parser.add_argument("--target", required=True)
    parser.add_argument("--clip", required=True)
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    return parser.parse_args(argv)


def armature():
    return max(
        (obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"),
        key=lambda obj: len(obj.data.bones),
    )


def print_pose(label, arm, bone_names, frame):
    bpy.context.scene.frame_set(frame)
    print(f"POSE {label} frame={frame}")
    for name in bone_names:
        bone = arm.pose.bones[name]
        head = bone.head
        tail = bone.tail
        direction = (tail - head).normalized()
        print(
            name,
            "head", tuple(round(v, 4) for v in head),
            "dir", tuple(round(v, 4) for v in direction),
        )


def main():
    options = args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=options.source, automatic_bone_orientation=False)
    source_arm = armature()
    source_action = source_arm.animation_data.action
    source_mid = int((source_action.frame_range[0] + source_action.frame_range[1]) // 2)
    print_pose("SOURCE", source_arm, SOURCE_BONES, source_mid)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=options.target)
    target_arm = armature()
    target_action = next(action for action in bpy.data.actions if action.name == options.clip)
    target_arm.animation_data_create()
    target_arm.animation_data.action = target_action
    target_mid = int((target_action.frame_range[0] + target_action.frame_range[1]) // 2)
    print_pose("TARGET", target_arm, TARGET_BONES, target_mid)


if __name__ == "__main__":
    main()
