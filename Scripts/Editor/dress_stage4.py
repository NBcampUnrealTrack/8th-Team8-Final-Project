"""Dress only PT_Stage_4; preserve its gameplay actors, transforms, routes and collision."""
import unreal,math,random,json,collections
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()); OUT=ROOT/'Saved/Stage04Polish'
MAP='/Game/Game/Stage/PT_Stage_4'; ART='/Game/Game/Stage/Stage04_Art'; TAG=unreal.Name('SF_Stage04_Dressing')
R=random.Random(430930)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem); tools=unreal.AssetToolsHelpers.get_asset_tools(); ml=unreal.MaterialEditingLibrary
world=unreal.EditorLoadingAndSavingUtils.load_map(MAP)
assert world
actors=list(sub.get_all_level_actors())
for a in actors:
 if TAG in a.tags:sub.destroy_actor(a)
original=[a for a in sub.get_all_level_actors() if not isinstance(a,unreal.InstancedFoliageActor)]

# World-aligned tiling lets floor and path layers share one grass sample, with organic shoulders.
grass=unreal.load_asset('/Game/Game/Stage/Props/texture/greengrass/Grass005_1K-PNG_Color')
dirt=unreal.load_asset('/Game/Game/Stage/Props/texture/brownmud/brown_mud_leaves_01_diff_1k')
assert grass and dirt

def material(name,kind):
 path=ART+'/'+name
 mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,ART,unreal.Material,unreal.MaterialFactoryNew())
 if ml.get_num_material_expressions(mat):return mat
 mat.set_editor_property('two_sided',True)
 uv=ml.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-600,0)
 pos=ml.create_material_expression(mat,unreal.MaterialExpressionWorldPosition,-600,100)
 sources={'UV':uv,'P':pos}
 for key,tex in [('Grass',grass),('Dirt',dirt)]:
  node=ml.create_material_expression(mat,unreal.MaterialExpressionTextureObject,-600,200+len(sources)*100);node.set_editor_property('texture',tex);sources[key]=node
 custom=ml.create_material_expression(mat,unreal.MaterialExpressionCustom,0,0)
 ins=[]
 for key in sources:
  i=unreal.CustomInput();i.set_editor_property('input_name',key);ins.append(i)
 if kind=='stone':code='float n=.5+.5*sin(P.x*.005+sin(P.y*.004)*2.0);return lerp(float3(.19,.22,.17),float3(.32,.34,.28),n);'
 else:
  code='''float2 w=P.xy/420.0;
float3 g=Texture2DSample(Grass,GrassSampler,w).rgb*float3(.82,.88,.78);
float3 d=Texture2DSample(Dirt,DirtSampler,P.xy/380.0).rgb;
float n=(sin(P.x*.007+sin(P.y*.003)*2.0)+sin(P.y*.009+sin(P.x*.002)*2.5))*.5;
float2 edge=min(UV,1.0-UV);
float fade=smoothstep(.018,.14,edge.x+n*.025)*smoothstep(.018,.19,edge.y+n*.04);
'''
  code+= 'return g;' if kind=='grass' else 'return lerp(g,d,fade*'+('.74' if kind=='road' else '.52')+');'
 custom.set_editor_properties(dict(code=code,inputs=ins,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3))
 for key,source in sources.items():ml.connect_material_expressions(source,'',custom,key)
 ml.connect_material_property(custom,'',unreal.MaterialProperty.MP_BASE_COLOR)
 rough=ml.create_material_expression(mat,unreal.MaterialExpressionConstant,0,180);rough.set_editor_property('r',.93);ml.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 spec=ml.create_material_expression(mat,unreal.MaterialExpressionConstant,0,260);spec.set_editor_property('r',.08);ml.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
 ml.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
 ml.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
 return mat
mats={k:material('M_S4_'+k.title(),k) for k in ['grass','road','zone','stone']}

