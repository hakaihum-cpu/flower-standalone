"""Real original user frames -> region-sensitive 16-color palette -> VQ256 8x4 / 24 clips.
No generated replacements or structure changes; evaluation only, not installable firmware.
"""
import os, sys, time, json, hashlib, struct, heapq
os.environ['OPENBLAS_NUM_THREADS']='2'
os.environ['OMP_NUM_THREADS']='2'
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from sklearn.cluster import MiniBatchKMeans
ROOT=Path('/mnt/data/fm1_codec_trial');OUT=ROOT/'original_only_16color_test';OUT.mkdir(exist_ok=True)
sys.path.insert(0,str(ROOT))
from photo96_sources import get,files
from real_palette_preview import colorize,PALETTE
CAP=69*4096-32
p=PALETTE.astype(np.uint16)
colors565=((p[:,0]>>3)<<11)|((p[:,1]>>2)<<5)|(p[:,2]>>3)
RGB=np.stack([((colors565>>11)*255//31),(((colors565>>5)&63)*255//63),((colors565&31)*255//31)],axis=-1).astype(np.uint8)

def tile(a):
 n=len(a);return a.reshape(n,24,4,12,8).transpose(0,1,3,2,4).reshape(n*288,32)
def untile(a,n):return a.reshape(n,24,12,4,8).transpose(0,1,3,2,4).reshape(n,96,96)

def train(clip,frames):
 # Train VQ words in display-RGB space from exact source geometry converted to 16 colors.
 x=tile(frames);rgb=RGB[x].reshape(len(x),96).astype(np.float32)
 rg=np.random.default_rng(10043+clip)
 init=rgb[rg.choice(len(x),256,replace=False)]
 km=MiniBatchKMeans(n_clusters=256,init=init,n_init=1,max_iter=10,batch_size=2048,
                    max_no_improvement=6,reassignment_ratio=0.004,random_state=clip+2026)
 km.fit(rgb)
 cent=km.cluster_centers_.reshape(256,32,3)
 def snap(a):return np.square(a[:,:,None,:]-RGB.astype(np.float32)[None,None,:,:]).sum(axis=3).argmin(axis=2).astype(np.uint8)
 cb=snap(cent)
 for it in range(3):
  centers=RGB[cb].reshape(256,96).astype(np.float32)
  distances=(centers*centers).sum(axis=1)[None,:]-2*(rgb@centers.T)
  ids=distances.argmin(axis=1).astype(np.uint8)
  if it==2:break
  cnt=np.bincount(ids,minlength=256)
  sums=np.zeros((256,96),np.float32)
  np.add.at(sums,ids,rgb)
  avg=np.divide(sums,cnt[:,None],out=np.zeros_like(sums),where=cnt[:,None]>0).reshape(256,32,3)
  new=snap(avg)
  cb[cnt>0]=new[cnt>0]
 recon=untile(cb[ids],len(frames))
 book=((cb[:,0::2]<<4)|cb[:,1::2]).astype(np.uint8).tobytes()
 maps=ids.tobytes()
 return book,maps,recon

# MHP2 mode sizes use canonical Huffman with independent per-frame byte alignment.
def lengths(data):
 f=np.bincount(np.frombuffer(data,np.uint8),minlength=256);h=[]
 for i,v in enumerate(f):
  if v:h.append((int(v),i,i))
 heapq.heapify(h);out=[0]*256
 if len(h)==1:out[h[0][1]]=1
 else:
  while len(h)>1:
   a=heapq.heappop(h);b=heapq.heappop(h)
   heapq.heappush(h,(a[0]+b[0],min(a[1],b[1]),(a[2],b[2])))
  def visit(n,depth):
   if isinstance(n,int):out[n]=depth
   else:visit(n[0],depth+1);visit(n[1],depth+1)
  visit(h[0][2],0)
 if max(out)>31:return [8]*256
 return out

def hsize(byte_data,ln):return (sum(ln[x] for x in byte_data)+7)//8

def total_size(books, maps):
 bsum=0;msum=0;mode_count=[0,0];per=[]
 for c in range(24):
  lb=lengths(books[c]);bs=hsize(books[c],lb);b_sum=bs
  st=c*27;end=min((c+1)*27,628);group=maps[st:end]
  plain=b''.join(group)
  lp=lengths(plain)
  plain_size=sum(hsize(fr,lp) for fr in group)
  trimmed=[bytes(x for i,x in enumerate(fr) if i<12 or x!=fr[i-12]) for fr in group]
  lu=lengths(b''.join(trimmed))
  up_size=sum((sum(lu[x] for x in arr)+(288-12)+7)//8 for arr in trimmed)
  mode=int(up_size<plain_size);mode_count[mode]+=1
  ms=up_size if mode else plain_size
  bsum+=bs;msum+=ms;per.append((c,bs,ms,mode))
 overhead=64+768+24*2*160+24+25*4+629*4
 return overhead+bsum+msum,{'header_tables_and_indices':overhead,'dictionaries':bsum,'maps':msum,'modes':mode_count,'per_clip':per}

def main():
 t=time.time();man=json.loads((ROOT/'manifest.json').read_text())
 src=[];goal=[]
 for bank,(name,_,_,n) in enumerate(files):
  digest=hashlib.sha256((ROOT/'original_sources'/name).read_bytes()).hexdigest()
  assert digest==man['files'][bank]['sha256'],f'SOURCE HASH MISMATCH: {name}'
  for i in range(n):
   im=np.asarray(get(bank,i).convert('RGB'),np.uint8)
   colored,_=colorize(im,bank)
   src.append(im);goal.append(colored)
  print(f'BANK {bank} verified {n} original frames',flush=True)
 src=np.stack(src);goal=np.stack(goal)
 assert goal.shape==(628,96,96) and goal.max()<16
 # All clips have one unchanging 16-entry RGB565 palette each.
 books=[];maps=[];pieces=[]
 for c in range(24):
  st=c*27;end=min((c+1)*27,628)
  book,mapping,recon=train(c,goal[st:end]);books.append(book)
  maps.extend([mapping[i*288:(i+1)*288] for i in range(end-st)])
  pieces.append(recon)
  print('CLIP',c,'of 24',round(time.time()-t,1),'s',flush=True)
 restored=np.concatenate(pieces)
 assert restored.shape==(628,96,96)
 core,parts=total_size(books,maps)
 print('MHP2 CORE',core,'bytes; correction room',CAP-core,flush=True)
 if core+629*4>CAP:
  raise RuntimeError(f'COLOR 16 VQ core {core} leaves no overlay room; NO GO')
 # Optimized overlay: actual changed 8x4 tiles, no resampling, no drawing.
 cost=(RGB[goal].astype(np.int16)-RGB[restored].astype(np.int16)).astype(np.int32)**2
 gains=cost.sum(axis=3).reshape(628,24,4,12,8).sum(axis=(2,4)).reshape(628,288)
 by_frame=[[] for _ in range(628)];candidates=[]
 for f in range(628):
  for i in np.argsort(-gains[f])[:8]:
   if gains[f,i]>0:candidates.append((int(gains[f,i]),f,int(i)))
 candidates.sort(reverse=True)
 allowed=min(8*628,max(0,(CAP-core-629*4)//18));selected=set()
 # One correction per frame first to avoid over-allocation to one clip.
 best=[None]*628
 for gain,f,tileid in candidates:
  if best[f] is None:best[f]=(f,tileid)
 for pair in best:
  if pair and len(selected)<allowed:selected.add(pair)
 for gain,f,tileid in candidates:
  if len(selected)>=allowed:break
  if sum(1 for ff,_ in selected if ff==f)>=8:continue
  selected.add((f,tileid))
 for f,tileid in sorted(selected):by_frame[f].append(tileid)
 corrected=restored.copy()
 for f,tiles in enumerate(by_frame):
  for u in tiles:
   y=(u//12)*4;x=(u%12)*8
   corrected[f,y:y+4,x:x+8]=goal[f,y:y+4,x:x+8]
 overlay=629*4+18*len(selected)
 final=core+overlay
 if final>CAP:raise RuntimeError('CAPACITY FAIL')
 # Prove all corrections are in nearest-source colour geometry, not artistic redraw.
 mse_before=np.square(RGB[restored].astype(np.float32)-RGB[goal].astype(np.float32)).mean()
 mse_after=np.square(RGB[corrected].astype(np.float32)-RGB[goal].astype(np.float32)).mean()
 assert mse_after<=mse_before
 report={'sha256_source_verified':True,'original_source_frames':628,'source_geometry':'96x96 unchanged',
         'fixed_palette':'16 RGB565 entries per clip; 24 clips','mhp2_core_size':core,
         'core_breakdown':parts,'patch_count':len(selected),'patched_frames':sum(bool(v) for v in by_frame),
         'patch_bytes':overlay,'total_bytes':final,'cap_bytes':CAP,'remaining_bytes':CAP-final,
         'mse_against_color_target_before_patch':float(mse_before),
         'mse_against_color_target_after_patch':float(mse_after),
         'n_source_pixels_retouched_or_redrawn':0,'time_seconds':round(time.time()-t,1),
         'host_mode_only':True,'actual_firmware_tested':False}
 (OUT/'capacity_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 # Store a reproducible exact raw legacy-layout VQ for offline analysis.
 raw=bytes(np.tile(colors565.astype('<u2').tobytes(),24)) +b''.join(books)+b''.join(maps)
 assert len(raw)==279936
 (OUT/'color_target_vq_legacy_layout.bin').write_bytes(raw)
 for k,arr in [('color_indices',goal),('decoded_vq',restored),('corrected',corrected)]:np.save(OUT/f'{k}.npy',arr)
 # Two compact comparison sheets with exact source, current master, before and after patch.
 oldraw=b''.join(__import__('base64').b64decode(''.join((ROOT/f'part_{j:04}.b64').read_text().split())) for j in range(1,13))
 assert hashlib.sha256(oldraw).hexdigest()==man['payload_sha256']
 current=np.zeros((628,96,96,3),np.uint8)
 for c in range(24):
  palette=np.frombuffer(oldraw,dtype='<u2',count=16,offset=c*32)
  rgb=np.stack([((palette>>11)*255//31),(((palette>>5)&63)*255//63),((palette&31)*255//31)],axis=-1).astype(np.uint8)
  book=np.frombuffer(oldraw,dtype=np.uint8,count=4096,offset=768+c*4096).reshape(256,16)
  code=np.zeros((256,32),np.uint8);code[:,0::2]=book>>4;code[:,1::2]=book&15
  for f in range(c*27,min((c+1)*27,628)):
   ids=np.frombuffer(oldraw,dtype=np.uint8,count=288,offset=99072+f*288)
   current[f]=rgb[untile(code[ids],1)[0]]
 frame_ids=[0,4,40,81,145,245,326,407,488,558,600,627]
 for part,indices in enumerate([frame_ids[:6],frame_ids[6:]]):
  w=288;gap=14;startx=16;header=54;step=w+30
  canvas=Image.new('RGB',(2*startx+4*w+3*gap,header+step*len(indices)),(12,14,18))
  d=ImageDraw.Draw(canvas)
  heads=['ORIGINAL','PREVIOUS VQ (NOT CURRENT MASTER)','16-C SOURCE RECOLOR VQ','16-C MHP2 DETAIL']
  for j,h in enumerate(heads):d.text((startx+j*(w+gap),15),h,fill=(245,245,245))
  for i,f in enumerate(indices):
   y=header+i*step
   for j,img in enumerate([src[f],current[f],RGB[restored[f]],RGB[corrected[f]]]):
    canvas.paste(Image.fromarray(img).resize((w,w),Image.Resampling.NEAREST),(startx+j*(w+gap),y))
   d.text((startx,y+w+4),f'Frame {f} / {len(by_frame[f])} source-derived tiles',fill='white')
  name=OUT/f'FM1_MHP2_REAL_SOURCE_16COLOR_CAPACITY_OK_{part+1}.png';canvas.save(name)
  print('IMAGE',name,flush=True)
 print('RESULT',json.dumps(report,ensure_ascii=False),flush=True)

if __name__=='__main__':main()