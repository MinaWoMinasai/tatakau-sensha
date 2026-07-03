import argparse
import sys

import bpy


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    args = parser.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=args.input)
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        print(f"MESH {obj.name}: vertices={len(obj.data.vertices)} polygons={len(obj.data.polygons)}")
        counts = [0] * len(obj.material_slots)
        for polygon in obj.data.polygons:
            if polygon.material_index < len(counts):
                counts[polygon.material_index] += 1
        for index, slot in enumerate(obj.material_slots):
            material = slot.material
            print(
                f"  material[{index}]={material.name if material else '<none>'} "
                f"polygons={counts[index]} backface_culling={getattr(material, 'use_backface_culling', None)} "
                f"surface_method={getattr(material, 'surface_render_method', None)}"
            )


if __name__ == "__main__":
    main()
