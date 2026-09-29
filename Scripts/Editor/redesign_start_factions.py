"""Reframe only Start as two faction camps. Existing gameplay levels and UI bindings are untouched."""
import math,shutil
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()); backup=root/'Saved/StartRedesign/Start.before_redesign.umap'
backup.parent.mkdir(parents=True,exist_ok=True)
if not backup.exists():shutil.copy2(root/'Content/Core/Levels/Start.umap',backup)
# The source image remains versioned independently of the imported Unreal texture.
dest='/Game/Core/UI/LobbyAtmosphere'; name='T_StartWordmark'
if not unreal.EditorAssetLibrary.does_asset_exist(dest+'/'+name):
 task=unreal.AssetImportTask(); task.set_editor_properties(dict(filename=str(root/'ContentSource/Start/T_StartWordmark.png'),destination_path=dest,destination_name=name,automated=True,save=True))
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex=unreal.load_asset(dest+'/'+name)
tex.set_editor_properties(dict(lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,never_stream=True,mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS))
unreal.EditorAssetLibrary.save_loaded_asset(tex)
# Local central readability gradient, no full-screen dimming or blur.
mat=unreal.load_asset(dest+'/M_StartMenuShade')
node=next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer()==mat)
node.set_editor_property('code','float x=abs(UV.x-.5); float center=1-smoothstep(.055,.28,x); float y=smoothstep(.42,.64,UV.y); float bottom=smoothstep(.84,1,UV.y)*.18; return float4(.018,.025,.026,center*y*.42+bottom);')
unreal.MaterialEditingLibrary.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Core/Levels/Start')
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);TAG='SF_StartFactionDress'
for a in sub.get_all_level_actors():
 if unreal.Name(TAG) in a.tags or unreal.Name('SF_StartSetDress') in a.tags:sub.destroy_actor(a)
actors=sub.get_all_level_actors()
def own(a,label):
 a.set_actor_label('StartFactions_'+label);a.set_editor_property('tags',[unreal.Name(TAG)]);a.set_folder_path('Start Presentation/Faction Camps');return a

def ground(a,at):
 c=a.get_component_by_class(unreal.SkeletalMeshComponent) or a.get_component_by_class(unreal.StaticMeshComponent)
 center,extent,_=unreal.SystemLibrary.get_component_bounds(c)
 a.set_actor_location(a.get_actor_location()+unreal.Vector(at[0]-center.x,at[1]-center.y,at[2]-(center.z-extent.z)),False,False)

def prop(label,path,at,height,yaw=0):
 mesh=unreal.load_asset(path)
 if not mesh:raise RuntimeError(path)
 b=mesh.get_bounding_box();scale=height/max(b.max.z-b.min.z,1)
 a=own(sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(),unreal.Rotator(yaw=yaw)),label)
 c=a.static_mesh_component;c.set_static_mesh(mesh);c.set_collision_profile_name('NoCollision')
 a.set_actor_scale3d(unreal.Vector(scale,scale,scale));ground(a,at);return a

def building(label,definition,at,height,yaw):
 data=unreal.load_asset(definition);visual=data.get_editor_property('tiers')[0].get_editor_property('visual')
 mesh=visual.get_editor_property('completed_mesh');return prop(label,mesh.get_path_name(),at,height,yaw)

def light(label,at,color,power,radius):
 a=own(sub.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*at)),label)
 c=a.get_component_by_class(unreal.PointLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE)
 c.set_editor_property('intensity_units',unreal.LightUnits.UNITLESS);c.set_intensity(power);c.set_attenuation_radius(radius);c.set_light_color(unreal.LinearColor(*color));c.set_cast_shadows(False)
 if label=='Campfire':a.set_editor_property('tags',[unreal.Name(TAG),unreal.Name('SF_StartFireLight')])

