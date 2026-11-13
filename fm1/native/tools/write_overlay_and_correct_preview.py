from pathlib import Path
import numpy as np,struct,json,hashlib,sys
from PIL import Image,ImageDraw
ROOT=Path('/mnt/data/fm1_codec_trial');OUT=ROOT/'original_only_16color_test'
sys.path.insert(0,str(ROOT))
from photo96_sources import get, files
from all628_trial import RGB
pal=np.load(OUT/'color_indices.npy');vq=np.load(OUT/'decoded_vq.npy');fixed=np.load(OUT/'corrected.npy')
core=(OUT/'source16_vq_mhp2.bin').read_bytes();assert len(core)==215925
assert vq.shape==fixed.shape==(628,96,96)
record=bytearray();offset=[0]
for f in range(628):
 for i in range(288):
  y=(i//12)*4;x=(i%12)*8
  tilefix=fixed[f,y:y+4,x:x+8]
  if np.array_equal(tilefix,vq[f,y:y+4,x:x+8]):continue
  tile=((tilefix[:,0::2]<<4)|tilefix[:,1::2]).astype(np.uint8).tobytes()
  record.extend(struct.pack('<H',i)+tile)
 offset.append(len(record))
overlay=struct.pack('<629I',*offset)+record
bundle=core+overlay
assert len(bundle)==282575,len(bundle)
assert len(bundle)<=282592
# Verify every recorded tile and every pixel against the generated candidate
check=vq.copy();base=629*4
for f in range(628):
 for at in range(offset[f],offset[f+1],18):
  i=struct.unpack_from('<H',overlay,base+at)[0]
  packed=np.frombuffer(overlay,dtype=np.uint8,count=16,offset=base+at+2).reshape(4,4)
  vals=np.empty((4,8),np.uint8);vals[:,0::2]=packed>>4;vals[:,1::2]=packed&15
  y=(i//12)*4;x=(i%12)*8;check[f,y:y+4,x:x+8]=vals
assert np.array_equal(check,fixed)
(OUT/'MHP2_16color_detail_ALL628_offline_not_flashable.bin').write_bytes(bundle)
report=json.loads((OUT/'capacity_report.json').read_text());report.update({'actual_encoded_mhp2_bytes':len(core),'actual_packed_overlay_bytes':len(overlay),'actual_final_bundle_bytes':len(bundle),'actual_binary_sha256':hashlib.sha256(bundle).hexdigest(),'verified_all628_overlay_pixels_exact':True,'physical_firmware_not_tested':True})
(OUT/'capacity_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('BUNDLE VERIFY',len(bundle),'bytes of 282592 cap, all 628 overlays exact',flush=True)
# Do not fake current master for all frames. Make source vs converted candidate for all representative frames.
start=[0]
for f in files:start.append(start[-1]+f[3])
frames=[0,4,40,81,145,245,326,407,488,558,600,627]
for pi,group in enumerate([frames[:6],frames[6:]],1):
 w=288;gap=12;margin=15;y0=50;rowh=315
 canvas=Image.new('RGB',(margin*2+w*3+gap*2,y0+rowh*len(group)),(15,16,19));d=ImageDraw.Draw(canvas)
 for col,label in enumerate(['ORIGINAL - NO REDRAW','SOURCE -> 16-C VQ','SOURCE -> 16-C MHP2 DETAIL']):
  d.text((margin+col*(w+gap),14),label,fill='white')
 for row,f in enumerate(group):
  bank=int(np.searchsorted(start,f,side='right')-1);a=f-start[bank]
  source=np.asarray(get(bank,a).convert('RGB'),np.uint8)
  items=[source,RGB[vq[f]],RGB[fixed[f]]]
  y=y0+row*rowh
  for col,im in enumerate(items):
   canvas.paste(Image.fromarray(im).resize((w,w),Image.Resampling.NEAREST),(margin+col*(w+gap),y))
  d.text((margin,y+w+5),f'frame {f:03d}  /  96 x 96  /  16 RGB565 colors per 27-frame clip',fill='white')
 path=OUT/f'SOURCE_VQ_MHP2_no_redraw_{pi}.png';canvas.save(path);print('PREVIEW',path)
# Two exact previously established master frames, extracted from previously published comparison PNG.
prev=Image.open(ROOT/'current_comparison/current_master_vs_source_patch_same_frames.png').convert('RGB')
# Coordinates were documented for same source 288x288 nearest-neighbour panels in old comparison.
master={0:prev.crop((332,75,620,363)),558:prev.crop((332,412,620,700))}
canvas=Image.new('RGB',(15*2+4*288+3*12,50+2*320),(15,16,19));d=ImageDraw.Draw(canvas)
for j,s in enumerate(['SOURCE FRAME','CURRENT MASTER (PRIOR EXPORT)','NEW 16-C VQ','NEW MHP2 DETAIL']):d.text((15+j*300,14),s,fill='white')
for k,f in enumerate([0,558]):
 bank=int(np.searchsorted(start,f,side='right')-1);orig=np.asarray(get(bank,f-start[bank]).convert('RGB'),np.uint8)
 imgs=[Image.fromarray(orig).resize((288,288),Image.Resampling.NEAREST),master[f],Image.fromarray(RGB[vq[f]]).resize((288,288),Image.Resampling.NEAREST),Image.fromarray(RGB[fixed[f]]).resize((288,288),Image.Resampling.NEAREST)]
 for j,im in enumerate(imgs):canvas.paste(im,(15+j*300,50+k*320))
 d.text((15,50+k*320+290),f'frame {f:03d}',fill='white')
p=OUT/'CURRENT_MASTER_vs_SOURCE_16C_MHP2_verified_frames.png';canvas.save(p);print('MASTER COMPARISON',p)