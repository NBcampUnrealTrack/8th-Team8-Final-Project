"""Revision 2: lower ridge relief, complete the camp, and use a safe map camera."""
import unreal,sys,math,random,json,shutil
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import ambush_layout as L
OUT=ROOT/'Saved/Ambush/Revision2';OUT.mkdir(parents=True,exist_ok=True)
assert not (OUT/'authored.json').exists(),'Revision already applied.'
shutil.copy2(ROOT/'Content/Game/Stage/PT_SFStageAmbush.umap',OUT/'Before.umap')
w=unreal.EditorLoadingAndSavingUtils.load_map(L.MAP);assert w
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors=list(sub.get_all_level_actors());assets=unreal.AssetToolsHelpers.get_asset_tools()
IGNORE=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('IGNORE')))
for a in actors:
 name=a.get_actor_label();p=a.get_actor_location()
 if not name.startswith('Ambush_') or name in ('Ambush_Terrain','Ambush_NavBounds','Ambush_Sun','Ambush_Sky','Ambush_Atmosphere','Ambush_DefenseObjective'):continue
 if name.startswith('Ambush_Lane_'):
  sp=a.get_editor_property('spline');points=[]
  for i in range(sp.get_number_of_spline_points()):
   q=sp.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD);q.z=L.lower_height(q.z);points.append(q)
  for i,q in enumerate(points):sp.set_location_at_spline_point(i,q,unreal.SplineCoordinateSpace.WORLD,False)
  sp.update_spline();continue
 if name.startswith(('Ambush_IntactBridge_','Ambush_BrokenBridge_','Ambush_BridgeRail_')):
  p.z+=L.lower_height(200)-200
 elif name.startswith('Ambush_FallenPlank_'):p.z=L.lower_height(p.z)
 elif name.startswith('Ambush_BridgePier_') or name=='Ambush_RavineNoWalk':
  center,extent=a.get_actor_bounds(False);bottom=L.lower_height(center.z-extent.z)
  top=(center.z+extent.z+L.lower_height(200)-200) if name.startswith('Ambush_BridgePier_') else L.lower_height(center.z+extent.z)
  s=a.get_actor_scale3d();s.z*=(top-bottom)/(2*extent.z);a.set_actor_scale3d(s);p.z=(top+bottom)/2
 else:
  ref=a.get_actor_bounds(False)[0] if isinstance(a,unreal.StaticMeshActor) else p
  u,v=L.uv(ref.x,ref.y);p.z+=L.height(u,v)-L.original_height(u,v)
  if name.startswith('Ambush_RidgeForest_'):
   b=a.static_mesh_component.static_mesh.get_bounds();s=a.get_actor_scale3d();bottom=(b.origin.z-b.box_extent.z)*s.z;s.z*=.9;p.z+=bottom-(b.origin.z-b.box_extent.z)*s.z;a.set_actor_scale3d(s)
 a.set_actor_location(p,False,True)

def import_mesh(file):
 task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=L.ART;task.destination_name=file.stem;task.automated=True;task.save=True;task.replace_existing=True
 opt=unreal.FbxImportUI();opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.automated_import_should_detect_type=False
 data=opt.static_mesh_import_data;data.combine_meshes=True;data.auto_generate_collision=False;data.convert_scene=False;data.convert_scene_unit=False;task.options=opt
 assets.import_asset_tasks([task]);m=unreal.load_asset(L.ART+'/'+file.stem);assert m
 m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);unreal.EditorAssetLibrary.save_loaded_asset(m);return m
source=ROOT/'ContentSource/Ambush';L.generate(source);terrain=import_mesh(source/'SM_AmbushBasin.obj')
floor=unreal.load_asset(L.ART+'/M_AmbushGround')
for expr in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
 if expr.get_outer()!=floor:continue
 code=expr.get_editor_property('code');code=code.replace('clear*.86','clear*(.42+.24*patch)').replace('float3(.67,.48,.29)','float3(.55,.45,.32)').replace('float3(.38,.40,.24)','float3(.43,.47,.29)');expr.set_editor_property('code',code)
