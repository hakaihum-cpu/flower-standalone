#!/usr/bin/env python3
"""Offline rebuild of stock 279936-byte FM-1 RGB565 8x4/256 VQ resource.
Never changes frame count, compressed layout, flash format, or user source.
"""
import os
os.environ['OPENBLAS_NUM_THREADS']='2'
os.environ['OMP_NUM_THREADS']='2'
import numpy as np, math, time, hashlib, json, sys, base64, struct
from PIL import Image
from pathlib import Path
from sklearn.cluster import MiniBatchKMeans
from photo96_sources import files, get
ROOT=Path(os.environ.get('FM1_USER_SHEETS','Resources/fm1_photo_96_inputs'))
OUT=Path(os.environ.get('FM1_RESOURCE_OUTPUT','fm1-photo96-build'))
OUT.mkdir(parents=True, exist_ok=True)
REPO=Path(__file__).resolve().parents[3]
MANIFEST=json.loads(Path(os.environ.get('FM1_MANIFEST', str(REPO/'Resources/fm1_photo_96_chunks/manifest.json'))).read_text())
EXPECTED_SHA256='df517e2d8c7902bed076610a07876dee660890799ba5dec6bc12bf366783bcf4'
N=628; SIDE=96; CLIP=27; CLIPS=24; BOOK=256; TILES=288
first=np.array([0,81,145,245,326,407,488,558,628]); assert sum(x[3] for x in files)==N
src=[];sha=[]
for bank,(name,cols,rows,num) in enumerate(files):
 data=(ROOT/name).read_bytes();digest=hashlib.sha256(data).hexdigest();sha.append((name,digest))
 assert digest==MANIFEST['files'][bank]['sha256'], 'source SHA differs: '+name
 for i in range(num):src.append(np.asarray(get(bank,i).convert('RGB'),dtype=np.uint8))
source=np.stack(src)
assert source.shape==(N,96,96,3)
print('SOURCE',source.shape,'actual image archive sha',sha,flush=True)

