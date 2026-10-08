"""Connected water surface, bridge decks and stone arches for the final stage."""
import math
import capital_final_layout as L

def bridge_point(b,s,t,z):
 a,c=b['axis'];x,y=b['center'];return (x+s*a-t*c,y+s*c+t*a,z)

def generate(folder):
 L.generate(folder)
 nx,ny=280,220
 samples={}
 for j in range(ny+1):
  for i in range(nx+1):
   u,v=-.1+1.2*i/nx,-.1+1.2*j/ny;samples[(i,j)]=(*L.xy(u,v),L.water_distance(u,v))
 verts=[];faces=[]
 for j in range(ny):
  for i in range(nx):
   corners=[samples[(i,j)],samples[(i+1,j)],samples[(i+1,j+1)],samples[(i,j+1)]]
   for inds in [(0,1,2),(0,2,3)]:
    triangle=[corners[k] for k in inds];poly=[]
    for a,b in zip(triangle,triangle[1:]+triangle[:1]):
     if a[2]<=0:poly.append(a[:2])
     if (a[2]<=0)!=(b[2]<=0):
      t=a[2]/(a[2]-b[2]);poly.append((a[0]+t*(b[0]-a[0]),a[1]+t*(b[1]-a[1])))
    if len(poly)<3:continue
    start=len(verts);verts.extend([(x,y,L.WATER_Z) for x,y in poly])
    for k in range(1,len(poly)-1):faces.append((start,start+k,start+k+1))
 L.write_mesh(folder/'SM_FinalConnectedWater.obj',verts,faces)
 L.write_mesh(folder/'SM_FinalWaterSafety.obj',[(x,y,0) for x,y,z in verts],faces)
 for idx,b in enumerate(L.BRIDGES):
  vs=[];fs=[];n=28
  for j in range(n+1):
   s=-b['half_length']+2*b['half_length']*j/n;z=L.bridge_z(s,b)
   vs.extend([bridge_point(b,s,-b['half_width'],z),bridge_point(b,s,b['half_width'],z),bridge_point(b,s,-b['half_width'],z-65),bridge_point(b,s,b['half_width'],z-65)])
  for j in range(n):
   a=j*4;c=a+4;fs.extend([(a,c,c+1),(a,c+1,a+1),(a+2,a+3,c+3),(a+2,c+3,c+2),(a,a+2,c+2),(a,c+2,c),(a+1,c+1,c+3),(a+1,c+3,a+3)])
  fs.extend([(0,1,3),(0,3,2),(4*n,4*n+2,4*n+3),(4*n,4*n+3,4*n+1)])
  L.write_mesh(folder/('SM_FinalBridgeDeck%d.obj'%idx),vs,fs)
  if b['style']=='stone':
   vs=[];fs=[]
   for side in (-1,1):
    start=len(vs)
    for j in range(n+1):
     s=-b['half_length']+2*b['half_length']*j/n;upper=L.bridge_z(s,b)-55;lower=-330+490*max(0,math.cos(math.pi*s/(2*b['half_length'])))
     for t,z in [(side*600,upper),(side*600,lower),(side*740,upper),(side*740,lower)]:vs.append(bridge_point(b,s,t,z))
    for j in range(n):
     a=start+j*4;c=a+4
     fs.extend([(a,c,c+1),(a,c+1,a+1),(a+2,a+3,c+3),(a+2,c+3,c+2),(a,a+2,c+2),(a,c+2,c),(a+1,c+1,c+3),(a+1,c+3,a+3)])
   L.write_mesh(folder/('SM_FinalBridgeArch%d.obj'%idx),vs,fs)

def water_material_code():
 code='float2 uv=float2(P.y/28000.+.5,.5-P.x/22000.);float wet=1e8;float2 dxy;float a;\n'
 for k,(u,v,rx,ry) in enumerate(L.LAKES):
  code+='dxy=(uv-float2(%f,%f))/float2(%f,%f);a=atan2(dxy.y,dxy.x);wet=min(wet,(length(dxy)/(1+.065*sin(3*a+%d)+.045*sin(5*a+1))-1)*%f);\n'%(u,v,rx,ry,k,min(rx*L.W,ry*L.H))
 code+='float river=1e8;'
 for a,b in L.CHANNEL_SEGMENTS:
  dx,dy=b[0]-a[0],b[1]-a[1]
  code+='river=min(river,length(p-(float2(%f,%f)+clamp(dot(p-float2(%f,%f),float2(%f,%f))/%f,0,1)*float2(%f,%f))));\n'%(a[0],a[1],a[0],a[1],dx,dy,dx*dx+dy*dy,dx,dy)
 code+='wet=min(wet,river-660.-45*sin(uv.x*83+uv.y*27)-25*sin(uv.y*103-uv.x*46));'
 code+='float damp=1-smoothstep(-120,360,wet+(patch-.5)*90);float3 earth=D*float3(.43,.32,.20);color=lerp(color,earth,damp);'
 return code

if __name__=='__main__':
 from pathlib import Path
 generate(Path(__file__).resolve().parents[2]/'ContentSource/CapitalFinal')