unreal.MaterialEditingLibrary.recompile_material(floor);unreal.EditorAssetLibrary.save_loaded_asset(floor)
next(a for a in actors if a.get_actor_label()=='Ambush_Terrain').static_mesh_component.set_material(0,floor)

# Seven irregular pointed logs form a short, solid breastwork.
vs=[];fs=[];rng=random.Random(611)
for j in range(7):
 x,y=0,(j-3)*62;radius=26+rng.uniform(-3,3);h=200+rng.uniform(-30,40);base=len(vs)
 for z in (0,h-45):
  for k in range(8):
   theta=k*math.pi/4;vs.append((x+radius*math.cos(theta),y+radius*math.sin(theta),z))
 vs.append((x+5,y-4,h));tip=base+16
 for k in range(8):
  n=(k+1)%8;fs.extend([(base+k,base+n,base+8+n),(base+k,base+8+n,base+8+k),(base+8+k,base+8+n,tip)])
L.write_mesh(source/'SM_AmbushPalisade.obj',vs,fs);palisade=import_mesh(source/'SM_AmbushPalisade.obj')
wood=unreal.load_asset(L.ART+'/M_AmbushWeatheredWood');catalog=json.loads((ROOT/'Scripts/Editor/stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'));cache={};counts={}
def prop(name,u,v,width,height,group,yaw=None,block=True,mat=None):
 if name not in cache:
  paths=[L.ART+'/'+name,'/Game/Game/Stage/StageHeroArt/Collision/SM_HeroSolid_'+name,catalog.get(name,{}).get('path',''),'/Game/Game/Stage/Props/asset/static_mesh/'+name,'/Game/Game/Stage/Props/asset/'+name]
  cache[name]=next((unreal.load_asset(p) for p in paths if p and unreal.EditorAssetLibrary.does_asset_exist(p)),None);assert cache[name],name
 m=cache[name];b=m.get_bounds();sx=width/max(b.box_extent.x*2,b.box_extent.y*2);sz=height/(b.box_extent.z*2);yaw=rng.uniform(0,360) if yaw is None else yaw;c,s=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));x,y=L.xy(u,v)
 p=unreal.Vector(x-(c*b.origin.x-s*b.origin.y)*sx,y-(s*b.origin.x+c*b.origin.y)*sx,L.height(u,v)-6-(b.origin.z-b.box_extent.z)*sz)
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,p,unreal.Rotator(pitch=0,yaw=yaw,roll=0));counts[group]=counts.get(group,0)+1;a.set_actor_label('Ambush_R2_%s_%03d'%(group,counts[group]));a.set_folder_path('Ambush/Detail/'+group);a.tags=[unreal.Name('SF_Ambush')];a.set_actor_scale3d(unreal.Vector(sx,sx,sz));comp=a.static_mesh_component;comp.set_static_mesh(m);comp.set_collision_profile_name('BlockAll' if block else 'NoCollision');comp.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,IGNORE)
 for i in range(max(1,m.get_num_sections(0))):
  if mat:comp.set_material(i,mat)
  elif name in catalog and i<len(catalog[name]['materials']) and catalog[name]['materials'][i]:comp.set_material(i,unreal.load_asset(catalog[name]['materials'][i]))
 return a
resources=[a.get_actor_location() for a in actors if isinstance(a,unreal.ResourceBase)]
def safe(u,v,road=780):
 x,y=L.xy(u,v)
 return L.road_distance(u,v)>road and L.exit_distance(u,v)>820 and L.ravine_q(u,v)>1.12 and all(math.hypot(x-p.x,y-p.y)>630 for p in resources)
