#!/usr/bin/env python3
"""Experimental source-based 16-colour SEL DETAIL across all 628 frames.
Does NOT install/modify firmware. Only generates a capacity-bounded research overlay.
Existing 16 RGB565 clip-specific palettes are kept byte-exact.
"""
import argparse,hashlib,json,math,struct,sys
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
import mhp2
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sources',type=Path,required=True,help='Eight privately supplied original contact sheets')
parser.add_argument('--baseline',type=Path,required=True,help='Exact approved 279936B VQ payload, not res.bin')
parser.add_argument('--out',type=Path,required=True,help='Offline experiment output directory')
parser.add_argument('--patches',type=int,default=2200)
parser.add_argument('--reserve',type=int,default=1700,help='Minimum remaining capacity in resource payload')
parser.add_argument('--emit-test-pixels',action='store_true')
args=parser.parse_args()
SRC=args.sources;OUT=args.out;OUT.mkdir(parents=True,exist_ok=True)
RAW=args.baseline
ROOT=Path(__file__).resolve().parents[3]
SHA='df517e2d8c7902bed076610a07876dee660890799ba5dec6bc12bf366783bcf4'
manifest=json.loads((ROOT/'Resources/fm1_photo_96_chunks/manifest.json').read_text())
source=[]
for bank,entry in enumerate(manifest['files']):
 name=entry['name'];cols,rows=entry['grid'];nframes=entry['frames']
 data=(SRC/name).read_bytes(); h=hashlib.sha256(data).hexdigest()
 assert h==entry['sha256'],(bank,name,h)
 from io import BytesIO
 with Image.open(BytesIO(data)) as original:
  original=original.convert('RGB')
  for i in range(nframes):
   x,y=i%cols,i//cols
   box=(round(x*original.width/cols)+2,round(y*original.height/rows)+2,
        round((x+1)*original.width/cols)-2,round((y+1)*original.height/rows)-2)
   frame=original.crop(box).resize((96,96),Image.Resampling.LANCZOS)
   source.append(np.asarray(frame,dtype=np.uint8))
