"""Shared S-route geometry for painted paths, lane spline and navigation QA."""
import math

ROWS = [(8500,600,-14000,10500),(3500,-350,-10500,10500),
        (-2000,480,-10500,10500),(-7500,-550,-14000,10500)]

def row_y(index,x):
    y,amplitude,lo,hi=ROWS[index]
    t=max(0,min(1,(x-lo)/(hi-lo)))
    return y+amplitude*math.sin(2*math.pi*t)*math.sin(math.pi*t)**2

def route_sections():
    sections=[]
    for i,(y,amp,lo,hi) in enumerate(ROWS):
        n=math.ceil((hi-lo)/650)
        xs=[lo+(hi-lo)*j/n for j in range(n+1)]
        if i%2: xs.reverse()
        sections.append([(x,row_y(i,x)) for x in xs])
        if i<3:
            cy=(y+ROWS[i+1][0])/2
            r=(y-ROWS[i+1][0])/2
            side=1 if i%2==0 else -1
            sections.append([(side*10500+side*r*math.sin(math.pi*j/18),
                              cy+r*math.cos(math.pi*j/18)) for j in range(19)])
    return sections

SECTIONS=route_sections()
ROUTE=[p for i,section in enumerate(SECTIONS) for p in (section if i==0 else section[1:])]

def divider_y(index,x):
    lo,hi=(-18250,10500) if index%2==0 else (-10500,18250)
    t=max(0,min(1,(x-lo)/(hi-lo)))
    return [6000,750,-4750][index]+430*math.sin(2*math.pi*t)*math.sin(math.pi*t)**2

def distance_to_route(x,y):
    best=float('inf')
    for a,b in zip(ROUTE,ROUTE[1:]):
        dx,dy=b[0]-a[0],b[1]-a[1]
        t=max(0,min(1,((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy)))
        best=min(best,math.hypot(x-a[0]-t*dx,y-a[1]-t*dy))
    return best

ROUTE_HLSL='''
float d=1e8;
float4 rows[4]={float4(8500,600,-14000,10500),float4(3500,-350,-10500,10500),float4(-2000,480,-10500,10500),float4(-7500,-550,-14000,10500)};
[unroll] for(int i=0;i<4;i++){
 float x=clamp(P.x,rows[i].z,rows[i].w);
 float t=(x-rows[i].z)/(rows[i].w-rows[i].z);
 float y=rows[i].x+rows[i].y*sin(6.2831853*t)*pow(sin(3.14159265*t),2);
 d=min(d,length(P.xy-float2(x,y)));
}
float3 arcs[3]={float3(10500,6000,2500),float3(-10500,750,2750),float3(10500,-4750,2750)};
[unroll] for(int i=0;i<3;i++){
 float2 q=P.xy-arcs[i].xy;
 if((i==1 && q.x<=0)||(i!=1 && q.x>=0)) d=min(d,abs(length(q)-arcs[i].z));
}
'''
