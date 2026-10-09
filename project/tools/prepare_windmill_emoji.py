"""Pack unmodified iPhone reference glyphs and derive their alpha silhouettes.

The glyph artwork is external Apple artwork; this script creates engine data,
not a substitute emoji font. Generated images stay in the ignored generated/.
"""
from pathlib import Path
import argparse
import json
from PIL import Image, ImageFilter
import hashlib
import numpy as np


def connected_alpha(image):
    """Retain the main connected silhouette plus a two-texel AA fringe.

    Source files and Legacy are untouched. Isolated alpha specks in transparent
    padding must not turn into additional bright emitters. This is connectivity,
    not a semantic classifier of facial features.
    """
    rgba = np.asarray(image).copy()
    inside = rgba[:, :, 3] >= 26
    visited = np.zeros_like(inside)
    h, w = inside.shape
    largest = []
    for y, x in zip(*np.nonzero(inside)):
        if visited[y, x]:
            continue
        component = [(int(y), int(x))]
        visited[y, x] = True
        for cy, cx in component:
            for dy, dx in [(0,1),(0,-1),(1,0),(-1,0),(1,1),(1,-1),(-1,1),(-1,-1)]:
                ny, nx = cy + dy, cx + dx
                if 0 <= ny < h and 0 <= nx < w and inside[ny,nx] and not visited[ny,nx]:
                    visited[ny,nx] = True
                    component.append((ny,nx))
        if len(component) > len(largest):
            largest = component
    keep = np.zeros((h,w), dtype=np.uint8)
    for y, x in largest:
        keep[y,x] = 255
    keep = np.asarray(Image.fromarray(keep).filter(ImageFilter.MaxFilter(5))) > 0
    rgba[~keep] = 0
    return Image.fromarray(rgba)


def simplify(points, epsilon):
    """Ramer-Douglas-Peucker simplification of an open boundary section."""
    if len(points) <= 2:
        return points
    a, b = points[0], points[-1]
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = (dx * dx + dy * dy) ** 0.5
    distances = [abs(dx * (p[1] - a[1]) - dy * (p[0] - a[0])) / length
                 if length else ((p[0] - a[0]) ** 2 + (p[1] - a[1]) ** 2) ** 0.5 for p in points]
    index = max(range(len(points)), key=distances.__getitem__)
    if distances[index] <= epsilon:
        return [a, b]
    return simplify(points[:index + 1], epsilon)[:-1] + simplify(points[index:], epsilon)


def silhouette(image):
    """Trace the largest exterior alpha boundary, preserving concave fingers."""
    alpha = image.getchannel("A")
    pixels = alpha.load()
    w, h = image.size
    def inside(x, y):
        return 0 <= x < w and 0 <= y < h and pixels[x, y] >= 128
    edges = {}
    for y in range(h):
        for x in range(w):
            if not inside(x, y):
                continue
            for neighbor, a, b in [((x,y-1),(x,y),(x+1,y)), ((x+1,y),(x+1,y),(x+1,y+1)),
                                   ((x,y+1),(x+1,y+1),(x,y+1)), ((x-1,y),(x,y+1),(x,y))]:
                if not inside(*neighbor):
                    edges.setdefault(a, []).append(b)
    loops = []
    while edges:
        start = next(iter(edges))
        current = start
        loop = [start]
        while current in edges:
            choices = edges[current]
            following = choices.pop()
            if not choices:
                del edges[current]
            current = following
            if current == start:
                break
            loop.append(current)
        if current == start and len(loop) > 2:
            loops.append(loop)
    if not loops:
        raise ValueError("Glyph has no closed alpha silhouette")
    points = max(loops, key=len)
    split = len(points) // 2
    epsilon = 1.0
    while True:
        result = simplify(points[:split + 1], epsilon)[:-1] + simplify(points[split:] + [points[0]], epsilon)[:-1]
        if len(result) <= 80:
            break
        epsilon *= 1.2
    return [[x / w - .5, .5 - y / h] for x, y in result]


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=root / "generated/neon_windmill/iphone")
    parser.add_argument("--output", type=Path, default=root / "generated/neon_windmill")
    args = parser.parse_args()
    manifest = json.loads((root / "project/resources/neon_windmill/iphone_sources.json").read_text(encoding="utf-8"))
    tile, pad = 192, 16
    atlas = Image.new("RGBA", (tile * 3, tile * 3))
    quality_atlas = Image.new("RGBA", atlas.size)
    glyphs = {}
    for index, glyph in enumerate(manifest["glyphs"]):
        image = Image.open(args.input / f'{glyph["name"]}_{glyph["codepoint"]}.png').convert("RGBA")
        # Resample only for texture packing; the source glyph's design is retained.
        image = image.resize((tile - pad * 2, tile - pad * 2), Image.Resampling.LANCZOS)
        x, y = index % 3 * tile + pad, index // 3 * tile + pad
        atlas.paste(image, (x, y))
        quality_atlas.paste(connected_alpha(image), (x,y))
        glyphs[glyph["codepoint"]] = {
            "uv": [x / atlas.width, y / atlas.height, (x + image.width) / atlas.width, (y + image.height) / atlas.height],
            "contour": silhouette(image),
            "sourceUrl": manifest["imageBaseUrl"] + glyph["name"] + "_" + glyph["codepoint"] + ".png",
        }
    args.output.mkdir(parents=True, exist_ok=True)
    atlas.save(args.output / "iphone_atlas.png")
    # Separate linear, premultiplied working texture. Never modify Apple sources
    # or the Legacy SRGB atlas. MIP averaging now retains RGB/alpha association
    # rather than pulling hidden transparent RGB into visible finger edges.
    pixels = np.asarray(quality_atlas, dtype=np.float32) / 255
    srgb = pixels[:, :, :3]
    linear = np.where(srgb <= .04045, srgb / 12.92, ((srgb + .055) / 1.055) ** 2.4)
    pixels[:, :, :3] = linear * pixels[:, :, 3:4]
    Image.fromarray(np.rint(pixels * 255).astype(np.uint8)).save(args.output / "iphone_linear_premultiplied.png")
    quality = {
        "encoding": "RGBA8 LinearData; RGB=linear RGB*alpha, A=coverage",
        "sourceSha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in sorted(args.input.glob("*.png"))},
        "atlasSha256": hashlib.sha256((args.output / "iphone_atlas.png").read_bytes()).hexdigest(),
        "qualitySha256": hashlib.sha256((args.output / "iphone_linear_premultiplied.png").read_bytes()).hexdigest(),
        "size": list(atlas.size), "paddingTexels": pad,
    }
    (args.output / "quality_texture.json").write_text(json.dumps(quality, indent=2), encoding="utf-8")
    (args.output / "iphone_atlas.json").write_text(json.dumps({"vendor":"Apple", "version":"iOS 26.4", "glyphs":glyphs}, indent=2), encoding="utf-8")
    print("Prepared 7 Apple reference glyphs; atlas 576x576; contours:", {key:len(value["contour"]) for key,value in glyphs.items()})


if __name__ == "__main__":
    main()
