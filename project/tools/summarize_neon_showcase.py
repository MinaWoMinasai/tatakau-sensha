"""Summarize genuine D3D12 timestamp CSV samples; never substitute CPU timings."""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path


def summarize(path):
    groups = {}
    valid_frames = set()
    invalid_frames = set()
    with path.open(newline="", encoding="utf-8-sig") as stream:
        for row in csv.DictReader(stream):
            if row["category"] == "frame" and row["name"] == "gpu_valid":
                frame = int(row["frame"])
                if float(row["value"]) == 1:
                    valid_frames.add(frame)
                else:
                    invalid_frames.add(frame)
        # Determine timestamp validity before accepting any GPU row. The flag
        # can follow the samples in the CSV, and invalid frames may still carry
        # stale numerical GPU values. Conflicting duplicate flags are rejected.
        valid_frames.difference_update(invalid_frames)
        stream.seek(0)
        for row in csv.DictReader(stream):
            if row["category"] != "gpu" or int(row["frame"]) not in valid_frames:
                continue
            value = float(row["value"])
            if math.isfinite(value) and value >= 0:
                groups.setdefault(row["name"], []).append(value)
    stats = {}
    for name, values in groups.items():
        ordered = sorted(values)
        stats[name] = {"samples": len(values), "mean_ms": statistics.mean(values),
                       "p95_ms": ordered[max(0, math.ceil(.95 * len(ordered)) - 1)],
                       "min_ms": ordered[0], "max_ms": ordered[-1]}
    metadata_path = path.with_suffix(".json")
    return {"source": str(path), "method": "D3D12 timestamp query; nearest-rank P95",
            "gpu_valid_frames": len(valid_frames), "gpu": stats,
            "settings": json.loads(metadata_path.read_text(encoding="utf-8-sig")) if metadata_path.exists() else None}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path, nargs="+")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = {"captures": [summarize(path) for path in args.csv]}
    text = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text + "\n", encoding="utf-8")
    else:
        print(text)
