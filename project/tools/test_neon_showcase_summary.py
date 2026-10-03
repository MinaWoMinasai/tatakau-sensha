"""Regression fixtures for genuine GPU timestamp validity, not CPU-time estimates."""

import csv
import json
import math
import unittest
from pathlib import Path

from summarize_neon_showcase import summarize


class ShowcaseSummaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.output = Path(__file__).resolve().parents[2] / "generated/neon_showcase_summary_unit"
        cls.output.mkdir(parents=True, exist_ok=True)

    def fixture(self, name, rows, bom=False):
        path = self.output / (name + ".csv")
        with path.open("w", newline="", encoding="utf-8-sig" if bom else "utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(["frame","category","name","value"])
            writer.writerows(rows)
        return path

    def test_stale_invalid_and_cpu_values_cannot_enter_gpu_statistics(self):
        path = self.fixture("invalid_and_cpu", [
            [10,"gpu","Neon",99999], [10,"frame","gpu_valid",0],
            [11,"gpu","Neon",2], [11,"frame","gpu_valid",1],
            [11,"cpu","Neon",99], [12,"frame","gpu_valid",1],
            [12,"cpu","Neon",99], [12,"gpu","Neon",4],
            [13,"gpu","Neon",88888], # No validity marker: unsupported/unavailable.
        ])
        report=summarize(path)
        self.assertEqual(report["gpu_valid_frames"],2)
        self.assertEqual(report["gpu"],{"Neon":{"samples":2,"mean_ms":3,"p95_ms":4,"min_ms":2,"max_ms":4}})

    def test_late_validity_rows_are_order_independent_and_p95_is_nearest_rank(self):
        rows=[[i,"gpu","Body",i] for i in range(1,21)]
        rows += [[i,"frame","gpu_valid",1] for i in range(20,0,-1)]
        rows += [[3,"gpu","Outline",.5],[9,"gpu","Outline",1.5]]
        result=summarize(self.fixture("p95",rows))
        self.assertEqual(result["gpu_valid_frames"],20)
        self.assertEqual(result["gpu"]["Body"],{"samples":20,"mean_ms":10.5,"p95_ms":19,"min_ms":1,"max_ms":20})
        self.assertEqual(result["gpu"]["Outline"],{"samples":2,"mean_ms":1,"p95_ms":1.5,"min_ms":.5,"max_ms":1.5})

    def test_nonfinite_negative_gpu_samples_are_excluded_but_zero_is_valid(self):
        path=self.fixture("finite",[
            [1,"frame","gpu_valid",1],[1,"gpu","Frame",float("nan")],
            [1,"gpu","Frame",float("inf")],[1,"gpu","Frame",float("-inf")],
            [1,"gpu","Frame",-1],[1,"gpu","Frame",0],[1,"gpu","Frame",.25],
        ])
        result=summarize(path)
        self.assertEqual(result["gpu"]["Frame"],{"samples":2,"mean_ms":.125,"p95_ms":.25,"min_ms":0,"max_ms":.25})
        self.assertTrue(all(math.isfinite(v) for v in result["gpu"]["Frame"].values()))
        json.dumps(result,allow_nan=False)

    def test_conflicting_or_nonfinite_validity_flags_reject_the_frame(self):
        path=self.fixture("conflicting",[
            [1,"frame","gpu_valid",1],[1,"gpu","Frame",500],[1,"frame","gpu_valid",0],
            [2,"frame","gpu_valid",float("nan")],[2,"gpu","Frame",600],
            [3,"frame","gpu_valid",1],[3,"frame","gpu_valid",float("inf")],[3,"gpu","Frame",700],
            [4,"frame","gpu_valid",1],[4,"gpu","Frame",.75],
        ])
        result=summarize(path)
        self.assertEqual(result["gpu_valid_frames"],1)
        self.assertEqual(result["gpu"]["Frame"]["mean_ms"],.75)

    def test_no_valid_gpu_measurement_is_reported_as_unavailable_not_zero(self):
        result=summarize(self.fixture("unavailable",[
            [1,"frame","gpu_valid",0],[1,"gpu","Frame",90],[1,"cpu","Frame",99],
            [2,"frame","gpu_valid",1],[2,"gpu","Frame",float("nan")],
        ]))
        self.assertEqual(result["gpu_valid_frames"],1)
        self.assertEqual(result["gpu"],{})

    def test_utf8_bom_csv_and_settings_are_preserved(self):
        path=self.fixture("metadata",[[1,"gpu","Neon",1.25],[1,"frame","gpu_valid",1]],bom=True)
        settings={"GPU":"RTX 4060 Laptop GPU","resolution":[1600,900],"comparison":"顔・前髪"}
        path.with_suffix(".json").write_text(json.dumps(settings,ensure_ascii=False),encoding="utf-8-sig")
        self.assertEqual(summarize(path)["settings"],settings)


if __name__=="__main__":
    unittest.main(verbosity=2)