# Evaluate the complete road union in world space on every ground layer. Overlapping
# authored planes otherwise paint opaque grass rectangles over neighbouring roads.
terrain_path=ART+'/M_S4_Terrain'
terrain=unreal.load_asset(terrain_path) if unreal.EditorAssetLibrary.does_asset_exist(terrain_path) else tools.create_asset('M_S4_Terrain',ART,unreal.Material,unreal.MaterialFactoryNew())
if not ml.get_num_material_expressions(terrain):
 terrain.set_editor_property('two_sided',True)
 sources={'P':ml.create_material_expression(terrain,unreal.MaterialExpressionWorldPosition,-600,0)}
 for key,tex in [('Grass',grass),('Dirt',dirt)]:
  node=ml.create_material_expression(terrain,unreal.MaterialExpressionTextureObject,-600,len(sources)*180);node.set_editor_property('texture',tex);sources[key]=node
 code='float road=0.0; float zone=0.0; float2 q; float2 d; float sd;\n'
 for a in original:
  label=a.get_actor_label()
  if not label.startswith(('GF_Route','GF_Zone')):continue
  tr=a.get_actor_transform();p=a.get_actor_location();s=a.get_actor_scale3d();yaw=math.radians(a.get_actor_rotation().yaw);c=math.cos(yaw);sn=math.sin(yaw)
  target='road' if label.startswith('GF_Route') else 'zone'
  hx=max(100,abs(s.x)*50-220);hy=max(100,abs(s.y)*50-220)
  code+=f'q=P.xy-float2({p.x:.3f},{p.y:.3f}); q=float2(q.x*{c:.8f}+q.y*{sn:.8f},-q.x*{sn:.8f}+q.y*{c:.8f}); d=abs(q)-float2({hx:.3f},{hy:.3f}); sd=length(max(d,0.0))+min(max(d.x,d.y),0.0)-100.0; {target}=max({target},1.0-smoothstep(-180.0,360.0,sd+90.0*sin(P.x*.006+sin(P.y*.004)*2.0)));\n'
 code+='''float3 g=Texture2DSample(Grass,GrassSampler,P.xy/1100.0).rgb*float3(.82,.87,.78);
float3 dcol=Texture2DSample(Dirt,DirtSampler,P.xy/1400.0).rgb;
float macro=.95+.05*sin(P.x*.0013+sin(P.y*.0018)*2.0);
return lerp(g,dcol,max(road*.77,zone*.58))*macro;'''
 custom=ml.create_material_expression(terrain,unreal.MaterialExpressionCustom,0,0);ins=[]
 for key in sources:
  inp=unreal.CustomInput();inp.set_editor_property('input_name',key);ins.append(inp)
 custom.set_editor_properties(dict(code=code,inputs=ins,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3))
 for key,node in sources.items():ml.connect_material_expressions(node,'',custom,key)
 ml.connect_material_property(custom,'',unreal.MaterialProperty.MP_BASE_COLOR)
 rough=ml.create_material_expression(terrain,unreal.MaterialExpressionConstant,0,180);rough.set_editor_property('r',.93);ml.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
 ml.recompile_material(terrain);unreal.EditorAssetLibrary.save_loaded_asset(terrain)
mats['grass']=terrain

for a in original:
 label=a.get_actor_label()
 if label.startswith(('GF_GrassTile','GF_Route','GF_Zone')):
  a.static_mesh_component.set_material(0,terrain)

# Only additional perimeter ground: original walkable floor and NavMesh limits remain intact.
plane=unreal.load_asset('/Engine/BasicShapes/Plane')
for label,x,y,sx,sy in [('West',-17500,0,90,360),('East',17500,0,90,360),('North',0,13500,260,90),('South',0,-13500,260,90)]:
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,-.2))
 a.set_actor_label('S4_PerimeterGround_'+label);a.tags=[TAG];a.set_folder_path('Stage 04/Backdrop/Ground')
 c=a.static_mesh_component;c.set_static_mesh(plane);c.set_material(0,mats['grass']);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION);c.set_cast_shadow(False)
 a.set_actor_scale3d(unreal.Vector(sx,sy,1))

