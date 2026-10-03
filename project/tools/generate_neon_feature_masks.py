"""Inspect AvatarSample_B UVs and reproduce the small Preview-only RG masks.

Requires the already available Pillow and NumPy. Never writes the input GLB.
All UVs are glTF TEXCOORD_0 (top-left texture origin), without an extra V flip.
"""

import argparse
import csv
import hashlib
import io
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MODEL = ROOT / "project/resources/models/neon_hologram/AvatarSample_B.glb"
DEFAULT_CONFIG = ROOT / "project/resources/models/neon_hologram/line_masks/authoring.json"
DEFAULT_INSPECTION = ROOT / "generated/neon_feature_mask_inspection"


class Glb:
    def __init__(self, path):
        data = path.read_bytes()
        if struct.unpack_from("<III", data) != (0x46546C67, 2, len(data)):
            raise ValueError("Expected a GLB 2.0 container")
        self.sha256 = hashlib.sha256(data).hexdigest().upper()
        chunks = {}
        offset = 12
        while offset < len(data):
            length, kind = struct.unpack_from("<II", data, offset)
            chunks[kind] = data[offset + 8:offset + 8 + length]
            offset += 8 + length
        self.doc = json.loads(chunks[0x4E4F534A])
        self.binary = chunks[0x004E4942]

    def accessor(self, index):
        accessor = self.doc["accessors"][index]
        view = self.doc["bufferViews"][accessor["bufferView"]]
        components = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[accessor["type"]]
        dtype = {5120: "i1", 5121: "u1", 5122: "<i2", 5123: "<u2", 5125: "<u4", 5126: "<f4"}[accessor["componentType"]]
        item = np.dtype(dtype).itemsize
        offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        return np.ndarray((accessor["count"], components), dtype=dtype, buffer=self.binary,
                          offset=offset, strides=(view.get("byteStride", item * components), item)).copy()

    def image_bytes(self, index):
        view = self.doc["bufferViews"][self.doc["images"][index]["bufferView"]]
        return self.binary[view.get("byteOffset", 0):view.get("byteOffset", 0) + view["byteLength"]]

    def image(self, index):
        return Image.open(io.BytesIO(self.image_bytes(index))).convert("RGBA")


