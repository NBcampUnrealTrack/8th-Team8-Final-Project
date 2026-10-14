"""Dress Hero with a smooth S-route, curved boundaries and a layered forest floor.

Run in an Unreal Python commandlet, then build_stage_hero_nav.py in full Editor.
Only Scenery-tagged dressing is replaced on reruns. Buildings/resources are omitted.
"""
import json
import math
import random
import re
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).parent))
from hero_curve_layout import ROUTE, SECTIONS, ROUTE_HLSL, divider_y, distance_to_route

MAP = '/Game/Game/Stage/PT_Stage_Hero'
TAG = unreal.Name('SF_StageHeroScenery')
ROOT = '/Game/Game/Stage/Props/asset/'
rng = random.Random(93026)
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if any(unreal.Name('SF_HeroScaled09') in a.tags for a in sub.get_all_level_actors()):
    raise RuntimeError('Hero has an organic terrain layout. Edit the current map in Unreal Editor; legacy dressing would restore the old corridors.')
saved = Path(unreal.Paths.project_saved_dir())
assets = json.loads(Path(__file__).with_name('stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'))
assets['Flower_bush_white']=dict(path=ROOT+'static_mesh/Flower_bush_white',materials=[ROOT+'texture_material/AllColors_Material'])

def terrain_snapshot():
    result = {}
    for a in sub.get_all_level_actors():
        n = a.get_actor_label()
        if n.startswith(('Hero_Ground_', 'Hero_Road_', 'Hero_Divider_', 'Hero_Border_', 'Hero_Cliff_')):
            t = a.get_actor_transform()
            result[n] = re.sub(r'0x[0-9A-Fa-f]+', 'ADDRESS', str(t))
    return result

before = terrain_snapshot()
baseline = saved/'hero_terrain_before_dressing.json'
if not baseline.exists():
    baseline.write_text(json.dumps(before,indent=2))
removed = []
for a in sub.get_all_level_actors():
    n = a.get_actor_label()
    if TAG in a.tags or n.startswith(('Hero_Resource_', 'Hero_Human_', 'Hero_Beast_')):
        removed.append(n)
        sub.destroy_actor(a)

# A world-space blend keeps the grass and worn earth continuous across existing
# floor tiles. It changes only their finish, never their shape or placement.
art = '/Game/Game/Stage/StageHeroArt'
unreal.EditorAssetLibrary.make_directory(art)
mat = unreal.load_asset(art+'/M_HeroForestFloor')
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_HeroForestFloor',art,unreal.Material,unreal.MaterialFactoryNew())
ml = unreal.MaterialEditingLibrary
ml.delete_all_material_expressions(mat)
def node(cls, **props):
    n = ml.create_material_expression(mat, cls)
    for k,v in props.items():
        n.set_editor_property(k,v)
    return n
pos = node(unreal.MaterialExpressionWorldPosition)
def custom_input(name):
    result=unreal.CustomInput()
    result.set_editor_property('input_name',name)
    return result
uv = node(unreal.MaterialExpressionCustom, code='return P.xy / 950.0;', output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2,
          inputs=[custom_input('P')])
ml.connect_material_expressions(pos,'',uv,'P')
grass = node(unreal.MaterialExpressionTextureSample, texture=unreal.load_asset('/Game/Game/Stage/Props/texture/greengrass/Grass005_1K-PNG_Color'))
mud = node(unreal.MaterialExpressionTextureSample, texture=unreal.load_asset('/Game/Game/Stage/Props/texture/brownmud/brown_mud_leaves_01_diff_1k'))
for n in (grass,mud):
    ml.connect_material_expressions(uv,'',n,'UVs')
noise_code = '''
struct ForestNoise {
 float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
 float value(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);}
};
ForestNoise n;
'''
mask_code = noise_code + ROUTE_HLSL + '''
float broad=n.value(P.xy/1250.0);
float patch=n.value(P.xy/390.0);
float grain=n.value(P.xy/95.0);
float edge=(patch-.5)*410+(grain-.5)*120;
float track=1-smoothstep(350,1120+(broad-.5)*350,d+edge);
float wear=track*(.58+.38*patch+.08*grain);
float glades=smoothstep(.58,.82,broad)*(.10+.25*patch);
return saturate(max(wear,glades));
'''
mask = node(unreal.MaterialExpressionCustom, code=mask_code, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
            inputs=[custom_input('P')])
ml.connect_material_expressions(pos,'',mask,'P')
blend = node(unreal.MaterialExpressionLinearInterpolate)
ml.connect_material_expressions(grass,'RGB',blend,'A')
ml.connect_material_expressions(mud,'RGB',blend,'B')
ml.connect_material_expressions(mask,'',blend,'Alpha')
tone = node(unreal.MaterialExpressionCustom, code=noise_code+'''float broad=n.value(P.xy/1850.0);float small=n.value(P.xy/220.0);return C*lerp(float3(.79,.86,.68),float3(1.13,1.08,.93),broad)*(.94+.12*small);''',
            output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,inputs=[custom_input('P'),custom_input('C')])
