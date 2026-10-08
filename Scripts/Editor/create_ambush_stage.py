"""Create Stage06: occupied basin, three ambush lanes, broken southern retreat."""
import unreal,sys,json,math,random,re
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import ambush_layout as L
OUT=ROOT/'Saved/Ambush';OUT.mkdir(exist_ok=True)
assert not (OUT/'created.json').exists(),'Map already authored; use a targeted refinement.'
resume='-AmbushResume' in unreal.SystemLibrary.get_command_line()
assert not unreal.EditorAssetLibrary.does_asset_exist(L.MAP) or resume
IGNORE=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('IGNORE')))
BLOCK=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('BLOCK')))
source=ROOT/'ContentSource/Ambush';L.generate(source)
assets=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
unreal.EditorAssetLibrary.make_directory(L.ART)
def import_mesh(file):
 existing=unreal.load_asset(L.ART+'/'+file.stem)
 if existing:return existing
 t=unreal.AssetImportTask();t.filename=str(file);t.destination_path=L.ART;t.destination_name=file.stem;t.automated=True;t.save=True
 o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;o.automated_import_should_detect_type=False
 d=o.static_mesh_import_data;d.combine_meshes=True;d.auto_generate_collision=False;d.convert_scene=False;d.convert_scene_unit=False;t.options=o
 assets.import_asset_tasks([t]);m=unreal.load_asset(t.imported_object_paths[0]);assert isinstance(m,unreal.StaticMesh)
 m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 unreal.EditorAssetLibrary.save_loaded_asset(m);return m
mesh=import_mesh(source/'SM_AmbushBasin.obj')
def material(name,code,emissive=False,textures=False):
 m=unreal.load_asset(L.ART+'/'+name)
 if m:ml.delete_all_material_expressions(m)
 else:m=assets.create_asset(name,L.ART,unreal.Material,unreal.MaterialFactoryNew())
 def node(cls,**kw):
  n=ml.create_material_expression(m,cls)
  for k,v in kw.items():n.set_editor_property(k,v)
  return n
 inputs=[]
 for n in (['P','G','D','Nrm'] if textures else ['P']):
  i=unreal.CustomInput();i.set_editor_property('input_name',n);inputs.append(i)
 p=node(unreal.MaterialExpressionWorldPosition)
 c=node(unreal.MaterialExpressionCustom,code=code,inputs=inputs,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 ml.connect_material_expressions(p,'',c,'P')
 if textures:
  normal=node(unreal.MaterialExpressionPixelNormalWS);ml.connect_material_expressions(normal,'',c,'Nrm')
  uvinput=unreal.CustomInput();uvinput.set_editor_property('input_name','P')
  uv=node(unreal.MaterialExpressionCustom,code='return P.xy/520.;',inputs=[uvinput],output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT2)
  ml.connect_material_expressions(p,'',uv,'P')
  for key,path in [('G','greengrass/Grass005_1K-PNG_Color'),('D','ground04/Ground106_1K-PNG_Color')]:
   t=node(unreal.MaterialExpressionTextureSample,texture=unreal.load_asset('/Game/Game/Stage/Props/texture/'+path))
   ml.connect_material_expressions(uv,'',t,'UVs');ml.connect_material_expressions(t,'RGB',c,key)
 ml.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
 if emissive:ml.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 r=node(unreal.MaterialExpressionConstant,r=.85 if not emissive else .25);ml.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
 ml.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m);return m
noise='struct N {float h(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}float v(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(h(i),h(i+float2(1,0)),f.x),lerp(h(i+float2(0,1)),h(i+1),f.x),f.y);}};N n;'
code=noise+'float2 p=P.xy;float d=1e8;'
for route in L.SEGMENTS+[L.EXIT_SEGMENTS]:
 for a,b in route:
  dx,dy=b[0]-a[0],b[1]-a[1]
  code+='d=min(d,length(p-(float2(%f,%f)+clamp(dot(p-float2(%f,%f),float2(%f,%f))/%f,0,1)*float2(%f,%f))));'%(a[0],a[1],a[0],a[1],dx,dy,dx*dx+dy*dy,dx,dy)
