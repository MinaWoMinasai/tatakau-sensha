import argparse
import os

import bpy
from mathutils import Matrix, Quaternion


BONE_MAP = {
    "mixamorig:Hips": "J_Bip_C_Hips",
    "mixamorig:Spine": "J_Bip_C_Spine",
    "mixamorig:Spine1": "J_Bip_C_Chest",
    "mixamorig:Spine2": "J_Bip_C_UpperChest",
    "mixamorig:Neck": "J_Bip_C_Neck",
    "mixamorig:Head": "J_Bip_C_Head",
    "mixamorig:LeftShoulder": "J_Bip_L_Shoulder",
    "mixamorig:LeftArm": "J_Bip_L_UpperArm",
    "mixamorig:LeftForeArm": "J_Bip_L_LowerArm",
    "mixamorig:LeftHand": "J_Bip_L_Hand",
    "mixamorig:RightShoulder": "J_Bip_R_Shoulder",
    "mixamorig:RightArm": "J_Bip_R_UpperArm",
    "mixamorig:RightForeArm": "J_Bip_R_LowerArm",
    "mixamorig:RightHand": "J_Bip_R_Hand",
    "mixamorig:LeftUpLeg": "J_Bip_L_UpperLeg",
    "mixamorig:LeftLeg": "J_Bip_L_LowerLeg",
    "mixamorig:LeftFoot": "J_Bip_L_Foot",
    "mixamorig:LeftToeBase": "J_Bip_L_ToeBase",
    "mixamorig:RightUpLeg": "J_Bip_R_UpperLeg",
    "mixamorig:RightLeg": "J_Bip_R_LowerLeg",
    "mixamorig:RightFoot": "J_Bip_R_Foot",
    "mixamorig:RightToeBase": "J_Bip_R_ToeBase",
}

# Mixamo attack clips contain keyed finger poses.  Keeping only the hand joint
# leaves the VRoid bind-pose fingers open, making every weapon attack look like
# a karate chop.  Mixamo's fourth finger node is an unskinned tip, so map the
# three deforming segments that both rigs share.
for mixamo_side, target_side in (("Left", "L"), ("Right", "R")):
    for mixamo_finger, target_finger in (
        ("Thumb", "Thumb"),
        ("Index", "Index"),
        ("Middle", "Middle"),
        ("Ring", "Ring"),
        ("Pinky", "Little"),
    ):
        for segment in range(1, 4):
            BONE_MAP[
                f"mixamorig:{mixamo_side}Hand{mixamo_finger}{segment}"
            ] = f"J_Bip_{target_side}_{target_finger}{segment}"


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--clip", action="append", nargs=2, metavar=("NAME", "FBX"), required=True)
    argv = []
    if "--" in __import__("sys").argv:
        argv = __import__("sys").argv[__import__("sys").argv.index("--") + 1 :]
    return parser.parse_args(argv)


def find_armature(objects, preferred_name=None):
    armatures = [obj for obj in objects if obj.type == "ARMATURE"]
    if preferred_name:
        for obj in armatures:
            if obj.name == preferred_name:
                return obj
    if not armatures:
        raise RuntimeError("Armature was not found")
    return max(armatures, key=lambda obj: len(obj.data.bones))


def clear_pose(armature):
    for bone in armature.pose.bones:
        bone.matrix_basis.identity()


