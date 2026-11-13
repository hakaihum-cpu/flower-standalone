#!/usr/bin/env python3
"""Source-only deterministic pixel palette conversion. No AI/redrawing."""
import os,sys,hashlib,json
from pathlib import Path
os.environ['OPENBLAS_NUM_THREADS']='2'
import cv2
import numpy as np
from PIL import Image,ImageDraw,ImageFont
ROOT=Path('/mnt/data/fm1_codec_trial')
sys.path.insert(0,str(ROOT))
from photo96_sources import files,get
FOLDER=ROOT/'original_sources'
MAN=json.loads((ROOT/'manifest.json').read_text())
PALETTE=np.array([
(5,5,8), #0 deep black
(27,12,14), #1 warm black
(51,23,22), #2 dark chocolate
(75,35,31), #3 reddish brown
(112,51,43), #4 red shadow
(145,67,54), #5 warm rust
(174,91,73), #6 skin shading
(201,128,104), #7 skin warm mid
(228,175,149), #8 skin highlight
(247,222,199), #9 cream highlight
(55,49,53), #10 cloth shadow
(107,95,104), #11 cloth grey mauve
(173,155,164), #12 cloth highlight
(175,72,41), #13 burnt orange
(233,111,64), #14 sunset coral
(251,177,111), #15 golden orange
],dtype=np.uint8)
# ramps are drawn from same 16 shared colors but selected by material mask.
HAIR=np.array([0,0,0,1,1,2,2,3,3,4,5,6,7,8,9,9],dtype=np.uint8)
SKIN=np.array([0,1,2,3,4,4,5,6,6,7,7,8,8,8,9,9],dtype=np.uint8)
BACKGROUND=np.array([0,1,2,3,3,4,5,13,6,14,7,15,8,8,9,9],dtype=np.uint8)
CLOTH=np.array([0,1,2,10,10,11,11,11,12,12,12,12,9,9,9,9],dtype=np.uint8)
SCENIC=np.array([0,1,2,3,4,5,13,6,14,7,15,8,9,9,9,9],dtype=np.uint8)
FACE_DETECTOR=cv2.CascadeClassifier(cv2.data.haarcascades+'haarcascade_frontalface_default.xml')

def face_mask(gray, cache=None):
    large=cv2.resize(gray,(288,288),interpolation=cv2.INTER_CUBIC)
    rect=FACE_DETECTOR.detectMultiScale(large,scaleFactor=1.08,minNeighbors=3,minSize=(60,60),maxSize=(275,275))
    if len(rect):
        x,y,w,h=max(rect,key=lambda v:v[2]*v[3]); x,y,w,h=x/3,y/3,w/3,h/3
    else:
        # only used for portrait bank, not unknown/scenic frames
        x,y,w,h=12,19,72,75
    yy,xx=np.mgrid[:96,:96]
    cx=x+0.53*w;cy=y+0.61*h;rx=.35*w;ry=.36*h
    ell=((xx-cx)/(rx+1e-6))**2+((yy-cy)/(ry+1e-6))**2
    mask=np.clip((1.20-ell)*2.2,0,1)
    return mask,(x,y,w,h)

def colorize(raw_rgb,bank,debug=False):
    g=cv2.cvtColor(raw_rgb,cv2.COLOR_RGB2GRAY)
    # protect actual original monochrome geometry by deriving EVERY output pixel from original brightness
    lum=np.clip(np.round(g.astype(np.float32)/255*15),0,15).astype(np.uint8)
    if bank>=6:
        # Color-rich user originals: direct nearest palette mapping keeps source hue and materials.
        d=(raw_rgb.astype(np.int16)[:,:,None,:]-PALETTE.astype(np.int16)[None,None,:,:]).astype(np.int32)
        idx=np.argmin((d*d).sum(axis=3),axis=2).astype(np.uint8)
        return idx,dict(bank=bank,segment='source_rgb_palette')
    mask,rect=face_mask(g)
    yy,xx=np.mgrid[:96,:96]
    # background is different material; face uses different warm shades from same monochrome input
    # Hair and shadow remain black/dark, independently of the material mask.
    skin=(mask>.48)&(g>80)
    # conservative non-face clothing: below lower face only, keeping original dark outlines
    lower=(rect[1]+rect[3]*.84)
    clothing=(yy>lower)&(g>75)&(np.abs(xx-(rect[0]+rect[2]*.54))<rect[2]*.64)
    if bank==1: # eyes extreme closeup; preserve white of sclera, keep dark iris.
        skin=(g>70)&(yy>21)&(yy<88)
        clothing=np.zeros((96,96),bool)
    if bank==2: # varied original sequences; infer bright skin only within detected face, avoid false bright backgrounds
        skin=(mask>.53)&(g>80)
    if bank==5: # varied scenes may not always be portraits
        skin=(mask>.63)&(g>105)
    index=BACKGROUND[lum]
    if bank in [1]: index=SKIN[lum]
    index[clothing]=CLOTH[lum[clothing]]
    index[skin]=SKIN[lum[skin]]
    # darkest original regions preserve strong black silhouette
    index[g<25]=0
    # near-zero shading used as dark for continuity
    info={'bank':bank,'face_box':[float(v) for v in rect], 'skin_frac':float(skin.mean()), 'cloth_frac':float(clothing.mean())}
    return index,info

if __name__=='__main__':
    fr=[0,4,40,81,145,245,326,407,488,558,600]
    bank_start=np.cumsum([0]+[x[3] for x in files])
    ims=[]; outs=[]
    for f in fr:
        bank=int(np.searchsorted(bank_start,f,side='right')-1)
        i=f-int(bank_start[bank]);src=np.asarray(get(bank,i),dtype=np.uint8)
        idx,info=colorize(src,bank)
        out=PALETTE[idx]
        ims.append(src);outs.append(out)
        print('FRAME',f,info,flush=True)
    scale=3;w=96*3;gap=18;left=20;title=70;rowh=96*3+32
    panel=Image.new('RGB',(left*2+w*2+gap,title+rowh*len(fr)+12),(16,17,20));d=ImageDraw.Draw(panel)
    d.text((left,14),'SOURCE PIXELS / unchanged shape',fill='white')
    d.text((left+w+gap,14),'EXACT SOURCE PIXELS / 16 shared object-color shades',fill='white')
    for row,f in enumerate(fr):
        y=title+row*rowh
        for col,src in enumerate([ims[row],outs[row]]):
            panel.paste(Image.fromarray(src).resize((w,w),Image.Resampling.NEAREST),(left+col*(w+gap),y))
        d.text((left,y+w+2),f'Frame {f:03}',fill='white')
    (ROOT/'source_only_palette_preview.png').parent.mkdir(exist_ok=True,parents=True)
    panel.save(ROOT/'source_only_palette_preview.png')
    print('saved',ROOT/'source_only_palette_preview.png')