cx,cy=L.xy(*L.CENTER)
code+='float r=length(p-float2(%f,%f));'%(cx,cy)
code+='float patch=n.v(p/400);float road=1-smoothstep(370,920,d+(patch-.5)*280);float clear=1-smoothstep(2450,3750,r);float3 grass=G*float3(.38,.40,.24);float3 dirt=D*float3(.67,.48,.29);float3 color=lerp(grass,dirt,max(road,clear*.86));float cliff=1-smoothstep(.5,.82,abs(Nrm.z));color=lerp(color,D*float3(.30,.29,.25),cliff);return color*(.82+.22*n.v(p/2300));'
floor=material('M_AmbushGround',code,textures=True)
wood=material('M_AmbushWeatheredWood',noise+'return lerp(float3(.033,.021,.013),float3(.12,.073,.037),n.v(float2(P.x/14,P.y/210)))*(.78+.22*n.v(P.xy/80));')
assert unreal.EditorLoadingAndSavingUtils.load_map(L.MAP) if resume else unreal.EditorLevelLibrary.new_level(L.MAP)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.EditorLevelLibrary.get_editor_world()
if resume:
 for a in list(sub.get_all_level_actors()):
  if a.get_actor_label().startswith('Ambush_'):sub.destroy_actor(a)
world.get_world_settings().set_editor_property('default_game_mode',unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Blueprints/GameMode/BP_SFGameMode'))
tag=unreal.Name('SF_Ambush');rng=random.Random(202610061);counts={};cache={}
catalog=json.loads((ROOT/'Scripts/Editor/stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'))
def spawn(cls,label,loc,group,yaw=0):
 a=sub.spawn_actor_from_class(cls,unreal.Vector(*loc),unreal.Rotator(pitch=0,yaw=yaw,roll=0));assert a,label
 a.set_actor_label(label);a.set_folder_path('Ambush/'+group);a.tags=[tag];return a
def meshactor(label,m,loc=(0,0,0),group='Terrain',mat=None,collision=True):
 a=spawn(unreal.StaticMeshActor,label,loc,group);c=a.static_mesh_component;c.set_static_mesh(m)
 if mat:
  for i in range(max(1,m.get_num_sections(0))):c.set_material(i,mat)
 c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
 if collision:c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,IGNORE)
 return a
ground=meshactor('Ambush_Terrain',mesh,mat=floor)
ground.static_mesh_component.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,BLOCK)
def getmesh(name):
 if name not in cache:
  solid='/Game/Game/Stage/StageHeroArt/Collision/SM_HeroSolid_'+name
  p=solid if unreal.EditorAssetLibrary.does_asset_exist(solid) else catalog[name]['path'] if name in catalog else name
  if not p.startswith('/'):p='/Game/Game/Stage/Props/asset/static_mesh/'+p
  cache[name]=unreal.load_asset(p);assert cache[name],p
 return cache[name]
def prop(name,u,v,width,height,group,yaw=None,mat=None,collision=True,z=None):
 m=getmesh(name);b=m.get_bounds();sx=width/max(2*b.box_extent.x,2*b.box_extent.y);sz=height/(2*b.box_extent.z)
 yaw=rng.uniform(0,360) if yaw is None else yaw;ca,sa=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));x,y=L.xy(u,v)
 z=L.height(u,v)-8 if z is None else z
 loc=(x-ca*b.origin.x*sx+sa*b.origin.y*sx,y-sa*b.origin.x*sx-ca*b.origin.y*sx,z-(b.origin.z-b.box_extent.z)*sz)
 counts[group]=counts.get(group,0)+1
 a=meshactor('Ambush_%s_%04d'%(group,counts[group]),m,loc,group,mat,collision)
 a.set_actor_scale3d(unreal.Vector(sx,sx,sz));a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
 if not mat and name in catalog:
  for i,p in enumerate(catalog[name]['materials']):
   if p:a.static_mesh_component.set_material(i,unreal.load_asset(p))
 return a

