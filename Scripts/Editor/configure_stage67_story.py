"""Install faction stories and align Stage06's play direction without rebuilding its layout."""
import copy,json,re,shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());out=root/'Saved/Stage67Story';out.mkdir(exist_ok=True)
source=json.loads((root/'ContentSource/Stage67Story_KO.json').read_text(encoding='utf-8'))
portraits=json.loads((root/'ContentSource/Stage67StoryPortraits.json').read_text(encoding='utf-8'))
packages=['Content/Game/IN_UI/DT/DT_SFStageIntro.uasset','Content/Core/DataTable/DT_SFStage.uasset',
 'Content/Game/Stage/PT_SFStageAmbush.umap','Content/Game/Stage/AmbushArt/BP_AmbushCamera.uasset',
 'Content/Game/Stage/AmbushArt/BP_AmbushGameMode.uasset']
for path in packages:
 backup=out/('before_portraits_'+Path(path).name)
 if not backup.exists():shutil.copy2(root/path,backup)

table=unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
before=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
template=next(r for r in before if r['Name']=='Stage03Clear')
end=template['Nodes'][-1]
assert 'NodeType=End' in end
common_camera=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Game/IN_UI/JSYBluePrints/BP_SFRTSCameraPawn'))
camera_settings={p:common_camera.get_editor_property(p) for p in ['initial_camera_distance','minimum_camera_distance','maximum_camera_distance','camera_pitch']}

def portrait_path(faction,expression):
 base='/Game/Game/IN_UI/Image/Intro/StoryUse/'
 path=base+('Etherial/Commander/T_EtherCommander_' if faction=='etherian' else 'Beast/Commander/Beast_Commander_')+expression
 path+='.'+path.rsplit('/',1)[1]
 assert unreal.load_asset(path),path
 return path

def node(row,index,body):
 suffix=template['Nodes'][0][template['Nodes'][0].index(',LeftCharacterImage='):]
 n='(NodeType=Dialogue,TitleText="",BodyText=NSLOCTEXT("SFStage67Story",'+json.dumps(row+'_'+str(index))+','+json.dumps(body,ensure_ascii=False)+')'+suffix
 # Preserve the author's narration; expressions and focus tell the scene visually.
 visual=portraits[row][index]
 frame='/Game/Game/IN_UI/Image/Intro/StoryUse/Flame/T_Curtain.T_Curtain'
 assert unreal.load_asset(frame)
 values={'LeftCharacterImage':portrait_path('etherian',visual['etherian']) if 'etherian' in visual else None,
         'RightCharacterImage':portrait_path('beast',visual['beast']) if 'beast' in visual else None,
         'PortraitFrameImage':frame,'GuideImage':None}
 for field,value in values.items():
  n=re.sub(field+r'=(?:"[^"\n]*"|None)',lambda _,f=field,v=value:f+'='+(json.dumps(v) if v else 'None'),n)
 n=re.sub(r'SpeakerSide=\w+','SpeakerSide='+visual['focus'],n)
 n=re.sub(r'LeftPortraitZoom=[0-9.]+','LeftPortraitZoom=1.100000',n)
 n=re.sub(r'RightPortraitZoom=[0-9.]+','RightPortraitZoom=1.000000',n)
 for field in ['PreviousNodeIndex','YesNodeIndex','NoNodeIndex']:
  n=re.sub(field+r'=-?\d+',field+'=-1',n)
 return re.sub(r'NextNodeIndex=-?\d+','NextNodeIndex='+str(index+1),n)

rows=[copy.deepcopy(r) for r in before if r['Name'] not in source]
for name,lines in source.items():
 assert len(portraits[name])==len(lines)
 r=copy.deepcopy(template)
 r.update(Name=name,StartNodeIndex=0,Nodes=[node(name,i,s) for i,s in enumerate(lines)]+[end],
          LockedFeatures='(GameplayTags=)',ObjectiveSuccessSound='None')
 rows.append(r)
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False))
after=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
def normalize(r):
 r=copy.deepcopy(r);r['Nodes']=[re.sub(r'NSLOCTEXT\("[^"]*", "[^"]*", ""\)','""',n) for n in r['Nodes']];return r
assert {r['Name']:normalize(r) for r in before if r['Name'] not in source}=={r['Name']:normalize(r) for r in after if r['Name'] not in source}
for name,lines in source.items():
 r=next(r for r in after if r['Name']==name)
 assert len(r['Nodes'])==len(lines)+1
 for i,body in enumerate(lines):
  assert all(p in r['Nodes'][i] for p in body.split('\n'))
  assert 'NextNodeIndex='+str(i+1)+',' in r['Nodes'][i]
  visual=portraits[name][i]
  for faction,field in [('etherian','LeftCharacterImage'),('beast','RightCharacterImage')]:
   expected=portrait_path(faction,visual[faction]) if faction in visual else None
   assert (expected in r['Nodes'][i]) if expected else (field+'=None' in r['Nodes'][i])
assert unreal.EditorAssetLibrary.save_loaded_asset(table,False)

map_path='/Game/Game/Stage/PT_SFStageAmbush'
assert unreal.EditorLoadingAndSavingUtils.load_map(map_path)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def signature(a):
 p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d()
 return [[p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z],[str(t) for t in a.tags]]
original={a.get_name():signature(a) for a in sub.get_all_level_actors() if not isinstance(a,unreal.PlayerStart)}
starts=[a for a in sub.get_all_level_actors() if isinstance(a,unreal.PlayerStart)]
assert len(starts)==1
starts[0].set_actor_rotation(unreal.Rotator(yaw=0),False)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
for path,settings in [('/Game/Game/Stage/AmbushArt/BP_AmbushGameMode',dict(player_building_placement_yaw=90.)),
                     ('/Game/Game/Stage/AmbushArt/BP_AmbushCamera',camera_settings)]:
 bp=unreal.load_asset(path);cdo=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
 for prop,value in settings.items():cdo.set_editor_property(prop,value)
 unreal.BlueprintEditorLibrary.compile_blueprint(bp)
 assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)

stage=unreal.load_asset('/Game/Core/DataTable/DT_SFStage')
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(stage))
for r in rows:
 if r['Name']=='Stage06':r['Minimap']=re.sub(r'ClockwiseQuarterTurns=\d+','ClockwiseQuarterTurns=0',r['Minimap'])
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(stage,json.dumps(rows,ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(stage,False)
assert unreal.EditorLoadingAndSavingUtils.load_map(map_path)
assert original=={a.get_name():signature(a) for a in sub.get_all_level_actors() if not isinstance(a,unreal.PlayerStart)}
assert all(abs(a.get_actor_rotation().yaw)<.01 for a in sub.get_all_level_actors() if isinstance(a,unreal.PlayerStart))
(out/'configuration_verified.json').write_text(json.dumps(dict(passed=True,rows=list(source),
 preserved_rows=[r['Name'] for r in before if r['Name'] not in source],camera_yaw=0,camera=camera_settings,commander_portraits=True,
 minimap_quarter_turns=0,building_placement_yaw=90,layout_preserved=True),ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('STAGE67_STORY_CONFIGURED')
