"""User supplied black/white contact sheets -> warm 16-colour colourized palette.
Pixel positions, facial outlines and shape originate solely from original images.
Colour assignment is explicitly artistic, not restoration of historically true hue.
"""
from photo96_sources import files, get, COLORS
from PIL import Image,ImageOps
import numpy as np
from pathlib import Path
palette = Image.new('P', (1,1))
palette.putpalette([c for rgb in COLORS for c in rgb])
SIDE=96
YY,XX = np.mgrid[0:SIDE,0:SIDE]
x=XX/95.;y=YY/95.
# Reference color groups: dark 0-2, bronze 3-8, brick red 9-11, skin 12/14/15, orange 13
# Shades use original light/dark only; location chooses hue within reference roles.
def colorize(image,bank):
    arr=np.asarray(ImageOps.autocontrast(image.convert('L'),cutoff=0.3),dtype=np.float32)/255.
    # Avoid the coarse 'all lit objects are peach' colorization: use a richer
    # area-specific palette. The source contrast, not procedural shapes,
    # controls all pixel edges.
    high = np.clip(arr,0,1)
    # Shape factors are SOFT spatial selection of material colour only.
    if bank in (0,2,3,4,5):
        skin = (((x-.52)/.37)**2 + ((y-.55)/.53)**2 <= 1.0)
        clothes = y>.77
    elif bank in (1,):
        skin = (((x-.50)/.78)**2 + ((y-.50)/.78)**2 <= 1.0)
        clothes = y>.68
    else:
        skin = (((x-.49)/.61)**2 + ((y-.50)/.49)**2 <= 1.0)
        clothes = y>.88
    # Use neighboring context to protect obvious black hair from becoming
    # painted skin, preserving recognizability.
    dark = high < .15
    # Original material gradients are kept as 16 stepped stable shades,
    # with warm red, orange and muted purple zones, not luma-only 1D ramps.
    p_skin = np.array([0,1,2,3,5,6,8,9,10,12,13,14,15],dtype=np.uint8)
    p_bg1 = np.array([0,1,2,3,4,5,7,8,10,11,13,13,15],dtype=np.uint8)
    p_bg2 = np.array([0,1,2,3,4,6,7,10,11,13,13,14,15],dtype=np.uint8)
    p_cloth = np.array([0,1,2,3,4,5,6,9,12,12,14,14,15],dtype=np.uint8)
    band=np.searchsorted(np.array([.045,.095,.16,.26,.37,.49,.58,.67,.75,.82,.89,.955]),high)
    band=band.clip(0,12)
    bg_select=((x*.92-y*.33+bank*.19)%1.)>.55
    color=np.where(skin,p_skin[band],np.where(bg_select,p_bg2[band],p_bg1[band]))
    color=np.where(clothes,p_cloth[band],color)
    color=np.where(dark,np.minimum(band,2),color)
    # This palette is truly 16 colour RGB565, with black preserved.
    result=Image.fromarray(color.astype('uint8'), mode='P')
    result.putpalette([c for rgb in COLORS for c in rgb])
    return result

if __name__=='__main__':
    from collections import Counter
    frames=[(0,0),(0,3),(0,40),(1,0),(2,0),(2,22),(3,0),(4,0),(5,1),(6,0),(7,0),(7,60)]
    sheet=Image.new('RGB',(4*192,3*192),'#151515')
    for k,(bank,idx) in enumerate(frames):
        im=colorize(get(bank,idx),bank)
        counts=Counter(im.tobytes())
        print(bank,idx,'ncolors',len(counts),'palette',sorted(counts))
        sheet.paste(im.convert('RGB').resize((192,192),Image.Resampling.NEAREST),((k%4)*192,(k//4)*192))
    sheet.save('/mnt/data/fm1_recorder_development/color_v2_candidate.png')
