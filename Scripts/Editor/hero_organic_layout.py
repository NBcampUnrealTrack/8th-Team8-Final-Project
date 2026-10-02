"""Organic Hero route in the 0.9-scale play area; shared paint/mesh/prop coordinates."""
import math

# Seven encounter sections remain in order. Unequal Bezier handles replace the
# three identical semicircles and straight, parallel runs.
CURVES=[
 [(-12600,7650),(-8300,9200),(-1500,6100),(9000,7750)],
 [(9000,7750),(13600,8475),(13700,1870),(9100,3250)],
 [(9100,3250),(3500,4930),(-2800,1650),(-9700,3150)],
 [(-9700,3150),(-14300,4150),(-13900,-2020),(-9650,-1650)],
 [(-9650,-1650),(-3300,-1100),(2500,-370),(9850,-1850)],
 [(9850,-1850),(14200,-2730),(14000,-7840),(9750,-6920)],
 [(9750,-6920),(3200,-5510),(-5500,-9150),(-12400,-7250)],
]

def bezier(c,t):
 u=1-t
 return tuple(u*u*u*c[0][k]+3*u*u*t*c[1][k]+3*u*t*t*c[2][k]+t*t*t*c[3][k] for k in range(2))

SECTIONS=[[bezier(c,j/(48 if i%2==0 else 24)) for j in range((48 if i%2==0 else 24)+1)] for i,c in enumerate(CURVES)]
ROUTE=[p for i,s in enumerate(SECTIONS) for p in (s if i==0 else s[1:])]

def row_y(i,x):
 c=CURVES[i*2];lo,hi=0.,1.;ascending=c[3][0]>c[0][0]
 x=max(min(c[0][0],c[3][0]),min(max(c[0][0],c[3][0]),x))
 for _ in range(22):
  m=(lo+hi)/2
  if (bezier(c,m)[0]<x)==ascending:lo=m
  else:hi=m
 return bezier(c,(lo+hi)/2)[1]

def divider_y(i,x):return (row_y(i,x)+row_y(i+1,x))*.5+110*math.sin(x/1630+i*1.9)
DIVIDERS=[(-17400,8800),(-9350,17400),(-17400,9400)]
# Two intentional, walkable side cuts. All stones, plants and blockers respect them.
GAPS={0:[(-4800,820)],1:[],2:[(3600,920)]}
def in_gap(i,x,pad=0):return any(abs(x-c)<r+pad for c,r in GAPS[i])
def island_half_width(i,x):return 1020+220*math.sin(x/1930+i*1.7)+120*math.sin(x/870+i)

# Compact woodland springs: center, elliptical radii, yaw, shoreline phase.
SPRINGS=[(-14900,11400,1660*math.sqrt(2),1240*math.sqrt(2),20,.6),(15500,-9150,1520*math.sqrt(2),1140*math.sqrt(2),-28,1.9)]
SPRING_STRETCH_Y=1.30
WATER_Z=[-92.,-76.]

def spring_radius(angle,phase):return 1+.12*math.sin(3*angle+phase)+.07*math.sin(5*angle+phase*1.3)

def spring_point(i,angle,offset=0):
 cx,cy,rx,ry,yaw,phase=SPRINGS[i];q=math.radians(yaw)
 r=spring_radius(angle,phase);x=(rx*r+offset)*math.cos(angle);y=(ry*r+offset)*math.sin(angle)
 return cx+x*math.cos(q)-y*math.sin(q),cy+(x*math.sin(q)+y*math.cos(q))*SPRING_STRETCH_Y

def spring_distance(x,y,i):
 cx,cy,rx,ry,yaw,phase=SPRINGS[i];q=math.radians(yaw);dx=x-cx;dy=(y-cy)/SPRING_STRETCH_Y
 px=(dx*math.cos(q)+dy*math.sin(q))/rx;py=(-dx*math.sin(q)+dy*math.cos(q))/ry
 return (math.hypot(px,py)-spring_radius(math.atan2(py,px),phase))*min(rx,ry)

def spring_hlsl():
 code='float waterD=1e8;float2 pondP;float pondAngle;\n'
 for cx,cy,rx,ry,yaw,phase in SPRINGS:
  q=math.radians(yaw);c=math.cos(q);s=math.sin(q)
  code+=f'pondP=P.xy-float2({cx},{cy});pondP.y/={SPRING_STRETCH_Y};pondP=float2(dot(pondP,float2({c:.8f},{s:.8f}))/{rx},dot(pondP,float2({-s:.8f},{c:.8f}))/{ry});pondAngle=atan2(pondP.y,pondP.x);waterD=min(waterD,(length(pondP)-(1+.12*sin(3*pondAngle+{phase})+.07*sin(5*pondAngle+{phase*1.3})))*{min(rx,ry)});\n'
 return code

def segment_distance(x,y,a,b):
 dx,dy=b[0]-a[0],b[1]-a[1];q=dx*dx+dy*dy
 t=max(0,min(1,((x-a[0])*dx+(y-a[1])*dy)/q)) if q else 0
 return math.hypot(x-a[0]-t*dx,y-a[1]-t*dy)
def distance_to_line(x,y,line):return min(segment_distance(x,y,a,b) for a,b in zip(line,line[1:]))
def distance_to_route(x,y):return min(distance_to_line(x,y,s) for s in SECTIONS)
def smooth(a,b,v):
 t=max(0,min(1,(v-a)/(b-a)));return t*t*(3-2*t)

def height(x,y):
 z=-5+90*math.sin(x/4100)*math.cos(y/3150)+55*math.sin((x+y)/2650)
 # Flat bases blend into low swells, without making ramps under user buildings.
 for px,py in [(-13320,7650),(-13050,-6750)]:
  z*=smooth(850,1950,math.hypot(x-px,y-py))
 for i,(cx,cy,rx,ry,_,_) in enumerate(SPRINGS):
  if abs(x-cx)<rx*1.3+650 and abs(y-cy)<rx*1.3+650:
   blend=smooth(-140,400,spring_distance(x,y,i))
   z=z*blend+(WATER_Z[i]-85)*(1-blend)
 return z

def hlsl_distance(lines,variable):
 # Piecewise projection follows the same authored curves as the lane and banks.
 code=f'float {variable}=1e8;\n'
 for line in lines:
  pts=line[::3]+([line[-1]] if line[-1]!=line[::3][-1] else [])
  for a,b in zip(pts,pts[1:]):
   code+=f'a=float2({a[0]:.3f},{a[1]:.3f});b=float2({b[0]:.3f},{b[1]:.3f});v=b-a;t=saturate(dot(P.xy-a,v)/dot(v,v));{variable}=min({variable},length(P.xy-a-t*v));\n'
 return code

SIDE_PATHS=[[(cx,divider_y(i,cx)+2000),(cx-120,divider_y(i,cx)),(cx+150,divider_y(i,cx)-2000)] for i,gaps in GAPS.items() for cx,r in gaps]
ROUTE_HLSL='float2 a,b,v;float t;\n'+hlsl_distance(SECTIONS,'d')+hlsl_distance(SIDE_PATHS,'sideD')
