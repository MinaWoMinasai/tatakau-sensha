"""Numerical tests for the authored quality comparison, not image-quality claims."""

import hashlib
import json
import unittest

import numpy as np
from PIL import Image

import generate_neon_quality_masks as quality


class QualityDataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = json.loads(quality.CONFIG.read_text(encoding="utf-8"))
        cls.glb = quality.Glb(quality.DEFAULT_MODEL)

    def test_input_and_original_candidates_are_preserved(self):
        self.assertEqual(self.glb.sha256, self.config["modelSha256"])
        for name, expected in self.config["comparisonBaseSha256"].items():
            actual = hashlib.sha256((quality.ASSETS.parent/name).read_bytes()).hexdigest().upper()
            self.assertEqual(actual, expected, name)

    def test_round_caps_have_declared_inside_sign_width_and_unit(self):
        curve = {"name":"horizontal", "points":[[.25,.5078125],[.4,.5078125],[.6,.5078125],[.75,.5078125]],"widthTexels":4,"opacity":.4}
        distance, opacity = quality.curve_field([curve],64,16)
        self.assertAlmostEqual(float(distance[32,32]),2,places=5)
        self.assertAlmostEqual(float(distance[34,32]),0,places=5)
        self.assertAlmostEqual(float(distance[35,32]),-1,places=5)
        self.assertAlmostEqual(float(distance[32,14]),.5,places=5)
        self.assertAlmostEqual(float(distance[32,13]),-.5,places=5)
        self.assertAlmostEqual(float(opacity[32,32]),.4,places=5)
        # Opacity is not used as a binary shape threshold.
        stronger = dict(curve,opacity=1)
        strong_distance, _ = quality.curve_field([stronger],64,16)
        np.testing.assert_array_equal(distance,strong_distance)

    def test_intersections_form_one_union_not_double_stroked_edges(self):
        horizontal={"points":[[.25,.5078125],[.4,.5078125],[.6,.5078125],[.75,.5078125]],"widthTexels":4,"opacity":1}
        vertical={"points":[[.5078125,.25],[.5078125,.4],[.5078125,.6],[.5078125,.75]],"widthTexels":2,"opacity":.3}
        union, _ = quality.curve_field([horizontal,vertical],64,16)
        a,_=quality.curve_field([horizontal],64,16)
        b,_=quality.curve_field([vertical],64,16)
        np.testing.assert_allclose(union,np.maximum(a,b))
        self.assertGreater(union[32,32],0)
        self.assertGreater(union[28,32],0)
        self.assertLess(union[28,28],0)

    def test_coverage_and_sdf_share_geometry_and_replacement(self):
        for version in self.config["versions"]:
            for mask in version["masks"]:
                stem=mask["id"]+"_"+version["id"]
                c=np.asarray(Image.open(quality.ASSETS/(stem+"_coverage.png")))
                d=np.asarray(Image.open(quality.ASSETS/(stem+"_sdf.png")))
                np.testing.assert_array_equal(c[:,:,1],d[:,:,1])
                self.assertTrue(np.any((c[:,:,1]==255)&(c[:,:,0]==0)))
                self.assertTrue(np.any(d[:,:,0]>128))
                if any(0 < c["opacity"] < 1 for c in mask["curves"]):
                    self.assertTrue(np.any((d[:,:,2]>0)&(d[:,:,2]<255)))
                self.assertTrue(np.all(c[:,:,3]==255) and np.all(d[:,:,3]==255))
                with Image.open(quality.ASSETS/(stem+"_sdf.png")) as im:
                    self.assertNotIn("gamma",im.info)
                with Image.open(quality.ASSETS/(stem+"_coverage.png")) as im:
                    self.assertNotIn("srgb",im.info)

    def test_minified_coverage_retains_energy_without_forced_width(self):
        for stem in ("face_v1","bangs_v1","face_v2","bangs_v2"):
            red=Image.open(quality.ASSETS/(stem+"_coverage.png")).getchannel("R")
            original=float(np.asarray(red).sum())
            for size in (512,256,128):
                small=np.asarray(red.resize((size,size),Image.Resampling.BOX))
                recovered=float(small.sum())*(1024/size)**2
                self.assertLess(abs(recovered-original)/original,.07)
                self.assertTrue(np.any((small>0)&(small<255)))

    def test_halo_is_fully_inside_replace_transition(self):
        for stem in ("face_v1","bangs_v1","face_v2","bangs_v2"):
            c=np.asarray(Image.open(quality.ASSETS/(stem+"_coverage.png")))
            self.assertTrue(np.all(c[:,:,1][c[:,:,2]>1]>=254))

    def test_new_versions_are_distinct_and_bounded_to_face_bangs(self):
        self.assertEqual([v["id"] for v in self.config["versions"]],["v1","v2"])
        self.assertEqual([len(m["curves"]) for m in self.config["versions"][0]["masks"]],[12,3])
        self.assertEqual([len(m["curves"]) for m in self.config["versions"][1]["masks"]],[13,3])
        for role in ("face","bangs"):
            self.assertNotEqual((quality.ASSETS/(role+"_v1_coverage.png")).read_bytes(),(quality.ASSETS/(role+"_v2_coverage.png")).read_bytes())

    def test_binding_pairs_use_existing_materials_and_uv0(self):
        bindings=json.loads((quality.ASSETS/"bindings.json").read_text(encoding="utf-8"))
        materials={m["name"] for m in self.glb.doc["materials"]}
        self.assertEqual(bindings["modelSha256"],self.glb.sha256)
        self.assertEqual(bindings["sdfRangeTexels"],16)
        self.assertEqual(bindings["haloWidthTexels"],4)
        self.assertEqual(bindings,quality.binding_provenance(quality.CONFIG))
        for version in bindings["versions"]:
            self.assertEqual(len(version["bindings"]),5)
            self.assertEqual(len({b["material"] for b in version["bindings"]}),5)
            self.assertEqual([b["role"] for b in version["bindings"]].count("face"),4)
            for binding in version["bindings"]:
                self.assertIn(binding["material"],materials)
                self.assertEqual(binding["uvSet"],0)
                for kind in ("coverage","sdf"):
                    expected=binding["role"]+"_"+version["id"]+"_"+kind+".png"
                    self.assertEqual(binding[kind+"File"],expected)
                    self.assertTrue((quality.ASSETS/expected).is_file())

    def test_invalid_values_and_input_hash_fail_before_output(self):
        import copy
        invalid=copy.deepcopy(self.config)
        invalid["versions"][0]["masks"][0]["curves"][0]["opacity"]=float("nan")
        with self.assertRaises(ValueError):quality.validate(invalid,self.glb)
        invalid=copy.deepcopy(self.config)
        invalid["versions"][0]["masks"][0]["curves"][0]["widthTexels"]=0
        with self.assertRaises(ValueError):quality.validate(invalid,self.glb)
        invalid=copy.deepcopy(self.config)
        invalid["modelSha256"]="0"*64
        with self.assertRaises(ValueError):quality.validate(invalid,self.glb)
        invalid=copy.deepcopy(self.config)
        invalid["haloWidthTexels"]=float("nan")
        with self.assertRaises(ValueError):quality.validate(invalid,self.glb)


if __name__=="__main__":
    unittest.main(verbosity=2)
