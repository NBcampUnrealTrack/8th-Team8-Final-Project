"""Build only new lobby presentation assets; existing maps and widget BPs are untouched.
Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
"""
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Core/UI/LobbyAtmosphere'
lib = unreal.MaterialEditingLibrary
unreal.EditorAssetLibrary.make_directory(DEST)

def import_texture(name):
    if not unreal.EditorAssetLibrary.does_asset_exist(DEST+'/'+name):
        task = unreal.AssetImportTask()
        task.set_editor_properties(dict(filename=str(ROOT/('ContentSource/Lobby/'+name+'.png')),
            destination_path=DEST, destination_name=name, automated=True, save=True))
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(DEST+'/'+name)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('never_stream', True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture

table = import_texture('T_LobbyLowPolyCamp')
atlas = import_texture('T_LobbyLowPolyAtlas')
icons = import_texture('T_LobbyStageIcons')

mat = unreal.load_asset(DEST+'/M_LobbyAtmosphere')
if mat:
    # CDO references root these expressions; update the existing node in place.
    custom = next(x for x in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if x.get_outer() == mat)
    custom.set_editor_property('code',(ROOT/'ContentSource/Lobby/LobbyAtmosphere.ush').read_text())
    for node in unreal.ObjectIterator(unreal.MaterialExpressionTextureObjectParameter):
        if node.get_outer() == mat:
            name = str(node.get_editor_property('parameter_name'))
            if name in ['Map','Table']:
                node.set_editor_property('texture',atlas if name == 'Map' else table)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log('LOBBY_ATMOSPHERE_UPDATED '+mat.get_path_name())
else:
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_LobbyAtmosphere', DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)

    def node(cls, x, y):
        return lib.create_material_expression(mat, cls, x, y)

    uv = node(unreal.MaterialExpressionTextureCoordinate,-800,0)
    elapsed = node(unreal.MaterialExpressionScalarParameter,-800,120)
    elapsed.set_editor_property('parameter_name','Elapsed')
    elapsed.set_editor_property('default_value',3.0)
    departure = node(unreal.MaterialExpressionScalarParameter,-800,240)
    departure.set_editor_property('parameter_name','Departure')
    departure.set_editor_property('default_value',0.0)
    map_tex = node(unreal.MaterialExpressionTextureObjectParameter,-800,360)
    map_tex.set_editor_property('parameter_name','Map')
    map_tex.set_editor_property('texture',atlas)
    table_tex = node(unreal.MaterialExpressionTextureObjectParameter,-800,480)
    table_tex.set_editor_property('parameter_name','Table')
    table_tex.set_editor_property('texture',table)

    custom = node(unreal.MaterialExpressionCustom,-350,0)
    custom.set_editor_property('code',(ROOT/'ContentSource/Lobby/LobbyAtmosphere.ush').read_text())
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('description','Unfolded atlas, lantern, drifting mist and motes')
    names = ['UV','Elapsed','Departure','Map','Table']
    inputs = []
    for name in names:
        item = unreal.CustomInput()
        item.set_editor_property('input_name',name)
        inputs.append(item)
    custom.set_editor_property('inputs',inputs)
    for source,name in zip([uv,elapsed,departure,map_tex,table_tex],names):
        assert lib.connect_material_expressions(source,'',custom,name)
    rgb = node(unreal.MaterialExpressionComponentMask,0,0)
    rgb.set_editor_properties(dict(r=True,g=True,b=True,a=False))
    alpha = node(unreal.MaterialExpressionComponentMask,0,150)
    alpha.set_editor_properties(dict(r=False,g=False,b=False,a=True))
    lib.connect_material_expressions(custom,'',rgb,'')
    lib.connect_material_expressions(custom,'',alpha,'')
    lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.log('LOBBY_ATMOSPHERE_CREATED '+mat.get_path_name())
