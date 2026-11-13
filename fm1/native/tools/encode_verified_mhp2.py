"""Experimental MHP2 compatible lossless pack / unpack and source-tile overlay.
Produces inspection-only .mhp2 + overlays. NOT flashable res.bin.
"""
import sys,struct,heapq,json,zlib
from pathlib import Path
import numpy as np
HERE=Path('/mnt/data/fm1_codec_trial/original_only_16color_test')
FRAME=628;CLIP=24;CAP=282592

def lengths(data):
 freq=np.bincount(np.frombuffer(data,np.uint8),minlength=256)
 h=[(int(v),i,i) for i,v in enumerate(freq) if v]
 heapq.heapify(h);out=[0]*256
 if len(h)==1:out[h[0][1]]=1
 else:
  while len(h)>1:
   a=heapq.heappop(h);b=heapq.heappop(h)
   heapq.heappush(h,(a[0]+b[0],min(a[1],b[1]),(a[2],b[2])))
  def visit(node,dep):
   if isinstance(node,int):out[node]=dep
   else:visit(node[0],dep+1);visit(node[1],dep+1)
  visit(h[0][2],0)
 if max(out)>31:return [8]*256
 return out

def canon(lens):
 active=sorted((n,i) for i,n in enumerate(lens) if n)
 code=0;last=0;enc=[(0,0)]*256;dec={}
 for n,i in active:
  code<<=n-last
  if code>=1<<n:raise ValueError('oversubscribed codes')
  enc[i]=(code,n);dec[(n,code)]=i;code+=1;last=n
 return enc,dec,max(lens)

class BitW:
 def __init__(self):self.data=bytearray();self.bits=0;self.pending=0
 def put(self,x,n):
  for i in range(n-1,-1,-1):
   self.pending=(self.pending<<1)|((x>>i)&1);self.bits+=1
   if self.bits==8:self.data.append(self.pending);self.pending=0;self.bits=0
 def done(self):
  if self.bits:self.data.append(self.pending<<(8-self.bits));self.bits=0;self.pending=0
  return bytes(self.data)

class BitR:
 def __init__(self,data):self.data=data;self.at=0
 def bit(self):
  if self.at>=len(self.data)*8:raise ValueError('bitstream ended early')
  i=self.at;self.at+=1
  return self.data[i>>3]>>(7-i%8)&1
 def sym(self,table,maxlen):
  v=0
  for n in range(1,maxlen+1):
   v=(v<<1)|self.bit()
   k=table.get((n,v))
   if k is not None:return k
  raise ValueError('invalid symbol')

def encode(data,enc):
 out=BitW()
 for s in data:
  c,n=enc[s];out.put(c,n)
 return out.done()

def length_bytes(ln):
 w=BitW()
 for a in ln:w.put(a,5)
 result=w.done();assert len(result)==160
 return result

def de_lengths(data):
 r=BitR(data);return [sum(r.bit()<<j for j in range(4,-1,-1)) for _ in range(256)]

def encode_frame(frame,enc,mode):
 w=BitW()
 for i,s in enumerate(frame):
  if mode and i>=12:
   same=s==frame[i-12];w.put(int(same),1)
   if same:continue
  c,n=enc[s];w.put(c,n)
 return w.done()

def decode_frame(data,dec,maxlen,mode):
 r=BitR(data);frame=bytearray(288)
 for i in range(288):
  if mode and i>=12 and r.bit():frame[i]=frame[i-12]
  else:frame[i]=r.sym(dec,maxlen)
 if (r.at+7)//8!=len(data):raise ValueError('extra frame bytes')
 return bytes(frame)

def pack(raw):
 assert len(raw)==279936
 pal=raw[:768];books=[raw[768+i*4096:768+(i+1)*4096] for i in range(24)]
 maps=[raw[99072+i*288:99072+(i+1)*288] for i in range(628)]
 tables=bytearray();modes=bytearray();bookdata=bytearray();mapdata=bytearray();bo=[0];mo=[0]
 for clip in range(CLIP):
  b=books[clip];ln=lengths(b);ec,_,_=canon(ln)
  bookdata.extend(encode(b,ec));bo.append(len(bookdata))
  group=maps[clip*27:min((clip+1)*27,628)]
  cand=[]
  for mode in [0,1]:
   if mode==0:s=b''.join(group)
   else:s=bytes(x for frame in group for at,x in enumerate(frame) if at<12 or x!=frame[at-12])
   ml=lengths(s);code,_,_=canon(ml)
   frames=[encode_frame(f,code,mode) for f in group]
   cand.append((sum(map(len,frames)),mode,ml,frames))
  _,mode,ml,frames=min(cand,key=lambda a:(a[0],a[1]))
  tables.extend(length_bytes(ln));tables.extend(length_bytes(ml));modes.append(mode)
  for f in frames:mapdata.extend(f);mo.append(len(mapdata))
 offs=[];at=64
 for size in [768,7680,24,100,2516,len(bookdata),len(mapdata)]:
  offs.append(at);at+=size
 header=bytearray(64)
 struct.pack_into('<4sHHIIHHHH',header,0,b'MHP2',1,0,at,0,24,628,96,96)
 struct.pack_into('<7I',header,24,*offs)
 payload=pal+tables+modes+struct.pack('<25I',*bo)+struct.pack('<629I',*mo)+bookdata+mapdata
 assert len(payload)+64==at
 struct.pack_into('<I',header,12,zlib.crc32(payload))
 return bytes(header)+payload

def unpack(blob):
 assert blob[:4]==b'MHP2'
 magic,v,flags,total,crc,nc,nf,w,h=struct.unpack_from('<4sHHIIHHHH',blob,0)
 if (v,flags,total,nc,nf,w,h)!=(1,0,len(blob),24,628,96,96):raise ValueError('invalid header')
 if zlib.crc32(blob[64:])!=crc:raise ValueError('invalid CRC')
 p,t,m,oi,fi,bs,ms=struct.unpack_from('<7I',blob,24)
 books=[];maps=[]
 bos=struct.unpack_from('<25I',blob,oi);fos=struct.unpack_from('<629I',blob,fi)
 for c in range(24):
  b_l=de_lengths(blob[t+c*320:t+c*320+160]);m_l=de_lengths(blob[t+c*320+160:t+c*320+320])
  _,bd,bn=canon(b_l);_,md,mn=canon(m_l)
  reader=BitR(blob[bs+bos[c]:bs+bos[c+1]])
  book=bytes(reader.sym(bd,bn) for _ in range(4096));books.append(book)
  if (reader.at+7)//8 != bos[c+1]-bos[c]:raise ValueError('bad book length')
  for f in range(c*27,min((c+1)*27,628)):
   mp=decode_frame(blob[ms+fos[f]:ms+fos[f+1]],md,mn,blob[m+c]);maps.append(mp)
 return blob[p:t]+b''.join(books)+b''.join(maps)

if __name__=='__main__':
 raw=(HERE/'color_target_vq_legacy_layout.bin').read_bytes()
 out=pack(raw)
 assert unpack(out)==raw
 print('EXACT MHP2 STREAM',len(out),'bytes, decoded 24 books + 628 frames, CRC OK',flush=True)
 (HERE/'source16_vq_mhp2.bin').write_bytes(out)