"""Create the editable Etherian last-stand blockout. Refuses to overwrite a map.
Run with the UE5.5 Python commandlet. Review the generated level in the editor.
"""
import unreal,sys,json,math,random
from pathlib import Path
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).parent))
import capital_final_layout as L
IGNORE=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('IGNORE')))
BLOCK=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('BLOCK')))
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/CapitalFinal';OUT.mkdir(exist_ok=True)
resume='-CapitalResume' in unreal.SystemLibrary.get_command_line() and not (OUT/'creation.json').exists()
assert not unreal.EditorAssetLibrary.does_asset_exist(L.MAP) or resume,'Map already exists; preserve manual edits.'
source=ROOT/'ContentSource/CapitalFinal';L.generate(source)
unreal.EditorAssetLibrary.make_directory(L.ART)
assets=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
def import_mesh(file):
 existing=unreal.load_asset(L.ART+'/'+file.stem)
 if existing:return existing
 t=unreal.AssetImportTask();t.filename=str(file);t.destination_path=L.ART;t.destination_name=file.stem;t.automated=True;t.save=True
 o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;o.automated_import_should_detect_type=False
 d=o.static_mesh_import_data;d.combine_meshes=True;d.auto_generate_collision=False;d.convert_scene=False;d.convert_scene_unit=False;t.options=o
 assets.import_asset_tasks([t]);m=unreal.load_asset(t.imported_object_paths[0]);assert isinstance(m,unreal.StaticMesh)
 m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 unreal.EditorAssetLibrary.save_loaded_asset(m);return m
meshes={f.stem:import_mesh(f) for f in sorted(source.glob('*.obj'))}
assert meshes['SM_CapitalTerrain'].get_bounding_box().max.z>500
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
noise='''struct N {float h(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}float v(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(h(i),h(i+float2(1,0)),f.x),lerp(h(i+float2(0,1)),h(i+1),f.x),f.y);}};N n;'''
code=noise+'float2 p=P.xy;float d=1e8;\n'
for a,b in L.SEGMENTS:
 dx,dy=b[0]-a[0],b[1]-a[1]
 code+='d=min(d,length(p-(float2(%f,%f)+clamp(dot(p-float2(%f,%f),float2(%f,%f))/%f,0,1)*float2(%f,%f))));\n'%(a[0],a[1],a[0],a[1],dx,dy,dx*dx+dy*dy,dx,dy)
cx,cy=L.xy(*L.CAPITAL);bx,by=L.xy(*L.BEAST)
code+='''float patch=n.v(p/420),macro=n.v(p/2600),edge=(patch-.5)*330;
float road=(1-smoothstep(370,810,d+edge))*(.64+.25*patch);
float ether=smoothstep(-5500,9000,p.y-p.x*.55);
float3 turf=G*lerp(float3(.63,.57,.40),float3(.52,.83,.77),ether);
float3 dirt=D*lerp(float3(.75,.49,.29),float3(.77,.85,.83),ether);
float3 color=lerp(turf,dirt,max(road,smoothstep(.65,.87,macro)*.3));
'''
code+='float city=1-smoothstep(3200,5900,length(p-float2(%f,%f))+(n.v(p/590)-.5)*1550);'% (cx,cy)
code+='float camp=1-smoothstep(1000,2350,length(p-float2(%f,%f)));'% (bx,by)
code+='''float2 tiles=p/float2(125,195);tiles.y+=floor(tiles.x)*.5;
float2 cell=frac(tiles);float joint=smoothstep(.014,.04,min(min(cell.x,1-cell.x),min(cell.y,1-cell.y)));
float slab=n.h(floor(tiles));float wear=n.v(p/120);
float3 stone=lerp(float3(.045,.075,.065),float3(.145,.20,.17),joint)*(.7+.3*slab+.18*wear);
float cracks=smoothstep(.57,.79,wear)*.34;
stone=lerp(stone,turf,cracks);
float spread=city*(1-smoothstep(80,260,P.z));color=lerp(color,stone,spread*(.64+.28*patch));color=lerp(color,D*float3(.63,.42,.26),camp*.63);
color=lerp(D*float3(.34,.39,.29),color,smoothstep(.45,.85,abs(Nrm.z)));
return color*(.88+.19*macro);'''
from capital_final_waterworks import water_material_code
code=code.replace('color=lerp(D*float3(.34,.39,.29)',water_material_code()+'color=lerp(D*float3(.34,.39,.29)')
floor_mat=material('M_CapitalFinal_Terrain',code,textures=True)
stone=material('M_CapitalFinal_Ivory',noise+'return float3(.55,.57,.48)*(.86+.17*n.v(P.xy/160));')
crystal=material('M_CapitalFinal_Crystal','float t=.5+.5*sin(P.z*.035+P.x*.016);return float3(.01,.62,.58)*(1.0+t*.65);',True)