BASE='/Game/Game/Stage/Props/asset/'
paths={
 'Oak':BASE+'designmesh/Nature/Meshes/Env/Trees/Oak/SM_OakAdultD',
 'Canopy':BASE+'designmesh/Nature/Meshes/Env/Trees/GenericTrees/SM_GenericTreeD',
 'Pine':BASE+'designmesh/Nature/Meshes/Env/Trees/Pines/SM_PineVariantAGrowingB',
 'Bush':BASE+'static_mesh/Bush_average',
 'Fern':BASE+'designmesh/Nature/Meshes/Env/Plants/Ferns/SM_FernDClusterVariantB',
 'Grass':BASE+'designmesh/Nature/Meshes/Env/GrassLumps/SM_GrassLumpD',
 'LowGrass':'/Game/Game/Stage/Foliage/SM_SF_LowGrass02',
 'Flowers':BASE+'static_mesh/Flower_bush_white',
 'Rock':BASE+'static_mesh/SM_Rocks_02',
 'RockB':BASE+'static_mesh/SM_Rocks_09',
 'Pebble':BASE+'designmesh/Nature/Meshes/Env/Rocks/SM_RockSmallA',
 'Rubble':BASE+'designmesh/Nature/Meshes/Arch/RuinedWalls/SM_RuinedWallRubbleA',
 'Wall':BASE+'designmesh/Nature/Meshes/Arch/RuinedWalls/SM_RuinedWallA',
 'Stump':BASE+'static_mesh/Stump_average_low',
 'Log':BASE+'static_mesh/Log_big_knotty',
 'Hill':BASE+'static_mesh/SM_Rocks_09'}
meshes={k:unreal.load_asset(p) for k,p in paths.items()}
assert all(meshes.values())
types={};instances={k:[] for k in paths};placements=[]
for key,mesh in meshes.items():
 name='FT_S4_'+key;path=ART+'/'+name
 ft=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,ART,unreal.FoliageType_InstancedStaticMesh,unreal.FoliageType_InstancedStaticMeshFactory())
 ft.set_editor_property('mesh',mesh)
 if key=='Fern':ft.set_editor_property('override_materials',[unreal.load_asset(BASE+'designmesh/Nature/Textures/baseline_palette_M')])
 if key in ['Rock','RockB','Pebble','Hill']:ft.set_editor_property('override_materials',[mats['stone']])
 large=key in ['Oak','Canopy','Pine','Hill','Wall','Rock','RockB']
 for prop in ['cast_shadow','cast_dynamic_shadow','cast_static_shadow']:ft.set_editor_property(prop,large)
 for prop in ['cast_contact_shadow','affect_distance_field_lighting','affect_dynamic_indirect_lighting','receives_decals','collision_with_world']:ft.set_editor_property(prop,False)
 ft.set_editor_property('custom_navigable_geometry',unreal.HasCustomNavigableGeometry.NO)
 body=ft.get_editor_property('body_instance');body.set_editor_property('collision_profile_name','NoCollision');body.set_editor_property('collision_enabled',unreal.CollisionEnabled.NO_COLLISION);ft.set_editor_property('body_instance',body)
 ft.set_editor_property('cull_distance',unreal.Int32Interval(0,0) if large else unreal.Int32Interval(14000,20000))
 ft.set_editor_property('enable_density_scaling',not large)
 unreal.EditorAssetLibrary.save_loaded_asset(ft);unreal.InstancedFoliageActor.remove_all_instances(world,ft);types[key]=ft

