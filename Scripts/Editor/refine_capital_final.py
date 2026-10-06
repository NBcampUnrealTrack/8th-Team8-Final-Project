"""Revision 1: dense editable woods, exact Stage2 water, three-access plateau.
Only edits PT_SFStageFinal and its private art. Run after the editor is closed.
"""
import unreal,sys,math,json,random,importlib.util
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import capital_final_layout as L
OUT=ROOT/'Saved/CapitalFinal/Revision1';OUT.mkdir(parents=True,exist_ok=True)
assert not (OUT/'refined.json').exists(),'Revision already applied; preserve subsequent manual edits.'
spec=importlib.util.spec_from_file_location('previous_layout',OUT/'layout_before.py');old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
world=unreal.EditorLoadingAndSavingUtils.load_map(L.MAP);assert world
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);ml=unreal.MaterialEditingLibrary
IGNORE=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('IGNORE')))
L.generate(ROOT/'ContentSource/CapitalFinal')
t=unreal.AssetImportTask();t.filename=str(ROOT/'ContentSource/CapitalFinal/SM_CapitalTerrain.obj');t.destination_path=L.ART;t.destination_name='SM_CapitalTerrain';t.automated=True;t.replace_existing=True;t.save=True
o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;o.automated_import_should_detect_type=False
d=o.static_mesh_import_data;d.combine_meshes=True;d.auto_generate_collision=False;d.convert_scene=False;d.convert_scene_unit=False;t.options=o
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
mesh=unreal.load_asset(L.ART+'/SM_CapitalTerrain');mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);unreal.EditorAssetLibrary.save_loaded_asset(mesh)
source=(ROOT/'Scripts/Editor/create_capital_final.py').read_text(encoding='utf-8');ctx={'L':L};exec(source[source.index("noise='''"):source.index('floor_mat=')],ctx)
floor=unreal.load_asset(L.ART+'/M_CapitalFinal_Terrain')
for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
 if n.get_outer()==floor and 'float road=' in n.get_editor_property('code'):n.set_editor_property('code',ctx['code'])
ml.recompile_material(floor);unreal.EditorAssetLibrary.save_loaded_asset(floor)
water=unreal.load_asset(L.ART+'/MI_FinalStage2Water');assert water
actors=list(sub.get_all_level_actors());deleted=0
vegetation=('BeastWoodland','EtherWoodland','Understory','ForestRocks','GroundPlants','EtherCrystals')
for a in actors:
 label=a.get_actor_label();pos=a.get_actor_location();u,v=L.uv(pos.x,pos.y)
 if label=='Final_Terrain':a.static_mesh_component.set_material(0,floor);continue
 if label.startswith('Final_Lake_'):a.static_mesh_component.set_material(0,water);continue
 if isinstance(a,unreal.LanePath):
  i=int(label.split('_')[2])-1;team=a.get_editor_property('team_index');route=L.ROUTES[i] if team else list(reversed(L.ROUTES[i]));sp=a.get_editor_property('spline');sp.clear_spline_points(False)
  for p in route:sp.add_spline_point(unreal.Vector(*L.world(*p)),unreal.SplineCoordinateSpace.WORLD,False)
  for j in range(len(route)):sp.set_spline_point_type(j,unreal.SplinePointType.LINEAR,False)
  sp.update_spline();continue
 if not label.startswith('Final_') or any(k in label for k in ['LakeCollision','LakeNoWalk','NavMesh','Sun','SkyLight','Atmosphere']):continue
 if any(label.startswith('Final_'+k+'_') for k in vegetation) and (L.ramp_distance(u,v)<1070 or L.base_distance(u,v,L.HILL)<1600 or .66<L.hill_q(u,v)<.91):
  sub.destroy_actor(a);deleted+=1;continue
 pos.z+=L.height(u,v)-old.height(u,v);a.set_actor_location(pos,False,False)

