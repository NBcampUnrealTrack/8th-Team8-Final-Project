"""Revision 2. Connect lakes below four individually editable combat bridges."""
import unreal,sys,json,math,random,importlib.util
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import capital_final_layout as L
from capital_final_waterworks import bridge_point
OUT=ROOT/'Saved/CapitalFinal/Revision2';assert not (OUT/'built.json').exists(),'Already applied; preserve subsequent changes.'
spec=importlib.util.spec_from_file_location('old_terrain',OUT/'layout_before.py');old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
w=unreal.EditorLoadingAndSavingUtils.load_map(L.MAP);assert w
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);assets=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
IGNORE=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('IGNORE')))
BLOCK=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('BLOCK')))
files=[ROOT/'ContentSource/CapitalFinal/SM_CapitalTerrain.obj']+sorted((ROOT/'ContentSource/CapitalFinal').glob('SM_Final*.obj'))
for f in files:
 t=unreal.AssetImportTask();t.filename=str(f);t.destination_path=L.ART;t.destination_name=f.stem;t.automated=True;t.replace_existing=True;t.save=True
 o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;o.automated_import_should_detect_type=False
 d=o.static_mesh_import_data;d.combine_meshes=True;d.auto_generate_collision=False;d.convert_scene=False;d.convert_scene_unit=False;t.options=o;assets.import_asset_tasks([t])
 m=unreal.load_asset(L.ART+'/'+f.stem);assert m,f
 m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);unreal.EditorAssetLibrary.save_loaded_asset(m)

# Keep existing placement edits; only clear vegetation submerged by the new water or inside a bridge.
removed=[]
for a in list(sub.get_all_level_actors()):
 label=a.get_actor_label();p=a.get_actor_location();u,v=L.uv(p.x,p.y)
 if label.startswith(('Final_Lake_','Final_LakeCollision_','Final_LakeNoWalk_')):removed.append(label);sub.destroy_actor(a);continue
 if isinstance(a,unreal.LanePath):
  i=int(label.split('_')[2])-1;route=L.ROUTES[i] if a.get_editor_property('team_index') else list(reversed(L.ROUTES[i]));sp=a.get_editor_property('spline');sp.clear_spline_points(False)
  for pt in route:sp.add_spline_point(unreal.Vector(*L.world(*pt)),unreal.SplineCoordinateSpace.WORLD,False)
  for j in range(len(route)):sp.set_spline_point_type(j,unreal.SplinePointType.LINEAR,False)
  sp.update_spline();continue
 if not label.startswith('Final_') or label=='Final_Terrain' or any(k in label for k in ['NavMesh','Sun','SkyLight','Atmosphere']):continue
 clearing=any(abs(L.bridge_local(u,v,b)[0])<b['half_length']+800 and abs(L.bridge_local(u,v,b)[1])<b['half_width']+330 for b in L.BRIDGES)
 if isinstance(a,unreal.StaticMeshActor) and (L.water_distance(u,v)<130 or clearing):removed.append(label);sub.destroy_actor(a);continue
 p.z+=L.height(u,v)-old.height(u,v);a.set_actor_location(p,False,False)

source=(ROOT/'Scripts/Editor/create_capital_final.py').read_text(encoding='utf-8');ctx={'L':L};exec(source[source.index("noise='''"):source.index('floor_mat=')],ctx)
floor=unreal.load_asset(L.ART+'/M_CapitalFinal_Terrain')
for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
 if n.get_outer()==floor and 'float road=' in n.get_editor_property('code'):n.set_editor_property('code',ctx['code'])
ml.recompile_material(floor);unreal.EditorAssetLibrary.save_loaded_asset(floor)
next(a for a in sub.get_all_level_actors() if a.get_actor_label()=='Final_Terrain').static_mesh_component.set_material(0,floor)

def material(name,code):
 m=unreal.load_asset(L.ART+'/'+name) if unreal.EditorAssetLibrary.does_asset_exist(L.ART+'/'+name) else assets.create_asset(name,L.ART,unreal.Material,unreal.MaterialFactoryNew())
 ml.delete_all_material_expressions(m);i=unreal.CustomInput();i.set_editor_property('input_name','P')
 c=ml.create_material_expression(m,unreal.MaterialExpressionCustom);c.set_editor_property('inputs',[i]);c.set_editor_property('code',ctx['noise']+code);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 p=ml.create_material_expression(m,unreal.MaterialExpressionWorldPosition);ml.connect_material_expressions(p,'',c,'P');ml.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
 r=ml.create_material_expression(m,unreal.MaterialExpressionConstant);r.set_editor_property('r',.88);ml.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS);m.set_editor_property('two_sided',True)
 ml.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m);return m
