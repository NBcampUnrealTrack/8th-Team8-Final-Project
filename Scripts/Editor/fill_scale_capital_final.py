"""Revision 5: fill the outer forest and shrink the final stage by five percent.
Preserves gameplay actor scales and existing authored placements. Run once, editor closed.
"""
import unreal,sys,math,json,random,shutil,re
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import capital_final_layout as L
OUT=ROOT/'Saved/CapitalFinal/Revision5';OUT.mkdir(exist_ok=True)
assert not (OUT/'built.json').exists(),'Already applied; never shrink the map twice.'
S=.95
shutil.copy2(ROOT/'Content/Game/Stage/PT_SFStageFinal.umap',OUT/'Before.umap')
shutil.copy2(ROOT/'Content/Core/DataTable/DT_SFStage.uasset',OUT/'Before_DT_SFStage.uasset')
shutil.copy2(ROOT/'Content/Game/Stage/CapitalFinalArt/M_CapitalFinal_Terrain.uasset',OUT/'Before_TerrainMaterial.uasset')
w=unreal.EditorLoadingAndSavingUtils.load_map(L.MAP);assert w
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);ml=unreal.MaterialEditingLibrary
actors=list(sub.get_all_level_actors());terrain=next(a for a in actors if a.get_actor_label()=='Final_Terrain')
assert abs(terrain.get_actor_scale3d().x-1)<.001,'Unexpected existing map scale.'
assert not any(a.get_attach_parent_actor() for a in actors),'Inspect attached actors before transforming.'
IGNORE=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('IGNORE')))
catalog=json.loads((ROOT/'Scripts/Editor/stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'))
rng=random.Random(20261006);cache={};counts={};occupied={}
def insert(x,y):occupied.setdefault((int(x//260),int(y//260)),[]).append((x,y))
def nearby(x,y,r):
 gx,gy=int(x//260),int(y//260);n=math.ceil(r/260)
 return any((x-px)**2+(y-py)**2<r*r for i in range(gx-n,gx+n+1) for j in range(gy-n,gy+n+1) for px,py in occupied.get((i,j),[]))
resources=[a.get_actor_location() for a in actors if isinstance(a,unreal.ResourceBase)]
for a in actors:
 if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh:
  name=a.static_mesh_component.static_mesh.get_name().lower()
  if any(k in name for k in ('tree','pine','birch','oak')):
   p=a.get_actor_location();insert(p.x,p.y)
def prop(name,u,v,width,height,group,block=True):
 if name not in cache:
  solid='/Game/Game/Stage/StageHeroArt/Collision/SM_HeroSolid_'+name
  path=solid if unreal.EditorAssetLibrary.does_asset_exist(solid) else catalog[name]['path'] if name in catalog else '/Game/Game/Stage/Props/asset/static_mesh/'+name
  cache[name]=unreal.load_asset(path);assert cache[name],name
 m=cache[name];b=m.get_bounds();sx=width/max(b.box_extent.x*2,b.box_extent.y*2);sz=height/(b.box_extent.z*2);yaw=rng.uniform(0,360);ca,sa=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));x,y=L.xy(u,v)
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x-(ca*b.origin.x-sa*b.origin.y)*sx,y-(sa*b.origin.x+ca*b.origin.y)*sx,L.height(u,v)-15-(b.origin.z-b.box_extent.z)*sz),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
 a.set_actor_scale3d(unreal.Vector(sx,sx,sz));a.set_actor_label('Final_R5_%s_%04d'%(group,counts.get(group,0)));a.set_folder_path('Final Capital/Outer Forest/'+group);a.tags=[unreal.Name('SF_CapitalFinal'),unreal.Name('SF_CapitalFinal_R5')]
 c=a.static_mesh_component;c.set_static_mesh(m);c.set_collision_profile_name('BlockAll' if block else 'NoCollision');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,IGNORE)
 for i,p in enumerate(catalog.get(name,{}).get('materials',[])):
  if p:c.set_material(i,unreal.load_asset(p))
 counts[group]=counts.get(group,0)+1;return a
def safe(u,v):
 x,y=L.xy(u,v)
 return L.water_distance(u,v)>260 and L.road_distance(u,v)>1150 and L.ramp_distance(u,v)>1050 and L.base_distance(u,v,L.BEAST)>2250 and L.base_distance(u,v,L.CAPITAL)>2800 and not any((x-p.x)**2+(y-p.y)**2<600**2 for p in resources)
candidates=[]
for i in range(116):
 for j in range(92):
  u=-.062+i*.0098+rng.uniform(-.003,.003);v=-.062+j*.0123+rng.uniform(-.004,.004)
  rim=min(u,v,1-u,1-v)
  if rim>.12 or not safe(u,v):continue
  candidates.append((u,v))
