"""Design space for the three-front woodland ambush; north is +X, east is +Y."""
import math
from pathlib import Path
from capital_final_layout import smoothline,segdist,smooth,write_mesh
MAP='/Game/Game/Stage/PT_SFStageAmbush'
ART='/Game/Game/Stage/AmbushArt'
W,H=22000.,20000.
# xy/uv and height remain in authoring space. Revision 3 expands the saved map in XY only.
MAP_SCALE=1.1
def world_xy(u,v):return tuple(c*MAP_SCALE for c in xy(u,v))
def world_uv(x,y):return uv(x/MAP_SCALE,y/MAP_SCALE)
CENTER=(.50,.565)
START=(.51,.65)
CAMPS=[(.18,.17),(.50,.095),(.83,.18)]
ROUTES=[smoothline(p,6) for p in [
 [CAMPS[0],(.20,.29),(.30,.33),(.28,.43),(.37,.50),(.46,.55)],
 [CAMPS[1],(.53,.18),(.49,.27),(.52,.34),(.50,.44),(.50,.525)],
 [CAMPS[2],(.82,.28),(.71,.34),(.74,.42),(.65,.49),(.55,.55)]]]
RETREAT=smoothline([(.50,.67),(.49,.74),(.50,.795),(.50,.945),(.50,1.03)],4)
def xy(u,v):return ((.5-v)*H,(u-.5)*W)
def uv(x,y):return (y/W+.5,.5-x/H)
SEGMENTS=[[(xy(*a),xy(*b)) for a,b in zip(r,r[1:])] for r in ROUTES]
EXIT_SEGMENTS=[(xy(*a),xy(*b)) for a,b in zip(RETREAT,RETREAT[1:])]
def lane_distance(u,v,i):return min(segdist(xy(u,v),a,b) for a,b in SEGMENTS[i])
def road_distance(u,v):return min(lane_distance(u,v,i) for i in range(3))
def exit_distance(u,v):return min(segdist(xy(u,v),a,b) for a,b in EXIT_SEGMENTS)
def center_distance(u,v):return math.dist(xy(u,v),xy(*CENTER))
def ravine_q(u,v):return math.hypot((u-.5)/.205,(v-.87)/.054)
def original_height(u,v):
 r=center_distance(u,v);noise=65*math.sin(u*29+v*6)*math.sin(v*27)+40*math.sin(u*56-v*38)
 z=65+noise+1700*smooth(3950,5450,r)+250*smooth(9000,12000,r)
 for i,width in enumerate((710,620,665)):
  d=lane_distance(u,v,i);blend=1-smooth(width,width+330,d)
  road=70+1540*smooth(3350,8900,r)+noise*.35
  z=z*(1-blend)+road*blend
 z=z*smooth(3050,3650,r)+65*(1-smooth(3050,3650,r))
 for c in CAMPS:
  d=math.dist(xy(u,v),xy(*c));h=70+1540*smooth(3350,8900,center_distance(*c))
  blend=1-smooth(550,1000,d);z=z*(1-blend)+h*blend
 if v>.66:
  e=1-smooth(560,920,exit_distance(u,v));z=z*(1-e)+(110+90*smooth(.69,.77,v))*e
 q=ravine_q(u,v);cut=1-smooth(.72,1.10,q)
 z=z*(1-cut)+(-1650+60*math.sin(u*60)*math.sin(v*70))*cut
 return z

def lower_height(z):
 return 65+(z-65)*(.25 if z>=65 else .65)

def height(u,v):return lower_height(original_height(u,v))
def world(u,v,offset=0):return (*world_xy(u,v),height(u,v)+offset)
def protected(u,v,pad=0):
 return center_distance(u,v)<3450+pad or road_distance(u,v)<1000+pad or exit_distance(u,v)<950+pad or any(math.dist(xy(u,v),xy(*c))<950+pad for c in CAMPS) or ravine_q(u,v)<1.05
def generate(folder):
 folder.mkdir(parents=True,exist_ok=True);nx,ny=220,200;vs=[];fs=[]
 for j in range(ny+1):
  for i in range(nx+1):
   u,v=-.075+1.15*i/nx,-.075+1.15*j/ny;vs.append((*xy(u,v),height(u,v)))
 for j in range(ny):
  for i in range(nx):
   a=j*(nx+1)+i;b=a+1;c=a+nx+1;d=c+1;fs.extend([(a,b,d),(a,d,c)])
 write_mesh(folder/'SM_AmbushBasin.obj',vs,fs)
if __name__=='__main__':generate(Path(__file__).resolve().parents[2]/'ContentSource/Ambush')
