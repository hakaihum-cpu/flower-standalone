"""Unit tests for the existing VQ black-frame substitution."""
import numpy as np
from black_frame_guard import (
    FRAME_COUNT, WIDTH, PADDED_HEIGHT, TILES, MIN_VISIBLE_PIXELS,
    replace_black_frames, visible_pixel_counts,
)
P = np.zeros(16, np.uint16)
P[1] = 0xFFFF


def frames():
    source = np.ones((FRAME_COUNT, PADDED_HEIGHT, WIDTH), np.uint8)
    ids = np.arange(FRAME_COUNT*TILES, dtype=np.int32).reshape(FRAME_COUNT,TILES)
    return source, source.copy(), ids


a,b,m=frames()
original=m.copy()
assert replace_black_frames(a,b,m,P,"EFFECTS",0)==[]
assert np.array_equal(m,original)

a,b,m=frames()
a[1]=0
assert replace_black_frames(a,b,m,P,"EFFECTS",0)==[(1,0)]
assert np.array_equal(m[1],m[0])

a,b,m=frames()
b[20]=0
assert replace_black_frames(a,b,m,P,"EUREKA",3)==[(128,127)]
assert np.array_equal(m[20],m[19])

a,b,m=frames()
a[0:3]=0
assert replace_black_frames(a,b,m,P,"EUREKA",2)==[(72,75),(73,75),(74,75)]
assert np.array_equal(m[0],m[3])

a,b,m=frames()
a[8]=0
a[8,0,:MIN_VISIBLE_PIXELS-1]=1
assert replace_black_frames(a,b,m,P,"EFFECTS",1)==[(44,43)]
a,b,m=frames()
a[8]=0
a[8,0,:MIN_VISIBLE_PIXELS]=1
assert replace_black_frames(a,b,m,P,"EFFECTS",1)==[]

a,b,m=frames()
a[:]=0
try:
    replace_black_frames(a,b,m,P,"EUREKA",0)
except ValueError as error:
    assert "no same-clip replacement" in str(error)
else:
    raise AssertionError("black clip must fail closed")

pal=np.zeros(16,np.uint16)
pal[1]=0x0841
pal[2]=0xFFFF
frameset=np.zeros((1,PADDED_HEIGHT,WIDTH),np.uint8)
frameset[0,0,:MIN_VISIBLE_PIXELS]=1
assert visible_pixel_counts(frameset,pal)[0]==0
frameset[0,0,:MIN_VISIBLE_PIXELS]=2
assert visible_pixel_counts(frameset,pal)[0]==MIN_VISIBLE_PIXELS
# Palette index zero is not transparency. A bright source image whose
# ONLY visible colour is palette[0] must never be replaced by its neighbour.
bright_zero=np.zeros(16,np.uint16)
bright_zero[0]=0xFFFF
all_zero=np.zeros((FRAME_COUNT,PADDED_HEIGHT,WIDTH),np.uint8)
assert np.all(visible_pixel_counts(all_zero,bright_zero)==WIDTH*(PADDED_HEIGHT-2))
selectors=np.arange(FRAME_COUNT*TILES,dtype=np.int32).reshape(FRAME_COUNT,TILES)
original=selectors.copy()
assert replace_black_frames(all_zero,all_zero.copy(),selectors,bright_zero,"EUREKA",0)==[]
assert np.array_equal(selectors,original)
# A genuinely BLACK palette entry zero remains invisible, without special
# index handling. Test mixed bright and black index-zero pixels as well.
dark_zero=np.zeros(16,np.uint16)
dark_zero[1]=0xFFFF
mixed=np.zeros((1,PADDED_HEIGHT,WIDTH),np.uint8)
mixed[0,0,:MIN_VISIBLE_PIXELS]=1
assert visible_pixel_counts(mixed,dark_zero)[0]==MIN_VISIBLE_PIXELS
print("PASS: nine black-frame substitution and palette-zero opacity tests")
