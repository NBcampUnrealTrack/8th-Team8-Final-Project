"""Restore unit portraits and soften menu music; preserve authored voice assignments."""
import unreal,json,shutil,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/ReleaseFeedback';backup=out/'before';backup.mkdir(exist_ok=True)
report={}
def save_backup(path):
 package=path.split('.')[0];rel='Content/'+package.removeprefix('/Game/')
 src=root/(rel+'.umap' if (root/(rel+'.umap')).exists() else rel+'.uasset')
 dst=backup/src.relative_to(root);dst.parent.mkdir(parents=True,exist_ok=True)
 if not dst.exists():shutil.copy2(src,dst)
def table(path):
 a=unreal.load_asset(path);save_backup(path)
 return a,json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(a))
try:
 unit,units=table('/Game/Core/DataTable/DT_Units')
 selection,selected=table('/Game/Game/IN_UI/DT/DT_SFSelectionUI')
 production,products=table('/Game/Core/DataTable/DT_SFProductionUI')
 by_name={r['Name']:r for r in selected}
 old_products={r['Name']:r for r in products}
 updated=[]
 for u in units:
  name=u['Name'];s=by_name[name]
  icon=s['SelectionIcon'] if s['SelectionIcon']!='None' else s['PortraitImage']
  assert isinstance(unreal.load_asset(icon),unreal.Texture2D),(name,icon)
  row=old_products.get(name,dict(Name=name))
  row.update(DisplayName=s['DisplayName'],Icon=icon)
  updated.append(row)
  # Preserve each unit's authored 3D capture while repairing missing row bindings.
  cls=unreal.load_class(None,u['UnitClass']);cdo=unreal.get_default_object(cls)
  prop='unit_row_name'
  if str(cdo.get_editor_property(prop))!=name:
   save_backup(cls.get_path_name().split('.')[0]);cdo.set_editor_property(prop,name)
   unreal.EditorAssetLibrary.save_asset(cls.get_path_name().split('.')[0])
 assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(production,json.dumps(updated,ensure_ascii=False))
 assert unreal.EditorAssetLibrary.save_loaded_asset(production)
 report['units']=len(units);report['production_icons']=len(updated);report['voice_policy']='Preserve original selection/move sounds; unvoiced units remain silent'
 for path in ['/Game/Core/Levels/Start','/Game/Core/Levels/Lobby']:
  save_backup(path);assert unreal.EditorLoadingAndSavingUtils.load_map(path)
  audio=[]
  for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
   if not isinstance(a,unreal.AmbientSound):continue
   c=a.get_component_by_class(unreal.AudioComponent);sound=c.get_editor_property('sound')
   if sound and '/Sound/BGM/' in sound.get_path_name():
    c.set_sound(unreal.load_asset('/Game/Sound/BGM/Title_Cue'))
    c.set_volume_multiplier(.32)
    c.set_editor_property('is_ui_sound',True)
    c.set_editor_property('allow_spatialization',False)
    audio.append(dict(actor=a.get_actor_label(),sound=c.get_editor_property('sound').get_path_name(),volume=.32))
  assert audio,path
  assert unreal.EditorLoadingAndSavingUtils.save_current_level();report[path]=audio
 definitions=[]
 for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game/Core/Primary/Buildings',True):
  obj=a.get_asset()
  if isinstance(obj,unreal.SFBuildingDefinition):
   definitions.append(dict(path=obj.get_path_name(),id=str(obj.get_editor_property('building_id')),grid=str(obj.get_editor_property('grid_size'))))
 report['definitions']=definitions
 (out/'content_applied.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
except Exception:
 (out/'content_error.txt').write_text(traceback.format_exc(),encoding='utf-8');raise