rng.shuffle(candidates)
for u,v in candidates:
 if counts.get('Trees',0)>=650:break
 x,y=L.xy(u,v)
 if nearby(x,y,245):continue
 # Broken clumps give the edge depth without forming another straight wall.
 if rng.random()>.78+.15*math.sin(u*41+v*9)*math.cos(v*43):continue
 beast=u+v<.95
 names=['Tree_average_bare','Tree_small_bare','tree_dead01'] if beast and rng.random()<.34 else ['SM_GenericTreeD','SM_GenericTreeB','SM_PineVariantAGrowingB','SM_BirchTreeAdultA']
 prop(rng.choice(names),u,v,rng.uniform(450,650),rng.uniform(410,650),'Trees');insert(x,y)
 for j in range(rng.randint(1,2)):
  pu,pv=u+rng.uniform(-.005,.005),v+rng.uniform(-.006,.006)
  if safe(pu,pv):prop(rng.choice(['Bush_average','GrassClump01','GrassClump02']),pu,pv,rng.uniform(120,245),rng.uniform(65,140),'Understory',False)
 if rng.random()<.28:
  pu,pv=u+rng.uniform(-.006,.006),v+rng.uniform(-.006,.006)
  if safe(pu,pv):
   size=rng.uniform(150,340);prop(rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_07']),pu,pv,size,size*rng.uniform(.5,.8),'Rocks')

# Apply one common transform to visible geometry, water, collision and spline actors.
preserved=[];scaled=0;lane_checks=[]
for a in list(sub.get_all_level_actors()):
 if isinstance(a,(unreal.DirectionalLight,unreal.SkyLight,unreal.SkyAtmosphere,unreal.RecastNavMesh)):continue
 p=a.get_actor_location();s=a.get_actor_scale3d()
 if isinstance(a,(unreal.SFCommander,unreal.PlayerStart,unreal.ResourceBase,unreal.SFBuildingSpawnAnchor,unreal.UnitSpawner)):
  ground=L.world(*L.uv(p.x,p.y))[2]
  a.set_actor_location(unreal.Vector(p.x*S,p.y*S,p.z+ground*(S-1)),False,False)
  preserved.append({'label':a.get_actor_label(),'scale_before':[s.x,s.y,s.z],'scale_after':[a.get_actor_scale3d().x,a.get_actor_scale3d().y,a.get_actor_scale3d().z]})
 else:
  points=[]
  if isinstance(a,unreal.LanePath):
   sp=a.get_editor_property('spline');points=[sp.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD) for i in range(sp.get_number_of_spline_points())]
  a.set_actor_location(p*S,False,False);a.set_actor_scale3d(s*S);scaled+=1
  if points:
   error=max((sp.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD)-q*S).length() for i,q in enumerate(points));assert error<.1,error;lane_checks.append(error)

# The terrain colour mask is authored in design-space, so it must follow the geometry.
floor=unreal.load_asset(L.ART+'/M_CapitalFinal_Terrain');adjusted=0
for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
 if n.get_outer()==floor and 'float road=' in n.get_editor_property('code'):
  code=n.get_editor_property('code');assert 'CAPITAL_MAP_SCALE' not in code
  code=code.replace('N n;','N n; P /= 0.95; /* CAPITAL_MAP_SCALE */',1);n.set_editor_property('code',code);adjusted+=1
assert adjusted>=1,adjusted
ml.recompile_material(floor);unreal.EditorAssetLibrary.save_loaded_asset(floor)
table=unreal.load_asset('/Game/Core/DataTable/DT_SFStage');rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table));before=json.loads(json.dumps(rows))
row=next(r for r in rows if r['Name']=='Stage07');assert 'PT_SFStageFinal' in row['StageLevel']
row['Minimap']=re.sub(r'(?P<key>\b[XY]|CaptureHeight|BoundsPadding)=(?P<num>-?\d+(?:\.\d+)?)',lambda m:m['key']+'='+str(float(m['num'])*S),row['Minimap'])
assert all(a==b for a,b in zip(before,rows) if a['Name']!='Stage07')
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False));assert unreal.EditorAssetLibrary.save_loaded_asset(table)
assert unreal.EditorLoadingAndSavingUtils.save_map(w,L.MAP)
(OUT/'built.json').write_text(json.dumps({'factor':S,'added':counts,'scaled_actors':scaled,'preserved_gameplay_scales':preserved,'spline_errors_cm':lane_checks,'minimap':row['Minimap'],'actors':len(sub.get_all_level_actors())},indent=2),encoding='utf-8')
unreal.log('CAPITAL_REVISION5_SAVED')
