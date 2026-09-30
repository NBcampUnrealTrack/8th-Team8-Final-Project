"""Add environment detail to the saved PT_Stage_4 without moving gameplay actors."""
import unreal,random,math,json
from pathlib import Path
MAP='/Game/Game/Stage/PT_Stage_4';ART='/Game/Game/Stage/Stage04_Art'
OUT=Path(unreal.Paths.project_dir())/'Saved/Stage04Polish'
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);tools=unreal.AssetToolsHelpers.get_asset_tools()
world=unreal.EditorLoadingAndSavingUtils.load_map(MAP)
actors=list(sub.get_all_level_actors());R=random.Random(430932)
palette=unreal.load_asset('/Game/Game/Stage/Props/asset/designmesh/Nature/Textures/baseline_palette_M')
stone=unreal.load_asset(ART+'/M_S4_Stone')
fernpath='/Game/Game/Stage/Props/asset/designmesh/Nature/Meshes/Env/Plants/Ferns/SM_FernDClusterVariantB'
fern=unreal.load_asset(fernpath)
ft=unreal.load_asset(ART+'/FT_S4_Fern');ft.set_editor_property('override_materials',[palette]);unreal.EditorAssetLibrary.save_loaded_asset(ft)
fixed=[]
for a in actors:
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  if c.static_mesh==fern:
   c.set_material(0,palette);fixed.append(c.get_path_name())

# Preserve the user's new building, resource and player placements exactly.
sites=[];planes=[]
for a in actors:
 cl=a.get_class().get_name();p=a.get_actor_location()
 if any(k in cl for k in ['Building','Mineral','Ether','Commander','PlayerStart','UnitSpawner']):
  radius=850 if 'Building' in cl else 500
  sites.append((p.x,p.y,radius))
 if a.get_actor_label().startswith(('GF_Route','GF_Zone')):
  planes.append((a.get_actor_transform(),a.get_actor_scale3d()))
def safe(x,y,pad=0):return all(math.hypot(x-a,y-b)>r+pad for a,b,r in sites)
def onroad(x,y,pad=0):
 for tr,sc in planes:
  p=tr.inverse_transform_location(unreal.Vector(x,y,0))
  if abs(p.x)<50+pad/max(abs(sc.x),1) and abs(p.y)<50+pad/max(abs(sc.y),1):return True
 return False

B='/Game/Game/Stage/Props/asset/'
defs={
 'FernLush':(fernpath,palette),
 'Meadow':(B+'static_mesh/Grass_bush_low_01',None),
 'BlueFlower':(B+'static_mesh/Flower_bush_blue',None),
 'Mushroom':(B+'static_mesh/hat_mushroom_red',None),
 'RuinDetail':(B+'designmesh/Nature/Meshes/Arch/RuinedWalls/SM_RuinedWallA',palette),
 'RubbleDetail':(B+'designmesh/Nature/Meshes/Arch/RuinedWalls/SM_RuinedWallRubbleA',palette),
 'RockDetail':(B+'static_mesh/SM_Rocks_02',stone),
 'ShrubDetail':(B+'static_mesh/Bush_average',None)}
types={};meshes={};instances={k:[] for k in defs}
for key,(path,mat) in defs.items():
 mesh=unreal.load_asset(path);assert mesh;meshes[key]=mesh
 name='FT_S4_'+key;ftpath=ART+'/'+name
 ft=unreal.load_asset(ftpath) if unreal.EditorAssetLibrary.does_asset_exist(ftpath) else tools.create_asset(name,ART,unreal.FoliageType_InstancedStaticMesh,unreal.FoliageType_InstancedStaticMeshFactory())
 ft.set_editor_property('mesh',mesh)
 if mat:ft.set_editor_property('override_materials',[mat])
 for prop in ['cast_shadow','cast_dynamic_shadow','cast_static_shadow']:ft.set_editor_property(prop,key in ['RockDetail','RuinDetail'])
 for prop in ['cast_contact_shadow','affect_distance_field_lighting','affect_dynamic_indirect_lighting','receives_decals','collision_with_world']:ft.set_editor_property(prop,False)
 ft.set_editor_property('custom_navigable_geometry',unreal.HasCustomNavigableGeometry.NO)
 body=ft.get_editor_property('body_instance');body.set_editor_property('collision_profile_name','NoCollision');body.set_editor_property('collision_enabled',unreal.CollisionEnabled.NO_COLLISION);ft.set_editor_property('body_instance',body)
 ft.set_editor_property('cull_distance',unreal.Int32Interval(16000,24000) if key not in ['RockDetail','RuinDetail'] else unreal.Int32Interval(0,0))
 unreal.EditorAssetLibrary.save_loaded_asset(ft);unreal.InstancedFoliageActor.remove_all_instances(world,ft);types[key]=ft