# Pivot compensation is essential: several imported props have distant origins.
def add(key,x,y,height,z=2,yaw=None,wide=1):
 mesh=meshes[key];b=mesh.get_bounds();o=b.origin;e=b.box_extent
 scale=height/max(e.z*2,1);scalevec=unreal.Vector(scale*wide,scale*wide,scale)
 rot=unreal.Rotator(0,R.uniform(0,360) if yaw is None else yaw,0)
 offset=unreal.Transform(rotation=rot,scale=scalevec).transform_location(unreal.Vector(o.x,o.y,o.z-e.z))
 t=unreal.Transform(location=unreal.Vector(x,y,z)-offset,rotation=rot,scale=scalevec)
 instances[key].append(t);placements.append((key,x,y,height))

# Gameplay clearances use the authored plane transforms, not their broad world AABBs.
planes=[]
for a in original:
 if a.get_actor_label().startswith(('GF_Route','GF_Zone')):
  tr=a.get_actor_transform();sc=a.get_actor_scale3d();planes.append((tr,abs(sc.x)*50,abs(sc.y)*50,a.get_actor_label()))
sites=[]
for a in original:
 if any(k in a.get_class().get_name() for k in ['Building','Mineral','Ether','Commander','PlayerStart']):
  p=a.get_actor_location();sites.append((p.x,p.y,850 if 'Building' in a.get_class().get_name() else 450))
def protected(x,y,pad=0):
 for sx,sy,r in sites:
  if math.hypot(x-sx,y-sy)<r+pad:return True
 for tr,hx,hy,label in planes:
  p=tr.inverse_transform_location(unreal.Vector(x,y,0))
  # Plane local bounds +/-50; compensate padding by its authored dimensions.
  if abs(p.x)<50+pad/max(hx/50,1) and abs(p.y)<50+pad/max(hy/50,1):return True
 return False

def site_clear(x,y,pad=0):return all(math.hypot(x-sx,y-sy)>r+pad for sx,sy,r in sites)

# Five-to-eight tree depths around the entire original ground perimeter.
forest=[]
for x0 in range(-20400,20401,690):
 for y0 in range(-16400,16401,690):
  x=x0+R.uniform(-205,205);y=y0+R.uniform(-205,205)
  outside=abs(x)>12800 or abs(y)>8950
  if not outside:continue
  # Keep the highest outer crowns irregular instead of a ruler-straight fence.
  margin=max(abs(x)-13000,abs(y)-9000)
  if margin>6600+400*math.sin(x*.0012+y*.0009):continue
  key=R.choices(['Oak','Canopy','Pine'],[4,5,2])[0]
  h=R.uniform(1250,2050)*(1.12 if margin>3300 else 1)
  add(key,x,y,h,wide=R.uniform(.8,1.12));forest.append((x,y,h))

# Close the sparse authored forest inside the map, keeping all three corridors open.
for x0 in range(-12200,12201,620):
 for y0 in range(-8500,8501,620):
  x=x0+R.uniform(-190,190);y=y0+R.uniform(-190,190)
  if protected(x,y,480):continue
  if abs(x)<3500 and abs(y)<6000:continue
  if R.random()<.18:continue
  h=R.uniform(900,1500)
  add(R.choice(['Oak','Canopy','Pine']),x,y,h,wide=R.uniform(.8,1.0));forest.append((x,y,h))

# Broken stone shoulders around the two existing blocked ridges, never across lane centers.
for cy,rx in [(3300,2250),(-3400,1900)]:
 for i in range(22):
  ang=2*math.pi*i/22
  x=rx*math.cos(ang);y=cy+380*math.sin(ang)
  add(R.choice(['Rock','RockB']),x,y,R.uniform(210,360),z=0)
  if i%3==0:add('Bush',x+R.uniform(-90,90),y+R.uniform(-80,80),R.uniform(180,280),z=40)
 for i in range(10):
  x=R.uniform(-rx*.8,rx*.8);y=cy+R.uniform(-130,130)
  add(R.choice(['Oak','Canopy']),x,y,R.uniform(650,950),z=80,wide=.75)
 for i in range(85):
  x=R.uniform(-rx-350,rx+350);y=cy+R.choice([-1,1])*R.uniform(500,800)
  if site_clear(x,y):add(R.choice(['Fern','Grass','Bush']),x,y,R.uniform(45,110))

