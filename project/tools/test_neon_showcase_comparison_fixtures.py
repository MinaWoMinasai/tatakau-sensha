"""Synthetic capture fixtures for the comparison validator; not visual evidence."""

import copy
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

import test_neon_showcase_comparison as validator


def synthetic_png(width=2,height=2,red=80):
    def chunk(kind,data):
        return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data)&0xffffffff)
    header=struct.pack(">IIBBBBB",width,height,8,6,0,0,0)
    rows=b"".join(b"\0"+bytes((red,0,0,255))*width for _ in range(height))
    return b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",header)+chunk(b"IDAT",zlib.compress(rows))+chunk(b"IEND",b"")


class ComparisonFixtureTests(unittest.TestCase):
    def setUp(self):
        generated=validator.ROOT/"generated"
        generated.mkdir(exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(prefix="neon_comparison_fixture_",dir=generated)
        self.directory=Path(self.temp.name)
        self.quality=json.loads((validator.ROOT/"project/resources/models/neon_hologram/line_masks/quality/bindings.json").read_text(encoding="utf-8"))
        self.files=[]
        for i,(_,candidate,split,mode,blend) in enumerate(validator.CASES):
            metadata=self.metadata(candidate,split,mode,blend)
            label=validator.CASES[i][0]
            metadata["comparison"]={"case":i,"label":label,"count":7,"samePausedPose":True,"restoreAppearanceAfterCapture":True}
            file=self.directory/f"{i:02d}_{label}.json"
            self.write(file,metadata)
            file.with_suffix(".png").write_bytes(synthetic_png(red=80+i))
            self.files.append(file)

    def tearDown(self):
        self.temp.cleanup()

    def write(self,path,value):
        path.write_text(json.dumps(value,ensure_ascii=False),encoding="utf-8")

    def metadata(self,candidate,split,mode,blend):
        neon={key:([1.,.1,.7] if "Color" in key or key.endswith("Color") else 1.) for key in validator.FIXED_NEON}
        neon.update(geometryLines=0,maskDiagnostic=0,lineDiagnostic=0,sdfRangeTexels=16,
                    sdfHaloWidthTexels=4,sdfLodBlendStart=1,sdfLodBlendEnd=2,
                    maskBlend=blend,maskRenderMode=mode,splitCoreHalo=split)
        version=self.quality["versions"][0 if candidate<4 else (1 if candidate<6 else 2)]
        surfaces=[]
        for binding in version["bindings"]:
            surface={"material":binding["material"],"mask":"","distanceMask":"","maskBound":False,"distanceBound":False,
                     "loadStatus":"Synthetic metadata fixture","lineStrength":1,"geometryStrength":0,"thresholdScale":1,"alphaCutoff":.03}
            if candidate>=2:
                surface.update(mask="resources/models/neon_hologram/line_masks/quality/"+binding["coverageFile"],
                               distanceMask="resources/models/neon_hologram/line_masks/quality/"+binding["sdfFile"],
                               maskBound=True,distanceBound=True,authoringRevision=version["authoringRevision"])
                for key in ("coverageSha256","sdfSha256"):
                    surface[validator.HASH_FIELDS[key]]=binding[key]
                for key in ("authoringSha256","authoringVersionSha256"):
                    surface[validator.HASH_FIELDS[key]]=version[key]
            surfaces.append(surface)
        for material in ("Body fixture","Shoes fixture"):
            surfaces.append({"material":material,"mask":"","distanceMask":"","maskBound":False,"distanceBound":False,
                             "loadStatus":"None","lineStrength":.1,"geometryStrength":0,"thresholdScale":1,"alphaCutoff":.03})
        return {"schemaVersion":1,"model":validator.MODEL,"expectedModelSha256":validator.MODEL_SHA,
                "build":"Development","renderScale":1,"mode":"Neon","framing":"face","qualityCandidate":candidate,
                "camera":{"position":[0,1.32,-1.15],"rotation":[0,0,0],"fovY":.45,"orbit":False},
                "transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]},
                "animation":{"clip":"Preview_Idle","time":.4,"paused":True,"speed":1},
                "bloom":{"threshold":1,"intensity":.7,"exposure":1,"diagnostic":0,"taa":False,"grayscale":False},
                "neon":neon,"submeshes":surfaces,"resolution":[2,2],"gpu":{"adapter":"Synthetic fixture"}}

    def mutate(self,index,callback):
        metadata=json.loads(self.files[index].read_text(encoding="utf-8"))
        callback(metadata)
        self.write(self.files[index],metadata)

    def validate(self,**kwargs):
        return validator.validate_comparison(self.directory,**kwargs)

    def test_seven_cases_and_unproven_restore_are_explicit(self):
        result=self.validate()
        self.assertTrue(result["pass"])
        self.assertEqual(len(result["captures"]),7)
        self.assertFalse(result["appearanceRestoration"]["verified"])
        images=result["captures"][5]["sources"][0]["images"]
        self.assertTrue(images[0]["manifestExpectedMatchesCurrentFile"])
        self.assertFalse(images[0]["gpuContentsHashed"])

    def test_old_hash_fields_supported_without_claiming_gpu_hashes(self):
        for index in range(2,7):
            self.mutate(index,lambda m:[s.__setitem__(key,s.pop(new)) for s in m["submeshes"] if s["mask"] for key,new in validator.HASH_FIELDS.items()])
        self.assertTrue(self.validate()["pass"])

    def test_explicit_case_modes_and_alias_conflicts_rejected(self):
        original=json.loads(self.files[4].read_text(encoding="utf-8"))
        for key,value in (("maskRenderMode",1),("splitCoreHalo",0),("maskBlend",0)):
            changed=copy.deepcopy(original);changed["neon"][key]=value;self.write(self.files[4],changed)
            with self.assertRaisesRegex(ValueError,"case state"):self.validate()
        self.write(self.files[4],original)
        self.mutate(4,lambda m:m["submeshes"][0].__setitem__("coverageSha256","0"*64))
        with self.assertRaisesRegex(ValueError,"Conflicting manifest"):self.validate()

    def test_pose_camera_body_bloom_and_submesh_changes_rejected(self):
        original=json.loads(self.files[3].read_text(encoding="utf-8"))
        edits=[lambda m:m["animation"].__setitem__("time",.5),
               lambda m:m["camera"]["position"].__setitem__(0,.1),
               lambda m:m["neon"].__setitem__("bodyEmission",2),
               lambda m:m["bloom"].__setitem__("intensity",.8),
               lambda m:m["submeshes"][0].__setitem__("lineStrength",.9)]
        for edit in edits:
            changed=copy.deepcopy(original);edit(changed);self.write(self.files[3],changed)
            with self.assertRaisesRegex(ValueError,"A/B input/settings"):self.validate()

    def test_source_sha_revision_material_and_distance_range_rejected(self):
        original=json.loads(self.files[5].read_text(encoding="utf-8"))
        edits=[lambda m:m["submeshes"][0].__setitem__("coverageManifestExpectedSha256","0"*64),
               lambda m:m["submeshes"][0].__setitem__("authoringManifestExpectedSha256","0"*64),
               lambda m:m["submeshes"][0].__setitem__("authoringVersionManifestExpectedSha256","0"*64),
               lambda m:m["submeshes"][0].__setitem__("authoringRevision","v2-before-r2"),
               lambda m:m["neon"].__setitem__("sdfRangeTexels",8)]
        for edit in edits:
            changed=copy.deepcopy(original);edit(changed);self.write(self.files[5],changed)
            with self.assertRaises(ValueError):self.validate()

    def test_png_magic_resolution_and_nonfinite_rejected(self):
        png=self.files[6].with_suffix(".png")
        png.write_bytes(b"not a PNG")
        with self.assertRaisesRegex(ValueError,"invalid PNG"):self.validate()
        png.write_bytes(synthetic_png(width=3))
        with self.assertRaisesRegex(ValueError,"resolution"):self.validate()
        png.write_bytes(synthetic_png())
        self.mutate(6,lambda m:m["neon"].__setitem__("coreIntensity",float("nan")))
        with self.assertRaisesRegex(ValueError,"Nonfinite"):self.validate()

    def test_new_optional_resolution_and_provenance_fields_remain_fixed(self):
        for index in range(7):
            self.mutate(index,lambda m:m.update(sceneViewportResolution=[2,2],resolutionSource="Synthetic raw PNG resource",textureProvenance="Manifest expected; GPU contents not hashed"))
        self.assertTrue(self.validate()["pass"])
        self.mutate(3,lambda m:m["sceneViewportResolution"].__setitem__(0,3))
        with self.assertRaisesRegex(ValueError,"A/B input/settings"):self.validate()
        self.mutate(3,lambda m:m.pop("sceneViewportResolution"))
        with self.assertRaisesRegex(ValueError,"A/B input/settings"):self.validate()

    def test_restoration_requires_both_metadata_and_raw_png_equality(self):
        before=self.directory/"before.json";after=self.directory/"after.json"
        state=self.metadata(4,1,1,1)
        for path in (before,after):
            self.write(path,state);path.with_suffix(".png").write_bytes(synthetic_png())
        self.assertTrue(self.validate(before_json=before,after_json=after)["appearanceRestoration"]["verified"])
        after.with_suffix(".png").write_bytes(synthetic_png(red=81))
        with self.assertRaisesRegex(ValueError,"byte-identical"):self.validate(before_json=before,after_json=after)
        after.with_suffix(".png").write_bytes(synthetic_png())
        altered=copy.deepcopy(state);altered["neon"]["maskBlend"]=0;self.write(after,altered)
        with self.assertRaisesRegex(ValueError,"rendering parameters"):self.validate(before_json=before,after_json=after)
        with self.assertRaisesRegex(ValueError,"Provide both"):self.validate(before_json=before)

    def test_protected_v1_v2_document_sha_remains_verifiable_after_v3_addition(self):
        authoring=json.loads((validator.ROOT/"project/resources/models/neon_hologram/line_masks/quality/authoring.json").read_text(encoding="utf-8"))
        previous=authoring["preservedQualitySource"]["authoringSha256"]
        for index in range(2,7):
            self.mutate(index,lambda m:[s.__setitem__("authoringManifestExpectedSha256",previous) for s in m["submeshes"] if s["mask"]])
        self.assertTrue(self.validate()["pass"])

    def test_inactive_dissolve_metadata_is_fixed_in_quality_comparison(self):
        from test_neon_dissolve_comparison_fixtures import dissolve_metadata
        effect=dissolve_metadata(active=False)
        for index in range(7):self.mutate(index,lambda m:m.update(dissolve=effect))
        self.assertTrue(self.validate()["pass"])
        self.mutate(3,lambda m:m["dissolve"].__setitem__("seed",123))
        with self.assertRaisesRegex(ValueError,"A/B input/settings"):self.validate()
        self.mutate(3,lambda m:m["dissolve"].__setitem__("seed",effect["seed"]))
        self.mutate(3,lambda m:m.pop("dissolve"))
        with self.assertRaisesRegex(ValueError,"A/B input/settings"):self.validate()


if __name__=="__main__":unittest.main(verbosity=2)