ml.connect_material_expressions(pos,'',tone,'P')
ml.connect_material_expressions(blend,'',tone,'C')
ml.connect_material_property(tone,'',unreal.MaterialProperty.MP_BASE_COLOR)
normal=node(unreal.MaterialExpressionTextureSample,texture=unreal.load_asset('/Game/Game/Stage/Props/texture/greengrass/Grass005_1K-PNG_NormalDX'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
ml.connect_material_expressions(uv,'',normal,'UVs')
normal_strength=node(unreal.MaterialExpressionCustom,code='return normalize(float3(N.xy*.5,N.z));',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,inputs=[custom_input('N')])
ml.connect_material_expressions(normal,'RGB',normal_strength,'N')
ml.connect_material_property(normal_strength,'',unreal.MaterialProperty.MP_NORMAL)
rough = node(unreal.MaterialExpressionConstant, r=.92)
ml.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
ml.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
for a in sub.get_all_level_actors():
    n=a.get_actor_label()
    if n.startswith(('Hero_Ground_', 'Hero_Road_')):
        a.static_mesh_component.set_material(0,mat)
        if n.startswith('Hero_Road_'):
            # Ground now carries the same world-space route. Legacy road cubes
            # have pitch rotations at turns, so leave their transforms intact
            # but retire their rendering/collision instead of leaving upright slabs.
            a.static_mesh_component.set_visibility(False)
            a.static_mesh_component.set_hidden_in_game(True)
            a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    elif n.startswith(('Hero_Divider_', 'Hero_Border_', 'Hero_Cliff_')):
        # Keep the original blocking geometry; dress its outline with real rocks
        # and walls instead of exposing stretched atlas textures on cubes.
        a.static_mesh_component.set_visibility(False)
        a.static_mesh_component.set_hidden_in_game(True)
        if n.startswith('Hero_Divider_'):
            a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    elif n=='Hero_Sun':
        a.set_actor_rotation(unreal.Rotator(pitch=-55,yaw=-35,roll=0),False)
        a.light_component.set_editor_property('atmosphere_sun_light',True)
    elif n=='Hero_SkyLight':
        a.light_component.set_editor_property('real_time_capture',True)
        a.light_component.set_editor_property('intensity',1.2)
    elif n=='Hero_PlayerRoute':
        spline=a.get_editor_property('spline')
        spline.clear_spline_points(False)
        for x,y in ROUTE:
            spline.add_spline_point(unreal.Vector(x,y,0),unreal.SplineCoordinateSpace.WORLD,False)
        for i in range(len(ROUTE)):
            spline.set_spline_point_type(i,unreal.SplinePointType.CURVE,False)
        spline.update_spline()
    elif n.startswith('Hero_Enemy_'):
        # Keep each encounter in its original section, on the new centerline.
        encounter=int(n.split('_')[2])-1
        idx=int(n.rsplit('_',1)[1])-1
        section=SECTIONS[encounter]
        k=int((len(section)-1)*(.70 if encounter%2==0 else .55))
        x,y=section[k]
        dx,dy=[(-180,-150),(180,150),(0,0)][idx]
        a.set_actor_location(unreal.Vector(x+dx,y+dy,92),False,False)

cache = {}
counts = {}
def place(name,x,y,width,height=None,yaw=None,group='Rocks',z=-8):
    if name not in cache:
        info=assets[name]
        mesh=unreal.load_asset(info['path'])
        assert mesh, name
        cache[name]=(mesh,mesh.get_bounds(),[unreal.load_asset(p) if p else None for p in info['materials']])
    mesh,b,mats=cache[name]
    yaw=rng.uniform(0,360) if yaw is None else yaw
    sx=width/(b.box_extent.x*2)
    sy=width/(b.box_extent.y*2)
    sz=height/(b.box_extent.z*2) if height else (sx+sy)/2
    c,s=math.cos(math.radians(yaw)),math.sin(math.radians(yaw))
    ox,oy=b.origin.x*sx,b.origin.y*sy
    location=unreal.Vector(x-c*ox+s*oy,y-s*ox-c*oy,z-(b.origin.z-b.box_extent.z)*sz)
    a=sub.spawn_actor_from_class(unreal.StaticMeshActor,location,unreal.Rotator(pitch=0,yaw=yaw,roll=0))
    counts[group]=counts.get(group,0)+1
    a.set_actor_label('Hero_Dress_%s_%04d'%(group,counts[group]))
    a.set_folder_path('Stage Hero/Scenery/'+group)
    a.set_editor_property('tags',[TAG])
    a.static_mesh_component.set_static_mesh(mesh)
    for i,m in enumerate(mats):
        if m:a.static_mesh_component.set_material(i,m)
    a.set_actor_scale3d(unreal.Vector(sx,sy,sz))
    # Dressing does not alter the approved navigable footprint.
    a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return a

trees=['SM_OakAdultD','SM_GenericTreeD','SM_GenericTreeB','SM_PineVariantAGrowingB','SM_BirchTreeAdultA']
rocks=['SM_Rocks_01','SM_Rocks_02','SM_Rocks_03','SM_Rocks_05','SM_Rocks_07','SM_Rocks_09']
def tree(x,y,small=False):
    name=rng.choices(trees,[37,30,12,16,5])[0]
    w=rng.uniform(720,1250) if not small else rng.uniform(430,730)
    h=rng.uniform(1400,2200) if not small else rng.uniform(850,1250)
    if 'Pine' in name or 'Birch' in name:w*=.6
    place(name,x,y,w,h,group='Woodland')
def rock(x,y,large=True):
    place(rng.choice(rocks),x,y,rng.uniform(380,740) if large else rng.uniform(100,280),
          rng.uniform(380,790) if large else rng.uniform(70,190),group='Boulders')
def bush(x,y):
    place('Bush_average',x,y,rng.uniform(140,330),rng.uniform(130,290),group='Undergrowth')
def tufts(x,y):
    for _ in range(rng.randint(4,7)):
        place(rng.choice(['GrassClump01','GrassClump02','SM_GrassLumpD']),x+rng.uniform(-270,270),y+rng.uniform(-240,240),
              rng.uniform(110,230),rng.uniform(65,140),group='Groundcover',z=-3)

# Deep irregular perimeter canopy, with smaller trees at the meadow edge.
for side in (-1,1):
    for depth in range(3):
        for x in range(-18000,18001,710):
            tree(x+rng.uniform(-230,230),side*(11200+depth*770)+rng.uniform(-180,180))
    for depth in range(3):
        for y in range(-10400,10401,710):
            tree(side*(16600+depth*730)+rng.uniform(-180,180),y+rng.uniform(-230,230))

# Stone boundary backed by woodland; gaps and rubble avoid a repeating fence.
def stone_edge(start,end,axis='x',fixed=0,divider=False):
    p=start
    while p<end:
        q=fixed+rng.uniform(-95,95)
        x,y=(p,q) if axis=='x' else (q,p)
        if rng.random()<.48:
            # Wall mesh runs along local Y, with varied broken tops.
            target=rng.uniform(740,1080)
            place_wall(x,y,target,rng.uniform(490,750),90 if axis=='x' else 0)
        else:
            rock(x,y)
        if rng.random()<.85:
            bush(x+rng.uniform(-270,270),y+rng.uniform(-350,350))
        tufts(x+rng.uniform(-330,330),y+rng.uniform(-360,360))
        p+=rng.uniform(540,790)

def place_wall(x,y,length,height,yaw):
    mesh,b,mats=cache.get('SM_RuinedWallA',(None,None,None))
    if mesh is None:
        tmp=place('SM_RuinedWallA',x,y,300,height,yaw,group='RuinedWalls')
        sub.destroy_actor(tmp)
        mesh,b,mats=cache['SM_RuinedWallA']
    sx=rng.uniform(230,320)/(2*b.box_extent.x)
    sy=length/(2*b.box_extent.y)
    sz=height/(2*b.box_extent.z)
    yaw+=rng.uniform(-5,5)
    c,s=math.cos(math.radians(yaw)),math.sin(math.radians(yaw))
    ox,oy=b.origin.x*sx,b.origin.y*sy
    a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x-c*ox+s*oy,y-s*ox-c*oy,-10-(b.origin.z-b.box_extent.z)*sz),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
    counts['RuinedWalls']=counts.get('RuinedWalls',0)+1
    a.set_actor_label('Hero_Dress_RuinedWalls_%04d'%counts['RuinedWalls'])
    a.set_folder_path('Stage Hero/Scenery/RuinedWalls')
    a.set_editor_property('tags',[TAG])
    a.static_mesh_component.set_static_mesh(mesh)
    a.static_mesh_component.set_material(0,mats[0])
    a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    a.set_actor_scale3d(unreal.Vector(sx,sy,sz))
    return a

for side in (-1,1):
    stone_edge(-16600,16700,'x',side*10900)
    stone_edge(-10500,10600,'y',side*16000)
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
def blocking_segment(a,b):
    dx,dy=b[0]-a[0],b[1]-a[1]
    actor=sub.spawn_actor_from_class(unreal.StaticMeshActor,
        unreal.Vector((a[0]+b[0])/2,(a[1]+b[1])/2,450),
        unreal.Rotator(pitch=0,yaw=math.degrees(math.atan2(dy,dx)),roll=0))
    counts['CurveCollision']=counts.get('CurveCollision',0)+1
    actor.set_actor_label('Hero_Dress_CurveCollision_%03d'%counts['CurveCollision'])
    actor.set_folder_path('Stage Hero/Scenery/CurveCollision')
    actor.set_editor_property('tags',[TAG])
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector((math.hypot(dx,dy)+40)/100,12.0,9.0))
    actor.static_mesh_component.set_visibility(False)
    actor.static_mesh_component.set_hidden_in_game(True)