for a in actors:
 label=a.get_actor_label()
 if 'MenuCamera' in a.get_class().get_name():
  camera=unreal.Vector(-3100,1400,900);target=unreal.Vector(1200,1400,450)
  a.set_actor_location(camera,False,False);a.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera,target),False)
  a.set_editor_property('LookTarget',target);a.set_editor_property('bContinuousDrift',True);a.set_editor_property('DriftAngle',.8);a.set_editor_property('DriftPeriod',48.);a.set_editor_property('DriftHeight',5.)
  c=a.get_component_by_class(unreal.CameraComponent);c.set_editor_property('field_of_view',63.);c.set_editor_property('constrain_aspect_ratio',False)
 if label=='DirectionalLight':
  a.set_actor_rotation(unreal.Rotator(pitch=-32,yaw=-25),False);c=a.get_component_by_class(unreal.DirectionalLightComponent);c.set_light_color(unreal.LinearColor(1.,.92,.80));c.set_intensity(1.9)
 if label=='SkyLight':
  c=a.get_component_by_class(unreal.SkyLightComponent);c.set_light_color(unreal.LinearColor(.72,.85,1.));c.set_intensity(.8)
 if label=='ExponentialHeightFog':
  c=a.get_component_by_class(unreal.ExponentialHeightFogComponent);c.set_fog_density(.027);c.set_start_distance(1500);c.set_fog_inscattering_color(unreal.LinearColor(.35,.5,.55))
 if label=='SpotLight':a.get_component_by_class(unreal.SpotLightComponent).set_intensity(0)
 if label=='BP_Beast_Commander':
  a.set_actor_scale3d(unreal.Vector(7,7,7));a.set_actor_rotation(unreal.Rotator(yaw=160),False);ground(a,(-850,630,10))
 if label=='BP_Ether_Commander':
  a.set_actor_scale3d(unreal.Vector(7.8,7.8,7.8));a.set_actor_rotation(unreal.Rotator(yaw=200),False);ground(a,(-750,2220,10))
 c=a.get_component_by_class(unreal.StaticMeshComponent)
 if c and isinstance(a,unreal.StaticMeshActor):
  center,extent,_=unreal.SystemLibrary.get_component_bounds(c)
  # Clear only the sightline, keeping the original woodland as a framing backdrop.
  if -1700<center.x<3000 and 300<center.y<2700 and any(s in label.lower() for s in ['tree','tent','bush','grass']):
   shift=(-550 if center.y<1400 else 3350)-center.y
   a.set_actor_location(a.get_actor_location()+unreal.Vector(0,shift,0),False,False)

base='/Game/Game/Stage/Props/asset/static_mesh/'
prop('BeastTent',base+'tent_red',(200,530,0),450,90)
prop('BeastLog',base+'log',(-950,-300,10),140,80)
prop('Firepit','/Game/Game/Stage/Props/asset/designmesh/Nature/Meshes/Props/Campfire/SM_CampfireASmall',(-1000,1050,0),100)
prop('Firewood','/Game/Game/Stage/Props/asset/designmesh/Nature/Meshes/Props/Campfire/SM_FirewoodPileA',(-1200,-160,0),90,35)
prop('EtherCrystal','/Game/Game/Mineral/BlueD/Blue_C',(-300,1950,0),230,-15)
prop('EtherCrystalSmall','/Game/Game/Mineral/BlueD/Blue_C',(-1000,2320,0),110,25)
building('EtherOutpost','/Game/Core/Primary/Buildings/Definitions/Human/DA_HumanBarrack',(1100,2380,0),820,180)
building('BeastOutpost','/Game/Core/Primary/Buildings/Definitions/Animal/DA_AnimalBarrack',(1500,350,0),680,180)
prop('LeftStone',base+'SM_Rocks_03',(-1900,-100,0),160,30)
prop('RightStone',base+'SM_Rocks_04',(-1750,2930,0),160,-30)
for i,(x,y) in enumerate([(-1800,350),(-1600,-100),(-1100,-250),(-1700,2600),(-1400,2900),(-400,2880)]):
 prop('Flowers'+str(i),base+('Flower_bush_white' if i<3 else 'Flower_bush_blue'),(x,y,3),45,i*42)
