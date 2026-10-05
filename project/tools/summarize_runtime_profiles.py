"""Summarize genuine CPU/GPU CSV scopes without adding nested scopes."""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path


def metric_groups(rows, valid, selected_frames=None):
    groups = {}
    for row in rows:
        if selected_frames is not None and int(row["frame"]) not in selected_frames:
            continue
        if row["category"] == "gpu" and int(row["frame"]) not in valid:
            continue
        value = float(row["value"])
        if not math.isfinite(value) or value < 0:
            raise ValueError(f"Invalid negative/nonfinite metric: {row}")
        groups.setdefault(row["category"] + "/" + row["name"], []).append(value)
    metrics = {}
    for key, values in groups.items():
        values.sort()
        metrics[key] = dict(samples=len(values), average=statistics.mean(values),
                            median=statistics.median(values),
                            p95=values[math.ceil(.95 * len(values)) - 1], maximum=values[-1])
    return metrics


def summarize(path):
    with path.open(newline="", encoding="utf-8-sig") as stream:
        rows = list(csv.DictReader(stream))
    valid, invalid = set(), set()
    for row in rows:
        if row["category"] == "frame" and row["name"] == "gpu_valid":
            (valid if float(row["value"]) == 1 else invalid).add(int(row["frame"]))
    valid -= invalid
    result = dict(source=str(path), gpuValidFrames=len(valid),
                  invalidGpuFrames=len(invalid), metrics=metric_groups(rows, valid))
    snapshot_path = path.parent / "snapshots.json"
    if snapshot_path.is_file():
        snapshots = json.loads(snapshot_path.read_text(encoding="utf-8-sig"))
        result["snapshotContext"] = {
            "frames": len(snapshots),
            "maximumProjectiles": max(row["projectiles"] for row in snapshots),
            "maximumEnemies": max(row["enemies"] for row in snapshots),
            "finalSnapshot": snapshots[-1],
            "phaseSampleCounts": {
                "living": sum(not row["bossDead"] for row in snapshots),
                "dissolve": sum(row["bossDead"] and row["visualHasResources"] for row in snapshots),
                "terminal": sum(row["bossDead"] and not row["visualHasResources"] for row in snapshots),
            },
            "limit": "Snapshot counts cover the full Session, not necessarily only the measured profiler window. Profiler frame index may include setup frames.",
        }
        # The adapter's explicit counter joins engine-frame timings to the
        # Session snapshots. Never guess that setup/capture frames align.
        by_session_frame = {row["frame"]: row for row in snapshots}
        engine_to_session = {
            int(row["frame"]): int(float(row["value"])) for row in rows
            if row["category"] == "count" and row["name"] == "Scenario simulation frame"
        }
        phases = {"living": set(), "dissolve": set(), "terminal": set()}
        for engine_frame, session_frame in engine_to_session.items():
            snapshot = by_session_frame.get(session_frame)
            if snapshot is None:
                continue
            phase = "living" if not snapshot["bossDead"] else "dissolve" if snapshot["visualHasResources"] else "terminal"
            phases[phase].add(engine_frame)
        if engine_to_session:
            result["measuredPhaseMetrics"] = {
                phase: {
                    "engineFrames": len(frames), "gpuValidFrames": len(frames & valid),
                    "sessionFrameMinimum": min(engine_to_session[f] for f in frames),
                    "sessionFrameMaximum": max(engine_to_session[f] for f in frames),
                    "metrics": metric_groups(rows, valid, frames),
                } for phase, frames in phases.items() if frames
            }
            result["phaseProvenance"] = "Joined by actual Scenario simulation frame counter; includes only measured CSV frames."
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    # Runtime copies contain authored map CSVs. The run manifest names actual
    # profiler captures; never interpret map cells as timing measurements.
    manifest_path = args.directory / "run-manifest.json"
    if manifest_path.is_file():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
        paths = [Path(case["csv"]) for case in manifest["results"]]
    else:
        paths = sorted(path for path in args.directory.rglob("*.csv")
                       if "resources" not in path.relative_to(args.directory).parts)
    if not paths:
        parser.error("No profiler CSV files found.")
    result = {
        "method": "Nearest-rank p95. Genuine D3D12 GPU rows require gpu_valid=1; contradictory flags are invalid. Nested scopes are not summed. Count metrics are counts; timings are milliseconds.",
        "captures": [summarize(path) for path in paths],
    }
    destination = args.output or args.directory / "performance-summary.json"
    destination.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for capture in result["captures"]:
        print(f"{Path(capture['source']).parent.name}/{Path(capture['source']).name}: GPU valid {capture['gpuValidFrames']}, invalid {capture['invalidGpuFrames']}")


if __name__ == "__main__":
    main()
