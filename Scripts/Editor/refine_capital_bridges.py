"""Revision 2. Connect lakes below four individually editable combat bridges."""
import unreal,sys,json,math,random,importlib.util
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import capital_final_layout as L
from capital_final_waterworks import bridge_point
OUT=ROOT/'Saved/CapitalFinal/Revision3';OUT.mkdir(exist_ok=True)
assert not (OUT/'built.json').exists(),'Already applied.'
import shutil
shutil.copy2(ROOT/'Content/Game/Stage/PT_SFStageFinal.umap',OUT/'Before.umap')
spec=importlib.util.spec_from_file_location('old_terrain',ROOT/'Saved/CapitalFinal/Revision2/terrain_before_shallows.py');old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old)
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


removed=[]
for a in list(sub.get_all_level_actors()):
 label=a.get_actor_label();p=a.get_actor_location();u,v=L.uv(p.x,p.y)
 if label.startswith('Final_WaterNoWalk_'):
  sub.destroy_actor(a);continue
 if not label.startswith('Final_') or label.startswith(('Final_Bridge','Final_ConnectedWater','Final_WaterSafety','Final_Terrain')):continue
 if isinstance(a,unreal.StaticMeshActor):
  if L.water_distance(u,v)<80:
   removed.append(label);sub.destroy_actor(a);continue
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

wood=material('M_FinalBridgeTimber','float grain=n.v(float2(P.x/9.,P.y/180.));float weather=n.v(P.xy/170);float3 c=lerp(float3(.036,.024,.015),float3(.125,.086,.050),grain);return lerp(c,float3(.105,.099,.079),weather*.38)*(.78+.22*n.v(P.xy/48));')
stone=material('M_FinalBridgeStone','float2 q=P.xy+float2(n.v(P.xy/135),n.v(P.yx/145))*40;float row=floor(q.x/145);float2 cell=float2(q.x/145,(q.y+row*127)/230);float2 f=frac(cell);float seam=smoothstep(.018,.065,min(min(f.x,f.y),min(1-f.x,1-f.y)));float variation=n.h(floor(cell));float3 c=lerp(float3(.083,.079,.065),float3(.18,.174,.15),variation);c*=.8+.2*n.v(P.xy/90);float moss=smoothstep(.50,.78,n.v(P.xy/230))*(1-seam*.83);return lerp(lerp(float3(.040,.037,.028),c,seam),float3(.052,.069,.030),moss*.55);')
water=unreal.load_asset(L.ART+'/MI_FinalStage2Water');cube=unreal.load_asset('/Engine/BasicShapes/Cube');counts={};rng=random.Random(202610013)
def meshactor(label,mesh,loc=(0,0,0),mat=None,blocking=False,ground=False,group='Waterways'):
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc));a.set_actor_label(label);a.set_folder_path('Final Capital/'+group);a.tags=[unreal.Name('SF_CapitalFinal'),unreal.Name('SF_CapitalFinal_R2')]
 c=a.static_mesh_component;c.set_static_mesh(mesh)
 if mat:c.set_material(0,mat)
 c.set_collision_profile_name('BlockAll' if blocking else 'NoCollision');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,BLOCK if ground else IGNORE)
 counts[group]=counts.get(group,0)+1;return a
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

catalog=json.loads((ROOT/'Scripts/Editor/stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'));cache={}
def nature(name,u,v,width,height,group,blocking):
 if name not in cache:
  solid='/Game/Game/Stage/StageHeroArt/Collision/SM_HeroSolid_'+name;p=solid if unreal.EditorAssetLibrary.does_asset_exist(solid) else catalog[name]['path'] if name in catalog else '/Game/Game/Stage/Props/asset/static_mesh/'+name;cache[name]=unreal.load_asset(p)
 m=cache[name];bound=m.get_bounds();scale=unreal.Vector(width/max(bound.box_extent.x*2,bound.box_extent.y*2),width/max(bound.box_extent.x*2,bound.box_extent.y*2),height/(bound.box_extent.z*2));yaw=rng.uniform(0,360);ca,sa=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));x,y=L.xy(u,v)
 loc=(x-(ca*bound.origin.x-sa*bound.origin.y)*scale.x,y-(sa*bound.origin.x+ca*bound.origin.y)*scale.x,L.height(u,v)-10-(bound.origin.z-bound.box_extent.z)*scale.z)
 a=meshactor('Final_R2_%s_%04d'%(group,counts.get(group,0)),m,loc,blocking=blocking,group=group);a.set_actor_scale3d(scale);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
 for j,p in enumerate(catalog.get(name,{}).get('materials',[])):
  if p:a.static_mesh_component.set_material(j,unreal.load_asset(p))


