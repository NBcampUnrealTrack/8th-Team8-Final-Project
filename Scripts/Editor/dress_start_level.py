"""Dress only /Game/Core/Levels/Start using existing low-poly assets.
Owned actors use SF_StartSetDress; rerunning replaces only these additions.
Original map backup is kept in Saved/LobbyPolish/Start.before_setdress.umap.
"""
import math
import shutil
from pathlib import Path
import unreal

MAP='/Game/Core/Levels/Start'
TAG='SF_StartSetDress'
root=Path(unreal.Paths.project_dir())
backup=root/'Saved/LobbyPolish/Start.before_setdress.umap'
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists(): shutil.copy2(root/'Content/Core/Levels/Start.umap',backup)
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=sub.get_all_level_actors()
for a in actors:
    if unreal.Name(TAG) in a.tags: sub.destroy_actor(a)
actors=sub.get_all_level_actors()

def own(a,label):
    a.set_actor_label('StartCamp_'+label)
    a.set_editor_property('tags',[unreal.Name(TAG)])
    a.set_folder_path('Start Presentation/Ether Camp')
    return a

BASE='/Game/Game/Stage/Props/asset/static_mesh/'
def prop(name,mesh,at,scale=1.,yaw=0.):
    asset=unreal.load_asset(mesh if mesh.startswith('/') else BASE+mesh)
    if not asset: raise RuntimeError('Missing '+mesh)
    b=asset.get_bounding_box()
    # Many imported props have remote pivots. Position their visible base, not their origin.
    center=unreal.Vector((b.min.x+b.max.x)*.5,(b.min.y+b.max.y)*.5,b.min.z)
    r=math.radians(yaw)
    cx=(center.x*math.cos(r)-center.y*math.sin(r))*scale
    cy=(center.x*math.sin(r)+center.y*math.cos(r))*scale
    loc=unreal.Vector(at[0]-cx,at[1]-cy,at[2]-center.z*scale)
    a=own(sub.spawn_actor_from_class(unreal.StaticMeshActor,loc,unreal.Rotator(pitch=0,yaw=yaw,roll=0)),name)
    c=a.static_mesh_component
    c.set_static_mesh(asset)
    c.set_collision_profile_name('NoCollision')
    a.set_actor_scale3d(unreal.Vector(scale,scale,scale))
    return a

def light(name,at,color,intensity,radius):
    a=own(sub.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*at)),name)
    if name=='FireGlow': a.set_editor_property('tags',[unreal.Name(TAG),unreal.Name('SF_StartFireLight')])
    c=a.get_component_by_class(unreal.PointLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units',unreal.LightUnits.UNITLESS)
    c.set_intensity(intensity)
    c.set_light_color(unreal.LinearColor(*color))
    c.set_attenuation_radius(radius)
    c.set_cast_shadows(False)
    return a

for a in actors:
    if 'MenuCamera' in a.get_class().get_name():
        a.set_actor_location(unreal.Vector(-2550,1400,850),False,False)
        a.set_editor_property('LookTarget',unreal.Vector(-850,1400,430))
        a.set_editor_property('bContinuousDrift',True)
        a.set_editor_property('DriftAngle',5.0)
        a.set_editor_property('DriftPeriod',36.0)
        a.set_editor_property('DriftHeight',12.0)
        a.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(a.get_actor_location(),unreal.Vector(-850,1400,430)),False)
        c=a.get_component_by_class(unreal.CameraComponent)
        c.set_editor_property('constrain_aspect_ratio',False)
        c.set_editor_property('field_of_view',75.0)
    if a.get_actor_label()=='DirectionalLight':
        a.set_actor_rotation(unreal.Rotator(pitch=-32,yaw=-35,roll=0),False)
        c=a.get_component_by_class(unreal.DirectionalLightComponent)
        c.set_light_color(unreal.LinearColor(1.0,.77,.52))
        c.set_intensity(3.2)
    if a.get_actor_label()=='SkyLight':
        c=a.get_component_by_class(unreal.SkyLightComponent)
        c.set_light_color(unreal.LinearColor(.55,.75,1.0))
        c.set_intensity(.85)
    if a.get_actor_label()=='ExponentialHeightFog':
        a.set_actor_location(unreal.Vector(-5600,-50,-100),False,False)
        c=a.get_component_by_class(unreal.ExponentialHeightFogComponent)
        c.set_fog_density(.021)
        c.set_fog_height_falloff(.18)
        c.set_start_distance(1000.)
        c.set_fog_inscattering_color(unreal.LinearColor(.12,.22,.28))


