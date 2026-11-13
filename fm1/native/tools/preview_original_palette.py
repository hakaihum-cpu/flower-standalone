#!/usr/bin/env python3
"""FM-1 SEL/FX original-source palette comparison (PRE-VQ ONLY).

Uses the user's ORIGINAL JPEG frames or SHA-verified SEL montage sheets.
Writes 8/16-color RGB565 images and a source/8/16 side-by-side comparison.
Does NOT read existing VQ/res.bin images, alter firmware, or change CI.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import math
import struct
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

SPECS={"fx":(144,36,(128,78)), "eureka":(576,36,(128,78)),
       "sel":(628,27,(96,96))}
FX_ORIGINAL_SHA="827b5aa6a339927cb909a5ffe1b129513d946911cc78ea96d34aa110e6cd1883"

def get_frames(kind, source, manifest, start, count):
    total, _, dims=SPECS[kind]
    if kind in ("fx","eureka"):
        blob=source.read_bytes()
        if blob[:4]!=b"CRF1" or len(blob)<8+12*total or int.from_bytes(blob[4:8],"little")!=total:
            raise ValueError("expected original CRF1 JPEG source pack, not VQ media")
        if kind=="fx" and hashlib.sha256(blob).hexdigest()!=FX_ORIGINAL_SHA:
            raise ValueError("original EFFECTS pack SHA256 does not match")
        images=[]
        for f in range(start,start+count):
            pos,flags,size=struct.unpack_from("<III",blob,8+12*f)
            data=blob[pos:pos+size]
            if pos<8+12*total or pos+size>len(blob) or len(data)!=size or data[:2]!=b"\xff\xd8" or data[-2:]!=b"\xff\xd9":
                raise ValueError(f"source JPEG {f} invalid")
            with Image.open(io.BytesIO(data)) as image:
                images.append(image.convert("RGB").resize(dims,Image.Resampling.LANCZOS))
        return images
    manifest_data=json.loads(manifest.read_text(encoding="utf-8"))
    sheets=manifest_data["files"]
    if len(sheets)!=8 or sum(s["frames"] for s in sheets)!=total:
        raise ValueError("SEL original manifest must cover all 628 frames")
    # Never substitute decoded res.bin/VQ pixels for the original sheets.
    for sheet in sheets:
        file=source/sheet["name"]
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest()!=sheet["sha256"]:
            raise ValueError(f"missing/changed ORIGINAL SEL image: {file}")
    images=[]
    for f in range(start,start+count):
        remaining=f
        for sheet in sheets:
            if remaining<sheet["frames"]:break
            remaining-=sheet["frames"]
        cols,rows=sheet["grid"]
        with Image.open(source/sheet["name"]) as original:
            image=original.convert("RGB")
            x,y=remaining%cols,remaining//cols
            box=(round(x*image.width/cols)+2,round(y*image.height/rows)+2,
                 round((x+1)*image.width/cols)-2,round((y+1)*image.height/rows)-2)
            images.append(image.crop(box).resize(dims,Image.Resampling.LANCZOS))
    return images

def q565(rgb):
    rgb=np.asarray(rgb,dtype=np.uint16)
    p=((rgb[:,0]>>3)<<11)|((rgb[:,1]>>2)<<5)|(rgb[:,2]>>3)
    out=np.stack(((p>>11)*255//31,((p>>5)&63)*255//63,(p&31)*255//31),axis=1)
    return p,out.astype(np.uint8)

def palette_from_original(frames, colors):
    """Clip-constant palette. Choose from actual source RGB, not a sepia LUT."""
    w,h=frames[0].size
    n=len(frames)
    # An incomplete last SEL clip has 7, not 27 frames. Avoid black
    # padding pixels becoming artificial palette training inputs.
    cols=min(6,n)
    while n%cols:
        cols-=1
    montage=Image.new("RGB",(cols*w,(n//cols)*h))
    for i,im in enumerate(frames):
        montage.paste(im,((i%cols)*w,(i//cols)*h))
    src=np.stack([np.asarray(im,dtype=np.uint8) for im in frames])
    gray=((77*src[...,0].astype(np.int32)+150*src[...,1].astype(np.int32)+
           29*src[...,2].astype(np.int32))>>8)
    edge=np.zeros_like(gray)
    edge[:,:,1:]+=abs(gray[:,:,1:]-gray[:,:,:-1])
    edge[:,1:,:]+=abs(gray[:,1:,:]-gray[:,:-1,:])
    weights=1+np.minimum(edge,127)/127.0
    best=None
    for method in (Image.Quantize.MEDIANCUT,Image.Quantize.MAXCOVERAGE,
                   Image.Quantize.FASTOCTREE):
        image=montage.quantize(colors=colors,method=method,dither=Image.Dither.NONE)
        rgb=np.array(image.getpalette()[:colors*3],dtype=np.uint8).reshape(colors,3)
        p565,screen=q565(rgb)
        # Match to actual RGB565 display palette (not unsnapped RGB888).
        points=src.reshape(-1,3).astype(np.int32)
        nearest=np.zeros(points.shape[0],dtype=np.uint8)
        best_dist=np.full(points.shape[0],2**30,dtype=np.int32)
        for j,color in enumerate(screen.astype(np.int32)):
            d=((points-color)**2).sum(axis=1)
            mask=d<best_dist
            nearest[mask]=j
            best_dist[mask]=d[mask]
        restored=screen[nearest.reshape(src.shape[:-1])]
        error=((src.astype(np.int32)-restored.astype(np.int32))**2).sum(axis=-1)
        weighted=(error*weights).mean(axis=(1,2))
        quality=(float(weighted.max()),float(weighted.mean()))
        candidate=(quality,p565,restored,error)
        if best is None or (quality<best[0] and len(set(map(int,p565)))>=len(set(map(int,best[1])))):
            best=candidate
    quality,pal,result,error=best
    psnr=10*math.log10(3*255*255/max(1e-9,float(error.mean())))
    report={"colors":colors,"rgb565":[f"0x{int(x):04X}" for x in pal],
            "distinct":len(set(map(int,pal))),"psnr_db":round(psnr,3),
            "worst_weighted":round(quality[0],2),
            "mean_weighted":round(quality[1],2)}
    return result,report

def write_compare(frames,results,output,start,kind,clip):
    # Source and both no-VQ variants use identical LCD pixel geometry.
    scale=3
    w,h=frames[0].size
    chosen=sorted({0,len(frames)//3,2*len(frames)//3,len(frames)-1})
    result=Image.new("RGB",(3*(w*scale+12)+12,26+len(chosen)*(h*scale+28)+12),"#171717")
    d=ImageDraw.Draw(result)
    d.text((12,4),f"{kind.upper()} ORIGINAL > 8/16 colors, NO VQ, clip {clip}",fill="white")
    for row,f in enumerate(chosen):
        for col,which in enumerate(("ORIGINAL","8 COLORS","16 COLORS")):
            data=frames[f] if col==0 else Image.fromarray(results[8 if col==1 else 16][f])
            x=12+col*(w*scale+12)
            y=26+row*(h*scale+28)
            d.text((x,y),f"frame {start+f:03} - {which}",fill="white")
            result.paste(data.resize((w*scale,h*scale),Image.Resampling.NEAREST),(x,y+20))
    result.save(output/"compare_pre_vq.png")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--mode",choices=tuple(SPECS),required=True)
    p.add_argument("--source",type=Path,required=True,
                   help="FX/EUREKA: original CRF1 pack; SEL: original sheet directory")
    p.add_argument("--manifest",type=Path,default=Path("Resources/fm1_photo_96_chunks/manifest.json"))
    p.add_argument("--clip",type=int,required=True,help="zero-based source clip")
    p.add_argument("--out",type=Path,required=True)
    a=p.parse_args()
    total,stride,_=SPECS[a.mode]
    start=a.clip*stride
    if a.clip<0 or start>=total:raise SystemExit("invalid clip")
    count=min(stride,total-start)  # SEL last clip is 7 frames, not 27.
    frames=get_frames(a.mode,a.source,a.manifest,start,count)
    if len(frames)!=count:raise AssertionError("not all original frames returned")
    a.out.mkdir(parents=True,exist_ok=True)
    outputs={}
    report={"mode":a.mode,"source_clip":a.clip,"start":start,"frames":count,
            "stage":"PRE-VQ, original JPEG/sheets only, zero dithering",
            "quality":{}}
    for colors in (8,16):
        reconstructed,quality=palette_from_original(frames,colors)
        outputs[colors]=reconstructed
        report["quality"][str(colors)]=quality
        folder=a.out/f"{colors}colors"
        folder.mkdir(exist_ok=True)
        for i,pixels in enumerate(reconstructed):
            Image.fromarray(pixels.astype(np.uint8)).save(folder/f"frame_{start+i:03}.png")
    write_compare(frames,outputs,a.out,start,a.mode,a.clip)
    (a.out/"quality.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    print("PASS original-source PRE-VQ:",a.mode, "clip",a.clip, "frames",count,
          "8c PSNR",report["quality"]["8"]["psnr_db"],
          "16c PSNR",report["quality"]["16"]["psnr_db"])
    print("PREVIEW",a.out/"compare_pre_vq.png")
if __name__=="__main__":
    main()