# Keep the continuous collision rails, replace the formal parapets with rough low stonework.
for a in list(sub.get_all_level_actors()):
 label=a.get_actor_label()
 if not label.startswith('Final_Bridge'):continue
 idx=int(label[len('Final_Bridge')]);b=L.BRIDGES[idx]
 if b['style']=='stone' and any(k in label for k in ('_Post_','_Cap_')):
  sub.destroy_actor(a)
 elif b['style']=='stone' and '_Rail_' in label:
  a.static_mesh_component.set_visibility(False);a.set_actor_hidden_in_game(True)
 elif b['style']=='timber' and '_Plank_' in label:
  scale=a.get_actor_scale3d();scale.y*=rng.uniform(.96,1.05);a.set_actor_scale3d(scale)
for i,b in enumerate(L.BRIDGES):
 if b['style']!='stone':continue
 for side in (-1,1):
  for j in range(22):
   along=-b['half_length']+(j+.5)*2*b['half_length']/22
   across=side*rng.uniform(769,800)
   x,y,z=bridge_point(b,along,across,L.bridge_z(along,b))
   name=rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_07','SM_Rocks_09'])
   nature(name,*L.uv(x,y),rng.uniform(210,280),rng.uniform(145,230),'BridgeRoughStone',True)
# Reposition these stones relative to the bridge surface, not the river bed.
for a in sub.get_all_level_actors():
 if not a.get_actor_label().startswith('Final_R2_BridgeRoughStone_'):continue
 p=a.get_actor_location();u,v=L.uv(p.x,p.y)
 b=min((b for b in L.BRIDGES if b['style']=='stone'),key=lambda b:math.dist((p.x,p.y),b['center']))
 along,across=L.bridge_local(u,v,b);p.z+=L.bridge_z(along,b)-L.height(u,v)-15;a.set_actor_location(p,False,False)
# Dense but clustered banks, with occasional boulders and open muddy shallows.
for j in range(170):
 for i in range(205):
  u=-.03+i*.0054+rng.uniform(-.002,.002);v=-.03+j*.0065+rng.uniform(-.002,.002);d=L.water_distance(u,v)
  if old.water_distance(u,v)-d<80 or not 70<d<560 or L.road_distance(u,v)<780 or L.ramp_distance(u,v)<900:continue
  if any(abs(L.bridge_local(u,v,b)[0])<b['half_length']+800 and abs(L.bridge_local(u,v,b)[1])<1000 for b in L.BRIDGES):continue
  cluster=.45+.3*math.sin(u*73+v*16)*math.sin(v*65)
  if rng.random()>cluster:continue
  if rng.random()<.64:
   size=rng.uniform(80,240) if rng.random()<.8 else rng.uniform(290,460)
   nature(rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_07','SM_Rocks_09']),u,v,size,size*rng.uniform(.45,.8),'RiverbankRocks',True)
  else:nature(rng.choice(['Bush_average','GrassClump01','GrassClump02']),u,v,rng.uniform(100,230),rng.uniform(55,140),'RiverbankPlants',False)

assert unreal.EditorLoadingAndSavingUtils.save_map(w,L.MAP)
(OUT/'built.json').write_text(json.dumps({'counts':counts,'removed':len(removed),'null_water_strips':volumes,'river_width_cm':1320,'actors':len(sub.get_all_level_actors())},indent=2),encoding='utf-8')
unreal.log('CAPITAL_NATURAL_BRIDGES_SAVED')
