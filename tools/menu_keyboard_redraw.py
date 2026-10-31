#!/usr/bin/env python3
"""Reproduce the authored keyboard with Overpass and shapes. Requires NumPy and Pillow."""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
S=2; W,H=2048*S,1024*S
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
FONT = str(ROOT / 'port/assets/fonts/Overpass-650.ttf')
OUT=(30,140,255,255); GLOW=(32,144,248,255); TEXT=(2,55,198,255); SHADOW=(0,0,30,200); FILL=(0,24,64,255)
def s(*v): return [int(x*1.024*S) for x in v]
import numpy as np
# Measured from the upscaled original (2048-wide pixels): outline, inner fill, gap between boxes, and glow alpha against distance outside.
BOX1=(87,157,1069,253); BOX2=(87,271,1069,671); OUTER=(87,157,1069,671); R=14
xx,yy=np.meshgrid((np.arange(W)+.5)/S,(np.arange(H)+.5)/S)
def sdf(b,r=R):
    cx,cy=(b[0]+b[2])/2,(b[1]+b[3])/2; hx,hy=(b[2]-b[0])/2-r,(b[3]-b[1])/2-r
    qx,qy=np.abs(xx-cx)-hx,np.abs(yy-cy)-hy
    return np.hypot(np.maximum(qx,0),np.maximum(qy,0))+np.minimum(np.maximum(qx,qy),0)-r
rgba=np.zeros((H,W,4),float)
def over(rgb,alpha):
    a=np.clip(alpha,0,1)[...,None]; dst=rgba[...,3:]
    out=a+dst*(1-a); rgba[...,:3]=np.where(out>0,(np.array(rgb,float)*a+rgba[...,:3]*dst*(1-a))/np.maximum(out,1e-6),rgba[...,:3]); rgba[...,3:]=out
GLOW_RGB=(36,150,252); CORE_RGB=(41,209,255); FILL_RGB=(2,25,66)
outer=sdf(OUTER); d1,d2=sdf(BOX1),sdf(BOX2); inside=np.minimum(d1,d2)
def glow(d):
    fade=np.clip((46-d)/20,0,1); fade=fade*fade*(3-2*fade)
    return .78*np.exp(-np.maximum(d,0)/15)*fade
# Each box has its own glow, like two rounded rectangles side by side; they add up in the space between them.
g1,g2=glow(d1),glow(d2); w=np.clip((10-outer)/20,0,1); w=w*w*(3-2*w)
over(GLOW_RGB,np.maximum(g1,g2)*(1-w)+(1-(1-g1)*(1-g2))*w)
aa=lambda d:np.clip(.5-d*S,0,1)
rgba[...,3]=rgba[...,3]*(1-aa(inside))
over(FILL_RGB,aa(inside)*(120/255.))
over(CORE_RGB,aa(np.abs(inside)-1.7)*.85)
im=Image.fromarray(np.uint8(np.clip(rgba,0,255)*[1,1,1,255]),'RGBA')
f=ImageFont.truetype(FONT,int(44*S))
layer=Image.new('RGBA',(W,H),(0,0,0,0)); ld=ImageDraw.Draw(layer)
shadow=Image.new('RGBA',(W,H),(0,0,0,0)); sd=ImageDraw.Draw(shadow)
def t(x,y,txt):
    ld.text(tuple(s(x,y)),txt,font=f,fill=TEXT,anchor='mm'); sd.text((s(x,y)[0]+4*S,s(x,y)[1]+4*S),txt,font=f,fill=SHADOW,anchor='mm')
cols=[365,435,505,575,645,715,785,855,925,995]; rows=[313,383,454,524,592]
for c,x in zip('1234567890',cols): t(x,rows[0],c)
for c,x in zip('abcdefghij',cols): t(x,rows[1],c)
for c,x in zip('klmnopqrst',cols): t(x,rows[2],c)
for c,x in zip('uvwxyz',cols): t(x,rows[3],c)
t(213,rows[0],'Done'); t(213,rows[1],'Shift'); t(213,rows[2],'Caps Lock')
t(882,rows[3],'Backspace'); t(532,rows[4],'Space')
for x,sg in ((808,-1),(948,1)):
    y=rows[4]
    for c,o in ((SHADOW,4*S),(TEXT,0)):
        tgt=shadow if o else layer; q=ImageDraw.Draw(tgt)
        q.line([a+o for a in s(x-sg*18,y)+s(x+sg*18,y)],fill=c,width=6*S)
        q.line([a+o for a in s(x+sg*18,y)+s(x+sg*4,y-12)],fill=c,width=6*S); q.line([a+o for a in s(x+sg*18,y)+s(x+sg*4,y+12)],fill=c,width=6*S)
im.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(2*S))); im.alpha_composite(layer)
im=im.resize((2048,1024),Image.LANCZOS); im.save(ROOT / 'port/assets/menus/xbox/art/keyboard.png')