catalog=json.loads((ROOT/'Scripts/Editor/stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'))
cache={};rng=random.Random(202610011);counts={};tag=unreal.Name('SF_CapitalFinal_R1')
def prop(name,u,v,width,height,group,blocking=True):
 if name not in cache:
  solid='/Game/Game/Stage/StageHeroArt/Collision/SM_HeroSolid_'+name
  p=solid if unreal.EditorAssetLibrary.does_asset_exist(solid) else catalog[name]['path'] if name in catalog else '/Game/Game/Stage/Props/asset/static_mesh/'+name;m=unreal.load_asset(p);assert m,name;cache[name]=m
 m=cache[name];b=m.get_bounds();sx=width/max(2*b.box_extent.x,2*b.box_extent.y);sz=height/(2*b.box_extent.z);yaw=rng.uniform(0,360);ca,sa=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));x,y=L.xy(u,v)
 loc=(x-ca*b.origin.x*sx+sa*b.origin.y*sx,y-sa*b.origin.x*sx-ca*b.origin.y*sx,L.height(u,v)-10-(b.origin.z-b.box_extent.z)*sz)
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(pitch=0,yaw=yaw,roll=0));c=a.static_mesh_component;c.set_static_mesh(m);a.set_actor_scale3d(unreal.Vector(sx,sx,sz))
 for i,p in enumerate(catalog.get(name,{}).get('materials',[])):
  if p:c.set_material(i,unreal.load_asset(p))
 c.set_collision_profile_name('BlockAll' if blocking else 'NoCollision');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,IGNORE)
 counts[group]=counts.get(group,0)+1;a.set_actor_label('Final_R1_%s_%04d'%(group,counts[group]));a.set_folder_path('Final Capital/'+group);a.tags=[unreal.Name('SF_CapitalFinal'),tag]
 return a

# Spatially irregular canopy layers, preserving the navigable roads and plateau entrances.
occupied={}
def insert(x,y):occupied.setdefault((int(x//270),int(y//270)),[]).append((x,y))
for a in sub.get_all_level_actors():
 if any(k in a.get_actor_label() for k in ['BeastWoodland','EtherWoodland']):
  p=a.get_actor_location();insert(p.x,p.y)
for i in range(86):
 for j in range(69):
  u=-.065+i*.0133+rng.uniform(-.005,.005);v=-.065+j*.0167+rng.uniform(-.006,.006)
  if L.protected(u,v,90) or rng.random()>.84:continue
  x,y=L.xy(u,v);gx,gy=int(x//270),int(y//270)
  if any((x-a)**2+(y-b)**2<260**2 for k in range(gx-1,gx+2) for q in range(gy-1,gy+2) for a,b in occupied.get((k,q),[])):continue
  beast=u+v<.94
  names=['Tree_average_bare','tree_dead01','Tree_small_bare'] if beast and rng.random()<.62 else ['SM_GenericTreeD','SM_GenericTreeB','SM_PineVariantAGrowingB','SM_BirchTreeAdultA']
  prop(rng.choice(names),u,v,rng.uniform(430,660),rng.uniform(380,630),'DenseBeastForest' if beast else 'DenseEtherForest');insert(x,y)
  if rng.random()<.65:prop('Bush_average',u+rng.uniform(-.004,.004),v+rng.uniform(-.004,.004),rng.uniform(130,240),rng.uniform(85,145),'ForestFloor',False)

# Broken clusters of pebbles, boulders and low shoreline plants, not a uniform stone ring.
for k,(cu,cv,rx,ry) in enumerate(L.LAKES):
 for i in range(145):
  a=rng.uniform(0,2*math.pi);r=rng.uniform(1.02,1.28);edge=1+.065*math.sin(3*a+k)+.045*math.sin(5*a+1)
  u,v=cu+rx*r*edge*math.cos(a),cv+ry*r*edge*math.sin(a)
  if L.road_distance(u,v)<810 or L.ramp_distance(u,v)<860:continue
  if rng.random()<.45:prop(rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_07']),u,v,rng.uniform(60,220),rng.uniform(35,140),'NaturalLakeBanks')
  else:prop(rng.choice(['GrassClump01','GrassClump02','Bush_average']),u,v,rng.uniform(100,230),rng.uniform(60,135),'ShorePlants',False)
# Stony escarpment explains the blocked hill sides while the three ramps remain clear.
for i in range(80):
 a=i*2*math.pi/80+rng.uniform(-.025,.025);q=rng.uniform(.76,.86);u=L.HILL[0]+.132*q*math.cos(a);v=L.HILL[1]+.145*q*math.sin(a)
 if L.ramp_distance(u,v)<1020:continue
 prop(rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_09']),u,v,rng.uniform(240,430),rng.uniform(210,410),'HillEscarpment')

assert unreal.EditorLoadingAndSavingUtils.save_map(world,L.MAP)
(OUT/'refined.json').write_text(json.dumps({'added':counts,'cleared_for_hill':deleted,'water':water.get_path_name(),'hill_height_cm':1050,'ramp_count':3,'actors':len(sub.get_all_level_actors())},indent=2),encoding='utf-8')
unreal.log('CAPITAL_REVISION1_SAVED')