light('Campfire',(-1000,1050,180),(1.,.38,.09),18000,1200)
light('BeastKey',(-1450,650,800),(1.,.48,.19),45000,1350)
light('EtherKey',(-1350,2190,800),(.22,.8,.88),38000,1350)
light('EtherCrystal',(-300,2500,200),(.06,.75,.8),11000,900)
fire=own(sub.spawn_actor_from_class(unreal.Emitter,unreal.Vector(-1000,1050,55)),'Fire')
fire.get_component_by_class(unreal.ParticleSystemComponent).set_template(unreal.load_asset('/Game/SmokeFX/Particles/P_Fire_01'))
fire.set_actor_scale3d(unreal.Vector(2,2,2))
# Small background soldiers use the existing meshes and idle clips, without AI/combat actors.
def soldier_material(name,texture):
 path=dest+'/'+name
 mat=unreal.load_asset(path)
 if not mat:
  mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
  mat.set_editor_property('used_with_skeletal_mesh',True)
  sample=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
  sample.set_editor_property('texture',unreal.load_asset(texture))
  unreal.MaterialEditingLibrary.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
  rough=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionConstant)
  rough.set_editor_property('r',.8)
  unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
  unreal.MaterialEditingLibrary.recompile_material(mat)
  unreal.EditorAssetLibrary.save_loaded_asset(mat)
 return mat

def soldier(label,mesh,anim,at,scale,yaw):
 mesh_asset=unreal.load_asset(mesh);clip=unreal.load_asset(anim)
 if not mesh_asset or not clip:raise RuntimeError('Missing soldier asset: '+mesh+' / '+anim)
 a=own(sub.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(*at),unreal.Rotator(yaw=yaw)),label)
 c=a.skeletal_mesh_component;c.set_skeletal_mesh_asset(mesh_asset);c.set_collision_profile_name('NoCollision')
 c.override_animation_data(clip,True,True,0.,1.)
 ether='Knight_Tier02' in mesh
 material=soldier_material('M_StartEtherGuardTier02' if ether else 'M_StartBeastGuard',
  '/Game/Game/Race_Etherial/Character/Human/Knight_Tier02/T_Human_Knight_Tier02' if ether else
  '/Game/Game/Race_Beast/Character/Fox/Fox_Warrior_Tier03/T_Fox_Warrior_Tier03')
 # Some imported meshes contain render sections but no material slot table.
 # Repair a presentation-only copy, leaving the gameplay mesh unchanged.
 if not mesh_asset.get_editor_property('materials'):
  copy_path=dest+'/SK_Start_'+mesh_asset.get_name()
  mesh_asset=unreal.load_asset(copy_path) or unreal.EditorAssetLibrary.duplicate_asset(mesh,copy_path)
  slot=unreal.SkeletalMaterial();slot.set_editor_property('material_interface',material)
  slot.set_editor_property('material_slot_name','Body')
  mesh_asset.set_editor_property('materials',[slot])
  unreal.EditorAssetLibrary.save_loaded_asset(mesh_asset)
  c.set_skeletal_mesh_asset(mesh_asset)
 for slot_index in range(max(1,c.get_num_materials())):c.set_material(slot_index,material)
 a.set_actor_scale3d(unreal.Vector(scale,scale,scale));ground(a,at)
knight='/Game/Game/Race_Etherial/Character/Human/Knight_Tier02/'
fox='/Game/Game/Race_Beast/Character/Fox/Fox_Warrior_Tier03/'
for i in range(2):
 soldier('EtherGuard'+str(i),knight+'SKM_Human_Knight_Tier02',knight+'Anim/Human_Knight_Tier02_Idle',(320+i*320,2110+i*130,5),3.8,200)
 soldier('BeastGuard'+str(i),fox+'Fox_Warrior_Tier03_Rig',fox+'Anim/Fox_Warrior_Tier03_Idle',(700+i*350,780-i*170,5),3.5,160)
unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log('START_FACTION_REDESIGN_SAVED')