# Absolute transforms make repeated dressing safe; actor names identify original map objects.
reposition={
    'StaticMeshActor_19':(-250,120,0),
    'StaticMeshActor_25':(650,2300,0),
    'StaticMeshActor_57':(400,2450,0),
    'StaticMeshActor_135':(100,150,0),
    'StaticMeshActor_136':(450,250,0),
    'StaticMeshActor_130':(600,2450,0),
    'StaticMeshActor_23':(600,2600,0),
    'StaticMeshActor_9':(-750,420,0),
    'StaticMeshActor_14':(-600,370,0),
    'StaticMeshActor_36':(-650,210,0),
    'StaticMeshActor_61':(-220,2400,0),
    'StaticMeshActor_62':(-300,2300,0),
}
for a in actors:
    if a.get_name() not in reposition: continue
    c=a.get_component_by_class(unreal.StaticMeshComponent)
    center,extent,_=unreal.SystemLibrary.get_component_bounds(c)
    goal=unreal.Vector(*reposition[a.get_name()])
    delta=goal-unreal.Vector(center.x,center.y,center.z-extent.z)
    a.set_actor_location(a.get_actor_location()+delta,False,False)

# A small staging camp opens toward the character, with a warm fire opposite cyan ore.
prop('Tent','tent_red',(130,680,5),1.1,90)
prop('Firepit','/Game/Game/Stage/Props/asset/designmesh/Nature/Meshes/Props/Campfire/SM_CampfireASmall',(-1120,770,10),3.4)
prop('Firewood','/Game/Game/Stage/Props/asset/designmesh/Nature/Meshes/Props/Campfire/SM_FirewoodPileA',(-1100,270,10),2.,30)
prop('CampLog','log',(-630,680,10),.50,85)
prop('CrystalLeft','/Game/Game/Mineral/BlueD/Blue_C',(-750,140,5),4.5,25)
prop('CrystalRight','/Game/Game/Mineral/BlueD/Blue_C',(-440,2090,5),6.6,-20)
prop('CrystalSmall','low-poly+crystal+cluster+3d+model',(-800,120,10),3.3,-35)
prop('RockLeft','SM_Rocks_03',(-310,380,0),1.4,20)
prop('RockRight','SM_Rocks_04',(50,2300,0),2.0,-20)
prop('ForegroundRock','SM_Rocks_03',(-1820,400,0),1.8,20)
prop('ForegroundRockRight','SM_Rocks_04',(-1700,2500,0),1.15,45)
for i,(x,y,s) in enumerate([(-1600,490,1.8),(-1210,300,1.6),(-740,160,1.8),(-240,280,2),(-450,2250,1.8),(-1200,2440,1.7),(-1500,2270,1.4)]):
    prop('Flowers%02d'%i,'Flower_bush_blue' if i%2 else 'Flower_bush_white',(x,y,5),s,i*47)
light('FireGlow',(-1120,770,185),(1.,.37,.085),18000.,1050.)
light('EtherGlowLeft',(-760,200,150),(.12,.65,1.),12000.,800.)
light('EtherGlowRight',(-560,1980,250),(.12,.8,1.),12000.,800.)
light('HeroFill',(-1800,1350,640),(.60,.81,1.),1600.,1700.)
fire=own(sub.spawn_actor_from_class(unreal.Emitter,unreal.Vector(-1120,770,75)),'Fire')
fire.get_component_by_class(unreal.ParticleSystemComponent).set_template(unreal.load_asset('/Game/SmokeFX/Particles/P_Fire_01'))
fire.set_actor_scale3d(unreal.Vector(3.,3.,3.))
unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log('START_SETDRESS_SAVED '+str(len([a for a in sub.get_all_level_actors() if unreal.Name(TAG) in a.tags])))