# Curving wooded islands: two irregular rocky edges and a semicircular nose.
for i in range(3):
    lo,hi=(-18400,10500) if i%2==0 else (-10500,18400)
    n=math.ceil((hi-lo)/560)
    centers=[(lo+(hi-lo)*j/n,divider_y(i,lo+(hi-lo)*j/n)) for j in range(n+1)]
    for a,b in zip(centers,centers[1:]):blocking_segment(a,b)
    for j,(x,y) in enumerate(centers):
        slope=(divider_y(i,x+20)-divider_y(i,x-20))/40
        angle=math.atan(slope)
        halfwidth=660+90*math.sin(x*.0011+i)
        for side in (-1,1):
            px=x-side*math.sin(angle)*halfwidth+rng.uniform(-65,65)
            py=y+side*math.cos(angle)*halfwidth+rng.uniform(-65,65)
            if rng.random()<.43:
                place_wall(px,py,rng.uniform(520,800),rng.uniform(340,540),math.degrees(angle)-90)
            else:
                place(rng.choice(rocks),px,py,rng.uniform(400,720),rng.uniform(300,560),group='Boulders')
            if j%2==0:tufts(px,py+side*180)
            if rng.random()<.6:bush(px,py)
        if j%2==0:
            tree(x+rng.uniform(-120,120),y+rng.uniform(-180,180),True)
            bush(x+200,y-200)
    end=hi if i%2==0 else lo
    cy=divider_y(i,end)
    side=1 if i%2==0 else -1
    for j in range(9):
        a=-math.pi/2+j*math.pi/8
        px=end+side*math.cos(a)*650
        py=cy+math.sin(a)*650
        place(rng.choice(rocks),px,py,rng.uniform(410,620),rng.uniform(320,530),group='Boulders')
        tufts(px+side*130,py)
    # A short final block rounds the effective turning boundary inside the rocks.
    blocking_segment((end,cy),(end+side*380,cy))

