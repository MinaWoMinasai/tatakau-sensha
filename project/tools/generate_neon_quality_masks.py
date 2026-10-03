"""Offline line-quality comparison data, preserving the earlier RG candidates.

Uses the existing Pillow/NumPy environment and the previous GLB inspection code.
No scipy, external service, runtime generation, or input-model writes are needed.
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
import PIL
from PIL import Image, ImageDraw

from generate_neon_feature_masks import Glb, DEFAULT_MODEL, ROOT, bezier


ASSETS = ROOT / "project/resources/models/neon_hologram/line_masks/quality"
INSPECTION = ROOT / "generated/neon_quality_mask_inspection"
CONFIG = ASSETS / "authoring.json"


def validate(config, glb):
    if config["schemaVersion"] != 1 or config["modelSha256"] != glb.sha256:
        raise ValueError("Quality authoring schema/input hash mismatch")
    if config["size"] != 1024 or config["supersample"] != 4:
        raise ValueError("This bounded comparison uses 1024-square data and 4x AA")
    if config["sdfRangeTexels"] != 16 or not np.isfinite(config["haloWidthTexels"]) or not 0 < config["haloWidthTexels"] <= 8:
        raise ValueError("Expected signed 16-texel distance range and a narrow halo")
    for version in config["versions"]:
        if version["id"] not in ("v1", "v2"):
            raise ValueError("Version must be a comparison basename")
        for mask in version["masks"]:
            if mask["id"] not in ("face", "bangs"):
                raise ValueError("Only the inspected face/bangs regions are supported")
            for curve in mask["curves"]:
                points = np.asarray(curve["points"], dtype=float)
                if points.shape != (4, 2) or not np.isfinite(points).all() or np.any((points < 0) | (points > 1)):
                    raise ValueError("Bezier points must be finite normalized UVs")
                if not np.isfinite([curve["widthTexels"], curve["opacity"]]).all() or not 0 < curve["widthTexels"] <= 8 or not 0 <= curve["opacity"] <= 1:
                    raise ValueError("Invalid shape width/independent opacity")
            for region in mask["replaceRegions"]:
                polygon = np.asarray(region["polygon"], dtype=float)
                if polygon.ndim != 2 or polygon.shape[1] != 2 or len(polygon) < 3 or not np.isfinite(polygon).all() or np.any((polygon < 0) | (polygon > 1)):
                    raise ValueError("Invalid bounded replacement polygon")


def curve_field(curves, size, spread):
    """Signed distance of the union of round-cap variable-width line regions.

    Distance is positive inside, in output texels. Geometry uses 512 cubic
    segments, independent of opacity. Each segment is a capsule; min outside
    distance (max signed distance) gives the union including joins/intersections.
    Local rectangles avoid a full atlas x curve sample temporary allocation.
    """
    distance = np.full((size, size), -spread, dtype=np.float32)
    opacity = np.zeros((size, size), dtype=np.float32)
    for curve in curves:
        points = bezier(curve["points"], 513) * size
        radius = curve["widthTexels"] * .5
        for a, b in zip(points[:-1], points[1:]):
            low = np.maximum(0, np.floor(np.minimum(a, b) - radius - spread - 1).astype(int))
            high = np.minimum(size, np.ceil(np.maximum(a, b) + radius + spread + 1).astype(int))
            if np.any(low >= high):
                continue
            yy, xx = np.mgrid[low[1]:high[1], low[0]:high[0]]
            p = np.stack((xx + .5, yy + .5), axis=2)
            ab = b - a
            t = np.clip(np.sum((p - a) * ab, axis=2) / max(float(ab @ ab), 1e-18), 0, 1)
            signed = radius - np.sqrt(np.sum((p - a - t[..., None] * ab) ** 2, axis=2))
            area = np.s_[low[1]:high[1], low[0]:high[0]]
            nearer = signed > distance[area]
            distance[area][nearer] = signed[nearer]
            opacity[area][nearer] = curve["opacity"]
    return distance, opacity


def coverage(curves, size, aa):
    image = Image.new("L", (size * aa, size * aa))
    # Max compose each curve rather than making draw order alter intersections.
    result = np.zeros((size * aa, size * aa), dtype=np.uint8)
    for curve in curves:
        layer = Image.new("L", image.size)
        draw = ImageDraw.Draw(layer)
        points = bezier(curve["points"], 513) * size * aa
        width = max(1, round(curve["widthTexels"] * aa))
        value = round(curve["opacity"] * 255)
        draw.line(list(map(tuple, points)), fill=value, width=width, joint="curve")
        r = width * .5
        for x, y in (points[0], points[-1]):
            draw.ellipse((x-r, y-r, x+r, y+r), fill=value)
        np.maximum(result, np.asarray(layer), out=result)
    return np.asarray(Image.fromarray(result).resize((size, size), Image.Resampling.LANCZOS)).copy()


def replacement(mask, distance, halo_width, size, aa):
    regions = Image.new("L", (size * aa, size * aa))
    draw = ImageDraw.Draw(regions)
    for region in mask["replaceRegions"]:
        draw.polygon([(u*size*aa, v*size*aa) for u, v in region["polygon"]], fill=255)
    regions = np.asarray(regions.resize((size, size), Image.Resampling.LANCZOS)).copy()
    # Broad erase regions stay G=1/R=0. Around each new curve, G includes the
    # entire narrow halo and 2 extra texels of smooth transition, so there is no
    # rectangular clipping boundary through its light. This does not expose
    # normally occluded surfaces or bypass the model's original alpha.
    support = np.clip((distance + halo_width + 2) / 2, 0, 1)
    support = support*support*(3-2*support)
    return np.maximum(regions, np.rint(support * 255).astype(np.uint8))


def make_mask(mask, config):
    size, aa, spread = config["size"], config["supersample"], config["sdfRangeTexels"]
    distance, opacity = curve_field(mask["curves"], size, spread)
    red = coverage(mask["curves"], size, aa)
    green = replacement(mask, distance, config["haloWidthTexels"], size, aa)
    halo = np.clip(1 + np.minimum(distance, 0) / config["haloWidthTexels"], 0, 1) ** 2 * opacity
    blue = np.rint(halo * 255).astype(np.uint8)
    alpha = np.full((size, size), 255, dtype=np.uint8)
    coverage_rgba = np.stack((red, green, blue, alpha), axis=2)
    encoded = np.rint(np.clip(.5 + distance / (2 * spread), 0, 1) * 255).astype(np.uint8)
    sdf_rgba = np.stack((encoded, green, np.rint(opacity * 255).astype(np.uint8), alpha), axis=2)
    return coverage_rgba, sdf_rgba, distance


def png_chunks(data):
    offset = 8
    result = []
    while offset < len(data):
        size = int.from_bytes(data[offset:offset+4], "big")
        result.append(data[offset+4:offset+8])
        offset += size + 12
    return result


def generate(config, glb, output, inspect=True):
    validate(config, glb)
    output.mkdir(parents=True, exist_ok=True)
    INSPECTION.mkdir(parents=True, exist_ok=True)
    files, metrics = [], []
    for version in config["versions"]:
        for mask in version["masks"]:
            cov, sdf, distance = make_mask(mask, config)
            stem = mask["id"] + "_" + version["id"]
            for kind, pixels in (("coverage", cov), ("sdf", sdf)):
                file = stem + "_" + kind + ".png"
                Image.fromarray(pixels).save(output / file)
                files.append(file)
            if inspect:
                base = glb.image(mask["baseColorImage"]).resize((config["size"], config["size"]), Image.Resampling.LANCZOS)
                overlay = Image.fromarray(np.stack((np.full_like(cov[:,:,0],255), np.full_like(cov[:,:,0],45), np.full_like(cov[:,:,0],150), cov[:,:,0]),axis=2))
                Image.alpha_composite(base, overlay).save(INSPECTION / (stem + "_selected_overlay.png"))
                for channel, index in (("R",0),("G",1),("Halo",2)):
                    Image.fromarray(cov[:,:,index]).save(INSPECTION / (stem + "_" + channel + ".png"))
            outside = np.maximum(-distance, 0)
            # SDF quantisation error within encoded range is bounded by half
            # of one 8-bit distance interval, not a visual-quality assertion.
            decoded = (sdf[:,:,0].astype(float)/255-.5)*(2*config["sdfRangeTexels"])
            max_error = float(np.max(np.abs(decoded-distance)))
            if max_error > config["sdfRangeTexels"]/255+1e-5:
                raise ValueError("Distance quantisation exceeds the declared half-step")
            if not np.any((cov[:,:,1] == 255) & (cov[:,:,0] == 0)):
                raise ValueError("Required erase-only region is missing")
            if np.any((outside < config["haloWidthTexels"]-.2) & (cov[:,:,2] > 1) & (cov[:,:,1] < 254)):
                raise ValueError("G cuts through a nonzero narrow halo")
            if np.any(cov[[0,-1],:,:3]) or np.any(cov[:,[0,-1],:3]) or np.any(sdf[[0,-1],:,:3]) or np.any(sdf[:,[0,-1],:3]):
                raise ValueError("Unexpected line field at the atlas border")
            weak = [c for c in mask["curves"] if 0 < c["opacity"] < 1]
            for curve in weak:
                middle = bezier(curve["points"],513)[256] * config["size"]
                x,y = np.clip(middle.astype(int),0,config["size"]-1)
                if sdf[y,x,0] < 128 or not 0 < sdf[y,x,2] < 255:
                    raise ValueError("A weak-opacity line lost its geometric inside region")
            metrics.append({"id": stem, "curves":len(mask["curves"]), "r_sum":float(cov[:,:,0].sum()/255),
                "halo_sum":float(cov[:,:,2].sum()/255), "erase_only_pixels":int(np.sum((cov[:,:,1]==255)&(cov[:,:,0]==0))),
                "distance_max_quantisation_error_texels":max_error,"geometry_preserves_weak_lines":True})
    return files, metrics


def diagnose_minification(config_path, output, files):
    """Keep offline texture filtering evidence separate from GPU/image claims."""
    sources = [(config_path.parent.parent / name, name) for name in ("face_candidate.png", "bangs_candidate.png")]
    sources += [(output/name, "quality/"+name) for name in files if name.endswith("_coverage.png")]
    report = {"note":"Offline BOX linear-coverage minification; not GPU Bloom or final-image quality measurement", "levels":[]}
    for path, label in sources:
        with Image.open(path) as im:
            red = im.getchannel("R")
            for size in (1024,512,256,128,64):
                pixels = np.asarray(red.resize((size,size),Image.Resampling.BOX))/255
                nonzero = pixels[pixels > 0]
                report["levels"].append({"file":label,"width":size,"nonzero_pixels":int(len(nonzero)),
                    "max_coverage":float(pixels.max()),"coverage_sum_in_1024_texel_area":float(pixels.sum()*(1024/size)**2),
                    "nonzero_coverage_p50":float(np.percentile(nonzero,50)),"fraction_over_0_5":float(np.mean(nonzero>.5))})
    (INSPECTION/"coverage_minification.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")


def binding_provenance(config_path):
    """Hash each version independently and bind the actual authored PNG pairs."""
    config=json.loads(config_path.read_text(encoding="utf-8"))
    bindings_path=config_path.parent/"bindings.json"
    bindings=json.loads(bindings_path.read_text(encoding="utf-8"))
    overall=hashlib.sha256(config_path.read_bytes()).hexdigest().upper()
    versions={v["id"]:v for v in config["versions"]}
    for version in bindings["versions"]:
        source_version=versions[version["id"]]
        source={key:config[key] for key in ("size","supersample","sdfRangeTexels","haloWidthTexels")}
        source["version"]=source_version
        canonical=json.dumps(source,sort_keys=True,separators=(",",":"),ensure_ascii=False).encode("utf-8")
        version["authoringSha256"]=overall
        version["authoringVersionSha256"]=hashlib.sha256(canonical).hexdigest().upper()
        version["authoringRevision"]=source_version.get("revision",source_version["id"]+"-r1")
        for binding in version["bindings"]:
            for kind in ("coverage","sdf"):
                name=binding[kind+"File"]
                if Path(name).name != name or not name.endswith(".png"):
                    raise ValueError("Provenance must refer to a PNG basename in the quality directory")
                binding[kind+"Sha256"]=hashlib.sha256((config_path.parent/name).read_bytes()).hexdigest().upper()
    return bindings


def sync_binding_provenance(config_path):
    bindings=binding_provenance(config_path)
    (config_path.parent/"bindings.json").write_text(json.dumps(bindings,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL)
    parser.add_argument("--config", type=Path, default=CONFIG)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    glb = Glb(args.model)
    before_hash = hashlib.sha256(args.model.read_bytes()).hexdigest().upper()
    config = json.loads(args.config.read_text(encoding="utf-8"))
    output = args.output or (INSPECTION / "regenerated" if args.verify else args.config.parent)
    files, metrics = generate(config, glb, output)
    if args.verify:
        for file in files:
            actual = args.config.parent / file
            if actual.read_bytes() != (output/file).read_bytes():
                raise ValueError("Stale or non-deterministic data: " + file)
            im = Image.open(actual)
            if im.mode != "RGBA" or im.size != (1024,1024) or np.any(np.asarray(im)[:,:,3] != 255):
                raise ValueError("Expected unpremultiplied RGBA8 with A=255")
            if any(c in (b"sRGB",b"gAMA",b"iCCP") for c in png_chunks(actual.read_bytes())):
                raise ValueError("Linear data PNG carries colour conversion metadata")
        expected=binding_provenance(args.config)
        actual=json.loads((args.config.parent/"bindings.json").read_text(encoding="utf-8"))
        if actual != expected:
            raise ValueError("Stale PNG / version / authoring SHA provenance in quality bindings")
    elif output.resolve() == args.config.parent.resolve():
        sync_binding_provenance(args.config)
    after_hash = hashlib.sha256(args.model.read_bytes()).hexdigest().upper()
    if before_hash != after_hash:
        raise ValueError("Input GLB changed")
    diagnose_minification(args.config,output,files)
    report = {"input_sha256":before_hash,"model_unchanged":True,"deterministic":args.verify,"libraries":{"Pillow":PIL.__version__,"NumPy":np.__version__},"metrics":metrics,
              "files":[{"file":f,"bytes":(output/f).stat().st_size,"sha256":hashlib.sha256((output/f).read_bytes()).hexdigest().upper()} for f in files]}
    (INSPECTION / "validation.json").write_text(json.dumps(report,indent=2)+"\n", encoding="utf-8")
    print(json.dumps(report,indent=2))


if __name__ == "__main__":
    main()
