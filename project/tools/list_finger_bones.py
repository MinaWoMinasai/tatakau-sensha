import argparse
import sys
import bpy


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    args = parser.parse_args(argv)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if args.input.lower().endswith(".fbx"):
        bpy.ops.import_scene.fbx(filepath=args.input, automatic_bone_orientation=False)
    else:
        bpy.ops.import_scene.gltf(filepath=args.input)
    armature = max(
        (obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"),
        key=lambda obj: len(obj.data.bones),
    )
    for bone in armature.data.bones:
        lower = bone.name.lower()
        if any(token in lower for token in ("hand", "thumb", "index", "middle", "ring", "little", "pinky")):
            print(bone.name)


if __name__ == "__main__":
    main()
