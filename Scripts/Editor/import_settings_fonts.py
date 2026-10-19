"""Import settings fonts via -ExecutePythonScript; FontFace import requires Slate.

Source: https://github.com/notofonts/noto-cjk/tree/main/Sans/SubsetOTF/KR
License: Content/ThirdPartyLicenses/NotoSansKR-OFL.txt
"""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
for weight in ('Medium', 'Bold'):
    name = 'NotoSansKR_' + weight
    task = unreal.AssetImportTask()
    task.set_editor_property('factory', unreal.FontFileImportFactory())
    task.set_editor_properties(dict(
        filename=str(root / 'ContentSource/Fonts' / ('NotoSansKR-' + weight + '.otf')),
        destination_path='/Game/Game/IN_UI/Font', destination_name=name,
        automated=True, replace_existing=False, save=True))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    face = unreal.load_asset('/Game/Game/IN_UI/Font/' + name)
    assert isinstance(face, unreal.FontFace), 'Font face import failed: ' + name
    unreal.log('SETTINGS_FONT_IMPORTED ' + face.get_path_name())
unreal.SystemLibrary.quit_editor()
