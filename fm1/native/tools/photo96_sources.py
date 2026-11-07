from PIL import Image,ImageOps,ImageFilter
import numpy as np
from pathlib import Path
files=[
('file_000000008e98820690554a4b75774ed5.png',9,9,81),
('file_00000000f1a08206a57bdcc319a336d0.png',8,8,64),
('file_00000000e7388206b7bb79a8b73154ee(2).png',10,10,100),
('file_00000000e9448206897342ec340f08d4(2).png',9,9,81),
('file_00000000cf38820685e20ea7661aea2d(2).png',9,9,81),
('file_000000006c3882069f4e9ec393c3615f(2).png',9,9,81),
('Gemini_Generated_Image (3).jpeg',10,8,70),
('Gemini_Generated_Image (4).jpeg',10,7,70)]
COLORS=[(0,0,0),(30,9,10),(47,23,20),(85,33,12),(68,49,36),(76,56,42),(102,66,53),(123,66,40),(136,63,35),(139,96,87),(167,89,52),(209,90,48),(175,117,98),(236,136,79),(222,169,133),(255,185,137)]
pal=Image.new('P',(1,1));pal.putpalette([c for rgb in COLORS for c in rgb])
import os
base=Path(os.environ.get("FM1_USER_SHEETS", "Resources/fm1_photo_96_inputs"))
def get(bank, index):
    name,cols,rows,_=files[bank]
    im=Image.open(base/name).convert('RGB')
    x,y=index%cols,index//cols
    # remove very thin contact-sheet gutters; keep every frame intact
    x0=int(round(x*im.width/cols))+2
    y0=int(round(y*im.height/rows))+2
    x1=int(round((x+1)*im.width/cols))-2
    y1=int(round((y+1)*im.height/rows))-2
    return im.crop((x0,y0,x1,y1)).resize((96,96),Image.Resampling.LANCZOS)

def render(im,bank):
    gray=np.asarray(ImageOps.autocontrast(im.convert('L'),cutoff=1),dtype=np.float32)/255.
    yy,xx=np.mgrid[0:96,0:96];xx=xx/95.;yy=yy/95.
    # Colour is NOT a grayscale-to-one-palette gradient: positionally-defined
    # skin/background/accent regions have different warm-hue ramps.
    fg_region=((xx-.52)/.48)**2+((yy-.59)/.57)**2<1.
    # Portrait frames favour skin highlights; eye frames favour pink/skin;
    # city/scenery frames favour the rust-to-orange background ramps.
    if bank in (1,6):
        fg_region=((xx-.50)/.78)**2+((yy-.50)/.80)**2<1.
    elif bank in (3,7):
        fg_region=((xx-.48)/.34)**2+((yy-.62)/.34)**2<1.
    elif bank in (2,):
        fg_region=((xx-.49)/.41)**2+((yy-.57)/.47)**2<1.
    # mid-bright portrait area is more likely skin; pure white uniform sky/walls
    # remain in background. No original facial pixel geometry changes.
    fg_region &= gray < .93
    bins=np.array([.06,.12,.2,.29,.38,.47,.56,.64,.72,.80,.87],dtype=np.float32)
    band=np.searchsorted(bins,gray,side='left').clip(0,11)
    skin=np.array([0,1,2,3,4,5,6,7,8,9,12,15],dtype=np.uint8)
    rust=np.array([0,1,2,3,4,5,7,8,10,11,13,14],dtype=np.uint8)
    # Side streets and evening environments use an amber/coral accent ramp.
    # This gives different palette roles to surfaces with the same brightness.
    accent=np.array([0,1,2,3,4,7,8,10,11,13,14,15],dtype=np.uint8)
    bright_side=np.sin((xx*.9+yy*.6+bank*.17)*3.14159)>0.58
    idx=np.where(fg_region,skin[band],np.where(bright_side,accent[band],rust[band]))
    out=Image.frombytes('P',(96,96),idx.tobytes())
    out.putpalette([c for rgb in COLORS for c in rgb])
    return out

if __name__=='__main__':
    samples=[]
    from collections import Counter
    for bank in range(8):
        for idx in (0,files[bank][3]//2,files[bank][3]-1):
            orig=get(bank,idx)
            result=render(orig,bank)
            counts=Counter(result.tobytes())
            print(bank,idx,len(counts),counts.most_common(5))
            samples.append(result.convert('RGB').resize((192,192),Image.Resampling.NEAREST))
    sheet=Image.new('RGB',(4*192,6*192),'#171717')
    for i,s in enumerate(samples):sheet.paste(s,((i%4)*192,(i//4)*192))
    sheet.save('/mnt/data/fm1_recorder_development/color_candidate.png')
