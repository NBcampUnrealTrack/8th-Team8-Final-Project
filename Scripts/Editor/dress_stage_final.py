"""Turn the final-map blockout into the project's playable stylized forest level.

Preserves terrain collision, four lanes, two nexuses and empty resource reserves.
Uses native instanced foliage for repeated meshes and editable actors for rocks.
Run with UnrealEditor-Cmd -run=pythonscript -script=<this file> -NullRHI.
"""
import json
import math
import random
import re
import shutil
import sys
from collections import defaultdict
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).parent))
import final_map_layout as layout

TAG=unreal.Name('SF_StageFinalForest')
ART=layout.ART
rng=random.Random(9302026)
saved=Path(unreal.Paths.project_saved_dir())/'StageFinal'
backup=saved/'Stage_Final.before_forest.umap'
if not backup.exists():
    shutil.copy2(Path(unreal.Paths.project_dir())/'Content/Game/Stage/Stage_Final.umap',backup)
assert unreal.EditorLoadingAndSavingUtils.load_map(layout.MAP)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.EditorLevelLibrary.get_editor_world()
actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
def transform_key(a):
    return re.sub(r'0x[0-9A-Fa-f]+','ADDRESS',str(a.get_actor_transform()))
protected={name:transform_key(a) for name,a in actors.items() if name.startswith(('Final_Impassable_','Final_Lane_','Final_Beast_','Final_Etherian_','Final_NavMesh','Final_NoWalk_'))}
for a in sub.get_all_level_actors():
    if TAG in a.tags: sub.destroy_actor(a)
catalog=json.loads(Path(__file__).with_name('stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'))
cache={}
def asset(name):
    if name not in cache:
        info=catalog[name]
        mesh=unreal.load_asset(info['path'])
        assert mesh,name
        cache[name]=(mesh,mesh.get_bounds(),[unreal.load_asset(p) if p else None for p in info['materials']])
    return cache[name]

outline=[layout.world(p)[:2] for p in layout.OUTLINE]
blocked={name:[layout.world(p)[:2] for p in poly] for name,poly in layout.BLOCKERS.items()}
segments=list(dict.fromkeys((a,b) for lane in layout.LANES for a,b in zip(lane,lane[1:])))
def route_distance(p):
    return min(layout.distance_segment(p,a,b) for a,b in segments)
def edge_distance(p,poly):
    return min(layout.distance_segment(p,a,b) for a,b in zip(poly,poly[1:]+poly[:1]))
def protected_space(p,radius):
    x,y=p
    for reserve in layout.RESERVES.values():
        cx,cy,_=reserve['center']; sx,sy=reserve['size']
        if abs(x-cx)<sx/2+radius+120 and abs(y-cy)<sy/2+radius+120: return True
    return any(math.hypot(x-n['xyz'][0],y-n['xyz'][1])<1900+radius for n in layout.NEXUSES.values())

# Natural world-space ground texture and softly worn paths, with no rectangular
# reserve markings. Both empty resource sites share the surrounding grass.
ml=unreal.MaterialEditingLibrary
mat=unreal.load_asset(ART+'/M_FinalForestFloor')
if not mat:
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_FinalForestFloor',ART,unreal.Material,unreal.MaterialFactoryNew())
ml.delete_all_material_expressions(mat)
def node(cls,**props):
    n=ml.create_material_expression(mat,cls)
    for key,value in props.items(): n.set_editor_property(key,value)
    return n
def inp(name):
    i=unreal.CustomInput(); i.set_editor_property('input_name',name); return i
pos=node(unreal.MaterialExpressionWorldPosition)
uv=node(unreal.MaterialExpressionCustom,code='return P.xy/950.0;',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2,inputs=[inp('P')])
ml.connect_material_expressions(pos,'',uv,'P')
grass=node(unreal.MaterialExpressionTextureSample,texture=unreal.load_asset('/Game/Game/Stage/Props/texture/greengrass/Grass005_1K-PNG_Color'))
earth=node(unreal.MaterialExpressionTextureSample,texture=unreal.load_asset('/Game/Game/Stage/Props/texture/brownmud/brown_mud_leaves_01_diff_1k'))
for n in (grass,earth): ml.connect_material_expressions(uv,'',n,'UVs')
noise='''
struct NaturalNoise {
 float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
 float value(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);}
}; NaturalNoise n;
'''
hlsl=['float2 p=P.xy;float d=1e8;']
for a,b in segments:
    dx,dy=b[0]-a[0],b[1]-a[1]
    hlsl.append('d=min(d,length(p-(float2(%.3f,%.3f)+clamp(dot(p-float2(%.3f,%.3f),float2(%.3f,%.3f))/%.3f,0,1)*float2(%.3f,%.3f))));'%(a[0],a[1],a[0],a[1],dx,dy,dx*dx+dy*dy,dx,dy))