def add(key,x,y,h,z=2,yaw=None,wide=1):
 b=meshes[key].get_bounds();o=b.origin;e=b.box_extent;s=h/max(2*e.z,1)
 sc=unreal.Vector(s*wide,s*wide,s);rot=unreal.Rotator(0,R.uniform(0,360) if yaw is None else yaw,0)
 offset=unreal.Transform(rotation=rot,scale=sc).transform_location(unreal.Vector(o.x,o.y,o.z-e.z))
 instances[key].append(unreal.Transform(location=unreal.Vector(x,y,z)-offset,rotation=rot,scale=sc))
def garden(cx,cy,radius,n):
 for i in range(n):
  angle=R.uniform(0,math.tau);r=radius*math.sqrt(R.random());x=cx+r*math.cos(angle);y=cy+r*math.sin(angle)
  if not safe(x,y):continue
  key=R.choices(['FernLush','Meadow','BlueFlower','Mushroom'],[5,7,2,1])[0]
  h=R.uniform(75,140) if key=='FernLush' else R.uniform(30,75)
  add(key,x,y,h,wide=R.uniform(.8,1.2))

# Broken ruin fragments and vegetation islands sit on the existing rock ridges.
for cy,width in [(3300,1900),(-3400,1600)]:
 for i,x in enumerate([-width,-width*.45,width*.15,width*.7]):
  if safe(x,cy,400):
   add('RuinDetail',x,cy,R.uniform(260,410),z=110,yaw=R.uniform(-14,14))
   add('RubbleDetail',x+200,cy-150,R.uniform(100,170),z=70)
 for sign in [-1,1]:
  for x in [-width,-width*.5,0,width*.5,width]:garden(x,cy+sign*550,320,38)

# Irregular pockets along the woodland edge and the unused ground between lanes.
centers=[]
for i in range(180):
 x=R.uniform(-12500,12500);y=R.uniform(-8700,8700)
 if not safe(x,y,280) or onroad(x,y,150):continue
 if any(math.hypot(x-a,y-b)<600 for a,b in centers):continue
 centers.append((x,y));garden(x,y,R.uniform(200,420),R.randint(16,28))
 if R.random()<.6:add('ShrubDetail',x,y,R.uniform(160,260))
 if R.random()<.45:add('RockDetail',x+150,y-110,R.uniform(130,290),wide=.8)

# Ground plants nestle around existing isolated trees rather than lining up in rows.
for a in actors:
 if not isinstance(a,unreal.StaticMeshActor):continue
 c=a.static_mesh_component
 if not c.static_mesh or not any(k in c.static_mesh.get_name() for k in ['Tree','Oak','Pine','Rocks']):continue
 p,e=a.get_actor_bounds(False)
 if abs(p.x)>12700 or abs(p.y)>8800 or not safe(p.x,p.y,250):continue
 garden(p.x,p.y,max(230,min(e.x,500)),24)

for key,ft in types.items():
 if instances[key]:unreal.InstancedFoliageActor.add_instances(world,ft,instances[key])
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
(OUT/'detail_report.json').write_text(json.dumps(dict(fixed_fern_components=len(fixed),added={k:len(v) for k,v in instances.items()},protected_gameplay_sites=len(sites)),indent=2),encoding='utf-8')
unreal.log('S4_DETAILS_SAVED')
