"""Import only the start-screen UI sprites. Do not load or save any level."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir()).resolve()
dest='/Game/Core/UI/LobbyAtmosphere'
name='T_StartMenuAtlas'
if not unreal.EditorAssetLibrary.does_asset_exist(dest+'/'+name):
    task=unreal.AssetImportTask()
    task.set_editor_properties(dict(filename=str(root/'ContentSource/Lobby'/f'{name}.png'),
        destination_path=dest,destination_name=name,automated=True,save=True))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture=unreal.load_asset(dest+'/'+name)
texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('never_stream',True)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log('START_MENU_ART_SAVED '+texture.get_path_name())
