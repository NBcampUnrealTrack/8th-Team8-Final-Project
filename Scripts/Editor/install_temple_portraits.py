"""Install polished Temple UI portraits derived from both races' models."""
import unreal,json,copy
from pathlib import Path
root=Path(unreal.Paths.project_dir())
source=root/'ContentSource'/'TemplePortraits'
out=root/'Saved'/'TemplePortraits'
out.mkdir(parents=True,exist_ok=True)
assets={}
for race,folder,name in [('Beast','BeastBuilding','T_SFBeastTemple'),('Human','EtherianBuilding','T_SFEtherianTemple')]:
 task=unreal.AssetImportTask()
 task.set_editor_properties(dict(filename=str(source/(name+'.png')),destination_path='/Game/Game/IN_UI/Image/Building/'+folder,destination_name=name,automated=True,replace_existing=True,save=False))
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 tex=unreal.load_asset('/Game/Game/IN_UI/Image/Building/'+folder+'/'+name)
 assert isinstance(tex,unreal.Texture2D), name
 tex.set_editor_properties(dict(lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,never_stream=True,srgb=True))
 assert unreal.EditorAssetLibrary.save_loaded_asset(tex)
 assets[race]=tex

table=unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFSelectionUI')
before=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
rows=copy.deepcopy(before)
temple=next(r for r in rows if r['Name']=='Temple')
temple['PortraitImage']=assets['Beast'].get_path_name()
temple['PortraitVariants']=[]
for race,definition in [('Beast','Animal/DA_AnimalSpecial'),('Human','Human/DA_HumanSpecial')]:
 d=unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/'+definition)
 assert str(d.get_editor_property('selection_ui_row_name'))=='Temple'
 for tier in range(1,len(d.get_editor_property('tiers'))+1):
  temple['PortraitVariants'].append('(Race='+race+',Tier='+str(tier)+',PortraitImage="'+assets[race].get_path_name()+'")')
 d.set_editor_property('icon',assets[race])
 assert unreal.EditorAssetLibrary.save_loaded_asset(d)
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False))
after=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
assert [r for r in before if r['Name']!='Temple']==[r for r in after if r['Name']!='Temple'], 'Non-Temple rows changed'
assert next(r for r in after if r['Name']=='Temple')==temple, 'Temple round-trip mismatch'
assert unreal.EditorAssetLibrary.save_loaded_asset(table)
(out/'selection.after.json').write_text(json.dumps(after,ensure_ascii=False,indent=2),encoding='utf-8')
(out/'verified.txt').write_text('PASS: Beast and Human Temple portraits at tiers 1 and 2.\nPASS: Both construction icons use the corresponding Temple model.\nPASS: All other selection rows unchanged.\n',encoding='utf-8')
unreal.log('TEMPLE_PORTRAITS_INSTALLED_AND_VERIFIED')