def retarget_clip(target_armature, clip_name, fbx_path):
    before_objects = set(bpy.data.objects)
    before_actions = set(bpy.data.actions)
    bpy.ops.import_scene.fbx(filepath=fbx_path, automatic_bone_orientation=False)
    imported_objects = [obj for obj in bpy.data.objects if obj not in before_objects]
    source_armature = find_armature(imported_objects)
    if source_armature.animation_data is None or source_armature.animation_data.action is None:
        raise RuntimeError(f"Animation was not found in {fbx_path}")
    source_action = source_armature.animation_data.action
    start = int(round(source_action.frame_range[0]))
    end = int(round(source_action.frame_range[1]))

    missing = [src for src, dst in BONE_MAP.items() if src not in source_armature.pose.bones or dst not in target_armature.pose.bones]
    if missing:
        raise RuntimeError(f"Required retarget bones are missing: {missing}")

    action = bpy.data.actions.new(clip_name)
    action.use_fake_user = True
    target_armature.animation_data_create()
    target_armature.animation_data.action = action
    clear_pose(target_armature)

    source_rest = {name: source_armature.data.bones[name].matrix_local.copy() for name in BONE_MAP}
    target_rest = {name: target_armature.data.bones[name].matrix_local.copy() for name in BONE_MAP.values()}
    source_object_rotation = source_armature.matrix_world.to_quaternion()
    target_object_rotation = target_armature.matrix_world.to_quaternion()

    scene = bpy.context.scene
    scene.frame_start = start
    scene.frame_end = end
    first_baked_rotations = {}
    is_looping = clip_name in {"Idle", "Walk", "Run", "FallingIdle"}
    loop_blend_frames = min(12, max(4, (end - start) // 5)) if is_looping else 0
    for frame in range(start, end + 1):
        scene.frame_set(frame)
        for source_name, target_name in BONE_MAP.items():
            source_pose = source_armature.pose.bones[source_name].matrix.copy()
            source_rest_matrix = source_rest[source_name]
            target_rest_matrix = target_rest[target_name]

            target_pose_bone = target_armature.pose.bones[target_name]
            target_pose_bone.rotation_mode = "QUATERNION"

            # Transfer only this bone's parent-relative delta.  A world-space pose
            # delta also contains every ancestor's rotation; applying that delta to
            # Spine, Chest, UpperChest, Neck and the arms independently accumulates
            # the same turn many times and makes the upper body twist wildly.
            source_pose_bone = source_armature.pose.bones[source_name]
            source_rest_bone = source_armature.data.bones[source_name]
            if source_pose_bone.parent is not None:
                source_pose_local = source_pose_bone.parent.matrix.inverted() @ source_pose
                source_rest_local = source_rest_bone.parent.matrix_local.inverted() @ source_rest_matrix
                source_parent_rest_world = (
                    source_object_rotation @ source_rest_bone.parent.matrix_local.to_quaternion()
                )
            else:
                source_pose_local = source_pose
                source_rest_local = source_rest_matrix
                source_parent_rest_world = source_object_rotation

            if target_pose_bone.parent is not None:
                target_parent_rest = target_pose_bone.parent.bone.matrix_local
                target_rest_local = target_parent_rest.inverted() @ target_rest_matrix
                target_parent_rest_world = (
                    target_object_rotation @ target_parent_rest.to_quaternion()
                )
                target_parent_pose_rotation = target_pose_bone.parent.matrix.to_quaternion()
            else:
                target_rest_local = target_rest_matrix
                target_parent_rest_world = target_object_rotation
                target_parent_pose_rotation = Quaternion()

            source_local_delta = (
                source_pose_local.to_quaternion() @ source_rest_local.to_quaternion().inverted()
            )
            delta_world = (
                source_parent_rest_world
                @ source_local_delta
                @ source_parent_rest_world.inverted()
            )
            target_local_delta = (
                target_parent_rest_world.inverted()
                @ delta_world
                @ target_parent_rest_world
            )
            target_local_pose_rotation = (
                target_local_delta @ target_rest_local.to_quaternion()
            )
            target_rotation = target_parent_pose_rotation @ target_local_pose_rotation

            # Keep the target skeleton's original parent/child connection and bone length.
            # Assigning the source/global position here creates local translation keys on
            # child bones and stretches the skinned mesh when the parent rotates.
            if target_pose_bone.parent is not None:
                target_local_rest = target_parent_rest.inverted() @ target_rest_matrix
                attached_matrix = target_pose_bone.parent.matrix @ target_local_rest
                attached_location = attached_matrix.to_translation()
            else:
                attached_location = target_rest_matrix.to_translation()

            target_pose_bone.matrix = Matrix.LocRotScale(
                attached_location,
                target_rotation,
                target_rest_matrix.to_scale(),
            )
            baked_rotation = target_pose_bone.rotation_quaternion.copy()
            if frame == start:
                first_baked_rotations[target_name] = baked_rotation.copy()
            elif loop_blend_frames > 0 and frame > end - loop_blend_frames:
                loop_t = (frame - (end - loop_blend_frames)) / loop_blend_frames
                smooth_loop_t = loop_t * loop_t * (3.0 - 2.0 * loop_t)
                baked_rotation = baked_rotation.slerp(
                    first_baked_rotations[target_name], smooth_loop_t
                )
            target_pose_bone.location = (0.0, 0.0, 0.0)
            target_pose_bone.scale = (1.0, 1.0, 1.0)
            target_pose_bone.rotation_quaternion = baked_rotation

            # Location and scale deliberately remain at their bind-pose values.
            # Only rotation is transferred from Mixamo.
            target_pose_bone.keyframe_insert("rotation_quaternion", frame=frame, group=target_name)

    action.frame_start = start
    action.frame_end = end
    target_armature.animation_data.action = None
    clear_pose(target_armature)

    for obj in imported_objects:
        bpy.data.objects.remove(obj, do_unlink=True)
    for imported_action in [item for item in bpy.data.actions if item not in before_actions and item != action]:
        bpy.data.actions.remove(imported_action)

    print(
        f"RETARGETED {clip_name}: frames={start}-{end}, "
        f"mapped_bones={len(BONE_MAP)}, rotation_only=True, "
        f"loop_blend_frames={loop_blend_frames}"
    )
    return action


def main():
    args = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=args.target)
    target_armature = find_armature(list(bpy.context.scene.objects))

    # Remove any placeholder/imported animation while preserving the source model and skin.
    for action in list(bpy.data.actions):
        bpy.data.actions.remove(action)

    actions = []
    for clip_name, clip_path in args.clip:
        actions.append(retarget_clip(target_armature, clip_name, clip_path))

    target_armature.animation_data_create()
    target_armature.animation_data.action = actions[0]
    bpy.context.scene.frame_start = int(actions[0].frame_range[0])
    bpy.context.scene.frame_end = int(actions[0].frame_range[1])
    bpy.context.scene.render.fps = 30

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=args.output,
        export_format="GLB",
        export_animations=True,
        export_animation_mode="ACTIONS",
        # Only the 22 explicitly keyed humanoid joints need animation channels.
        # Force-sampling every VRoid helper bone multiplies memory/file size by
        # the number of clips and becomes unstable once combo actions are added.
        export_force_sampling=False,
        export_skins=True,
        export_morph=True,
        export_materials="EXPORT",
    )
    print(f"EXPORTED {args.output}: actions={[action.name for action in actions]}")


if __name__ == "__main__":
    main()