wood=material('M_FinalBridgeTimber','float grain=n.v(float2(P.x/13.,P.y/300.));return lerp(float3(.065,.027,.010),float3(.24,.12,.043),grain)*(.8+.2*n.v(P.xy/90));')
stone=material('M_FinalBridgeStone','float2 uv=float2(P.x,P.y+floor(P.x/180.)*120.)/float2(180,290);float2 f=frac(uv);float edge=smoothstep(.008,.03,min(min(f.x,f.y),min(1-f.x,1-f.y)));return lerp(float3(.055,.066,.062),float3(.24,.285,.255),edge)*(.72+.25*n.v(P.xy/110));')
water=unreal.load_asset(L.ART+'/MI_FinalStage2Water');cube=unreal.load_asset('/Engine/BasicShapes/Cube');counts={};rng=random.Random(202610012)
def meshactor(label,mesh,loc=(0,0,0),mat=None,blocking=False,ground=False,group='Waterways'):
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc));a.set_actor_label(label);a.set_folder_path('Final Capital/'+group);a.tags=[unreal.Name('SF_CapitalFinal'),unreal.Name('SF_CapitalFinal_R2')]
 c=a.static_mesh_component;c.set_static_mesh(mesh)
 if mat:c.set_material(0,mat)
 c.set_collision_profile_name('BlockAll' if blocking else 'NoCollision');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,BLOCK if ground else IGNORE)
 counts[group]=counts.get(group,0)+1;return a
meshactor('Final_ConnectedWater',unreal.load_asset(L.ART+'/SM_FinalConnectedWater'),mat=water)
a=meshactor('Final_WaterSafety',unreal.load_asset(L.ART+'/SM_FinalWaterSafety'),blocking=True,group='Navigation/Water');a.set_actor_hidden_in_game(True);a.static_mesh_component.set_visibility(False)
# Shallow null-area slabs block underwater movement, leaving the raised bridge deck untouched.
step=220.;volumes=0
for row in range(int(L.H*1.2/step)+1):
 x=-L.H*.6+row*step;active=None
 for col in range(int(L.W*1.2/step)+2):
  y=-L.W*.6+col*step;wet=col<=int(L.W*1.2/step) and L.water_distance(*L.uv(x,y))<100
  if wet and active is None:active=y
  if not wet and active is not None:
   end=y-step;mid=(active+end)/2;a=sub.spawn_actor_from_class(unreal.NavModifierVolume,unreal.Vector(x,mid,-430));a.set_actor_label('Final_WaterNoWalk_%03d'%volumes);a.set_folder_path('Final Capital/Navigation/Water');a.set_editor_property('area_class',unreal.NavArea_Null)
   center,e=a.get_actor_bounds(False);a.set_actor_scale3d(unreal.Vector((step*.52)/e.x,((end-active+step)*.51)/e.y,510/e.z));active=None;volumes+=1

def box(label,b,s,t,z,dimensions,mat,blocking=False):
 a=meshactor(label,cube,bridge_point(b,s,t,z),mat,blocking,False,'Bridges/'+b['style']);a.set_actor_scale3d(unreal.Vector(*(d/100 for d in dimensions)));a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=math.degrees(math.atan2(b['axis'][1],b['axis'][0])),roll=0),False);return a