# Dense ridge forest, with a clear basin and readable squad-width approaches.
for i in range(83):
 for j in range(75):
  u=-.04+i*.0132+rng.uniform(-.004,.004);v=-.04+j*.0146+rng.uniform(-.004,.004)
  if L.protected(u,v) or rng.random()>.91:continue
  name=rng.choice(['Tree_average_bare','Tree_small_bare','tree_dead01']) if rng.random()<.30 else rng.choice(['SM_PineVariantAGrowingB','SM_PineVariantAGrowingB','SM_GenericTreeD','SM_BirchTreeAdultA'])
  prop(name,u,v,rng.uniform(390,570),rng.uniform(420,660),'RidgeForest')
  if rng.random()<.40:prop('Bush_average',u,v,rng.uniform(115,200),rng.uniform(65,135),'Understory',collision=False)
# Layered natural rock faces around the basin, with genuine openings at all three ramps.
for i in range(160):
 a=i*2*math.pi/160;rad=rng.uniform(4280,4920);cx,cy=L.xy(*L.CENTER);u,v=L.uv(cx+math.cos(a)*rad,cy+math.sin(a)*rad)
 if L.road_distance(u,v)<1120 or L.exit_distance(u,v)<1000 or L.ravine_q(u,v)<1.15:continue
 prop(rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_09']),u,v,rng.uniform(490,860),rng.uniform(400,730),'BasinCliffs')
for i in range(100):
 a=i*2*math.pi/100;u=.5+.205*rng.uniform(.97,1.08)*math.cos(a);v=.87+.054*rng.uniform(.96,1.08)*math.sin(a)
 if L.exit_distance(u,v)<760:continue
 prop(rng.choice(['SM_Rocks_01','SM_Rocks_07','SM_Rocks_09']),u,v,rng.uniform(300,590),rng.uniform(500,1000),'RavineCliffs')
for j in range(1850):
 u,v=rng.uniform(-.02,1.02),rng.uniform(-.02,1.02);d=L.road_distance(u,v)
 if L.ravine_q(u,v)<1.12 or L.center_distance(u,v)<2500 or min(d,L.exit_distance(u,v))<610:continue
 prop(rng.choice(['GrassClump01','GrassClump02','Bush_average']),u,v,rng.uniform(80,180),rng.uniform(40,95),'GroundCover',collision=False)
# Half-ruined captured compound; gaps intentionally align to north/west/east fronts.
for i in range(22):
 a=i*2*math.pi/22;u=L.CENTER[0]+.101*math.cos(a);v=L.CENTER[1]+.096*math.sin(a)
 if L.road_distance(u,v)<840 or L.exit_distance(u,v)<740:continue
 prop('fence',u,v,rng.uniform(390,470),rng.uniform(160,220),'CapturedPalisade',yaw=-math.degrees(a))
for u,v in [(.44,.61),(.56,.61),(.42,.545)]:
 prop('Log_big_regular',u,v,290,95,'CapturedDebris')
for k,(u,v) in enumerate(L.CAMPS):
 prop('tent_red',u-.028,v,450,330,'BeastCamps')
 for j in (-1,1):
  prop('fence',u+j*.031,v+.036,410,225,'BeastPalisade',yaw=0)
  prop('SM_HeroBeastSkull',u+j*.023,v+.027,115,165,'BoneStandards',mat=unreal.load_asset('/Game/Game/Stage/StageHeroArt/M_HeroBeastBone'),z=L.height(u+j*.023,v+.027)+150)
 for j in range(5):
  pu,pv=u+rng.uniform(-.045,.045),v+rng.uniform(-.045,.045)
  if L.road_distance(pu,pv)>820:prop('Branch_average_bare',pu,pv,240,170,'BeastDebris')

cube=unreal.load_asset('/Engine/BasicShapes/Cube')
def box(label,loc,dimensions,yaw=0,pitch=0,collision=True):
 a=meshactor(label,cube,loc,'Retreat Bridge',wood,collision);a.set_actor_scale3d(unreal.Vector(*(s/100 for s in dimensions)));a.set_actor_rotation(unreal.Rotator(pitch=pitch,yaw=yaw,roll=0),False);return a
intact=[];broken=[]
# A real gap, with fractured planks and beams falling into the dry gorge.
for j in range(28):
 v=.795+(j+.5)*.15/28;x,y=L.xy(.5,v)
 full=box('Ambush_IntactBridge_%02d'%j,(x,y,172),(L.H*.15/28-2,1150+rng.uniform(-15,15),55));full.set_actor_hidden_in_game(True);full.set_actor_enable_collision(False);intact.append(full)
 if j<7 or j>20:
  fractured=box('Ambush_BrokenBridge_%02d'%j,(x,y+rng.uniform(-35,35),172-rng.uniform(0,45)),(L.H*.15/28-8,1100+rng.uniform(-100,100),60),yaw=rng.uniform(-5,5),pitch=rng.uniform(-9,9));broken.append(fractured)
 elif j%2==0:
  debris=box('Ambush_FallenPlank_%02d'%j,(x+rng.uniform(-180,180),rng.uniform(-550,550),-1250+rng.uniform(0,550)),(120,1000,70),yaw=rng.uniform(-45,45),pitch=rng.uniform(-35,35));broken.append(debris)
for side in (-1,1):
 for v in (.797,.83,.91,.943):
  x,y=L.xy(.5,v);box('Ambush_BridgePier_%s_%s'%(side,v),(x,side*635,-300),(150,150,1200))
 for v in (.81,.935):
  x,y=L.xy(.5,v);box('Ambush_BridgeRail_%s_%s'%(side,v),(x,side*630,325),(650,90,110),pitch=rng.uniform(-7,7))
# Null area only at the ravine floor, below the walkable intact bridge.
null=spawn(unreal.NavModifierVolume,'Ambush_RavineNoWalk',(*L.xy(.5,.87),-1200),'Navigation')
null.set_editor_property('area_class',unreal.NavArea_Null);_,e=null.get_actor_bounds(False);null.set_actor_scale3d(unreal.Vector(1100/e.x,4900/e.y,1180/e.z))

anchorcls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Structure/Buildings/BuildingSpawner/BP_BuildingSpawn')
def anchor(label,kind,definition,at):
 a=spawn(anchorcls,label,L.world(*at),'Gameplay/Buildings',-90)
 for key,val in [('team_index',0),('building_type',kind),('initial_tier_index',0),('preview_definition',unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/Human/'+definition))]:a.set_editor_property(key,val)
 return a
anchor('Ambush_CommandPost',unreal.SFBuildingType.NEXUS,'DA_HumanNexus',L.CENTER)
anchor('Ambush_CapturedBarrack',unreal.SFBuildingType.BARRACKS,'DA_HumanBarrack',(.435,.63))
anchor('Ambush_SupplyHouse',unreal.SFBuildingType.HOUSE,'DA_HumanHouse',(.57,.63))
spawn(unreal.PlayerStart,'Ambush_PlayerStart',L.world(*L.START,offset=100),'Gameplay/Player')
commander=spawn(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Game/Race_Etherial/Character/Unit_Commander/BP_Ether_Commander'),'Ambush_Commander',L.world(*L.START,offset=100),'Gameplay/Player')
commander.set_editor_property('bMyCommander',True);commander.get_component_by_class(unreal.CombatTargetComponent).set_editor_property('team_index',0)
for resource,points in [('Mineral',[(.37,.60),(.37,.63)]),('Ether',[(.64,.60),(.64,.63)])]:
 for k,(u,v) in enumerate(points):
  for j in range(2):
   cls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Structure/Resources/BP_'+resource+('_S' if j else ''))
   spawn(cls,'Ambush_%s_%d_%d'%(resource,k,j),L.world(u+j*.017,v),'Gameplay/Resources')
spawners=[]
for i,route in enumerate(L.ROUTES):
 lane=spawn(unreal.LanePath,'Ambush_Lane_%d'%i,L.world(*route[0]),'Gameplay/Lanes');lane.set_editor_property('team_index',1);lane.set_editor_property('lane_half_width',560.)
 sp=lane.get_editor_property('spline');sp.clear_spline_points(False)
 for p in route:sp.add_spline_point(unreal.Vector(*L.world(*p)),unreal.SplineCoordinateSpace.WORLD,False)
 for j in range(len(route)):sp.set_spline_point_type(j,unreal.SplinePointType.LINEAR,False)
 sp.update_spline()
 for j,unit in enumerate(['Brawler','Hunter']):
  a=spawn(unreal.UnitSpawner,'Ambush_Spawner_%d_%s'%(i,unit),L.world(*route[0],offset=95),'Gameplay/AmbushWaves')
  cls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Units/Beast_Fox/BP_Fox_'+unit)
  for key,val in [('unit_class',cls),('unit_row_name',unit),('team_index',1),('lane',lane),('spawn_interval',32.+i*4+j*13),('initial_delay',12.+i*9+j*15),('max_alive',4),('max_total',10),('spawn_count_per_cycle',1),('bStartActive',False),('bRequireOwnerBuilding',False),('bStopWhenOwnerBuildingDestroyed',False)]:a.set_editor_property(key,val)
  spawners.append(a)
director=spawn(unreal.SFAmbushDefenseDirector,'Ambush_DefenseObjective',(0,0,0),'Gameplay/Objective')
director.set_editor_property('spawners',spawners);director.set_editor_property('intact_bridge',intact);director.set_editor_property('broken_bridge',broken)
nav=spawn(unreal.NavMeshBoundsVolume,'Ambush_NavBounds',(0,0,300),'Navigation');_,e=nav.get_actor_bounds(False);nav.set_actor_scale3d(unreal.Vector(10500/e.x,11600/e.y,3000/e.z))
sun=spawn(unreal.DirectionalLight,'Ambush_Sun',(0,0,2500),'Lighting');sun.set_actor_rotation(unreal.Rotator(pitch=-53,yaw=-32,roll=0),False);sun.light_component.set_editor_property('intensity',2.7);sun.light_component.set_light_color(unreal.LinearColor(1.,.91,.76,1.))
sky=spawn(unreal.SkyLight,'Ambush_Sky',(0,0,2000),'Lighting');sky.light_component.set_editor_property('intensity',.78)
spawn(unreal.SkyAtmosphere,'Ambush_Atmosphere',(0,0,0),'Lighting')
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-13500,0,24000),unreal.Rotator(pitch=-62,yaw=0,roll=0))
assert unreal.EditorLoadingAndSavingUtils.save_map(world,L.MAP)
table=unreal.load_asset('/Game/Core/DataTable/DT_SFStage');rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table));(OUT/'stage_before.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
row=next(r for r in rows if r['Name']=='Stage06');assert row['StageLevel']=='None' or 'PT_SFStageAmbush' in row['StageLevel']
row.update(StageLevel=L.MAP+'.PT_SFStageAmbush',PlayerRace='Human',TeamSetups=['(TeamIndex=1,Race=Beast)'],StartingMineral=650,StartingEther=180)
row['BuildingRules']=[r for r in row['BuildingRules'] if 'BuildingId=""' not in r]
row['Minimap']='(bAutoDetectBounds=False,WorldMin=(X=-10500,Y=-11600),WorldMax=(X=10500,Y=11600),CaptureHeight=19000,ClockwiseQuarterTurns=0,BoundsPadding=200)'
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False));assert unreal.EditorAssetLibrary.save_loaded_asset(table)
(OUT/'created.json').write_text(json.dumps({'map':L.MAP,'counts':counts,'actors':len(sub.get_all_level_actors()),'stage':row},ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('AMBUSH_STAGE_CREATED')