assert unreal.EditorLoadingAndSavingUtils.load_map(L.MAP) if resume else unreal.EditorLevelLibrary.new_level(L.MAP)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.EditorLevelLibrary.get_editor_world()
if resume:
 for a in sub.get_all_level_actors():
  if unreal.Name('SF_CapitalFinal') in a.tags:sub.destroy_actor(a)
world.get_world_settings().set_editor_property('default_game_mode',unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Blueprints/GameMode/BP_SFGameMode'))
tag=unreal.Name('SF_CapitalFinal');rng=random.Random(20261001);counts={};cache={}
catalog=json.loads((ROOT/'Scripts/Editor/stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'))
def spawn(cls,label,loc,group,yaw=0):
 a=sub.spawn_actor_from_class(cls,unreal.Vector(*loc),unreal.Rotator(pitch=0,yaw=yaw,roll=0));assert a,label
 a.set_actor_label(label);a.set_folder_path('Final Capital/'+group);a.tags=[tag];return a
def meshactor(label,m,loc=(0,0,0),group='Terrain',mat=None,collision=True):
 a=spawn(unreal.StaticMeshActor,label,loc,group);c=a.static_mesh_component;c.set_static_mesh(m)
 if mat:
  for i in range(max(1,m.get_num_sections(0))):c.set_material(i,mat)
 c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
 if collision:c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,IGNORE)
 return a
ground=meshactor('Final_Terrain',meshes['SM_CapitalTerrain'],mat=floor_mat)
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
 a=meshactor('Final_%s_%04d'%(group,counts[group]),m,loc,group,mat,collision)
 a.set_actor_scale3d(unreal.Vector(sx,sx,sz));a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
 if not mat and name in catalog:
  for i,p in enumerate(catalog[name]['materials']):
   if p:a.static_mesh_component.set_material(i,unreal.load_asset(p))
 return a
