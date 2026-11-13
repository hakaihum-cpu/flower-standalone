#!/usr/bin/env python3
"""Offline source-only loss-controlled patch feasibility, never writes firmware.

Uses the exact eight user-supplied source montage files, and an explicitly
named 279936-byte VQ baseline. It NEVER labels an older VQ archive as current.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
import mhp2

MANIFEST = [
 ("file_000000008e98820690554a4b75774ed5.png",9,9,81,"24a69a98804e44ef8ba78fc8ccd259c28c0ee52526cc662b5478ca9b5d78132f"),
 ("file_00000000f1a08206a57bdcc319a336d0.png",8,8,64,"40c47bac6a17b199a3e515610bec5a95fdb6a3a477ee9e1a28848cf637f7f368"),
 ("file_00000000e7388206b7bb79a8b73154ee(2).png",10,10,100,"77543d5543d927f8254281f75144490079f502788d55633d9c2a2dc005a91c2c"),
 ("file_00000000e9448206897342ec340f08d4(2).png",9,9,81,"5288f17826dcd0649fd1d8566d8bfa02d45d4a5a15faafa5385b82ba6bba9efc"),
 ("file_00000000cf38820685e20ea7661aea2d(2).png",9,9,81,"64fdfa02fa8127e4cc110061abe5ef05a74477300ceb8ade7c235b38b7849669"),
 ("file_000000006c3882069f4e9ec393c3615f(2).png",9,9,81,"c82bae262f5d2314d8667ae08117e1df63bbd1b295b054f6bc6859291ade5074"),
 ("Gemini_Generated_Image (3).jpeg",10,8,70,"049336425575d8aebc0c0e83a3588a65cbf53e51019e66e02c067a5700921669"),
 ("Gemini_Generated_Image (4).jpeg",10,7,70,"85e11797296b3ef801e571a804e15db3c368273701b2a6cc764ef6442e72674e"),
]

def load_sources(root:Path):
    out=[]
    for bank,(name,cols,rows,n,digest) in enumerate(MANIFEST):
        path=root/name
        if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            raise ValueError(f"Source original SHA mismatch: {name}")
        with Image.open(path) as raw:
            image=raw.convert("RGB")
            for i in range(n):
                x,y=i%cols,i//cols
                x0=round(x*image.width/cols)+2
                y0=round(y*image.height/rows)+2
                x1=round((x+1)*image.width/cols)-2
                y1=round((y+1)*image.height/rows)-2
                frame=image.crop((x0,y0,x1,y1)).resize((96,96),Image.Resampling.LANCZOS)
                out.append(np.asarray(frame,dtype=np.uint8))
        print("VERIFIED ORIGINAL source bank",bank,"frames",n,flush=True)
    return np.stack(out)

def palettes_and_indices(raw):
    palettes=[]
    for clip in range(24):
        enc=np.frombuffer(raw,dtype="<u2",count=16,offset=clip*32)
        r=(enc>>11)&31;g=(enc>>5)&63;b=enc&31
        palette=np.stack([(r*255+15)//31,(g*255+31)//63,(b*255+15)//31],axis=-1).astype(np.uint8)
        palettes.append(palette)
    frames=np.empty((628,96,96),dtype=np.uint8)
    data=np.frombuffer(raw,dtype=np.uint8)
    for clip in range(24):
        tiles=data[768+clip*4096:768+(clip+1)*4096].reshape(256,4,4)
        words=np.empty((256,4,8),dtype=np.uint8)
        words[:,:,:,] = np.repeat(tiles,2,axis=2)
        words[:,:,0::2]=tiles>>4
        words[:,:,1::2]=tiles&15
        for f in range(clip*27,min((clip+1)*27,628)):
            tile_ids=data[99072+f*288:99072+(f+1)*288].reshape(24,12)
            frames[f]=words[tile_ids].transpose(0,2,1,3).reshape(96,96)
    return np.stack(palettes),frames

def make_patches(src, rgb_palette, initial_indices, number=1800):
    # Preserve current clip palette, frame identity and 96x96 geometry.
    # A patch is exactly one 8x4 tile encoded as a 2-byte tile ID + 16
    # bytes of packed 4-bit original-nearest-palette pixel indices.
    proposals=[]
    closest_tiles=[]
    for f in range(len(src)):
        clip=f//27
        palette=rgb_palette[clip].astype(np.int32)
        source=src[f].astype(np.int32)
        current=rgb_palette[clip][initial_indices[f]].astype(np.int32)
        distance=((source.reshape(-1,3)[:,None,:]-palette[None,:,:])**2).sum(axis=2)
        closest=distance.argmin(axis=1).astype(np.uint8).reshape(96,96)
        best=rgb_palette[clip][closest].astype(np.int32)
        # RGB error + 2x source-edge importance to favor eyes, lips, outlines.
        gray=(77*source[:,:,0]+150*source[:,:,1]+29*source[:,:,2])>>8
        edge=np.zeros((96,96),dtype=np.int32)
        edge[:,1:]+=np.abs(gray[:,1:]-gray[:,:-1]);edge[1:,:]+=np.abs(gray[1:,:]-gray[:-1,:])
        weight=1.0+np.clip(edge,0,100)/50.0
        err_before=((source-current)**2).sum(axis=-1)
        err_after=((source-best)**2).sum(axis=-1)
        gain=((err_before-err_after)*weight).reshape(24,4,12,8).sum(axis=(1,3)).ravel()
        gain_rgb=(err_before-err_after).reshape(24,4,12,8).sum(axis=(1,3)).ravel()
        # Require BOTH higher edge fidelity and non-worse actual RGB MSE.
        # Guarantees source-referenced frame PSNR never falls due to overlay.
        for tile in np.flatnonzero((gain>0) & (gain_rgb>0)):
            proposals.append((float(gain[tile]),f,int(tile)))
        closest_tiles.append(closest)
        if f%135==0:print("SOURCE PATCH tested",f,"/628",flush=True)
    # Reserve at least one highest-value patch per frame, then fill globally
    # in descending salience order, taking no more than 8 from any frame.
    winners=[]; selected=set();counts=[0]*628
    for gain,f,t in sorted(proposals, key=lambda x:-x[0]):
        if counts[f]==0 and len(winners)<number:
            winners.append((f,t,gain));selected.add((f,t));counts[f]=1
    for gain,f,t in sorted(proposals,key=lambda x:-x[0]):
        if len(winners)>=number:break
        if (f,t) in selected or counts[f]>=8:continue
        winners.append((f,t,gain));selected.add((f,t));counts[f]+=1
    idx=initial_indices.copy()
    patch_by_frame=[[] for _ in range(628)]
    for f,t,g in winners:
        y=(t//12)*4;x=(t%12)*8
        tile=closest_tiles[f][y:y+4,x:x+8]
        idx[f,y:y+4,x:x+8]=tile
        packed=((tile[:,0::2]<<4)|tile[:,1::2]).tobytes()
        patch_by_frame[f].append((t,packed,g))
    offsets=[0];data=bytearray()
    for patches in patch_by_frame:
        for tile_id,packed,g in sorted(patches):
            data.extend(struct.pack("<H",tile_id));data.extend(packed)
        offsets.append(len(data))
    overlay=struct.pack("<"+"I"*len(offsets),*offsets)+data
    # Strict roundtrip decode of all 628 patched frames from original VQ.
    verify=initial_indices.copy()
    directory=struct.unpack_from("<"+"I"*629,overlay)
    for f in range(628):
        start,end=directory[f],directory[f+1]
        if (end-start)%18:raise AssertionError("misaligned overlay")
        for offset in range(start,end,18):
            base=4*629+offset
            tile_id=struct.unpack_from("<H",overlay,base)[0]
            pixels=np.frombuffer(overlay[base+2:base+18],dtype=np.uint8)
            decoded=np.empty((4,8),dtype=np.uint8)
            decoded[:,0::2]=(pixels>>4).reshape(4,4)
            decoded[:,1::2]=(pixels&15).reshape(4,4)
            y=(tile_id//12)*4;x=(tile_id%12)*8
            verify[f,y:y+4,x:x+8]=decoded
    if not np.array_equal(verify,idx):raise AssertionError("overlay roundtrip changed pixels")
    return idx,overlay,{"patch_count":len(winners),"frames_touched":sum(bool(c) for c in counts),
                        "overlay_bytes":len(overlay),"gain_sum":sum(x[2] for x in winners)}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--sources",type=Path,required=True)
    p.add_argument("--baseline",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--patches",type=int,default=1800)
    p.add_argument("--emit-test-pixels",action="store_true",
                   help="write 628x4608 expected pixels for independent C host test")
    args=p.parse_args()
    args.out.mkdir(parents=True,exist_ok=True)
    raw=args.baseline.read_bytes()
    if len(raw)!=279936:raise ValueError("invalid VQ baseline length")
    source=load_sources(args.sources)
    pal,initial=palettes_and_indices(raw)
    patched,overlay,details=make_patches(source,pal,initial,args.patches)
    rgb1=np.stack([pal[i//27][initial[i]] for i in range(628)])
    rgb2=np.stack([pal[i//27][patched[i]] for i in range(628)])
    mse_before=float(np.mean((source.astype(np.float32)-rgb1.astype(np.float32))**2))
    mse_after=float(np.mean((source.astype(np.float32)-rgb2.astype(np.float32))**2))
    psnr=lambda mse:10*math.log10(255*255/max(mse,1e-20))
    core,report=mhp2.pack(raw)
    size_with_overlay=len(core)+len(overlay)
    summary={"baseline_vq_sha256":hashlib.sha256(raw).hexdigest(),
             "sources":"8 files, verified SHA256", "base_mhp2_bytes":len(core),
             "patched_mhp2_plus_overlay_bytes":size_with_overlay,
             "resource_payload_cap":282592,"old_raw_vq_bytes":279936,
             "image_quality_before_psnr_db":round(psnr(mse_before),4),
             "image_quality_after_psnr_db":round(psnr(mse_after),4),
             "psnr_gain_db":round(psnr(mse_after)-psnr(mse_before),4),
             **details}
    (args.out/"trial_patches.mhp2_overlay").write_bytes(overlay)
    if args.emit_test_pixels:
        packed_indices=((patched[:,:,0::2]<<4)|patched[:,:,1::2]).astype(np.uint8)
        (args.out/"expected_patched_pixels.bin").write_bytes(packed_indices.tobytes())
    (args.out/"quality.json").write_text(json.dumps(summary,indent=2),encoding="utf-8")
    show=[0,85,145,245,326,558,600,627]
    w=96*3;h=96*3
    result=Image.new("RGB",(w*3+40,8*(h+25)+25),(22,22,22));draw=ImageDraw.Draw(result)
    for row,f in enumerate(show):
        y=25+row*(h+25)
        for col,(label,arr) in enumerate([("ORIGINAL",source[f]),("OLDER VQ",rgb1[f]),("MHP2 + ORIGINAL DETAIL PATCH",rgb2[f])]):
            x=10+col*(w+10)
            draw.text((x,y-17),f"{label} frame {f:03}",fill="white")
            image=Image.fromarray(arr.astype(np.uint8)).resize((w,h),Image.Resampling.NEAREST)
            result.paste(image,(x,y))
    result.save(args.out/"source_vs_vq_vs_patch.png")
    print("SOURCES PATCH EXPERIMENT",json.dumps(summary,indent=2),flush=True)

if __name__=="__main__":main()