mask_code=noise+'\n'.join(hlsl)+'''
float broad=n.value(p/1700.0), patch=n.value(p/340.0), grain=n.value(p/100.0);
float edge=(patch-.5)*320+(grain-.5)*90;
float track=(1-smoothstep(380,1130,d+edge))*(.54+.35*patch);
float base=0;
'''
for n in layout.NEXUSES.values():
    x,y,_=n['xyz']
    mask_code+='base=max(base,(1-smoothstep(850,2100,length(p-float2(%.1f,%.1f))))*.42);\n'%(x,y)
mask_code+='return saturate(max(max(track,base),smoothstep(.67,.86,broad)*.20));'
mask=node(unreal.MaterialExpressionCustom,code=mask_code,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,inputs=[inp('P')])
ml.connect_material_expressions(pos,'',mask,'P')
blend=node(unreal.MaterialExpressionLinearInterpolate)
ml.connect_material_expressions(grass,'RGB',blend,'A'); ml.connect_material_expressions(earth,'RGB',blend,'B'); ml.connect_material_expressions(mask,'',blend,'Alpha')
tone=node(unreal.MaterialExpressionCustom,code=noise+'float broad=n.value(P.xy/1600.0);return C*lerp(float3(.94,1.02,.87),float3(1.08,1.12,.97),broad);',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,inputs=[inp('P'),inp('C')])
ml.connect_material_expressions(pos,'',tone,'P'); ml.connect_material_expressions(blend,'',tone,'C'); ml.connect_material_property(tone,'',unreal.MaterialProperty.MP_BASE_COLOR)
normal=node(unreal.MaterialExpressionTextureSample,texture=unreal.load_asset('/Game/Game/Stage/Props/texture/greengrass/Grass005_1K-PNG_NormalDX'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
ml.connect_material_expressions(uv,'',normal,'UVs')
gentle=node(unreal.MaterialExpressionCustom,code='return normalize(float3(N.xy*.4,N.z));',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,inputs=[inp('N')])
ml.connect_material_expressions(normal,'RGB',gentle,'N'); ml.connect_material_property(gentle,'',unreal.MaterialProperty.MP_NORMAL)
rough=node(unreal.MaterialExpressionConstant,r=.92); ml.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
ml.recompile_material(mat); assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
actors['Final_Playable_Ground'].static_mesh_component.set_material(0,mat)

counts=defaultdict(int)
placements=[]
instances=defaultdict(list)
def transform(name,p,width,height,yaw=None,z=-8):
    mesh,b,mats=asset(name)
    scale=width/max(2*b.box_extent.x,2*b.box_extent.y)
    sz=height/(2*b.box_extent.z)
    yaw=rng.uniform(0,360) if yaw is None else yaw
    c,s=math.cos(math.radians(yaw)),math.sin(math.radians(yaw))
    ox,oy=b.origin.x*scale,b.origin.y*scale
    location=unreal.Vector(p[0]-c*ox+s*oy,p[1]-s*ox-c*oy,z-(b.origin.z-b.box_extent.z)*sz)
    return unreal.Transform(location=location,rotation=unreal.Rotator(pitch=0,yaw=yaw,roll=0),scale=unreal.Vector(scale,scale,sz))
def place(name,p,width,height,group,instanced=False,yaw=None,z=-8):
    radius=width*.72
    if protected_space(p,radius): return False
    if group in ('Woodland','IslandTrees','Boulders','RuinedWalls') and route_distance(p)<layout.LANE_WIDTH/2+radius+80: return False
    t=transform(name,p,width,height,yaw,z)
    if instanced:
        instances[name].append(t)
    else:
        mesh,b,mats=asset(name)
        a=sub.spawn_actor_from_class(unreal.StaticMeshActor,t.translation,t.rotation.rotator())
        a.set_actor_scale3d(t.scale3d)
        a.set_actor_label('Final_Forest_%s_%04d'%(group,counts[group]+1)); a.set_folder_path('Stage Final/Environment/'+group); a.set_editor_property('tags',[TAG])
        a.static_mesh_component.set_static_mesh(mesh)
        for i,m in enumerate(mats):
            if m: a.static_mesh_component.set_material(i,m)
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    counts[group]+=1
    placements.append(dict(group=group,xy=list(p),radius=radius))
    return True
trees=['SM_OakAdultD','SM_GenericTreeD','SM_GenericTreeB','SM_PineVariantAGrowingB','SM_BirchTreeAdultA']
rocks=['SM_Rocks_01','SM_Rocks_02','SM_Rocks_03','SM_Rocks_05','SM_Rocks_07','SM_Rocks_09']
def tree(p,group='Woodland'):
    name=rng.choices(trees,[30,30,12,22,6])[0]
    width=rng.uniform(700,1050); height=rng.uniform(1050,1800)
    if 'Pine' in name or 'Birch' in name: width*=.62
    return place(name,p,width,height,group,True)
def groundcover(p):
    for _ in range(rng.randint(2,5)):
        q=(p[0]+rng.uniform(-180,180),p[1]+rng.uniform(-180,180))
        if route_distance(q)<640: continue
        place(rng.choice(['GrassClump01','GrassClump02','SM_GrassLumpD']),q,rng.uniform(90,200),rng.uniform(55,110),'Groundcover',True,z=-3)
    if rng.random()<.36: place('Bush_average',p,rng.uniform(170,300),rng.uniform(130,240),'Undergrowth',True)

# Hide only the visual blockout surfaces; their approved collision and null
# navigation areas remain the sole barriers. Rock and forest visuals cover them.
for name in blocked:
    actors['Final_Impassable_'+name].static_mesh_component.set_visibility(False)
    actors['Final_Impassable_'+name].static_mesh_component.set_hidden_in_game(True)

# Native support ground beneath the decorative perimeter, outside playable bounds.
base=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-120))
base.set_actor_label('Final_Forest_BackgroundGround'); base.set_folder_path('Stage Final/Environment/Backdrop'); base.set_editor_property('tags',[TAG])
base.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube')); base.static_mesh_component.set_material(0,mat)
base.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
base.set_actor_scale3d(unreal.Vector((layout.HEIGHT+11000)/100,(layout.WIDTH+11000)/100,2))

