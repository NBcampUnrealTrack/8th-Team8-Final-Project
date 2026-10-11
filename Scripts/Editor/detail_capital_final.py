"""Revision 4: broad faceted crystals, stone bridges, road grasses, clearer water.
Only modifies the final map and its private art; refuses repeated application.
"""
import unreal,sys,math,json,random,shutil
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import capital_final_layout as L
from capital_final_waterworks import bridge_point
OUT=ROOT/'Saved/CapitalFinal/Revision4';OUT.mkdir(exist_ok=True)
assert not (OUT/'built.json').exists(),'Already applied; preserve subsequent edits.'
shutil.copy2(ROOT/'Content/Game/Stage/PT_SFStageFinal.umap',OUT/'Before.umap')
w=unreal.EditorLoadingAndSavingUtils.load_map(L.MAP);assert w
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);assets=unreal.AssetToolsHelpers.get_asset_tools();ml=unreal.MaterialEditingLibrary
IGNORE=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('IGNORE')))
BLOCK=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('BLOCK')))
for name in ['SM_FinalEtherDiamondCluster','SM_FinalBridgeArch1','SM_FinalBridgeArch3']:
 t=unreal.AssetImportTask();t.filename=str(ROOT/'ContentSource/CapitalFinal'/(name+'.obj'));t.destination_path=L.ART;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=True
 o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;o.automated_import_should_detect_type=False
 d=o.static_mesh_import_data;d.combine_meshes=True;d.auto_generate_collision=False;d.convert_scene=False;d.convert_scene_unit=False
 if name=='SM_FinalEtherDiamondCluster':d.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
 t.options=o;assets.import_asset_tasks([t])
 m=unreal.load_asset(L.ART+'/'+name);assert m,name
 m.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE);unreal.EditorAssetLibrary.save_loaded_asset(m)

# Separate lit colour from emission: facets stay turquoise instead of clipping white.
mat=unreal.load_asset(L.ART+'/M_CapitalFinal_Crystal');ml.delete_all_material_expressions(mat)
def constant3(color):
 n=ml.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(*color,1));return n
base=constant3((.015,.34,.29));glow=constant3((.002,.045,.038))
ml.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR);ml.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
for val,prop in [(.42,unreal.MaterialProperty.MP_ROUGHNESS),(.20,unreal.MaterialProperty.MP_SPECULAR)]:
 n=ml.create_material_expression(mat,unreal.MaterialExpressionConstant);n.set_editor_property('r',val);ml.connect_material_property(n,'',prop)
ml.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
crystal=unreal.load_asset(L.ART+'/SM_FinalEtherDiamondCluster');cb=crystal.get_bounds();changed=[]
for a in sub.get_all_level_actors():
 if not isinstance(a,unreal.StaticMeshActor) or not any(k in a.get_actor_label() for k in ['EtherCrystals','GateCrystals']):continue
 c=a.static_mesh_component;old=c.static_mesh.get_bounds();s=a.get_actor_scale3d();p=a.get_actor_location()
 width=max(old.box_extent.x*2*s.x,old.box_extent.y*2*s.y)*1.45;height=old.box_extent.z*2*s.z*1.18
 yaw=math.radians(a.get_actor_rotation().yaw);cx=p.x+math.cos(yaw)*old.origin.x*s.x-math.sin(yaw)*old.origin.y*s.y;cy=p.y+math.sin(yaw)*old.origin.x*s.x+math.cos(yaw)*old.origin.y*s.y
 bottom=p.z+(old.origin.z-old.box_extent.z)*s.z
 sx=width/max(cb.box_extent.x*2,cb.box_extent.y*2);sz=height/(cb.box_extent.z*2)
 c.set_static_mesh(crystal);c.set_material(0,mat);c.set_collision_profile_name('BlockAll');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,IGNORE)
 a.set_actor_scale3d(unreal.Vector(sx,sx,sz));a.set_actor_location(unreal.Vector(cx-(math.cos(yaw)*cb.origin.x-math.sin(yaw)*cb.origin.y)*sx,cy-(math.sin(yaw)*cb.origin.x+math.cos(yaw)*cb.origin.y)*sx,bottom-(cb.origin.z-cb.box_extent.z)*sz),False,False)
 changed.append(a.get_actor_label())

stone=unreal.load_asset(L.ART+'/M_FinalBridgeStone');rng=random.Random(202610014);counts={}
def meshactor(label,mesh,loc,group,material=None,blocking=False):
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc));a.set_actor_label(label);a.set_folder_path('Final Capital/'+group);a.tags=[unreal.Name('SF_CapitalFinal'),unreal.Name('SF_CapitalFinal_R4')]
 c=a.static_mesh_component;c.set_static_mesh(mesh)
 if material:c.set_material(0,material)
 c.set_collision_profile_name('BlockAll' if blocking else 'NoCollision');c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,IGNORE)
 counts[group]=counts.get(group,0)+1;return a
catalog=json.loads((ROOT/'Scripts/Editor/stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'));cache={}
def nature(name,u,v,width,height,group,blocking=False,z=None):
 if name not in cache:
  solid='/Game/Game/Stage/StageHeroArt/Collision/SM_HeroSolid_'+name
  path=solid if unreal.EditorAssetLibrary.does_asset_exist(solid) else catalog[name]['path'] if name in catalog else '/Game/Game/Stage/Props/asset/static_mesh/'+name
  cache[name]=unreal.load_asset(path);assert cache[name],name
 m=cache[name];b=m.get_bounds();sx=width/max(b.box_extent.x*2,b.box_extent.y*2);sz=height/(b.box_extent.z*2);yaw=rng.uniform(0,360);ca,sa=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));x,y=L.xy(u,v)
 ground=L.height(u,v) if z is None else z
 a=meshactor('Final_R4_%s_%04d'%(group,counts.get(group,0)),m,(x-(ca*b.origin.x-sa*b.origin.y)*sx,y-(sa*b.origin.x+ca*b.origin.y)*sx,ground-12-(b.origin.z-b.box_extent.z)*sz),group,blocking=blocking)
 a.set_actor_scale3d(unreal.Vector(sx,sx,sz));a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
 for j,path in enumerate(catalog.get(name,{}).get('materials',[])):
  if path:a.static_mesh_component.set_material(j,unreal.load_asset(path))
 return a

