"""PT_SFStageFinal: sketch-space east=+Y, north=+X; centimetres, Z-up."""
import math
from pathlib import Path

MAP='/Game/Game/Stage/PT_SFStageFinal'
ART='/Game/Game/Stage/CapitalFinalArt'
W,H=28000.,22000.
BEAST=(.20,.17)
CAPITAL=(.82,.81)
START=(.755,.80)
HILL=(.600,.610)
RAMPS=[[(.550,.430),(.590,.570)],[(.445,.600),(.570,.610)],[(.755,.780),(.635,.650)]]
LAKES=[(.375,.335,.102,.104),(.697,.297,.108,.101),(.310,.605,.079,.096)]
ROUTE_UV=[
 [(.255,.19),(.36,.145),(.50,.165),(.68,.15),(.81,.19),(.835,.31),(.835,.49),(.815,.65),(.79,.72)],
 [(.255,.19),(.36,.145),(.50,.165),(.53,.29),(.55,.43),(.575,.52),(.600,.610),(.635,.650),(.700,.720),(.755,.780)],
 [(.20,.25),(.175,.38),(.20,.47),(.33,.48),(.425,.51),(.445,.60),(.525,.606),(.600,.610),(.635,.650),(.700,.720),(.755,.780)],
 [(.20,.25),(.175,.38),(.16,.55),(.19,.76),(.30,.83),(.48,.865),(.65,.85),(.75,.79)],
]
def smoothline(p,n=5):
 out=[]
 for i in range(len(p)-1):
  a,b,c,d=p[max(0,i-1)],p[i],p[i+1],p[min(len(p)-1,i+2)]
  for j in range(n):
   t=j/n
   out.append(tuple(.5*((2*b[k])+(-a[k]+c[k])*t+(2*a[k]-5*b[k]+4*c[k]-d[k])*t*t+(-a[k]+3*b[k]-3*c[k]+d[k])*t*t*t) for k in (0,1)))
 return out+[p[-1]]
ROUTES=[smoothline(p) for p in ROUTE_UV]
CHANNEL_UV=[
 [(.46,.34),(.49,.342),(.535,.34),(.575,.325),(.61,.32)],
 [(.35,.405),(.345,.445),(.34,.48),(.33,.535)],
 [(.775,.325),(.815,.333),(.85,.34),(.91,.34),(1.065,.385)],
 [(.305,.665),(.285,.715),(.25,.77),(.225,.815),(.17,.88),(.13,.96),(.115,1.065)],
]
CHANNELS=[smoothline(p,3) for p in CHANNEL_UV]
WATER_Z=-28.
def xy(u,v):return ((.5-v)*H,(u-.5)*W)
def uv(x,y):return (y/W+.5,.5-x/H)
def smooth(a,b,x):
 t=max(0,min(1,(x-a)/(b-a)));return t*t*(3-2*t)
def pond_q(u,v,k):
 a,b,rx,ry=LAKES[k];dx,dy=(u-a)/rx,(v-b)/ry;t=math.atan2(dy,dx)
 return math.hypot(dx,dy)/(1+.065*math.sin(3*t+k)+.045*math.sin(5*t+1))
def height(u,v):
 z=65+80*math.sin(u*20+v*5)*math.sin(v*18)+45*math.sin(u*39-v*16)
 hill=hill_q(u,v)
 rise=1050*(1-smooth(.72,.84,hill))
 p=xy(u,v)
 for a,b in RAMPS:
  a,b=xy(*a),xy(*b);dx,dy=b[0]-a[0],b[1]-a[1]
  t=max(0,min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/(dx*dx+dy*dy)))
  d=math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy)
  blend=1-smooth(620,940,d)
  rise=rise*(1-blend)+1050*smooth(0,1,t)*blend
 z+=rise
 # Both bases are level, without changing character/building asset scales.
 for center,r in [(BEAST,.112),(CAPITAL,.14)]:
  d=math.hypot((u-center[0])*W,(v-center[1])*H)
  z=z*smooth(r*H*.65,r*H,d)+35*(1-smooth(r*H*.65,r*H,d))
 edge=max(abs(u-.5)/.53,abs(v-.5)/.53)
 z+=950*smooth(.99,1.15,edge)
 for b in BRIDGES:
  along,across=bridge_local(u,v,b)
  blend=(1-smooth(b['half_length']-100,b['half_length']+1000,abs(along)))*(1-smooth(b['half_width']-50,b['half_width']+550,abs(across)))
  if abs(along)>b['half_length']-500:z=z*(1-blend)+260*blend
 d=water_distance(u,v)
 if d<0:
  depth=12+258*smooth(0,900,-d)
  z=WATER_Z-depth+10*math.sin(u*55)*math.sin(v*43)*smooth(0,500,-d)
 else:
  bank=smooth(0,550,d);z=z*bank+WATER_Z*(1-bank)
 return z
def world(u,v,offset=0):
 z=height(u,v)
 for b in BRIDGES:
  s,t=bridge_local(u,v,b)
  if abs(s)<=b['half_length'] and abs(t)<b['half_width']:z=max(z,bridge_z(s,b))
 return (*xy(u,v),z+offset)
def segdist(p,a,b):
 dx,dy=b[0]-a[0],b[1]-a[1];t=max(0,min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/max(1,dx*dx+dy*dy)))
 return math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy)