water=unreal.load_asset(L.ART+'/MI_FinalStage2Water');assert water
for k,(u,v,rx,ry) in enumerate(L.LAKES):
 meshactor('Final_Lake_%d'%k,meshes['SM_CapitalWater%d'%k],group='Water',mat=water,collision=False)
 block=meshactor('Final_LakeCollision_%d'%k,meshes['SM_CapitalLakeBlock%d'%k],group='Collision')
 block.set_actor_hidden_in_game(True);block.static_mesh_component.set_visibility(False)
 null=spawn(unreal.NavModifierVolume,'Final_LakeNoWalk_%d'%k,(*L.xy(u,v),950),'Navigation')
 null.set_actor_scale3d(unreal.Vector(ry*L.H/100*1.2,rx*L.W/100*1.2,2));null.set_editor_property('area_class',unreal.NavArea_Null)
 for j in range(34):
  a=j*2*math.pi/34;r=1.10+rng.uniform(.025,.12)
  if j%5==0:continue
  uu=u+rx*r*math.cos(a);vv=v+ry*r*math.sin(a)
  if L.road_distance(uu,vv)>930:prop(rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_07']),uu,vv,rng.uniform(170,320),rng.uniform(90,190),'LakeBanks')

# Small trees and dense understory define the route; every prop remains editable.
for i in range(49):
 for j in range(40):
  u=-.06+i*.023+rng.uniform(-.007,.007);v=-.06+j*.029+rng.uniform(-.008,.008)
  if L.protected(u,v,240):continue
  if rng.random()>.70:continue
  beast=u+v<.94
  trees=['Tree_average_bare','tree_dead01','Tree_small_bare'] if beast and rng.random()<.65 else ['SM_GenericTreeD','SM_GenericTreeB','SM_PineVariantAGrowingB','SM_BirchTreeAdultA']
  prop(rng.choice(trees),u,v,rng.uniform(390,650),rng.uniform(370,640),'BeastWoodland' if beast else 'EtherWoodland')
  for q in range(2):
   du,dv=u+rng.uniform(-.008,.008),v+rng.uniform(-.01,.01)
   prop('Bush_average',du,dv,rng.uniform(130,250),rng.uniform(80,150),'Understory',collision=False)
  if rng.random()<.30:prop(rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_09']),u+.007,v-.005,rng.uniform(230,420),rng.uniform(170,330),'ForestRocks')
for i in range(360):
 u,v=rng.uniform(.03,.97),rng.uniform(.04,.96)
 if any(L.pond_q(u,v,k)<1.13 for k in range(3)) or L.base_distance(u,v,L.CAPITAL)<2300:continue
 if L.road_distance(u,v)<500:continue
 prop(rng.choice(['GrassClump01','GrassClump02','SM_GrassLumpD']),u,v,rng.uniform(70,140),rng.uniform(35,75),'GroundPlants',collision=False)
# Crystals embedded in the capital-side banks and ancient woodland.
for i in range(65):
 u,v=rng.uniform(.48,.96),rng.uniform(.39,.98)
 if L.protected(u,v,120):continue
 prop('/Game/Game/Mineral/BlueD/Blue_C',u,v,rng.uniform(130,260),rng.uniform(110,290),'EtherCrystals',mat=crystal)
# Visible palisades, fallen logs and skull standards along the occupied approaches.
for i in range(25):
 u=.075+i*.026;v=.075+.011*math.sin(i*.7)
 if u>.46:break
 prop('fence',u,v,460,250,'BeastPalisade',yaw=90)
 if i%3==0:
  prop('SM_HeroBeastSkull',u,v,140,160,'BeastBoneStandards',mat=unreal.load_asset('/Game/Game/Stage/StageHeroArt/M_HeroBeastBone'),z=L.height(u,v)+230)
for i in range(18):
 u,v=rng.uniform(.08,.39),rng.uniform(.27,.79)
 if L.protected(u,v,140):continue
 prop(rng.choice(['Log_big_regular','Branch_average_bare']),u,v,rng.uniform(220,450),rng.uniform(80,150),'BeastDebris')
for u,v in [(.10,.17),(.28,.065),(.08,.27)]:prop('tent_red',u,v,650,490,'BeastCamp',collision=True)
# The city wall backs the defenders; broad broken gates face the approach roads.
for i in range(13):
 u=.64+i*.027;v=.977+.010*math.sin(i*.7)
 prop('SM_RuinedWallA',u,v,700,410,'CapitalRearWall',yaw=0,mat=stone)
for i in range(8):prop('SM_RuinedWallA',.985,.69+i*.033,690,430,'CapitalEastWall',yaw=90,mat=stone)
for u,v in [(.72,.70),(.84,.67),(.665,.80),(.70,.91)]:
 prop('SM_RuinedWallA',u,v,850,330,'LastDefense',yaw=35,mat=stone)
 prop('/Game/Game/Mineral/BlueD/Blue_C',u+.008,v,190,420,'GateCrystals',mat=crystal)

anchorcls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Structure/Buildings/BuildingSpawner/BP_BuildingSpawn')
def anchor(label,team,kind,definition,at,tier=0):
 a=spawn(anchorcls,label,L.world(*at),'Gameplay/Buildings',90 if team else -90)
 for key,value in [('team_index',team),('building_type',kind),('initial_tier_index',tier),('preview_definition',unreal.load_asset(definition))]:a.set_editor_property(key,value)
 a.set_actor_location_and_rotation(unreal.Vector(*L.world(*at)),a.get_actor_rotation(),False,False)
 return a
anchor('Final_EtherNexus',0,unreal.SFBuildingType.NEXUS,'/Game/Core/Primary/Buildings/Definitions/Human/DA_HumanNexus',L.CAPITAL)
anchor('Final_BeastNexus',1,unreal.SFBuildingType.NEXUS,'/Game/Core/Primary/Buildings/Definitions/Animal/DA_AnimalNexus',L.BEAST)
lanes={}
for i,route in enumerate(L.ROUTES):
 for team in (0,1):
  points=route if team else list(reversed(route));a=spawn(unreal.LanePath,'Final_Lane_%d_Team%d'%(i+1,team),L.world(*points[0]),'Gameplay/Lanes')
  a.set_editor_property('team_index',team);a.set_editor_property('lane_half_width',680.)
  s=a.get_editor_property('spline');s.clear_spline_points(False)
  for p in points:s.add_spline_point(unreal.Vector(*L.world(*p)),unreal.SplineCoordinateSpace.WORLD,False)
  for j in range(len(points)):s.set_spline_point_type(j,unreal.SplinePointType.LINEAR,False)
  s.update_spline();lanes[(i,team)]=a
start=spawn(unreal.PlayerStart,'Final_PlayerStart',L.world(*L.START,offset=100),'Gameplay/Player')
commander=spawn(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Game/Race_Etherial/Character/Unit_Commander/BP_Ether_Commander'),'Final_EtherCommander',L.world(*L.START,offset=100),'Gameplay/Player')
commander.set_editor_property('bMyCommander',True)
commander.get_component_by_class(unreal.CombatTargetComponent).set_editor_property('team_index',0)
for resource,centers in [('Mineral',[(.90,.78),(.75,.91),(.13,.12)]),('Ether',[(.91,.88),(.34,.12)])]:
 for k,(u,v) in enumerate(centers):
  for j in range(4):
   uu=u+(j%2)*.014;vv=v+(j//2)*.017
   cls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Structure/Resources/BP_'+resource+('_S' if j>1 else ''))
   spawn(cls,'Final_%s_%d_%d'%(resource,k,j),L.world(uu,vv),'Gameplay/Resources')
# Limited pressure for an initial playable blockout; each lane has its own spawner.
for i in range(4):
 u,v=L.ROUTES[i][0];sp=spawn(unreal.UnitSpawner,'Final_BeastWave_%d'%i,L.world(u,v,offset=95),'Gameplay/EnemyWaves')
 cls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Units/Beast_Fox/BP_Fox_'+('Hunter' if i%2 else 'Brawler'))
 for k,val in [('unit_class',cls),('team_index',1),('lane',lanes[(i,1)]),('spawn_interval',35.+i*5),('initial_delay',55.+i*12),('max_alive',4),('max_total',16),('spawn_count_per_cycle',1),('owner_building_search_radius',3200.),('bRequireOwnerBuilding',True),('bStopWhenOwnerBuildingDestroyed',True)]:sp.set_editor_property(k,val)
nav=spawn(unreal.NavMeshBoundsVolume,'Final_NavMeshBounds',(0,0,300),'Navigation');nav.set_actor_scale3d(unreal.Vector(L.H/200*1.04,L.W/200*1.04,18))
sun=spawn(unreal.DirectionalLight,'Final_Sun',(0,0,1200),'Lighting');sun.set_actor_rotation(unreal.Rotator(pitch=-57,yaw=-28,roll=0),False);sun.light_component.set_editor_property('intensity',3.)
sky=spawn(unreal.SkyLight,'Final_SkyLight',(0,0,1500),'Lighting');sky.light_component.set_editor_property('intensity',.85)
spawn(unreal.SkyAtmosphere,'Final_Atmosphere',(0,0,0),'Lighting')
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-15000,0,28000),unreal.Rotator(pitch=-62,yaw=0,roll=0))
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
table=unreal.load_asset('/Game/Core/DataTable/DT_SFStage');rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
(OUT/'stage_before.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
row=next(r for r in rows if r['Name']=='Stage07')
row.update(StageLevel=L.MAP+'.PT_SFStageFinal',PlayerRace='Human',TeamSetups=['(TeamIndex=1,Race=Beast)'])
row['Minimap']='(bAutoDetectBounds=False,WorldMin=(X=-11440,Y=-14560),WorldMax=(X=11440,Y=14560),CaptureHeight=20000,ClockwiseQuarterTurns=0,BoundsPadding=200)'
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False));assert unreal.EditorAssetLibrary.save_loaded_asset(table)
(OUT/'creation.json').write_text(json.dumps({'map':L.MAP,'counts':counts,'actors':len(sub.get_all_level_actors()),'stage':row},ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('CAPITAL_FINAL_CREATED')