source=np.stack(source);assert source.shape==(628,96,96,3)
raw=RAW.read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
base=mhp2.pack(raw)
if isinstance(base,tuple):base=base[0]
assert mhp2.unpack(base)==raw
palettebytes=np.frombuffer(raw,dtype='<u2',count=24*16).reshape(24,16)
palette=np.stack([((palettebytes>>11)*255//31),(((palettebytes>>5)&63)*255//63),((palettebytes&31)*255//31)],axis=-1).astype(np.uint8)
allidx=np.empty((628,96,96),dtype=np.uint8)
for f in range(628):
 c=f//27
 words=np.frombuffer(raw,dtype=np.uint8,count=4096,offset=768+c*4096).reshape(256,4,4)
 wordpx=np.empty((256,4,8),dtype=np.uint8)
 wordpx[:,:,::2]=words>>4;wordpx[:,:,1::2]=words&15
 ids=np.frombuffer(raw,dtype=np.uint8,count=288,offset=99072+f*288).reshape(24,12)
 allidx[f]=wordpx[ids].transpose(0,2,1,3).reshape(96,96)
assert np.max(allidx)<16
PSNR=lambda e:10*math.log10(65025/max(1e-10,float(e)))
proposals=[];frameBest={};candidates={};patch_gain_raw=[]
for f in range(628):
 c=f//27;pal=palette[c].astype(np.int32);src=source[f].astype(np.int32)
 current=pal[allidx[f]].astype(np.int32)
 dist=((src.reshape(-1,3)[:,None,:]-pal[None,:,:])**2).sum(axis=-1)
 nearest=dist.argmin(axis=1).astype(np.uint8).reshape(96,96)
 best=pal[nearest]
 gray=(77*src[:,:,0]+150*src[:,:,1]+29*src[:,:,2])>>8
 edge=np.zeros((96,96),dtype=np.int32)
 edge[:,1:]+=abs(gray[:,1:]-gray[:,:-1]);edge[1:,:]+=abs(gray[1:,:]-gray[:-1,:])
 weight=1.+np.clip(edge,0,100)/50.
 err0=((src-current)**2).sum(axis=-1)
 err1=((src-best)**2).sum(axis=-1)
 gain=((err0-err1)*weight).reshape(24,4,12,8).sum(axis=(1,3)).ravel()
 gain_rgb=(err0-err1).reshape(24,4,12,8).sum(axis=(1,3)).ravel()
 p=[(float(gain[t]),f,int(t)) for t in np.flatnonzero((gain>0)&(gain_rgb>0))]
 p.sort(reverse=True)
 frameBest[f]=p[:8]
 for g,_,tileid in frameBest[f]:
  y=tileid//12*4;x=tileid%12*8
  packed=((nearest[y:y+4,x:x+8:2]<<4)|nearest[y:y+4,x+1:x+8:2]).astype(np.uint8).tobytes()
  assert len(packed)==16
  candidates[f,tileid]=packed
 proposals.extend(p)
 if f%100==0:print('evaluated',f,'/628',flush=True)
# Room in res.bin: resource cap = 282592 B, candidate MHP2 core = 238535B.
# 629 x uint32 directories, 18 bytes per 8x4 patch; reserve at least 1700B for extensions.
CAP=282592;BUDGET_PATCHES=args.patches
if BUDGET_PATCHES<628 or len(base)+629*4+BUDGET_PATCHES*18>CAP-args.reserve:
 raise ValueError('Requested repair tile count exceeds protected FM-1 res.bin payload; no output written')
selected={};by_frame=[[] for _ in range(628)]
def add(f,t,g):
 if (f,t) in selected or len(by_frame[f])>=8:return False
 selected[f,t]=g;by_frame[f].append((t,g));return True
# Preserve the previous user-selected DETAIL samples: top eight tiles of frames 0, 558.
for f in (0,558):
 for g,_,t in frameBest[f]:add(f,t,g)
# Every other frame receives one best positive source-derived repair, if available.
for f in range(628):
 if not by_frame[f] and frameBest[f]:
  g,_,t=frameBest[f][0];add(f,t,g)
# Spend the remainder on the greatest edge-weighted source fidelity improvements.
for g,f,t in sorted(proposals,key=lambda z:(-z[0],z[1],z[2])):
 if len(selected)>=BUDGET_PATCHES:break
 add(f,t,g)
assert len(selected)<=BUDGET_PATCHES and all(len(t)<=8 for t in by_frame)
frameOffsets=[0];rec=bytearray();patched=allidx.copy()
for f in range(628):
 for tile,g in sorted(by_frame[f]):
  packed=candidates[f,tile];rec.extend(struct.pack('<H',tile));rec.extend(packed)
  x=tile%12*8;y=tile//12*4;v=np.frombuffer(packed,dtype=np.uint8).reshape(4,4)
  patched[f,y:y+4,x:x+8:2]=v>>4
  patched[f,y:y+4,x+1:x+8:2]=v&15
 frameOffsets.append(len(rec))
overlay=struct.pack('<629I',*frameOffsets)+rec
assert len(overlay)==629*4+len(selected)*18
# Independently replay all 628 frame patches from the actual binary overlay.
readback=allidx.copy()
directory=struct.unpack_from('<629I',overlay,0)
assert directory[0]==0 and directory[-1]==len(rec)
for f in list(range(628))[::-1]:
 a=directory[f];b=directory[f+1]
 assert 0<=a<=b<=len(rec) and (b-a)%18==0
 for at in range(a,b,18):
  tile=struct.unpack_from('<H',overlay,629*4+at)[0];assert tile<288
  vals=np.frombuffer(overlay[629*4+at+2:629*4+at+18],dtype=np.uint8).reshape(4,4)
  x=tile%12*8;y=tile//12*4
  readback[f,y:y+4,x:x+8:2]=vals>>4
  readback[f,y:y+4,x+1:x+8:2]=vals&15
assert np.array_equal(patched,readback)
oldrgb=palette[(np.arange(628)//27)[:,None,None],allidx]
newrgb=palette[(np.arange(628)//27)[:,None,None],patched]
# source is RGB888 and pal is quantised RGB565, compare RGB colour fidelity at same 96x96.
mse0=((source.astype(np.float32)-oldrgb.astype(np.float32))**2).mean(axis=(1,2,3))
mse1=((source.astype(np.float32)-newrgb.astype(np.float32))**2).mean(axis=(1,2,3))
assert np.all(mse1<=mse0+1e-5)
quality={'original_frames':628,'originals_sha256_verified':8,'current_master_vq_sha256':SHA,
 'palette':'same 24 original-source RGB565 16-colour palettes as approved master; no recolouring',
 'old_raw_vq_bytes':len(raw),'new_mhp2_base_bytes':len(base),'overlay_directory_bytes':629*4,
 'overlay_records':len(selected),'overlay_record_bytes':len(rec),'overlay_total_bytes':len(overlay),
 'total_candidate_resource_payload_bytes':len(base)+len(overlay),'max_resource_payload_bytes':CAP,
 'remaining_capacity_bytes':CAP-len(base)-len(overlay),
 'frames_with_patches':sum(bool(x) for x in by_frame),'frames_without_patches':sum(not x for x in by_frame),
 'patches_per_frame_min':min(map(len,by_frame)),'patches_per_frame_max':max(map(len,by_frame)),
 'all_628_RGB_psnr_current_dB':round(PSNR(mse0.mean()),3),'all_628_RGB_psnr_MHP2_DETAIL_dB':round(PSNR(mse1.mean()),3),
 'average_per_frame_PSNR_gain_dB':round(float(np.mean([PSNR(mse1[f])-PSNR(mse0[f]) for f in range(628)])),3),
 'min_per_frame_PSNR_gain_dB':round(min(PSNR(mse1[f])-PSNR(mse0[f]) for f in range(628)),3),
 'max_per_frame_PSNR_gain_dB':round(max(PSNR(mse1[f])-PSNR(mse0[f]) for f in range(628)),3),
 'per_bank':[],'select_frames':{}}
for bank,st,en in zip(range(8),[0,81,145,245,326,407,488,558],[81,145,245,326,407,488,558,628]):
 quality['per_bank'].append({'bank':bank,'frames':en-st,'patches':sum(len(x) for x in by_frame[st:en]),'psnr_current':round(PSNR(mse0[st:en].mean()),3),'psnr_detail':round(PSNR(mse1[st:en].mean()),3)})
show=[0,40,81,145,245,326,407,488,558,600,627]
for f in show:quality['select_frames'][str(f)]={'patch_count':len(by_frame[f]),'PSNR_current':round(PSNR(mse0[f]),2),'PSNR_MHP2_DETAIL':round(PSNR(mse1[f]),2)}
assert quality['total_candidate_resource_payload_bytes']<=CAP-args.reserve
(OUT/'current_sel_all628_MHP2_DETAIL_overlay.bin').write_bytes(overlay)
(OUT/'current_sel_all628_MHP2_base.bin').write_bytes(base)
if args.emit_test_pixels:
 packed=((patched[:,:,0::2]<<4)|patched[:,:,1::2]).astype(np.uint8)
 (OUT/'expected_patched_pixels.bin').write_bytes(packed.tobytes())
(OUT/'quality_and_capacity.json').write_text(json.dumps(quality,ensure_ascii=False,indent=2))
# Sample image: native 96px frames enlarged via nearest neighbor only; no regeneration.
fontpath='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
font=ImageFont.truetype(fontpath,16)
fontb=ImageFont.truetype(fontpath,20)
for page,(start,end) in enumerate([(0,5),(5,len(show))],1):
 subset=show[start:end];scale=3;side=96*scale;pad=16;pitch=side+40;top=82
 sheet=Image.new('RGB',(3*(side+pad)+pad,top+len(subset)*pitch+pad),(18,18,20));d=ImageDraw.Draw(sheet)
 d.text((pad,10),'SEL 96x96 / actual master / all-628 MHP2 DETAIL',font=fontb,fill='white')
 for i,h in enumerate(('ORIGINAL SOURCE','CURRENT MASTER','MHP2 DETAIL')):
  d.text((pad+i*(side+pad),48),h,font=font,fill='white')
 for row,f in enumerate(subset):
  y=top+row*pitch
  for i,im in enumerate((source[f],oldrgb[f],newrgb[f])):
   x=pad+i*(side+pad)
   sheet.paste(Image.fromarray(im).resize((side,side),Image.Resampling.NEAREST),(x,y))
   d.text((x,y+side+4),f'FRAME {f:03d} / {len(by_frame[f])} patches',font=font,fill=(215,215,215))
 path=OUT/f'MIYAKO_SEL_MASTER_vs_MHP2_DETAIL_all628_part{page}.png';sheet.save(path)
 print('SAMPLE',path,flush=True)
print(json.dumps(quality,ensure_ascii=False,indent=2),flush=True)