def hill_q(u,v):
 return math.hypot((u-HILL[0])/.132,(v-HILL[1])/.145)
def ramp_distance(u,v):return min(segdist(xy(u,v),xy(*a),xy(*b)) for a,b in RAMPS)
SEGMENTS=[(xy(*a),xy(*b)) for route in ROUTES for a,b in zip(route,route[1:])]
CHANNEL_SEGMENTS=[(xy(*a),xy(*b)) for route in CHANNELS for a,b in zip(route,route[1:])]

def water_distance(u,v):
 p=xy(u,v)
 channel=min(segdist(p,a,b) for a,b in CHANNEL_SEGMENTS)-660
 channel-=45*math.sin(u*83+v*27)+25*math.sin(v*103-u*46)
 return min(channel,min((pond_q(u,v,k)-1)*min(rx*W,ry*H) for k,(_,_,rx,ry) in enumerate(LAKES)))

def crossing(channel,route):
 for a,b in zip(channel,channel[1:]):
  a,b=xy(*a),xy(*b);r=(b[0]-a[0],b[1]-a[1])
  for c,d in zip(route,route[1:]):
   c,d=xy(*c),xy(*d);s=(d[0]-c[0],d[1]-c[1]);den=r[0]*s[1]-r[1]*s[0]
   if abs(den)<.01:continue
   q=(c[0]-a[0],c[1]-a[1]);t=(q[0]*s[1]-q[1]*s[0])/den;k=(q[0]*r[1]-q[1]*r[0])/den
   if 0<=t<=1 and 0<=k<=1:
    norm=math.hypot(*s);return {'center':(a[0]+t*r[0],a[1]+t*r[1]),'axis':(s[0]/norm,s[1]/norm),'half_length':1700.,'half_width':700.}
 raise ValueError('Channel must cross its lane')
BRIDGES=[crossing(CHANNELS[i],ROUTES[j]) for i,j in enumerate([1,2,0,3])]
for i,b in enumerate(BRIDGES):b['style']='stone';b['lane']=[1,2,0,3][i]
def bridge_local(u,v,b):
 x,y=xy(u,v);x-=b['center'][0];y-=b['center'][1];a,c=b['axis'];return x*a+y*c,-x*c+y*a
def bridge_z(s,b):return 260+100*max(0,math.cos(math.pi*s/(2*b['half_length'])))
def road_distance(u,v):return min(segdist(xy(u,v),a,b) for a,b in SEGMENTS)
def base_distance(u,v,c):return math.hypot((u-c[0])*W,(v-c[1])*H)
def protected(u,v,pad=0):
 return road_distance(u,v)<850+pad or ramp_distance(u,v)<820+pad or base_distance(u,v,HILL)<1500+pad or (.67<hill_q(u,v)<.89) or base_distance(u,v,BEAST)<2300+pad or base_distance(u,v,CAPITAL)<3150+pad or any(pond_q(u,v,k)<1.12+pad/2200 for k in range(3))
def write_mesh(path,verts,faces):
 lines=['# centimetres; OBJ-to-Unreal Y preconversion','o '+path.stem,'s 1']
 lines+=['v %.4f %.4f %.4f'%(x,-y,z) for x,y,z in verts]
 lines+=['vt %.5f %.5f'%(x/550,y/550) for x,y,z in verts]
 lines+=['f '+' '.join('%d/%d'%(i+1,i+1) for i in reversed(f)) for f in faces]
 path.write_text('\n'.join(lines)+'\n',encoding='utf-8')
def generate(folder):
 folder.mkdir(parents=True,exist_ok=True)
 nx,ny=280,220;verts=[];faces=[]
 for j in range(ny+1):
  for i in range(nx+1):
   u,v=-.10+1.20*i/nx,-.10+1.20*j/ny;verts.append((*xy(u,v),height(u,v)))
 # UV->world has positive determinant, so these faces point upwards.
 for j in range(ny):
  for i in range(nx):
   a=j*(nx+1)+i;b=a+1;c=a+nx+1;d=c+1
   faces.extend([(a,b,d),(a,d,c)])
 write_mesh(folder/'SM_CapitalTerrain.obj',verts,faces)
 for k,(u,v,rx,ry) in enumerate(LAKES):
  ring=[]
  for j in range(72):
   a=2*math.pi*j/72;r=1+.065*math.sin(3*a+k)+.045*math.sin(5*a+1)
   ring.append((*xy(u+rx*r*math.cos(a),v+ry*r*math.sin(a)),-28.))
  verts=[(*xy(u,v),-28.)]+ring
  write_mesh(folder/('SM_CapitalWater%d.obj'%k),verts,[(0,i+1,(i+1)%72+1) for i in range(72)])
  # Closed collision volume blocks entry into water; its top has a null nav area.
  vs=[(x,y,z) for z in (-500,900) for x,y,_ in ring];fs=[]
  for i in range(72):
   j=(i+1)%72;fs.extend([(i,j,j+72),(i,j+72,i+72)])
  for i in range(1,71):fs.extend([(0,i+1,i),(72,72+i,72+i+1)])
  write_mesh(folder/('SM_CapitalLakeBlock%d.obj'%k),vs,fs)