# Foliage follows both curved shoulders, with patches of earth between clumps.
for j in range(1,len(ROUTE)-1,2):
    x,y=ROUTE[j]
    dx,dy=ROUTE[j+1][0]-ROUTE[j-1][0],ROUTE[j+1][1]-ROUTE[j-1][1]
    length=math.hypot(dx,dy)
    for side in (-1,1):
        offset=side*rng.uniform(1080,1400)
        px,py=x-dy/length*offset,y+dx/length*offset
        if not (-15100<px<15100 and -10200<py<10200):continue
        tufts(px,py)
        if rng.random()<.35:bush(px,py)
        for _ in range(rng.randint(1,3)):
            place(rng.choice(rocks),px+rng.uniform(-240,240),py+rng.uniform(-220,220),rng.uniform(60,150),rng.uniform(35,95),group='TrailGravel')
        if rng.random()<.25:
            place('Flower_bush_white',px+100,py-100,rng.uniform(90,170),rng.uniform(55,90),group='Wildflowers')

# Loose meadow patches break up the formerly uniform green between the route
# and the woodland, while the start/end clearings retain broad open ground.
for _ in range(150):
    x,y=rng.uniform(-15200,15200),rng.uniform(-10300,10300)
    if distance_to_route(x,y)<1050:continue
    if any(abs(y-divider_y(i,x))<1100 for i in range(3)):continue
    tufts(x,y)
    if rng.random()<.3:
        place('Flower_bush_white',x+150,y,160,85,group='Wildflowers')

assert terrain_snapshot()==before,'Terrain transforms changed'
original={k:re.sub(r'0x[0-9A-Fa-f]+','ADDRESS',v) for k,v in json.loads(baseline.read_text()).items()}
assert before==original,'Original terrain changed'
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
actual={}
for a in sub.get_all_level_actors():
    if TAG in a.tags:
        group=str(a.get_folder_path()).split('/')[-1]
        actual[group]=actual.get(group,0)+1
report=dict(terrain_preserved=len(before),scenery=actual,removed=removed,
            buildings=0,resources=0)
(saved/'hero_dressing_report.json').write_text(json.dumps(report,indent=2))