for i,b in enumerate(L.BRIDGES):
 mat=wood if b['style']=='timber' else stone
 deck=meshactor('Final_Bridge%d_WalkDeck'%i,unreal.load_asset(L.ART+'/SM_FinalBridgeDeck%d'%i),mat=mat,blocking=True,ground=True,group='Bridges/'+b['style'])
 if b['style']=='timber':
  deck.static_mesh_component.set_visibility(False);deck.set_actor_hidden_in_game(True)
  for j in range(26):
   s=-b['half_length']+(j+.5)*2*b['half_length']/26;z=L.bridge_z(s,b)-28
   a=box('Final_Bridge%d_Plank_%02d'%(i,j),b,s,rng.uniform(-8,8),z,(2*b['half_length']/26-2,1400+rng.uniform(-20,20),55),wood)
   pitch=math.degrees(math.atan2(L.bridge_z(s+5,b)-L.bridge_z(s-5,b),10));a.set_actor_rotation(unreal.Rotator(pitch=pitch,yaw=math.degrees(math.atan2(b['axis'][1],b['axis'][0])),roll=0),False)
 else:meshactor('Final_Bridge%d_Arch'%i,unreal.load_asset(L.ART+'/SM_FinalBridgeArch%d'%i),mat=stone,group='Bridges/stone')
 for side in (-1,1):
  for j in range(7):
   s=-b['half_length']+j*2*b['half_length']/6;z=L.bridge_z(s,b)
   box('Final_Bridge%d_Post_%d_%d'%(i,side,j),b,s,side*775,z+105,(100 if b['style']=='timber' else 150,100 if b['style']=='timber' else 150,265),mat,True)
   if b['style']=='stone':box('Final_Bridge%d_Cap_%d_%d'%(i,side,j),b,s,side*775,z+258,(180,180,40),stone)
   if b['style']=='timber' and j in (1,5):box('Final_Bridge%d_Pile_%d_%d'%(i,side,j),b,s,side*620,-50,(120,120,700),wood)
  for j in range(6):
   s=-b['half_length']+(j+.5)*2*b['half_length']/6;z=L.bridge_z(s,b)
   box('Final_Bridge%d_Rail_%d_%d'%(i,side,j),b,s,side*775,z+(150 if b['style']=='timber' else 85),(2*b['half_length']/6+20,70 if b['style']=='timber' else 110,65 if b['style']=='timber' else 175),mat,True)

catalog=json.loads((ROOT/'Scripts/Editor/stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'));cache={}
def nature(name,u,v,width,height,group,blocking):
 if name not in cache:
  solid='/Game/Game/Stage/StageHeroArt/Collision/SM_HeroSolid_'+name;p=solid if unreal.EditorAssetLibrary.does_asset_exist(solid) else catalog[name]['path'] if name in catalog else '/Game/Game/Stage/Props/asset/static_mesh/'+name;cache[name]=unreal.load_asset(p)
 m=cache[name];bound=m.get_bounds();scale=unreal.Vector(width/max(bound.box_extent.x*2,bound.box_extent.y*2),width/max(bound.box_extent.x*2,bound.box_extent.y*2),height/(bound.box_extent.z*2));yaw=rng.uniform(0,360);ca,sa=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));x,y=L.xy(u,v)
 loc=(x-(ca*bound.origin.x-sa*bound.origin.y)*scale.x,y-(sa*bound.origin.x+ca*bound.origin.y)*scale.x,L.height(u,v)-10-(bound.origin.z-bound.box_extent.z)*scale.z)
 a=meshactor('Final_R2_%s_%04d'%(group,counts.get(group,0)),m,loc,blocking=blocking,group=group);a.set_actor_scale3d(scale);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
 for j,p in enumerate(catalog.get(name,{}).get('materials',[])):
  if p:a.static_mesh_component.set_material(j,unreal.load_asset(p))

# Dense but clustered banks, with occasional boulders and open muddy shallows.
for j in range(170):
 for i in range(205):
  u=-.03+i*.0054+rng.uniform(-.002,.002);v=-.03+j*.0065+rng.uniform(-.002,.002);d=L.water_distance(u,v)
  if not 70<d<560 or L.road_distance(u,v)<780 or L.ramp_distance(u,v)<900:continue
  if any(abs(L.bridge_local(u,v,b)[0])<b['half_length']+800 and abs(L.bridge_local(u,v,b)[1])<1000 for b in L.BRIDGES):continue
  cluster=.45+.3*math.sin(u*73+v*16)*math.sin(v*65)
  if rng.random()>cluster:continue
  if rng.random()<.64:
   size=rng.uniform(80,240) if rng.random()<.8 else rng.uniform(290,460)
   nature(rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_07','SM_Rocks_09']),u,v,size,size*rng.uniform(.45,.8),'RiverbankRocks',True)
  else:nature(rng.choice(['Bush_average','GrassClump01','GrassClump02']),u,v,rng.uniform(100,230),rng.uniform(55,140),'RiverbankPlants',False)
assert unreal.EditorLoadingAndSavingUtils.save_map(w,L.MAP)
(OUT/'built.json').write_text(json.dumps({'counts':counts,'removed':len(removed),'null_water_strips':volumes,'bridges':L.BRIDGES,'actors':len(sub.get_all_level_actors())},indent=2),encoding='utf-8')
unreal.log('CAPITAL_BRIDGES_SAVED')
