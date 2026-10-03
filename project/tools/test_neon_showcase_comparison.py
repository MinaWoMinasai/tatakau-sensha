"""Validate seven engine captures at one paused pose, plus optional appearance restoration.

Uses only the standard library. Does not modify/retouch images or infer visual
quality from hashes. The comparison flags alone do not prove restoration.
"""

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODEL = "resources/models/neon_hologram/AvatarSample_B.glb"
MODEL_SHA = "7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518"
CASES = [
    ("original_auto",0,0,0,0), ("original_authored",1,0,0,1),
    ("v1_coverage",2,0,1,1), ("v1_coverage_core_halo",2,1,1,1),
    ("v1_sdf_core_halo",3,1,2,1), ("v2_coverage_core_halo",4,1,1,1),
    ("v2_sdf_core_halo",5,1,2,1),
]
FIXED_NEON = (
    "bodyColor","bodyEmission","outlineWidth","outlineEnabled","internalEnabled",
    "rimStrength","rimPower","outlineIntensity","lineColor","internalWidth",
    "internalIntensity","internalThreshold","maskColor","maskIntensity","maskDiagnostic",
    "geometryLines","geometryWidth","geometryIntensity","geometryColor","alphaCutout",
    "lineDiagnostic","coreColor","coreIntensity","haloColor","haloIntensity",
    "outlineCoreColor","outlineCoreIntensity","sdfHaloWidthTexels","sdfLodBlendStart","sdfLodBlendEnd",
)
FIXED_SUBMESH = ("material","lineStrength","geometryStrength","thresholdScale","alphaCutoff")
HASH_FIELDS = {
    "coverageSha256":"coverageManifestExpectedSha256",
    "sdfSha256":"sdfManifestExpectedSha256",
    "authoringSha256":"authoringManifestExpectedSha256",
    "authoringVersionSha256":"authoringVersionManifestExpectedSha256",
}
FIXED_DISSOLVE = (
    "active", "playing", "enabled", "progress", "direction", "scanMin", "scanMax",
    "noiseStrength", "noiseScale", "seed", "edgeEnabled", "edgeColor", "edgeIntensity",
    "edgeWidth", "elapsed", "waitDuration", "duration", "playbackSpeed", "directionPreset",
    "coordinateSpace", "distanceUnits",
)


def reported_hash(surface,key):
    """Accept old field names but keep their manifest-expected meaning explicit."""
    old,new=surface.get(key),surface.get(HASH_FIELDS[key])
    require(old is None or new is None or old==new,"Conflicting manifest SHA aliases: "+key)
    return new if new is not None else (old or "")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def finite(value, label):
    if isinstance(value,float):
        require(math.isfinite(value),"Nonfinite metadata: "+label)
    elif isinstance(value,list):
        for i,item in enumerate(value):finite(item,f"{label}[{i}]")
    elif isinstance(value,dict):
        for key,item in value.items():finite(item,label+"."+key)