def inspect(glb, output):
    output.mkdir(parents=True, exist_ok=True)
    report = {"model_sha256": glb.sha256, "uv_origin": "glTF TEXCOORD_0, image top-left; engine compensates Assimp V flip", "materials": [], "primitives": []}
    images = {}
    for i, material in enumerate(glb.doc["materials"]):
        texture = material.get("pbrMetallicRoughness", {}).get("baseColorTexture", {})
        image_index = glb.doc["textures"][texture["index"]]["source"] if "index" in texture else None
        item = {"index": i, "name": material.get("name", ""), "base_color_image": image_index,
                "texcoord": texture.get("texCoord", 0), "alpha_mode": material.get("alphaMode", "OPAQUE"),
                "alpha_cutoff": material.get("alphaCutoff", 0.5), "double_sided": material.get("doubleSided", False)}
        if image_index is not None:
            images.setdefault(image_index, glb.image(image_index))
            image = images[image_index]
            alpha = np.asarray(image.getchannel("A"))
            item.update(image_size=list(image.size), transparent_pixels=int(np.sum(alpha == 0)), partial_alpha_pixels=int(np.sum((alpha > 0) & (alpha < 255))))
            (output / f"base_image_{image_index}.png").write_bytes(glb.image_bytes(image_index))
        report["materials"].append(item)

    colors = [(255, 80, 150), (80, 255, 255), (255, 230, 80), (100, 200, 100), (120, 100, 255), (255, 180, 80), (220, 80, 220)]
    for mesh_index, mesh in enumerate(glb.doc["meshes"]):
        for primitive_index, primitive in enumerate(mesh["primitives"]):
            uv = glb.accessor(primitive["attributes"]["TEXCOORD_0"])
            pos = glb.accessor(primitive["attributes"]["POSITION"])
            triangles = glb.accessor(primitive["indices"]).ravel().reshape(-1, 3)
            material = report["materials"][primitive["material"]]
            image = images[material["base_color_image"]].resize((1024, 1024), Image.Resampling.LANCZOS)
            overlay = Image.new("RGBA", image.size)
            draw = ImageDraw.Draw(overlay)
            for triangle in triangles:
                draw.polygon([tuple(point * 1023) for point in uv[triangle]], outline=colors[primitive["material"]] + (150,))
            Image.alpha_composite(image, overlay).save(output / f"uv_mesh{mesh_index}_primitive{primitive_index}.png")
            np.savez_compressed(output / f"mesh{mesh_index}_primitive{primitive_index}.npz", uv=uv, position=pos, triangles=triangles)
            used = np.unique(triangles)
            item = {"mesh": mesh_index, "mesh_name": mesh.get("name", ""), "primitive": primitive_index,
                    "material": material["name"], "material_index": primitive["material"], "vertices": len(used), "triangles": len(triangles),
                    "uv_min": uv[used].min(axis=0).tolist(), "uv_max": uv[used].max(axis=0).tolist(),
                    "position_min": pos[used].min(axis=0).tolist(), "position_max": pos[used].max(axis=0).tolist(),
                    "uv_unique": len(np.unique(uv[used], axis=0))}
            report["primitives"].append(item)
            # UV maps coloured by object-space Z expose front/back overlap without changing UVs.
            coord_map = Image.new("RGBA", (1024, 1024))
            coord_draw = ImageDraw.Draw(coord_map)
            for triangle in triangles:
                centroid = pos[triangle].mean(axis=0)
                color = (255, 80, 130, 120) if centroid[2] >= 0 else (50, 210, 255, 120)
                coord_draw.polygon([tuple(point * 1023) for point in uv[triangle]], fill=color)
            Image.alpha_composite(image, coord_map).save(output / f"uv_depth_mesh{mesh_index}_primitive{primitive_index}.png")
    (output / "uv_material_report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    hair = glb.doc["meshes"][2]["primitives"][0]
    uv = glb.accessor(hair["attributes"]["TEXCOORD_0"])
    pos = glb.accessor(hair["attributes"]["POSITION"])
    quantized = np.round(uv * 1000000).astype(int)
    front = (pos[:, 2] < -0.035) & (pos[:, 1] > 1.35)
    back = pos[:, 2] > 0.035
    front_uv = set(map(tuple, quantized[front]))
    back_uv = set(map(tuple, quantized[back]))
    shared = front_uv & back_uv
    replacement = (uv[:, 0] >= 0.542) & (uv[:, 0] <= 0.685) & (uv[:, 1] >= 0.16) & (uv[:, 1] <= 0.365)
    unique_uv, inverse = np.unique(quantized, axis=0, return_inverse=True)
    multi_position = []
    for group in range(len(unique_uv)):
        indices = np.flatnonzero(inverse == group)
        if len(indices) > 1 and np.max(np.ptp(pos[indices], axis=0)) > 0.0001:
            multi_position.append(indices)
    audit = {"front_rule": "object-space Z < -0.035 and Y > 1.35", "back_rule": "Z > 0.035",
             "front_vertices": int(front.sum()), "back_vertices": int(back.sum()), "shared_quantized_uvs": len(shared),
             "same_uv_distinct_positions_groups": len(multi_position),
             "candidate_same_uv_distinct_positions_groups": sum(bool(replacement[indices[0]]) for indices in multi_position),
             "candidate_region_front_vertices": int(np.sum(replacement & front)),
             "candidate_region_back_vertices": int(np.sum(replacement & back)),
             "constraint": "Hair atlas has repeated UVs; material/UV mask cannot select only one geometric strand"}
    (output / "hair_uv_sharing.json").write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(audit, indent=2))


def bezier(points, samples=160):
    if len(points) != 4:
        raise ValueError("A curve requires exactly four cubic Bezier control points")
    p = np.asarray(points, dtype=float)
    t = np.linspace(0, 1, samples)[:, None]
    return (1-t)**3*p[0] + 3*(1-t)**2*t*p[1] + 3*(1-t)*t**2*p[2] + t**3*p[3]