# Several staggered tree rows frame the playable outline; the four corridors and
# both rear resource clearings remain completely free of trunks and canopies.
for x in range(-19400,19401,680):
    for y in range(-23400,23401,680):
        p=(x+rng.uniform(-170,170),y+rng.uniform(-170,170))
        if not layout.point_inside(p,outline) and edge_distance(p,outline)<4400:
            tree(p)
            if rng.random()<.12: groundcover(p)

for name,poly in blocked.items():
    # Natural stone perimeter, placed just inside the exact collision contour.
    for a,b in zip(poly,poly[1:]+poly[:1]):
        length=math.hypot(b[0]-a[0],b[1]-a[1])
        if length<.1: continue
        steps=max(1,math.ceil(length/540))
        for j in range(steps):
            t=(j+.5)/steps; p=(a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t)
            nx,ny=-(b[1]-a[1])/length,(b[0]-a[0])/length
            if not layout.point_inside((p[0]+nx*220,p[1]+ny*220),poly): nx,ny=-nx,-ny
            p=(p[0]+nx*260,p[1]+ny*260)
            if abs(p[0])>layout.HEIGHT/2+800 or abs(p[1])>layout.WIDTH/2+800: continue
            if rng.random()<.24:
                place('SM_RuinedWallA',p,rng.uniform(520,800),rng.uniform(430,650),'RuinedWalls',yaw=math.degrees(math.atan2(b[1]-a[1],b[0]-a[0]))-90)
            else:
                place(rng.choice(rocks),p,rng.uniform(540,800),rng.uniform(600,950),'Boulders')
            if rng.random()<.62: groundcover(p)
    # Dense varied forest fills the blocked islands instead of exposing slabs.
    xmin,xmax=min(p[0] for p in poly),max(p[0] for p in poly)
    ymin,ymax=min(p[1] for p in poly),max(p[1] for p in poly)
    for x in range(int(xmin),int(xmax),710):
        for y in range(int(ymin),int(ymax),710):
            p=(x+rng.uniform(-170,170),y+rng.uniform(-170,170))
            if not layout.point_inside(p,poly) or edge_distance(p,poly)<400: continue
            if abs(p[0])>layout.HEIGHT/2+2000 or abs(p[1])>layout.WIDTH/2+2000: continue
            tree(p,'IslandTrees')

