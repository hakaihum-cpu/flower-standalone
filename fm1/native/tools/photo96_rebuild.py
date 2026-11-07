#!/usr/bin/env python3
"""Rebuild 628 FM-1 photo tiles from the eight approved user sheets.
96x96 fixed 16-color RGB565, 8x4 VQ, 256-entry codebook per 27 frames.
No generated imagery or facial reconstruction; original pixels are sampled.
Reproduce offline before firmware build; CircleCI uses the checked-in payload.
"""
import os,hashlib,json,time
os.environ['OPENBLAS_NUM_THREADS']='1'
os.environ['OMP_NUM_THREADS']='1'
from pathlib import Path
from PIL import Image
import numpy as np
from sklearn.cluster import MiniBatchKMeans
from photo96_sources import files,get,COLORS
from photo96_colorize import colorize as render
N_FRAMES=sum(e[3] for e in files)
SIDE=96
CLIP=27
CLIPS=(N_FRAMES+CLIP-1)//CLIP
COLS=SIDE//8
ROWS=SIDE//4
MAP_STRIDE=COLS*ROWS
ENTRIES=256
BYTES_PER_TILE=16
COLOR=np.array(COLORS,dtype=np.float32)
COLOR565=np.array([((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b in COLORS],dtype=np.uint16)
assert N_FRAMES==628 and CLIPS==24 and len(set(COLOR565.tolist()))==16
OUT=Path(os.environ.get("FM1_RESOURCE_OUTPUT", "fm1-photo96-build"))
OUT.mkdir(parents=True,exist_ok=True)

def mk_tiles(data):
    return data.reshape((-1,ROWS,4,COLS,8)).transpose((0,1,3,2,4)).reshape((-1,32))

def decode_tiles(tiles):
    return tiles.reshape((-1,ROWS,COLS,4,8)).transpose((0,1,3,2,4)).reshape((-1,SIDE,SIDE))

def compressed_clip(original,seed):
    indices=mk_tiles(original)
    features=COLOR[indices].reshape((-1,96))
    # K-means ensures 256 codewords reflect ALL source frames, not just top
    # frequent patterns; quantize the dictionary to the 16 user colors.
    model=MiniBatchKMeans(n_clusters=256,init='k-means++',n_init=1,
         max_iter=26,batch_size=1024,random_state=seed,
         max_no_improvement=10,reassignment_ratio=0.005)
    model.fit(features)
    centers=model.cluster_centers_.reshape((256,32,3))
    distances=((centers[:,:,None,:]-COLOR[None,None,:,:])**2).sum(axis=3)
    book=distances.argmin(axis=2).astype(np.uint8)
    rgbbook=COLOR[book].reshape((256,96))
    # Reassign to the ACTUAL quantized codebook, not float centers.
    ctr2=(rgbbook*rgbbook).sum(axis=1)
    labels=[]
    for part in np.array_split(features,8):
        dist=ctr2[None,:]-2.0*part@rgbbook.T
        labels.append(dist.argmin(axis=1).astype(np.uint8))
    labels=np.concatenate(labels)
    recovered=decode_tiles(book[labels])
    real=COLOR[original]
    restored=COLOR[recovered]
    mse=np.mean((real-restored)**2)
    psnr=10*np.log10(255*255/max(mse,1e-5))
    packed=((book[:,::2]<<4)|book[:,1::2]).astype(np.uint8)
    return packed.tobytes(),labels.tobytes(),psnr,recovered

if __name__=='__main__':
    begin=time.monotonic()
    frames=[]
    file_info=[]
    for bank,(filename,cols,rows,num) in enumerate(files):
        source=Path(os.environ.get("FM1_USER_SHEETS", "Resources/fm1_photo_96_inputs"))/filename
        file_info.append(dict(bank=bank,filename=filename,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),grid=f'{cols}x{rows}',frames=num))
        for i in range(num):
            pic=render(get(bank,i),bank)
            arr=np.frombuffer(pic.tobytes(),dtype=np.uint8).reshape((SIDE,SIDE))
            frames.append(arr.copy())
    frames=np.stack(frames,axis=0)
    print('source frames',frames.shape,'time',round(time.monotonic()-begin,1),flush=True)
    palettes=COLOR565.astype('<u2').tobytes()*CLIPS
    all_books=[];maps=[];psnrs=[];previews=[]
    choices=[0,40,80,81,145,245,326,407,488,558,627]
    for clip in range(CLIPS):
        low=clip*CLIP;high=min(N_FRAMES,low+CLIP)
        book,indices,psnr,rec=compressed_clip(frames[low:high],clip)
        assert len(book)==4096 and len(indices)==(high-low)*MAP_STRIDE
        all_books.append(book);maps.append(indices);psnrs.append(psnr)
        for ix in choices:
            if low<=ix<high:
                im=Image.fromarray(rec[ix-low],mode='P')
                im.putpalette([c for rgb in COLORS for c in rgb]);previews.append((ix,im))
        print(f'{clip+1}/{CLIPS} frames {low}..{high-1}, PSNR {psnr:.2f}dB, t {time.monotonic()-begin:.1f}s',flush=True)
    payload=palettes+b''.join(all_books)+b''.join(maps)
    assert len(payload)==279936,len(payload)
    assert len(payload)+32<=282624
    dst=OUT/'miyako_media_96_16c_v2.bin'
    dst.write_bytes(payload)
    verify=Image.new('RGB',(6*192,2*192),'#151515')
    for index,(frm,im) in enumerate(previews):
        verify.paste(im.convert('RGB').resize((192,192),Image.Resampling.NEAREST),((index%6)*192,(index//6)*192))
    verify.save(OUT/'miyako_96_vq_actual_preview_v2.png')
    sha=hashlib.sha256(payload).hexdigest()
    info=dict(frames=N_FRAMES,side=SIDE,colors=16,clip_frames=CLIP,clip_count=CLIPS,map_stride=MAP_STRIDE,
              palette_offset=0,book_offset=len(palettes),map_offset=len(palettes)+sum(map(len,all_books)),
              payload_bytes=len(payload),sha256=sha,psnr_min=round(min(psnrs),3),psnr_mean=round(float(np.mean(psnrs)),3),sources=file_info)
    (OUT/'miyako_96_manifest_v2.json').write_text(json.dumps(info,indent=2,ensure_ascii=False,default=float))
    print('COMPLETE',json.dumps({k:v for k,v in info.items() if k!='sources'},default=float),flush=True)
