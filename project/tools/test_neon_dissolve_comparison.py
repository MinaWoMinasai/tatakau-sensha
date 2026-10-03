"""Validate eight unretouched, fixed-pose engine dissolve captures.

Checks metadata and source identity, not visual quality, clipping correctness or
GPU contents. The original seven-condition quality validator remains separate.
"""

import argparse
import copy
import json
import math
from pathlib import Path

import test_neon_showcase_comparison as quality


CASES = (
    ("disabled",0,0,True,True), ("progress_0",0,1,True,True),
    ("progress_025",.25,1,True,True), ("progress_050",.5,1,True,True),
    ("progress_075",.75,1,True,True), ("progress_1",1,1,True,True),
    ("no_noise_050",.5,1,False,True), ("no_edge_050",.5,1,True,False),
)
VARIABLE_DISSOLVE = ("progress","elapsed","enabled","noiseStrength","edgeEnabled")


def fixed_settings(metadata):
    result=quality.fixed_settings(metadata)
    result["dissolve"]=copy.deepcopy(result["dissolve"])
    for key in VARIABLE_DISSOLVE:result["dissolve"].pop(key)
    # Unlike the seven quality cases, mask selection and all its parameters are
    # fixed for all eight dissolve cases, including the inactive rendering case.
    result["neon"]=metadata["neon"]
    result["qualityCandidate"]=metadata["qualityCandidate"]
    result["submeshes"]=metadata["submeshes"]
    return result


def validate_comparison(directory,repo=quality.ROOT,before_json=None,after_json=None):
    quality.require((before_json is None)==(after_json is None),"Provide both --before-json and --after-json")
    model=repo/"project"/quality.MODEL
    quality.require(model.is_file() and quality.digest(model)==quality.MODEL_SHA,"Original GLB hash differs from expected source")
    result={"directory":str(directory),"modelSha256":quality.MODEL_SHA,
            "method":"Raw PNG magic/IHDR, recorded fixed frozen pose/scan/camera/appearance, exact manifest-expected source hashes; no image retouching or visual-quality score",
            "captures":[]}
    reference=None
    noise_strength=None
    for index,(label,progress,enabled,noise,edge) in enumerate(CASES):
        stem=directory/(f"{index:02d}_"+label)
        metadata=quality.load_metadata(stem.with_suffix(".json"))
        image=quality.png_info(stem.with_suffix(".png"))
        quality.require(metadata["resolution"]==[image["width"],image["height"]],"PNG / recorded resolution mismatch")
        quality.require(metadata.get("dissolveComparison")=={"case":index,"label":label,"count":8,
                "sameFrozenPose":True,"restorePlaybackAfterCapture":True},"Dissolve comparison case contract differs")
        effect=metadata.get("dissolve",{})
        quality.validate_dissolve_metadata(effect)
        quality.require(effect["active"] is True and effect["playing"] is False,
                        "Eight-way comparison must hold the controller and displayed pose frozen")
        quality.require(effect["progress"]==progress and effect["enabled"]==enabled,
                        "Explicit dissolve case endpoint differs: "+label)
        expected_elapsed=effect["waitDuration"]+effect["duration"]*progress
        quality.require(math.isclose(effect["elapsed"],expected_elapsed,rel_tol=1e-6,abs_tol=1e-5),
                        "Dissolve seek time differs from wait + duration * progress")
        if index==0:
            noise_strength=effect["noiseStrength"]
            quality.require(noise_strength>0,"Noise diagnostic requires a nonzero reference noise strength")
        quality.require(effect["noiseStrength"]==(noise_strength if noise else 0)
                        and effect["edgeEnabled"]==int(edge),"Explicit noise / edge diagnostic differs: "+label)
        quality.require(metadata["mode"]=="Neon" and metadata["animation"]["paused"] is True
                        and metadata["camera"]["orbit"] is False,"Dissolve comparison must hold Neon pose/camera paused")
        quality.require(metadata["bloom"]["grayscale"] is False and metadata["bloom"]["taa"] is False,
                        "Dissolve comparison must exclude menu grayscale / TAA history")
        quality.require(metadata["neon"]["geometryLines"]==0,"Mesh Geometry Lines must be OFF for this comparison")
        fixed=fixed_settings(metadata)
        if reference is None:reference=fixed
        quality.require(fixed==reference,"Dissolve A/B changed plane / seed / frozen blend / camera / appearance: "+label)
        candidate=metadata["qualityCandidate"]
        quality.require(isinstance(candidate,int) and 0<=candidate<=7,"Unknown line quality candidate")
        if candidate>=2:
            version="v1" if candidate<4 else ("v2" if candidate<6 else "v3")
            quality.verify_quality_sources(metadata,2,repo,version_override=version)
        result["captures"].append({"case":index,"label":label,"progress":progress,"png":image,
            "json":str(stem.with_suffix(".json")),"sources":quality.sources(metadata,repo)})
    # The endpoint contract is strict: enabling progress=0 must not add a new
    # boundary emission or alter the previous appearance. Same raw PNG bytes
    # are stronger than an unchecked metadata checkbox, but do not prove every
    # possible animation/scene obeys the endpoint contract.
    quality.require(result["captures"][0]["png"]["sha256"]==result["captures"][1]["png"]["sha256"],
                    "Progress 0 raw PNG differs from disabled effect")
    result["progressZeroByteIdenticalToDisabled"]=True
    result["fixedSettings"]=reference
    result["appearanceRestoration"]=quality.restoration(before_json,after_json,repo) if before_json else {
        "verified":False,"reason":"Batch restore intent alone does not prove restoration; provide before/after raw PNG + JSON."}
    result["pass"]=True
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory",type=Path)
    parser.add_argument("--before-json",type=Path)
    parser.add_argument("--after-json",type=Path)
    parser.add_argument("--output",type=Path)
    args=parser.parse_args()
    result=validate_comparison(args.directory,before_json=args.before_json,after_json=args.after_json)
    text=json.dumps(result,ensure_ascii=False,indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(text+"\n",encoding="utf-8")
    else:print(text)


if __name__=="__main__":main()