removed=[]
for a in list(sub.get_all_level_actors()):
 label=a.get_actor_label()
 if not label.startswith(('Final_Bridge1_','Final_Bridge3_')):continue
 if '_WalkDeck' in label:
  a.static_mesh_component.set_visibility(True);a.set_actor_hidden_in_game(False);a.static_mesh_component.set_material(0,stone);a.set_folder_path('Final Capital/Bridges/stone')
 elif '_Rail_' in label:
  # Keep an invisible continuous blocker outside the walking surface.
  scale=a.get_actor_scale3d();scale.y=1.1;scale.z=1.75;a.set_actor_scale3d(scale)
  p=a.get_actor_location();p.z-=65;a.set_actor_location(p,False,False);a.static_mesh_component.set_visibility(False);a.set_actor_hidden_in_game(True);a.set_folder_path('Final Capital/Bridges/stone')
 else:removed.append(label);sub.destroy_actor(a)
for i in (1,3):
 b=L.BRIDGES[i];meshactor('Final_Bridge%d_Arch'%i,unreal.load_asset(L.ART+'/SM_FinalBridgeArch%d'%i),(0,0,0),'Bridges/stone',stone)
 for side in (-1,1):
  for j in range(22):
   along=-b['half_length']+(j+.5)*2*b['half_length']/22;x,y,z=bridge_point(b,along,side*rng.uniform(769,800),L.bridge_z(along,b))
   nature(rng.choice(['SM_Rocks_01','SM_Rocks_03','SM_Rocks_07','SM_Rocks_09']),*L.uv(x,y),rng.uniform(210,280),rng.uniform(145,230),'BridgeRoughStone',True,z=z-15)

# Grasses follow the road shoulders in irregular patches; combat lanes remain readable.
occupied=set()
for route in L.ROUTES:
 for auv,buv in zip(route,route[1:]):
  a,b=L.xy(*auv),L.xy(*buv);dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
  for k in range(max(1,int(length/150))):
   t=rng.random();x,y=a[0]+dx*t,a[1]+dy*t
   for side in (-1,1):
    if rng.random()>.78:continue
    for j in range(rng.randint(2,4)):
     offset=rng.uniform(600,1090)*side;px=x-dy/length*offset+rng.uniform(-100,100);py=y+dx/length*offset+rng.uniform(-100,100);u,v=L.uv(px,py)
     key=(int(px/100),int(py/100))
     if key in occupied or L.water_distance(u,v)<180 or L.road_distance(u,v)<490:continue
     if any(abs(L.bridge_local(u,v,br)[0])<br['half_length']+300 and abs(L.bridge_local(u,v,br)[1])<br['half_width']+180 for br in L.BRIDGES):continue
     if L.base_distance(u,v,L.BEAST)<1600 or L.base_distance(u,v,L.CAPITAL)<2200:continue
     z=L.height(u,v)
     if max(abs(L.height(u+100/L.W,v)-z),abs(L.height(u,v+100/L.H)-z))>70:continue
     occupied.add(key);nature(rng.choice(['GrassClump01','GrassClump02','SM_GrassLumpD']),u,v,rng.uniform(80,165),rng.uniform(35,85),'RoadsideGrass')

water=unreal.load_asset(L.ART+'/MI_FinalStage2Water')
names=[str(n) for n in ml.get_vector_parameter_names(water)];water_changes={}
def set_override(property_name,name,value):
 values=list(water.get_editor_property(property_name))
 for i,item in enumerate(values):
  if str(item.get_editor_property('parameter_info').get_editor_property('name'))==name:
   item.set_editor_property('parameter_value',value);values[i]=item;water.set_editor_property(property_name,values);return
 assert property_name=='scalar_parameter_values',name
 info=unreal.MaterialParameterInfo();info.set_editor_property('name',name)
 item=unreal.ScalarParameterValue();item.set_editor_property('parameter_info',info);item.set_editor_property('parameter_value',value)
 values.append(item);water.set_editor_property(property_name,values)
for wanted,val in [('Absorption Coefficient',(.36,.15,.10,0)),('Scattering',(.035,.09,.075,1.5))]:
 name=next(n for n in names if n.lower()==wanted.lower())
 water_changes[name]={'before':str(ml.get_material_instance_vector_parameter_value(water,name)),'after':val}
 set_override('vector_parameter_values',name,unreal.LinearColor(*val))
for name,val in [('Default Near Normal Strength',.16),('Default Distant Normal Strength',.18),('Default Distant Normal StrengthB',.15),('Water Roughness',.16)]:
 set_override('scalar_parameter_values',name,val)
ml.update_material_instance(water);unreal.EditorAssetLibrary.save_loaded_asset(water)
assert unreal.EditorLoadingAndSavingUtils.save_map(w,L.MAP)
(OUT/'built.json').write_text(json.dumps({'crystals_replaced':changed,'new_props':counts,'removed_timber_parts':len(removed),'water':water_changes,'actors':len(sub.get_all_level_actors())},indent=2),encoding='utf-8')
unreal.log('CAPITAL_REVISION4_SAVED')
