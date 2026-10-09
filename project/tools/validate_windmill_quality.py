"""Validate genuine windmill readbacks and summarize timestamps, not aesthetic scores."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image
from summarize_neon_showcase import summarize


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def pixels(path):
    return np.asarray(Image.open(path).convert("RGB"))


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    root = args.root
    comparison = root / "comparison"
    report = read(comparison / "runtime_report.json")
    check(report["completed"] and not report["errors"] and len(report["captures"]) == 48,
          "Incomplete actual readback comparison")
    conditions = ["timeSeconds", "phase", "recognition", "center", "camera", "yaw", "angle", "visibility", "resolution"]
    differences = {}
    for state in ["orbit", "approach", "before", "after", "surge"]:
        reference = read(comparison / f"{state}_0_off.json")
        for mode in range(3):
            off = read(comparison / f"{state}_{mode}_off.json")
            on = read(comparison / f"{state}_{mode}_on.json")
            for frame in [off,on]:
                check(all(frame[key] == reference[key] for key in conditions), f"Pose/camera changed: {state}/{mode}")
                check(frame["renderMode"] == mode and frame["emojiQuads"] == 5 and not frame["billboard"], "Wrong plate state")
                check(frame["post"]["temporal"] == 0 and frame["post"]["exposure"] == reference["post"]["exposure"], "Temporal/exposure changed")
                check(frame["post"]["threshold"] == .800000011920929 and frame["post"]["gain"] == reference["post"]["gain"], "Bloom strength changed")
            check(off["post"]["bloomMode"] == 0 and on["post"]["bloomMode"] == 2, "Bloom OFF/ON mismatch")
            a = pixels(comparison / f"{state}_{mode}_off.png")
            b = pixels(comparison / f"{state}_{mode}_on.png")
            check(a.shape == (720,1280,3), "Backbuffer dimensions changed")
            delta = np.abs(a.astype(int) - b.astype(int))
            differences[f"{state}/{mode}"] = {"pixels_delta_over_3":int((delta.max(axis=2)>3).sum()), "mean_abs_rgb_delta":float(delta.mean())}
            check(delta.max() > 3, "No real Bloom image effect")
    parity = {}
    baseline = root / "head-baseline"
    for old, new in [("orbit_off","orbit_0_off"),("orbit_on","orbit_0_on"),("lock_on","before_0_on"),("recognized_on","after_0_on")]:
        if (baseline / f"{old}.png").exists():
            a = pixels(baseline / f"{old}.png")
            b = pixels(comparison / f"{new}.png")
            parity[old] = int(np.count_nonzero(a != b))
            check(parity[old] == 0, f"Legacy no longer matches HEAD: {old}")
    # Diagnostics must contain actual source light with Bloom disabled.
    diagnostics = {}
    for name in ["base","core","inner","halo","surface"]:
        meta = read(comparison / f"diagnostic_{name}.json")
        a = pixels(comparison / f"diagnostic_{name}.png")
        diagnostics[name] = int((a.max(axis=2)>12).sum())
        check(meta["post"]["bloomMode"] == 0 and not meta["background"] and diagnostics[name]>0,
              f"Missing isolated source {name}")
    quality_dir = root.parent / "neon_windmill"
    core = pixels(comparison / "diagnostic_core.png")
    for name in ["core_wide_halo", "core_no_halo"]:
        check(np.array_equal(core, pixels(comparison / f"diagnostic_{name}.png")), "Halo changed isolated Core")
    check(np.array_equal(pixels(comparison / "diagnostic_combined.png"),
                         pixels(comparison / "diagnostic_bloom_zero_gain.png")), "Bloom gain 0 is not equivalent to OFF")
    for name in ["no_halo", "wide_halo", "narrow_core", "wide_core", "zero_emission", "dim_base"]:
        check(not np.array_equal(pixels(comparison / "diagnostic_combined.png"),pixels(comparison / f"diagnostic_{name}.png")),
              f"Source adjustment had no effect: {name}")
    manifest = read(quality_dir / "quality_texture.json")
    for name, digest in manifest["sourceSha256"].items():
        check(hashlib.sha256((quality_dir / "iphone" / name).read_bytes()).hexdigest() == digest, "Apple source changed")
    premult = np.asarray(Image.open(quality_dir / "iphone_linear_premultiplied.png"))
    check(np.all(premult[:,:,:3] <= premult[:,:,3:4]), "Non-premultiplied linear data")
    check(np.all(premult[premult[:,:,3]==0,:3]==0), "Hidden transparent RGB bleeds into MIPs")
    motion = None
    if (root / "motion/runtime_report.json").exists():
        m = read(root / "motion/runtime_report.json")
        check(m["completed"] and len(m["captures"]) == 480 and not m["errors"], "Motion capture incomplete")
        phases = set()
        for index in range(480):
            frame = read(root / f"motion/frame_{index:04d}.json")
            check(abs(frame["timeSeconds"] - index/30) < 1e-5, "Motion seek changed")
            check(frame["renderMode"] == 2 and frame["post"]["temporal"] == 0, "Motion mode changed")
            phases.add(frame["phase"])
        check(phases == set(range(6)), "Missing original phase")
        motion = {"frames":480,"fps":30,"seconds":16,"phases":sorted(phases),"method":"fixed timestep genuine DX12 poses, PNG sequence; not real-time frame pacing"}
    performance = []
    for mode in range(3):
        directory = root / f"perf{mode}"
        if (directory / "gpu.csv").exists():
            metadata = read(directory / "runtime_report.json")
            check(metadata["completed"] and metadata["renderMode"] == mode, "Performance condition mismatch")
            stats = summarize(directory / "gpu.csv")
            stats["settings"] = metadata
            check(stats["gpu_valid_frames"] == 300 and stats["gpu"]["Windmill scene"]["samples"] == 300, "Invalid timestamp samples")
            performance.append(stats)
    output = {"passed":True,"legacy_pixel_parity":parity,"bloom_differences":differences,
              "diagnostic_visible_pixels":diagnostics,"motion":motion,"performance":performance,
              "limits":"RGB8 differences are not HDR radiometry or aesthetic scores. Human final appearance review pending."}
    (root / "validation.json").write_text(json.dumps(output, indent=2, ensure_ascii=False), encoding="utf-8")
    print("PASS: same pose/camera/exposure, real Bloom/source diagnostics, Legacy parity, source protection, optional motion/timestamps")


if __name__ == "__main__":
    main()