def compare_assimp(glb, source):
    materials = {}
    for mesh in glb.doc["meshes"]:
        for primitive in mesh["primitives"]:
            used = np.unique(glb.accessor(primitive["indices"]).ravel())
            name = glb.doc["materials"][primitive["material"]]["name"]
            materials[name] = (glb.accessor(primitive["attributes"]["POSITION"])[used],
                               glb.accessor(primitive["attributes"]["TEXCOORD_0"])[used])
    report = {"model_sha256": glb.sha256, "samples_compared": 0, "max_uv_error": 0.0, "examples": []}
    examples_per_material = {}
    with source.open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            position, uv = materials[row["material"]]
            point = np.asarray([float(row[key]) for key in ("x", "y", "z")])
            candidates = np.max(np.abs(position - point), axis=1) < 0.0000002
            if not candidates.any():
                raise ValueError(f"Assimp position has no glTF match: {row}")
            expected = uv[candidates]
            engine_uv = np.asarray([float(row["engineU"]), float(row["engineV"])])
            assimp_uv = np.asarray([float(row["assimpU"]), 1.0-float(row["assimpV"])])
            error = float(min(np.max(np.abs(expected-engine_uv), axis=1)))
            if error > 0.000002 or np.max(np.abs(engine_uv-assimp_uv)) > 0.000002:
                raise ValueError(f"UV direction/mapping mismatch: {row}")
            report["samples_compared"] += 1
            report["max_uv_error"] = max(report["max_uv_error"], error)
            count = examples_per_material.get(row["material"], 0)
            if count < 2:
                report["examples"].append({"material": row["material"], "glTF_uv": expected[np.argmin(np.max(np.abs(expected-engine_uv), axis=1))].tolist(),
                                           "assimp_uv": [float(row["assimpU"]), float(row["assimpV"])], "engine_uv": engine_uv.tolist()})
                examples_per_material[row["material"]] = count + 1
    if report["samples_compared"] < 100 or len(examples_per_material) != 7:
        raise ValueError("Expected samples covering all seven Assimp submeshes")
    (DEFAULT_INSPECTION / "assimp_uv_comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


def generate(glb, config_path, output):
    config = json.loads(config_path.read_text(encoding="utf-8"))
    if glb.sha256 != config["model_sha256"]:
        raise ValueError("Input model hash differs from the inspected authoring source")
    if config["schema_version"] != 1 or config["supersample"] != 4:
        raise ValueError("Expected authoring schema 1 with four-times supersampling")
    for mask in config["masks"]:
        if Path(mask["file"]).name != mask["file"] or not mask["file"].endswith(".png") or mask["size"] != 1024:
            raise ValueError("Mask outputs must be 1024-square PNG basenames")
        for region in mask["replace_regions"]:
            points = np.asarray(region["polygon"], dtype=float)
            if points.ndim != 2 or points.shape[1] != 2 or len(points) < 3 or not np.isfinite(points).all() or np.any((points < 0) | (points > 1)):
                raise ValueError("Replacement polygons require finite normalized UV coordinates")
            weight = region.get("weight", 1)
            if not np.isfinite(weight) or not 0 <= weight <= 1:
                raise ValueError("Replacement weights must be in [0,1]")
        for curve in mask["curves"]:
            points = np.asarray(curve["points"], dtype=float)
            width, coverage = curve["width_texels"], curve.get("coverage", 1)
            if points.shape != (4, 2) or not np.isfinite(points).all() or np.any((points < 0) | (points > 1)):
                raise ValueError("Curves require four finite normalized UV control points")
            if not np.isfinite([width, coverage]).all() or not 0 < width <= 8 or not 0 <= coverage <= 1:
                raise ValueError("Curve width/coverage is outside the small candidate range")
    output.mkdir(parents=True, exist_ok=True)
    inspection = DEFAULT_INSPECTION
    inspection.mkdir(parents=True, exist_ok=True)
    for mask in config["masks"]:
        size = mask["size"]
        aa = config["supersample"]
        working = size * aa
        red = Image.new("L", (working, working))
        green = Image.new("L", (working, working))
        rdraw, gdraw = ImageDraw.Draw(red), ImageDraw.Draw(green)
        for region in mask["replace_regions"]:
            points = [(u*working, v*working) for u, v in region["polygon"]]
            gdraw.polygon(points, fill=round(255*region.get("weight", 1)))
        for curve in mask["curves"]:
            points = bezier(curve["points"]) * working
            width = max(1, round(curve["width_texels"] * aa))
            rdraw.line([tuple(point) for point in points], fill=round(curve.get("coverage", 1)*255), width=width, joint="curve")
            radius = width / 2
            for x, y in (points[0], points[-1]):
                rdraw.ellipse((x-radius, y-radius, x+radius, y+radius), fill=round(curve.get("coverage", 1)*255))
        red = red.resize((size, size), Image.Resampling.LANCZOS)
        green = green.resize((size, size), Image.Resampling.LANCZOS)
        image = Image.merge("RGBA", (red, green, Image.new("L", (size, size), 0), Image.new("L", (size, size), 255)))
        image.save(output / mask["file"])
        Image.merge("RGB", (red, red, red)).save(inspection / (Path(mask["file"]).stem + "_R.png"))
        Image.merge("RGB", (green, green, green)).save(inspection / (Path(mask["file"]).stem + "_G.png"))
        base = glb.image(mask["base_color_image"]).resize((size, size), Image.Resampling.LANCZOS)
        overlay = Image.merge("RGBA", (Image.new("L", (size, size), 255), Image.new("L", (size, size), 40), Image.new("L", (size, size), 160), red))
        Image.alpha_composite(base, overlay).save(inspection / (Path(mask["file"]).stem + "_selected_overlay.png"))
        print(f"Created {output / mask['file']} ({size}x{size}, B=0, A=255)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--inspect", action="store_true")
    parser.add_argument("--verify", action="store_true", help="Rebuild into generated and validate deterministic RG data")
    parser.add_argument("--compare-assimp", type=Path, help="Compare Assimp UV probe CSV with raw glTF POSITION/TEXCOORD_0")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    glb = Glb(args.model)
    if args.compare_assimp:
        compare_assimp(glb, args.compare_assimp)
    elif args.inspect:
        inspect(glb, args.output or DEFAULT_INSPECTION)
    elif args.verify:
        validation = DEFAULT_INSPECTION / "regenerated"
        generate(glb, args.config, validation)
        config = json.loads(args.config.read_text(encoding="utf-8"))
        report = {"model_sha256": glb.sha256, "masks": []}
        for mask in config["masks"]:
            original = args.config.parent / mask["file"]
            candidate = validation / mask["file"]
            if original.read_bytes() != candidate.read_bytes():
                raise ValueError(f"Non-deterministic or stale candidate: {mask['file']}")
            image = Image.open(original)
            pixels = np.asarray(image)
            if image.mode != "RGBA" or image.size != (mask["size"], mask["size"]) or pixels.dtype != np.uint8:
                raise ValueError("Expected unpremultiplied RGBA8 data")
            if np.any(pixels[:, :, 2] != 0) or np.any(pixels[:, :, 3] != 255):
                raise ValueError("B must be 0 and A must be 255")
            if "srgb" in image.info or "gamma" in image.info:
                raise ValueError("A data mask must not carry color conversion metadata")
            red, green = pixels[:, :, 0], pixels[:, :, 1]
            middle_red = int(np.sum((red > 0) & (red < 255)))
            middle_green = int(np.sum((green > 0) & (green < 255)))
            clear_auto = int(np.sum((green == 255) & (red == 0)))
            if not middle_red or not middle_green or not clear_auto:
                raise ValueError("Expected AA coverage/replace values and G=1/R=0 pixels")
            if not np.all(pixels[0, :, 0:2] == 0) or not np.all(pixels[:, 0, 0:2] == 0):
                raise ValueError("Unexpected lines at the atlas edge")
            # Bound the authored data to the selected polygons/curve hulls plus
            # four texels for reconstruction filtering; other atlas areas stay G=0.
            selected = Image.new("L", image.size)
            selected_draw = ImageDraw.Draw(selected)
            for region in mask["replace_regions"]:
                points = np.asarray(region["polygon"]) * mask["size"]
                low, high = points.min(axis=0) - 4, points.max(axis=0) + 4
                selected_draw.rectangle(tuple(low) + tuple(high), fill=255)
            for curve in mask["curves"]:
                points = np.asarray(curve["points"]) * mask["size"]
                low, high = points.min(axis=0) - 8, points.max(axis=0) + 8
                selected_draw.rectangle(tuple(low) + tuple(high), fill=255)
            if np.any(((red > 0) | (green > 0)) & (np.asarray(selected) == 0)):
                raise ValueError("Authored mask leaked outside the selected feature regions")
            report["masks"].append({"file": mask["file"], "sha256": hashlib.sha256(original.read_bytes()).hexdigest().upper(),
                                    "size": list(image.size), "r_nonzero_pixels": int(np.sum(red > 0)),
                                    "g_nonzero_pixels": int(np.sum(green > 0)), "r_middle_pixels": middle_red,
                                    "g_middle_pixels": middle_green, "g_one_r_zero_pixels": clear_auto, "deterministic": True})
        (DEFAULT_INSPECTION / "mask_data_validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
    else:
        generate(glb, args.config, args.output or args.config.parent)


if __name__ == "__main__":
    main()
