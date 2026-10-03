"""Synthetic comparison fixtures; these are not engine render evidence."""

import copy
import json
import unittest

import test_neon_dissolve_comparison as validator
import test_neon_showcase_comparison_fixtures as quality_fixtures
from test_neon_showcase_comparison_fixtures import synthetic_png


def dissolve_metadata(active=True):
    effect={"active":active,"playing":False,"enabled":int(active),"progress":0,
        "direction":[.7,-.7,0],"scanMin":-.8,"scanMax":.8,
        "noiseStrength":.06,"noiseScale":8,"seed":17,"edgeEnabled":1,
        "edgeColor":[1,.04,.4],"edgeIntensity":8,"edgeWidth":.025,
        "elapsed":.2,"waitDuration":.2,"duration":2,"playbackSpeed":1,"directionPreset":0,
        "coordinateSpace":"Frozen skinned model space before World; start camera plane remains fixed.",
        "distanceUnits":"scan bounds, noise strength and edge width in model units; noise scale in lattice cells/model unit"}
    if active:
        pose={"translation":[0,.2,0],"scale":[1,1,1],"rotation":[0,0,0,1]}
        effect.update(frozenPlayback={"animationIndex":1,"clip":"Preview_Attack","time":.4,
            "speed":1,"playing":True,"loop":False,"paused":False,"transitionActive":True,
            "transitionDuration":.25,"transitionElapsed":.1,"transitionStartPose":[copy.deepcopy(pose)]},
            startWorld=[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]],
            startCameraWorld=[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,1.32,-1.15,1]],
            frozenJointPose=[dict(pose,joint="Chest"),dict(pose,joint="LeftUpperArm")])
    return effect


class DissolveComparisonFixtureTests(unittest.TestCase):
    def setUp(self):
        self.helper=quality_fixtures.ComparisonFixtureTests("runTest")
        self.helper.setUp()
        self.directory=self.helper.directory/"dissolve"
        self.directory.mkdir()
        self.files=[]
        for i,(label,progress,enabled,noise,edge) in enumerate(validator.CASES):
            metadata=self.helper.metadata(6,1,1,1)
            effect=dissolve_metadata()
            effect.update(progress=progress,enabled=enabled,elapsed=.2+2*progress,
                          noiseStrength=.06 if noise else 0,edgeEnabled=int(edge))
            metadata["dissolve"]=effect
            metadata["dissolveComparison"]={"case":i,"label":label,"count":8,
                "sameFrozenPose":True,"restorePlaybackAfterCapture":True}
            file=self.directory/(f"{i:02d}_"+label+".json")
            self.helper.write(file,metadata)
            file.with_suffix(".png").write_bytes(synthetic_png(red=80 if i<2 else 80+i))
            self.files.append(file)

    def tearDown(self):self.helper.tearDown()

    def validate(self,**kwargs):return validator.validate_comparison(self.directory,**kwargs)

    def mutate(self,index,callback):
        metadata=json.loads(self.files[index].read_text(encoding="utf-8"))
        callback(metadata)
        self.helper.write(self.files[index],metadata)

    def test_eight_cases_use_same_frozen_blend_and_v3_source(self):
        result=self.validate()
        self.assertTrue(result["pass"])
        self.assertEqual(len(result["captures"]),8)
        self.assertTrue(result["progressZeroByteIdenticalToDisabled"])
        self.assertFalse(result["appearanceRestoration"]["verified"])
        self.assertEqual(result["fixedSettings"]["dissolve"]["frozenPlayback"]["transitionElapsed"],.1)
        self.assertEqual(result["fixedSettings"]["qualityCandidate"],6)

    def test_seed_plane_frozen_blend_pose_camera_or_body_changes_rejected(self):
        original=json.loads(self.files[3].read_text(encoding="utf-8"))
        edits=[lambda m:m["dissolve"].__setitem__("seed",19),
               lambda m:m["dissolve"]["direction"].__setitem__(0,.8),
               lambda m:m["dissolve"].__setitem__("scanMax",.9),
               lambda m:m["dissolve"]["startWorld"][3].__setitem__(0,.1),
               lambda m:m["dissolve"]["frozenPlayback"].__setitem__("time",.5),
               lambda m:m["dissolve"]["frozenPlayback"].__setitem__("transitionElapsed",.15),
               lambda m:m["dissolve"]["frozenPlayback"]["transitionStartPose"][0]["rotation"].__setitem__(0,.1),
               lambda m:m["dissolve"]["frozenJointPose"][1]["translation"].__setitem__(0,.1),
               lambda m:m["camera"]["position"].__setitem__(0,.1),
               lambda m:m["neon"].__setitem__("coreIntensity",3),
               lambda m:m.__setitem__("qualityCandidate",4)]
        for edit in edits:
            changed=copy.deepcopy(original);edit(changed);self.helper.write(self.files[3],changed)
            with self.assertRaisesRegex(ValueError,"Dissolve A/B changed"):self.validate()

    def test_explicit_progress_noise_edge_and_elapsed_contracts(self):
        original=json.loads(self.files[6].read_text(encoding="utf-8"))
        for key,value in (("progress",.6),("noiseStrength",.03),("edgeEnabled",0),("elapsed",.1),("enabled",0)):
            changed=copy.deepcopy(original);changed["dissolve"][key]=value;self.helper.write(self.files[6],changed)
            with self.assertRaises(ValueError):self.validate()

    def test_missing_frozen_pose_and_incomplete_or_nonfinite_metadata_rejected(self):
        original=json.loads(self.files[3].read_text(encoding="utf-8"))
        edits=[lambda m:m.pop("dissolve"),lambda m:m["dissolve"].pop("seed"),
               lambda m:m["dissolve"].pop("frozenJointPose"),
               lambda m:m["dissolve"]["frozenPlayback"].pop("transitionStartPose"),
               lambda m:m["dissolve"].__setitem__("noiseScale",float("nan"))]
        for edit in edits:
            changed=copy.deepcopy(original);edit(changed);self.helper.write(self.files[3],changed)
            with self.assertRaises(ValueError):self.validate()

    def test_zero_progress_and_disabled_raw_images_must_match(self):
        self.files[1].with_suffix(".png").write_bytes(synthetic_png(red=81))
        with self.assertRaisesRegex(ValueError,"Progress 0 raw PNG differs"):self.validate()

    def test_restore_proves_checkpoint_and_image_equivalence(self):
        before=self.directory/"before.json";after=self.directory/"after.json"
        state=self.helper.metadata(6,1,1,1)
        state["dissolve"]=dissolve_metadata(active=False)
        for path in (before,after):
            self.helper.write(path,state);path.with_suffix(".png").write_bytes(synthetic_png())
        self.assertTrue(self.validate(before_json=before,after_json=after)["appearanceRestoration"]["verified"])
        changed=copy.deepcopy(state);changed["dissolve"]["seed"]=18;self.helper.write(after,changed)
        with self.assertRaisesRegex(ValueError,"Appearance restore changed"):self.validate(before_json=before,after_json=after)


if __name__=="__main__":unittest.main(verbosity=2)