# Camp pockets leave the central construction area and all approaches open.
for u,v in [(.407,.565),(.594,.56),(.423,.68),(.586,.683)]:
 if not safe(u,v):continue
 prop('tent_red',u,v,380,245,'CapturedCamp',yaw=90)
 for j in range(4):
  prop('SM_Wooden_Crate_01',u+rng.uniform(-.012,.012),v+rng.uniform(.012,.025),rng.uniform(70,110),rng.uniform(65,100),'Supplies')
 prop('SM_Wooden_Barrel_01',u+.012,v,85,110,'Supplies')
for i in range(38):
 angle=i*math.pi*2/38;u=.5+.123*math.cos(angle);v=.565+.118*math.sin(angle)
 if not safe(u,v):continue
 prop('SM_AmbushPalisade',u,v,360,200,'Breastworks',yaw=-math.degrees(angle),mat=wood)
for u,v in L.CAMPS:
 for side in (-1,1):
  for j in range(2):prop('SM_AmbushPalisade',u+side*(.032+j*.018),v+.036,350,250,'BeastFortifications',yaw=90,mat=wood)
  prop('SM_HeroBeastSkull',u+side*.034,v+.017,125,175,'BeastStandards',mat=unreal.load_asset('/Game/Game/Stage/StageHeroArt/M_HeroBeastBone'))
# Small stones and low grasses soften shoulders without impeding unit movement.
for i in range(1450):
 u,v=rng.uniform(.14,.87),rng.uniform(.14,.81);d=min(L.road_distance(u,v),L.exit_distance(u,v))
 if not safe(u,v,road=610) or d>1350 or L.center_distance(u,v)<1900:continue
 name=rng.choice(['GrassClump01','GrassClump02','Bush_average'])
 prop(name,u,v,rng.uniform(75,150),rng.uniform(35,80),'VergePlants',block=False)
 if rng.random()<.24:prop(rng.choice(['SM_Rocks_01','SM_Rocks_07']),u+.003,v,rng.uniform(55,120),rng.uniform(35,75),'VergeStones',block=False)
for u,v in [(.455,.775),(.545,.782),(.46,.951),(.54,.951)]:
 prop('Log_big_knotty',u,v,310,120,'RetreatWreckage')
 prop('SM_Wooden_Crate_02',u+.006,v+.008,125,95,'RetreatWreckage')

# Map-local camera defaults keep the minimum zoom safely above the lowered canopy.
camera_src='/Game/Game/IN_UI/JSYBluePrints/BP_SFRTSCameraPawn';gm_src='/Game/Core/Blueprints/GameMode/BP_SFGameMode'
for src,name in [(camera_src,'BP_AmbushCamera'),(gm_src,'BP_AmbushGameMode')]:
 dest=L.ART+'/'+name
 if not unreal.EditorAssetLibrary.does_asset_exist(dest):assert unreal.EditorAssetLibrary.duplicate_asset(src,dest)
 cls=unreal.EditorAssetLibrary.load_blueprint_class(dest);cdo=unreal.get_default_object(cls)
 if name=='BP_AmbushCamera':
  before={p:cdo.get_editor_property(p) for p in ['initial_camera_distance','minimum_camera_distance','maximum_camera_distance']}
  for key,value in [('initial_camera_distance',2100.),('minimum_camera_distance',1600.),('maximum_camera_distance',3400.)]:cdo.set_editor_property(key,value)
 else:cdo.set_editor_property('default_pawn_class',unreal.EditorAssetLibrary.load_blueprint_class(L.ART+'/BP_AmbushCamera'))
 bp=unreal.load_asset(dest);unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
w.get_world_settings().set_editor_property('default_game_mode',unreal.EditorAssetLibrary.load_blueprint_class(L.ART+'/BP_AmbushGameMode'))
unreal.EditorLoadingAndSavingUtils.save_map(w,L.MAP)
(OUT/'authored.json').write_text(json.dumps({'details':counts,'camera_before':before},indent=2),encoding='utf-8')
