"""Read-only audit for the final nine playtest issues."""
import json, traceback
from pathlib import Path
import unreal
out=Path(unreal.Paths.project_saved_dir())/'ReleaseFeedback'
out.mkdir(exist_ok=True)
def export(asset,name):
 task=unreal.AssetExportTask();task.set_editor_properties(dict(object=asset,filename=str(out/(name+'.t3d')),automated=True,prompt=False,replace_identical=True,exporter=unreal.ObjectExporterT3D()))
 unreal.Exporter.run_asset_export_task(task)
try:
 for path in ['/Game/Core/DataTable/DT_Units','/Game/Core/DataTable/DT_SFProductionUI','/Game/Core/DataTable/DT_SFStage','/Game/Game/IN_UI/DT/DT_SFSelectionUI']:
  table=unreal.load_asset(path)
  (out/(path.split('/')[-1]+'.json')).write_text(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table),encoding='utf-8')
 for path in ['/Game/Game/IN_UI/HUD/WBP/Events/WBP_SFResourcePanel','/Game/Core/UI/WBP_Start','/Game/Core/UI/WBP_Lobby']:
  export(unreal.load_asset(path),path.split('/')[-1])
 assets=unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path('/Game',recursive=True)
 sounds=[a for a in assets if str(a.asset_class_path.asset_name) in ('SoundCue','SoundWave')]
 (out/'sounds.json').write_text(json.dumps([str(a.package_name) for a in sounds],indent=2),encoding='utf-8')
 for a in sounds:
  if '/BGM/' in str(a.package_name) or 'Voice' in str(a.asset_name):export(a.get_asset(),str(a.asset_name))
 rows=json.loads((out/'DT_SFStage.json').read_text(encoding='utf-8'))
 maps=[]
 for row in rows:
  for v in row.values():
   if isinstance(v,str) and v.startswith('/Game/') and unreal.EditorAssetLibrary.does_asset_exist(v) and v.split('.')[-1].startswith(('PT_','Proto')):maps.append(v.split('.')[0])
 maps=list(dict.fromkeys(maps))
 # Asset registry also provides the authored menu worlds.
 maps+= [str(a.package_name) for a in assets if str(a.asset_class_path.asset_name)=='World' and any(x in str(a.package_name).lower() for x in ('start','lobby'))]
 levels=[]
 for path in maps:
  unreal.EditorLoadingAndSavingUtils.load_map(path)
  entry=dict(map=path,resources=[],audio=[],anchors=[])
  for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
   p=a.get_actor_location()
   if isinstance(a,unreal.ResourceBase):
    g=a.get_editor_property('resource_grid_size')
    entry['resources'].append(dict(name=a.get_actor_label(),path=a.get_path_name(),pos=[p.x,p.y,p.z],grid=[g.x,g.y],cls=a.get_class().get_path_name()))
   if isinstance(a,unreal.AmbientSound):
    c=a.get_component_by_class(unreal.AudioComponent);s=c.get_editor_property('sound');entry['audio'].append(dict(name=a.get_actor_label(),sound=s.get_path_name() if s else None,volume=c.get_editor_property('volume_multiplier')))
   if 'Anchor' in a.get_class().get_name():entry['anchors'].append(dict(name=a.get_actor_label(),pos=[p.x,p.y,p.z]))
  levels.append(entry)
 (out/'levels.json').write_text(json.dumps(levels,ensure_ascii=False,indent=2),encoding='utf-8')
 (out/'done.txt').write_text('OK',encoding='utf-8')
except Exception:
 (out/'error.txt').write_text(traceback.format_exc(),encoding='utf-8');raise
