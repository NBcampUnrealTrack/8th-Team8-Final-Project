"""Final four-lane terrain plan. Units are Unreal centimetres.

Image east is world +Y and image north is world +X, matching SFMinimap.
Only this module needs editing to change dimensions or blocker footprints.
"""
import math
from pathlib import Path

WIDTH = 36000.0
HEIGHT = 28800.0
LANE_WIDTH = 2200.0
MAP = '/Game/Game/Stage/Stage_Final'
ART = '/Game/Game/Stage/StageFinalArt'

def world(uv, z=0.0):
    return ((0.5-uv[1])*HEIGHT, (uv[0]-0.5)*WIDTH, z)

def soften(points, passes=2):
    for _ in range(passes):
        points = [p for a,b in zip(points, points[1:]+points[:1])
                  for p in ((.75*a[0]+.25*b[0], .75*a[1]+.25*b[1]),
                            (.25*a[0]+.75*b[0], .25*a[1]+.75*b[1]))]
    return points

OUTLINE = soften([
    (.018,.075),(.07,.015),(.255,.012),(.32,.06),(.78,.06),
    (.90,.025),(.98,.04),(.995,.185),(.975,.27),(.99,.42),
    (.965,.53),(.99,.68),(.985,.975),(.765,.99),(.71,.945),
    (.30,.96),(.15,.975),(.015,.97),(.025,.77),(.025,.30),
])
BLOCKERS = {
    'Northwest_Island': soften([(.22,.245),(.365,.24),(.405,.275),(.408,.45),(.38,.485),(.23,.475),(.195,.42),(.194,.30)]),
    'North_Central_Island': soften([(.52,.238),(.70,.24),(.73,.285),(.735,.38),(.70,.475),(.64,.488),(.57,.445),(.515,.395),(.49,.30)]),
    'Northeast_Boundary': soften([(.86,.24),(1.02,.225),(1.02,.43),(.91,.425),(.86,.39)]),
    'Southwest_Island': soften([(.21,.61),(.35,.605),(.50,.63),(.595,.66),(.60,.70),(.55,.775),(.22,.77),(.185,.72),(.188,.635)]),
    'South_Boundary': soften([(.12,.925),(.58,.93),(.735,.925),(.78,1.04),(.05,1.04)]),
    'East_Boundary': soften([(.955,.54),(1.04,.51),(1.04,.69),(.975,.67),(.94,.615)]),
}

# Lines deliberately stop in front of the nexuses; bases and resource reserves
# remain buildable instead of inheriting LanePath's no-building corridor.
LANES_UV = [
    [(.245,.20),(.30,.145),(.43,.145),(.455,.205),(.455,.31),(.49,.435),(.545,.52),(.72,.54),(.82,.65),(.82,.76)],
    [(.245,.20),(.30,.145),(.43,.145),(.62,.145),(.775,.145),(.79,.23),(.79,.38),(.815,.485),(.835,.61),(.82,.76)],
    [(.145,.25),(.105,.33),(.105,.47),(.145,.535),(.32,.535),(.53,.535),(.72,.56),(.80,.66),(.82,.76)],
    [(.145,.25),(.105,.33),(.105,.535),(.105,.75),(.12,.83),(.30,.845),(.50,.845),(.685,.845),(.74,.79),(.79,.76),(.82,.76)],
]
LANES = [[world(p)[:2] for p in lane] for lane in LANES_UV]
NEXUSES = {
    'Beast': dict(team=1, definition='/Game/Core/Primary/Buildings/Definitions/Animal/DA_AnimalNexus', xyz=(8500.,-11900.,0.), yaw=90.),
    'Etherian': dict(team=0, definition='/Game/Core/Primary/Buildings/Definitions/Human/DA_HumanNexus', xyz=(-8400.,12100.,0.), yaw=-90.),
}
RESERVES = {
    'Beast': dict(center=(11400.,-12600.,0.), size=(3600.,5400.)),
    'Etherian': dict(center=(-11400.,12600.,0.), size=(3600.,5400.)),
}

def point_inside(point, polygon):
    x,y = point
    hit = False
    for a,b in zip(polygon, polygon[1:]+polygon[:1]):
        if (a[1]>y)!=(b[1]>y) and x < (b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:
            hit = not hit
    return hit

def distance_segment(p,a,b):
    dx,dy = b[0]-a[0],b[1]-a[1]
    t = max(0., min(1.,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/(dx*dx+dy*dy)))
    return math.hypot(p[0]-a[0]-t*dx,p[1]-a[1]-t*dy)

def triangles(poly):
    """Ear clipping permits curved concave islands without collision shortcuts."""
    indices = list(range(len(poly)))
    area = sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(poly,poly[1:]+poly[:1]))
    if area < 0:
        indices.reverse()
    def cross(a,b,c):
        return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    result = []
    while len(indices)>3:
        found = False
        for k, b in enumerate(indices):
            a,c = indices[k-1],indices[(k+1)%len(indices)]
            if cross(poly[a],poly[b],poly[c]) <= 1e-7:
                continue
            if any(cross(poly[a],poly[b],poly[j])>=-1e-7 and cross(poly[b],poly[c],poly[j])>=-1e-7 and cross(poly[c],poly[a],poly[j])>=-1e-7 for j in indices if j not in (a,b,c)):
                continue
            result.append((a,b,c))
            indices.pop(k)
            found=True
            break
        if not found:
            raise ValueError('Invalid terrain polygon')
    result.append(tuple(indices))
    return result

def write_obj(path, uv_polygon, bottom, top):
    """Closed polygon prism with world-space XY and Z-up coordinates."""
    poly = [world(p)[:2] for p in uv_polygon]
    n = len(poly)
    verts = [(x,y,bottom) for x,y in poly] + [(x,y,top) for x,y in poly]
    faces=[]
    for a,b,c in triangles(poly):
        faces.extend(((c,b,a),(a+n,b+n,c+n)))
    ccw = sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(poly,poly[1:]+poly[:1]))>0
    for i in range(n):
        j=(i+1)%n
        fs=[(i,j,j+n),(i,j+n,i+n)]
        faces.extend(fs if ccw else [tuple(reversed(f)) for f in fs])
    lines=['# StrikeForge Stage_Final; centimetres, Z-up','o '+path.stem,'s off']
    # FBX's OBJ importer converts right-handed OBJ to Unreal by negating Y.
    # Preconvert the source coordinates and winding so imported world XY
    # exactly matches the layout, spline coordinates and terrain material.
    lines += ['v %.6f %.6f %.6f'%(v[0],-v[1],v[2]) for v in verts]
    lines += ['vt %.6f %.6f'%(v[0]/1600.,v[1]/1600.) for v in verts]
    lines += ['f '+' '.join('%d/%d'%(i+1,i+1) for i in reversed(face)) for face in faces]
    path.write_text('\n'.join(lines)+'\n',encoding='utf-8')

def generate_sources(directory):
    directory.mkdir(parents=True,exist_ok=True)
    write_obj(directory/'SM_Final_Ground.obj', OUTLINE,-200.,0.)
    for name,poly in BLOCKERS.items():
        write_obj(directory/('SM_Final_'+name+'.obj'),poly,-10.,850.)

if __name__ == '__main__':
    generate_sources(Path(__file__).resolve().parents[2]/'ContentSource/Stage_Final')
    print('Final map geometry sources created: 360 x 288 m, four 22 m lanes.')