def palette_from_images(images,method):
 n=len(images)
 collage=Image.new('RGB',(SIDE*n,SIDE))
 for i,im in enumerate(images):collage.paste(Image.fromarray(im),(i*SIDE,0))
 q=collage.quantize(colors=16,method=method,dither=Image.Dither.NONE)
 raw=q.getpalette();seen=q.getcolors() or []
 count=max(1,min(16,max((i for _,i in seen),default=0)+1))
 colors=np.array([raw[i*3:i*3+3] for i in range(count)],np.uint8)
 if len(colors)<16:colors=np.concatenate([colors,np.repeat(colors[-1:],16-len(colors),axis=0)])
 rgb565=((colors[:,0].astype(np.uint16)>>3)<<11)|((colors[:,1].astype(np.uint16)>>2)<<5)|(colors[:,2].astype(np.uint16)>>3)
 pal=np.stack([((rgb565>>11)*255//31),(((rgb565>>5)&63)*255//63),((rgb565&31)*255//31)],axis=1).astype(np.uint8)
 pim=Image.new('P',(1,1));pim.putpalette(pal.ravel().tolist())
 # All n frames get the same 16-colour fixed palette, no temporal per-frame change.
 idx=np.stack([np.asarray(Image.fromarray(im).quantize(palette=pim,dither=Image.Dither.NONE),np.uint8) for im in images])
 assert idx.max()<16
 return rgb565,pal,idx

def tiles(frames):return frames.reshape(-1,24,4,12,8).transpose(0,1,3,2,4).reshape(-1,32)
def untile(codewords,n):return codewords.reshape(n,24,12,4,8).transpose(0,1,3,2,4).reshape(n,96,96)

def train(original,pal,indices,seed):
 n=original.shape[0]
 X=tiles(indices)
 feature=pal[X].reshape(-1,96).astype(np.float32)
 model=MiniBatchKMeans(n_clusters=256,init='k-means++',n_init=1,max_iter=35,batch_size=1536,random_state=seed,max_no_improvement=12,reassignment_ratio=0.005)
 model.fit(feature)
 centers=model.cluster_centers_.reshape(256,32,3)
 diff=centers[:,:,None,:]-pal[None,None,:,:].astype(np.float32)
 cb=np.argmin(np.sum(diff*diff,axis=3),axis=2).astype(np.uint8)
 cent=pal[cb].reshape(256,96).astype(np.float32)
 cnorm=(cent*cent).sum(axis=1)
 ids=[]
 for chunk in np.array_split(feature,6):
  dist=cnorm[None,:]-2*chunk@cent.T
  ids.append(np.argmin(dist,axis=1).astype(np.uint8))
 ids=np.concatenate(ids)
 output=pal[untile(cb[ids],n)].astype(np.int16)
 mse=np.mean((original.astype(np.int16)-output)**2,axis=(1,2,3)).astype(np.float64)
 return cb,ids,mse

allpal=[];allbook=[];allmaps=[];summary=[]
start=time.monotonic()
for clip in range(int(os.environ.get("FROM_CLIP", "0")), int(os.environ.get("TO_CLIP", "24"))):
 lo=clip*CLIP;hi=min(N,lo+CLIP);im=source[lo:hi];nc=hi-lo
 quality=None;chosen=None
 for method in (Image.Quantize.MEDIANCUT,Image.Quantize.MAXCOVERAGE,Image.Quantize.FASTOCTREE):
  palette565,pal,indices=palette_from_images(im,method)
  for candidate in range(3):
   cb,ids,mse=train(im,pal,indices,clip*7919+candidate*139+int(method))
   # Prefer minimizing mean error with a small worst-frame penalty, preserve
   # emotional details in hard closeups and coloured sunset transitions.
   score=(float(mse.mean()),float(mse.max()))
   if quality is None or (score[0]+0.10*score[1] < quality[0]+0.10*quality[1]):
    quality=score;chosen=(palette565,cb,ids,mse,method,candidate)
 p,cb,ids,mses,method,candidate=chosen
 book=((cb[:,0::2]<<4)|cb[:,1::2]).tobytes()
 assert len(book)==4096 and len(ids)==nc*288
 allpal.append(p.astype('<u2').tobytes());allbook.append(book);allmaps.append(ids.tobytes())
 clipdir=OUT/"newclip";clipdir.mkdir(exist_ok=True)
 (clipdir/f"pal_{clip:02}.bin").write_bytes(allpal[-1])
 (clipdir/f"book_{clip:02}.bin").write_bytes(allbook[-1])
 (clipdir/f"map_{clip:02}.bin").write_bytes(allmaps[-1])
 quality_new=float(mses.mean())
 info=dict(clip=clip,start=lo,end=hi-1,frames=nc,new_psnr=round(10*math.log10(65025/max(1e-9,quality_new)),2),min_psnr=round(10*math.log10(65025/max(1e-9,float(mses.max()))),2),palette_method=str(method),seed=candidate,elapsed=round(time.monotonic()-start,1))
 print('CLIP',info,flush=True)
 summary.append(info)
 (clipdir/f"quality_{clip:02}.json").write_text(json.dumps(info))
 if clip in (0,3,7,13,20,23):
  pal=np.stack([((p>>11)*255//31),(((p>>5)&63)*255//63),((p&31)*255//31)],axis=1).astype(np.uint8)
  q=pal[untile(cb[ids],nc)]
  side=Image.new('RGB',(96*2,96*3))
  for j,f in enumerate((0,nc//2,nc-1)):
   side.paste(Image.fromarray(im[f]),(0,j*96));side.paste(Image.fromarray(q[f]),(96,j*96))
  side.resize((576,864),Image.Resampling.NEAREST).save(OUT/f'compare_vq_clip_{clip:02}.png')

if int(os.environ.get("TO_CLIP", "24")) < 24:
 print('PARTIAL',os.environ.get('FROM_CLIP'),os.environ.get('TO_CLIP'),flush=True)
 sys.exit(0)
allpal=[(OUT/'newclip'/f'pal_{i:02}.bin').read_bytes() for i in range(24)]
allbook=[(OUT/'newclip'/f'book_{i:02}.bin').read_bytes() for i in range(24)]
allmaps=[(OUT/'newclip'/f'map_{i:02}.bin').read_bytes() for i in range(24)]
summary=[json.loads((OUT/'newclip'/f'quality_{i:02}.json').read_text()) for i in range(24)]
payload=b''.join(allpal)+b''.join(allbook)+b''.join(allmaps)
assert len(allpal)==CLIPS and len(allbook)==CLIPS and len(payload)==279936,(len(payload),len(allpal),len(allbook))
assert len(payload)<=69*4096-32
sha256=hashlib.sha256(payload).hexdigest()
assert sha256 == EXPECTED_SHA256, 'different encoder output: examine dependency version and original image SHA'
outfile=OUT/'miyako_media_96_16c_quality.bin';outfile.write_bytes(payload)
encoded=base64.b64encode(payload).decode('ascii')
outdir=OUT/'newparts';outdir.mkdir(exist_ok=True)
# Exactly the archived 12-part layout, 32768 base64 chars each; final is shorter.
parts=[encoded[i:i+32768] for i in range(0,len(encoded),32768)];assert len(parts)==12
for i,p in enumerate(parts): (outdir/f'part_{i+1:04}.b64').write_text(p+'\n')
report=dict(sha256=sha256,source_shas=sha,frames=N,format='RGB565 16 colours, 8x4 256 tile VQ, 27-frame clips',bytes=len(payload),resource_with_crc_bytes=len(payload)+32,clip_count=CLIPS,clip_quality=summary,total_time=round(time.monotonic()-start,1))
(OUT/'quality_report.json').write_text(json.dumps(report,indent=2,ensure_ascii=False))
print('COMPLETE',json.dumps({k:v for k,v in report.items() if k not in ('clip_quality','source_shas')}),flush=True)