"""Revision 4: textured cliff faces and the player's east-facing overview (retreat on right)."""
import unreal,json,re,shutil
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Ambush/Revision4';OUT.mkdir(parents=True,exist_ok=True)
MAP='/Game/Game/Stage/PT_SFStageAmbush';ART='/Game/Game/Stage/AmbushArt'
assert not (OUT/'authored.json').exists(), 'Revision already applied'
for p in ['Content/Game/Stage/PT_SFStageAmbush.umap','Content/Core/DataTable/DT_SFStage.uasset','Content/Game/Stage/AmbushArt/M_AmbushGround.uasset']:
 dest=OUT/('Before_'+Path(p).name)
 if not dest.exists():shutil.copy2(ROOT/p,dest)
w=unreal.EditorLoadingAndSavingUtils.load_map(MAP);assert w
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);ml=unreal.MaterialEditingLibrary
floor=unreal.load_asset(ART+'/M_AmbushGround')
code=next(n.get_editor_property('code') for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer()==floor and 'float road=' in n.get_editor_property('code'))
old='float grain=n.v(float2((P.x+P.y)/270,P.z/160));float3 rock=lerp(float3(.075,.071,.062),float3(.21,.20,.17),grain);'
assert old in code
code=code.replace(old,'float3 rock=Rock;')
assets=unreal.AssetToolsHelpers.get_asset_tools()
stone=unreal.load_asset(ART+'/M_AmbushNaturalRock')
if not stone:stone=assets.create_asset('M_AmbushNaturalRock',ART,unreal.Material,unreal.MaterialFactoryNew())

def build(m,is_floor):
 ml.delete_all_material_expressions(m)
 m.set_editor_property('used_with_nanite',True)
 m.set_editor_property('tangent_space_normal',False)
 def node(cls,**kw):
  n=ml.create_material_expression(m,cls)
  for k,v in kw.items():n.set_editor_property(k,v)
  return n
 def custom(code,links,out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
  inputs=[]
  for name in links:
   i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
  n=node(unreal.MaterialExpressionCustom,code=code,inputs=inputs,output_type=out)
  for name,(source,pin) in links.items():assert ml.connect_material_expressions(source,pin,n,name)
  return n
 p=node(unreal.MaterialExpressionWorldPosition);normal=node(unreal.MaterialExpressionVertexNormalWS)
 col=node(unreal.MaterialExpressionTextureObject,texture=unreal.load_asset('/Game/Game/Stage/Props/texture/aerialrocks/aerial_rocks_02_diff_1k'))
 nor=node(unreal.MaterialExpressionTextureObject,texture=unreal.load_asset('/Game/Game/Stage/Props/texture/aerialrocks/aerial_rocks_02_nor_gl_1k'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
 rough=node(unreal.MaterialExpressionTextureObject,texture=unreal.load_asset('/Game/Game/Stage/Props/texture/aerialrocks/aerial_rocks_02_rough_1k'),sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
 links={'P':(p,''),'N':(normal,'')}
 prefix='float3 w=pow(abs(N),4);w/=max(w.x+w.y+w.z,.0001);float3 q=P/650.;'
 sample='Texture2DSample(T,TSampler,q.yz)*w.x+Texture2DSample(T,TSampler,q.xz)*w.y+Texture2DSample(T,TSampler,q.xy)*w.z'
 c=custom(prefix+'float3 c=('+sample+').rgb;float l=dot(c,float3(.2126,.7152,.0722));return lerp(l.xxx,c,.55)*float3(.80,.81,.79);',{**links,'T':(col,'')})
 r=custom(prefix+'float r=('+sample+').r;return '+('lerp(.85,clamp(r,.70,.97),1-smoothstep(.5,.82,abs(N.z)));' if is_floor else 'clamp(r,.70,.97);'),{**links,'T':(rough,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
 ncode=prefix+'''float2 a=Texture2DSample(T,TSampler,q.yz).rg*2-1;
float2 b=Texture2DSample(T,TSampler,q.xz).rg*2-1;
float2 c=Texture2DSample(T,TSampler,q.xy).rg*2-1;
a*=float2(.42,-.42);b*=float2(.42,-.42);c*=float2(.42,-.42);
float3 nx=float3(sqrt(saturate(1-dot(a,a)))*sign(N.x),a.x,a.y);
float3 ny=float3(b.x,sqrt(saturate(1-dot(b,b)))*sign(N.y),b.y);
float3 nz=float3(c.x,c.y,sqrt(saturate(1-dot(c,c)))*sign(N.z));
float3 rockNormal=normalize(nx*w.x+ny*w.y+nz*w.z);
'''+('return normalize(lerp(N,rockNormal,1-smoothstep(.5,.82,abs(N.z))));' if is_floor else 'return rockNormal;')
 n=custom(ncode,{**links,'T':(nor,'')})
 if is_floor:
  uv=custom('return P.xy/520.;',{'P':(p,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT2)
  textures={}
  for key,path in [('G','greengrass/Grass005_1K-PNG_Color'),('D','ground04/Ground106_1K-PNG_Color')]:
   t=node(unreal.MaterialExpressionTextureSample,texture=unreal.load_asset('/Game/Game/Stage/Props/texture/'+path));ml.connect_material_expressions(uv,'',t,'UVs');textures[key]=(t,'RGB')
  c=custom(code,{'P':(p,''),'Nrm':(normal,''),'Rock':(c,''),**textures})
 for expression,prop in [(c,unreal.MaterialProperty.MP_BASE_COLOR),(r,unreal.MaterialProperty.MP_ROUGHNESS),(n,unreal.MaterialProperty.MP_NORMAL)]:assert ml.connect_material_property(expression,'',prop)
 ml.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
build(floor,True);build(stone,False)
changed=[]
for a in sub.get_all_level_actors():
 label=a.get_actor_label()
 if isinstance(a,unreal.StaticMeshActor) and label.startswith(('Ambush_RavineCliffs_','Ambush_RavineTalus_','Ambush_BasinCliffs_')):
  comp=a.static_mesh_component
  for slot in range(comp.get_num_materials()):comp.set_material(slot,stone)
  changed.append(label)
 if isinstance(a,unreal.PlayerStart):a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=90,roll=0),False)
table=unreal.load_asset('/Game/Core/DataTable/DT_SFStage');rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table));before=json.loads(json.dumps(rows))
row=next(r for r in rows if r['Name']=='Stage06');assert 'PT_SFStageAmbush' in row['StageLevel']
row['Minimap']=re.sub(r'ClockwiseQuarterTurns=\d+','ClockwiseQuarterTurns=3',row['Minimap'])
assert all(a==b for a,b in zip(before,rows) if a['Name']!='Stage06')
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False));assert unreal.EditorAssetLibrary.save_loaded_asset(table)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-1450,-15500,20500),unreal.Rotator(pitch=-55,yaw=90,roll=0))
assert unreal.EditorLoadingAndSavingUtils.save_map(w,MAP)
(OUT/'authored.json').write_text(json.dumps({'rock_actors':len(changed),'camera_yaw':90,'minimap':row['Minimap']},indent=2))
unreal.log('AMBUSH_ROCK_AND_VIEW_SAVED')