# Low foliage and gravel emphasize both lane shoulders without covering units.
for a,b in segments:
    dx,dy=b[0]-a[0],b[1]-a[1]; length=math.hypot(dx,dy)
    for j in range(max(1,int(length/650))):
        t=(j+.5)/max(1,int(length/650))
        for side in (-1,1):
            offset=side*rng.uniform(1210,1530)
            p=(a[0]+dx*t-dy/length*offset,a[1]+dy*t+dx/length*offset)
            if not layout.point_inside(p,outline) or any(layout.point_inside(p,poly) for poly in blocked.values()): continue
            if route_distance(p)<1130: continue
            groundcover(p)
            if rng.random()<.6: place(rng.choice(rocks),p,rng.uniform(70,180),rng.uniform(40,130),'TrailGravel',True)
for _ in range(750):
    p=(rng.uniform(-layout.HEIGHT/2,layout.HEIGHT/2),rng.uniform(-layout.WIDTH/2,layout.WIDTH/2))
    if not layout.point_inside(p,outline) or any(layout.point_inside(p,poly) for poly in blocked.values()) or route_distance(p)<1450: continue
    if edge_distance(p,outline)<450: continue
    groundcover(p)

# InstancedFoliageActor batches the repeated vegetation into native HISM clusters.
foliage_dir=ART+'/Foliage'
unreal.EditorAssetLibrary.make_directory(foliage_dir)
factory_cls=next((getattr(unreal,n) for n in ('FoliageType_InstancedStaticMeshFactory','FoliageTypeFactory') if hasattr(unreal,n)),None)
assert factory_cls, 'Native foliage factory unavailable'
foliage_assets={}
for name,transforms in instances.items():
    ft=unreal.load_asset(foliage_dir+'/FT_Final_'+name)
    if ft:
        unreal.InstancedFoliageActor.remove_all_instances(world,ft)
    else:
        ft=unreal.AssetToolsHelpers.get_asset_tools().create_asset('FT_Final_'+name,foliage_dir,unreal.FoliageType_InstancedStaticMesh,factory_cls())
    assert ft,name
    mesh,b,mats=asset(name)
    ft.set_editor_property('mesh',mesh); ft.set_editor_property('override_materials',mats)
    ft.set_editor_property('collision_with_world',False)
    body=ft.get_editor_property('body_instance'); body.set_editor_property('collision_enabled',unreal.CollisionEnabled.NO_COLLISION); ft.set_editor_property('body_instance',body)
    ft.set_editor_property('custom_navigable_geometry',unreal.HasCustomNavigableGeometry.DONT_EXPORT)
    ft.set_editor_property('enable_density_scaling',name not in trees)
    ft.set_editor_property('cast_shadow',name in trees)
    ft.set_editor_property('affect_distance_field_lighting',name in trees)
    ft.set_editor_property('cull_distance',unreal.Int32Interval(min=0,max=0))
    unreal.InstancedFoliageActor.add_instances(world,ft,transforms)
    assert unreal.EditorAssetLibrary.save_loaded_asset(ft)
    foliage_assets[ft.get_path_name()]=len(transforms)

actors['Final_Sun'].set_actor_rotation(unreal.Rotator(pitch=-55,yaw=-35,roll=0),False)
actors['Final_Sun'].light_component.set_editor_property('intensity',5.)
actors['Final_Sun'].light_component.set_editor_property('atmosphere_sun_light',True)
actors['Final_SkyLight'].light_component.set_editor_property('real_time_capture',True)
actors['Final_SkyLight'].light_component.set_editor_property('intensity',1.2)

# The project disables auto-exposure, so use its engine daylight environment
# for readable ambient light and a blue sky at gameplay camera height.
sky=actors['Final_SkyLight'].light_component
sky.set_editor_property('real_time_capture',False)
sky.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
sky.set_editor_property('intensity',1.4)
sky_cls=unreal.load_class(None,'/Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere_C')
sky_actor=sub.spawn_actor_from_class(sky_cls,unreal.Vector(0,0,0))
sky_actor.set_actor_label('Final_Forest_SkySphere'); sky_actor.set_folder_path('Stage Final/Environment/Lighting'); sky_actor.set_editor_property('tags',[TAG])
sky_actor.set_editor_property('Directional light actor',actors['Final_Sun'])
sky_actor.call_method('UpdateSunDirection')
after={a.get_actor_label():transform_key(a) for a in sub.get_all_level_actors() if a.get_actor_label() in protected}
assert after==protected,'Protected gameplay placement changed'
assert all(not protected_space(tuple(p['xy']),p['radius']) for p in placements)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,layout.MAP)
report=dict(map=layout.MAP,counts=dict(counts),foliage_assets=foliage_assets,instances=sum(len(v) for v in instances.values()),protected_actors=len(protected),resource_reserves=layout.RESERVES,placements=placements)
(saved/'forest_dressing_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('FINAL_FOREST_DRESSED: '+str(dict(counts)))