def png_info(path):
    data=path.read_bytes()
    require(len(data)>=33 and data[:8]==b"\x89PNG\r\n\x1a\n","Missing/invalid PNG: "+str(path))
    require(data[8:16]==b"\x00\x00\x00\x0dIHDR","PNG requires a first 13-byte IHDR: "+str(path))
    width,height=struct.unpack_from(">II",data,16)
    require(width>0 and height>0,"PNG dimensions must be positive")
    return {"path":str(path),"width":width,"height":height,"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest().upper()}


def load_metadata(path):
    value=json.loads(path.read_text(encoding="utf-8-sig"))
    finite(value,str(path))
    require(value.get("model")==MODEL and value.get("expectedModelSha256")==MODEL_SHA,"Unexpected model/source SHA metadata")
    require(value.get("build")=="Development","Showcase must be Developer-only")
    require(value.get("schemaVersion")==1,"Unknown capture metadata schema")
    if "dissolve" in value: validate_dissolve_metadata(value["dissolve"])
    return value


def validate_dissolve_metadata(effect):
    require(isinstance(effect,dict) and all(key in effect for key in FIXED_DISSOLVE),
            "Dissolve metadata is incomplete")
    require(isinstance(effect["active"],bool) and isinstance(effect["playing"],bool),
            "Dissolve state flags must be boolean")
    require(effect["enabled"] in (0,1) and effect["edgeEnabled"] in (0,1)
            and 0<=effect["progress"]<=1,"Invalid dissolve endpoint state")
    for key in ("direction","edgeColor"):
        require(isinstance(effect[key],list) and len(effect[key])==3,"Dissolve vector dimensions differ: "+key)
    require(effect["noiseStrength"]>=0 and effect["noiseScale"]>0 and effect["edgeIntensity"]>=0
            and effect["edgeWidth"]>=0 and effect["elapsed"]>=0 and effect["waitDuration"]>=0
            and effect["duration"]>0 and effect["playbackSpeed"]>0,"Invalid dissolve units/timing")
    require(isinstance(effect["seed"],int) and 0<=effect["seed"]<=0xffffffff,"Dissolve seed must be uint32")
    require(effect["coordinateSpace"]=="Frozen skinned model space before World; start camera plane remains fixed.",
            "Dissolve coordinate-space contract changed")
    require(effect["distanceUnits"]=="scan bounds, noise strength and edge width in model units; noise scale in lattice cells/model unit",
            "Dissolve distance-unit contract changed")
    if not effect["active"]: return
    require(sum(x*x for x in effect["direction"])>1e-12 and effect["scanMax"]>effect["scanMin"],
            "Frozen dissolve scan plane/range is invalid")
    require(all(key in effect for key in ("frozenPlayback","startWorld","startCameraWorld","frozenJointPose")),
            "Frozen dissolve checkpoint metadata missing")
    playback=effect["frozenPlayback"]
    require(all(key in playback for key in ("animationIndex","clip","time","speed","playing","loop","paused",
            "transitionActive","transitionDuration","transitionElapsed")),"Frozen playback/blend state incomplete")
    if playback["transitionActive"]:
        require(bool(playback.get("transitionStartPose")),"Frozen transition start pose is missing")
    for key in ("startWorld","startCameraWorld"):
        matrix=effect[key]
        require(isinstance(matrix,list) and len(matrix)==4 and all(isinstance(row,list) and len(row)==4 for row in matrix),
                "Frozen start matrix dimensions differ: "+key)
    poses=effect["frozenJointPose"]
    require(isinstance(poses,list) and len(poses)>0,"Frozen displayed joint pose is missing")
    require(len({pose.get("joint") for pose in poses})==len(poses),"Frozen joint names are duplicated")
    for pose in poses:
        require(isinstance(pose.get("joint"),str) and bool(pose["joint"]),"Frozen joint name missing")
        require(all(isinstance(pose.get(key),list) and len(pose[key])==length
                    for key,length in (("translation",3),("scale",3),("rotation",4))),"Frozen pose dimensions differ")


def fixed_settings(metadata):
    # Only the explicit case table above varies reconstruction/mask application.
    # All colors/intensities, geometry state and submesh tuning remain checked.
    result={key:metadata[key] for key in ("model","expectedModelSha256","build","renderScale","mode","framing","camera","transform","animation","bloom","resolution")}
    result["neon"]={key:metadata["neon"][key] for key in FIXED_NEON}
    result["submeshes"]=[{key:s[key] for key in FIXED_SUBMESH} for s in metadata["submeshes"]]
    for key in ("gpu","sceneViewportResolution","resolutionSource","textureProvenance","capture",
                "validation","queueTimestampFrequencyHz","dissolve"):
        if key in metadata:result[key]=metadata[key]
    return result


def sources(metadata, repo):
    records=[]
    for surface in metadata["submeshes"]:
        record={"material":surface["material"],"images":[]}
        for path_key,hash_key in (("mask","coverageSha256"),("distanceMask","sdfSha256")):
            name=surface.get(path_key,"")
            if not name:continue
            asset=(repo/"project"/name).resolve()
            directory=(repo/"project/resources/models/neon_hologram/line_masks").resolve()
            require(asset.is_relative_to(directory),"Capture source escapes authored mask directory")
            require(asset.is_file(),"Captured source file missing: "+str(asset))
            actual=digest(asset)
            reported=reported_hash(surface,hash_key)
            require(not reported or reported==actual,"Captured manifest expects a different source revision: "+name)
            record["images"].append({"path":name,"actualSha256":actual,"reportedSha256":reported or None,
                                     "manifestExpectedMatchesCurrentFile":bool(reported),"gpuContentsHashed":False})
        for key in ("authoringSha256","authoringVersionSha256"):
            record[key]=reported_hash(surface,key)
        record["authoringRevision"]=surface.get("authoringRevision","")
        records.append(record)
    return records


def verify_quality_sources(metadata, case, repo, version_override=None):
    if case<2 and version_override is None:return
    version=version_override or ("v1" if case<=4 else "v2")
    assigned=[s for s in metadata["submeshes"] if s.get("mask")]
    require(len(assigned)==5,"Quality candidate must assign the inspected five materials")
    directory=repo/"project/resources/models/neon_hologram/line_masks/quality"
    binding_config=json.loads((directory/"bindings.json").read_text(encoding="utf-8"))
    authoring_path=directory/"authoring.json"
    authoring=json.loads(authoring_path.read_text(encoding="utf-8"))
    selected=next(v for v in binding_config["versions"] if v["id"]==version)
    source_version=next(v for v in authoring["versions"] if v["id"]==version)
    canonical={key:authoring[key] for key in ("size","supersample","sdfRangeTexels","haloWidthTexels")}
    canonical["version"]=source_version
    version_sha=hashlib.sha256(json.dumps(canonical,sort_keys=True,separators=(",",":"),ensure_ascii=False).encode("utf-8")).hexdigest().upper()
    require(selected["authoringSha256"]==digest(authoring_path) and selected["authoringVersionSha256"]==version_sha,"Current quality manifest has stale authoring hashes")
    require(metadata["neon"]["sdfRangeTexels"]==binding_config["sdfRangeTexels"],"Quality case distance range does not match selected data")
    expected={b["material"]:b for b in selected["bindings"]}
    require({s["material"] for s in assigned}==set(expected),"Quality Material assignments differ from the inspected manifest")
    for surface in assigned:
        require(surface.get("maskBound") is True and surface.get("distanceBound") is True,"Quality comparison requires both source pairs loaded")
        require("_"+version+"_coverage.png" in surface["mask"] and "_"+version+"_sdf.png" in surface["distanceMask"],"Quality case source version mismatch")
        for key in ("coverageSha256","sdfSha256","authoringSha256","authoringVersionSha256"):
            value=reported_hash(surface,key)
            require(len(value)==64 and all(c in "0123456789ABCDEF" for c in value),"Quality source hash missing/invalid: "+key)
        binding=expected[surface["material"]]
        for kind,path_key in (("coverage","mask"),("sdf","distanceMask")):
            require(surface[path_key]=="resources/models/neon_hologram/line_masks/quality/"+binding[kind+"File"],"Quality source path differs from exact Material assignment")
            require(reported_hash(surface,kind+"Sha256")==binding[kind+"Sha256"],"Capture manifest source hash differs from current selected Material")
        # A new version changes the whole-document SHA while protected V1/V2
        # source objects and PNGs remain identical. Only that exact saved source
        # document is allowed, and only with the unchanged per-version hash.
        whole_hash=reported_hash(surface,"authoringSha256")
        protected=authoring.get("preservedQualitySource",{})
        previous=protected.get("versions",{}).get(version,{})
        allowed={selected["authoringSha256"]}
        if previous.get("authoringVersionSha256")==selected["authoringVersionSha256"]:
            allowed.add(protected.get("authoringSha256",""))
        require(whole_hash in allowed,"Capture authoring revision does not match current data: authoringSha256")
        require(reported_hash(surface,"authoringVersionSha256")==selected["authoringVersionSha256"],
                "Capture authoring revision does not match current data: authoringVersionSha256")
        require(surface.get("authoringRevision")==selected["authoringRevision"],"Quality authoring revision differs")


def restoration(before_json,after_json,repo):
    before,after=load_metadata(before_json),load_metadata(after_json)
    require(fixed_settings(before)==fixed_settings(after),"Appearance restore changed camera / pose / settings")
    require(before["neon"]==after["neon"] and before["qualityCandidate"]==after["qualityCandidate"]
            and before["submeshes"]==after["submeshes"],"Appearance restore changed selected source or rendering parameters")
    a,b=png_info(before_json.with_suffix(".png")),png_info(after_json.with_suffix(".png"))
    require(a["sha256"]==b["sha256"],"Restored clean PNG is not byte-identical to before (not a pixel-equivalence claim)")
    sources(before,repo);sources(after,repo)
    return {"verified":True,"before":str(before_json),"after":str(after_json),"pngByteIdentical":True}


def validate_comparison(directory,repo=ROOT,before_json=None,after_json=None):
    require((before_json is None)==(after_json is None),"Provide both --before-json and --after-json")
    model=(repo/"project"/MODEL)
    require(model.is_file() and digest(model)==MODEL_SHA,"Original GLB hash differs from expected source")
    result={"directory":str(directory),"method":"Raw PNG magic/IHDR + recorded engine settings + manifest-expected source hashes; GPU contents are not hashed; no image quality score",
            "modelSha256":MODEL_SHA,"captures":[]}
    reference=None
    original_range=None
    for index,(label,candidate,split,mode,blend) in enumerate(CASES):
        stem=directory/(f"{index:02d}_"+label)
        metadata=load_metadata(stem.with_suffix(".json"))
        image=png_info(stem.with_suffix(".png"))
        require(metadata["resolution"]==[image["width"],image["height"]],"PNG / recorded resolution mismatch")
        require(metadata.get("comparison")=={"case":index,"label":label,"count":7,"samePausedPose":True,"restoreAppearanceAfterCapture":True},"Comparison case contract differs")
        require(metadata["qualityCandidate"]==candidate and metadata["neon"]["splitCoreHalo"]==split
                and metadata["neon"]["maskRenderMode"]==mode and metadata["neon"]["maskBlend"]==blend,"Explicit quality case state differs")
        require(metadata["mode"]=="Neon" and metadata["animation"]["paused"] is True and metadata["camera"]["orbit"] is False,"Seven-way comparisons must hold Neon pose and camera paused")
        require(metadata["bloom"]["grayscale"] is False and metadata["bloom"]["taa"] is False,"Showcase comparison must exclude menu grayscale / TAA history")
        require(metadata["neon"]["geometryLines"]==0,"Mesh Geometry Lines must be OFF for the line-art comparison")
        if "dissolve" in metadata:
            require(metadata["dissolve"]["active"] is False and metadata["dissolve"]["enabled"]==0,
                    "Seven-way quality comparison must keep dissolve inactive/disabled")
        if index==0:original_range=metadata["neon"]["sdfRangeTexels"]
        if index<2:require(metadata["neon"]["sdfRangeTexels"]==original_range,"Inactive distance range changed between original cases")
        fixed=fixed_settings(metadata)
        if reference is None:reference=fixed
        require(fixed==reference,"A/B input/settings changed outside explicit case state: "+label)
        verify_quality_sources(metadata,index,repo)
        result["captures"].append({"case":index,"label":label,"candidate":candidate,"png":image,"json":str(stem.with_suffix(".json")),"sources":sources(metadata,repo)})
    result["fixedSettings"]=reference
    result["appearanceRestoration"]=restoration(before_json,after_json,repo) if before_json else {
        "verified":False,"reason":"Case metadata records restore intent only; separate before/after PNG+JSON were not supplied."}
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
