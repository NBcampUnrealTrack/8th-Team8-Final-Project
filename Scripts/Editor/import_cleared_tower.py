"""Import the conquered watchtower icon only. No maps, progress, or widget assets are modified."""
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
dest='/Game/Core/UI/LobbyAtmosphere'
name='T_LobbyClearedTower'
task=unreal.AssetImportTask()
task.set_editor_properties(dict(
    filename=str(root/'ContentSource/Lobby'/f'{name}.png'),
    destination_path=dest,destination_name=name,
    automated=True,replace_existing=True,save=False))
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture=unreal.load_asset(dest+'/'+name)
assert texture, 'Cleared tower import failed'
texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('never_stream',True)
texture.set_editor_property('srgb',True)
assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log('CLEARED_TOWER_IMPORTED '+texture.get_path_name())