# Forest-floor clusters, logs and stones near the inner tree belt.
for x,y,h in forest:
 if abs(x)>15500 or abs(y)>11500:continue
 for i in range(R.randint(2,4)):
  px=x+R.uniform(-400,400);py=y+R.uniform(-400,400)
  if not site_clear(px,py,80):continue
  add(R.choice(['Bush','Fern']),px,py,R.uniform(70,210))
 if R.random()<.28:add(R.choice(['Rock','RockB']),x+R.uniform(-260,260),y+R.uniform(-260,260),R.uniform(200,440),z=0)
 if R.random()<.1:add(R.choice(['Stump','Log']),x+260,y-240,R.uniform(90,160))

# Hand-readable rubble accents outside base pads and at expansion boundaries.
for sign in [-1,1]:
 for i in range(6):
  add('Wall',sign*(12000+R.uniform(100,280)),-2900+i*1130,R.uniform(220,360),yaw=90)
  add('Rubble',sign*12000,-2750+i*1130,R.uniform(70,140))
 for i in range(7):
  add('Rock',sign*(3150+R.uniform(0,250)),-6250-i*330,R.uniform(190,350))
 for i in range(3):
  if site_clear(-1900+i*1900,sign*7950,220):add('Wall',-1900+i*1900,sign*7950,R.uniform(170,250),yaw=0)

# Small plants and pebbles follow path shoulders, leaving the lane middle open.
for tr,hx,hy,label in planes:
 if 'Route' not in label:continue
 for side in [-1,1]:
  for i in range(max(3,int(hx*2/160))):
   t=(i+.5)/max(3,int(hx*2/160));p=tr.transform_location(unreal.Vector(-48+96*t,side*R.uniform(43,55),0))
   if not site_clear(p.x,p.y,70):continue
   add(R.choices(['Fern','Grass','LowGrass','Pebble','Flowers'],[3,5,4,2,1])[0],p.x,p.y,R.uniform(35,90),z=3)

# Grass is clustered with gaps: useful gameplay pads remain legible.
for i in range(5800):
 x=R.uniform(-12800,12800);y=R.uniform(-8900,8900)
 if not site_clear(x,y,80):continue
 inroad=protected(x,y,-180)
 density=.12 if inroad else .72
 if R.random()>density:continue
 add(R.choices(['LowGrass','Grass','Fern','Flowers'],[7,5,2,1])[0],x,y,R.uniform(22,55) if inroad else R.uniform(30,90),z=3)

# Large, low distant rock forms break the forest silhouette beyond playable bounds.
for side in [-1,1]:
 for i in range(14):
  add('Hill',-17500+i*2700,side*(14500+R.uniform(-500,500)),R.uniform(800,1400),z=-100,wide=R.uniform(.8,1.2))
 for i in range(9):
  add('Hill',side*(18100+R.uniform(-400,400)),-11500+i*2800,R.uniform(800,1400),z=-100)

for key,ft in types.items():
 if instances[key]:unreal.InstancedFoliageActor.add_instances(world,ft,instances[key])
 unreal.log('S4_INSTANCES '+key+' '+str(len(instances[key])))
for a in sub.get_all_level_actors():
 if isinstance(a,unreal.InstancedFoliageActor):a.set_folder_path('Stage 04/Foliage')
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
(OUT/'dressing_report.json').write_text(json.dumps(dict(counts={k:len(v) for k,v in instances.items()},original_actor_count=len(original),decor_collision='NoCollision',seed=430930),indent=2),encoding='utf-8')
unreal.log('STAGE04_DRESSING_SAVED')
