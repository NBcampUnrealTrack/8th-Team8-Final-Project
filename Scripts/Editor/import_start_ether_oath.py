"""Import the approved title and its UV-local animation; never load/save the Start map."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
dest = '/Game/Core/UI/LobbyAtmosphere'
lib = unreal.MaterialEditingLibrary
texture = unreal.load_asset(dest+'/T_StartEtherOath') if unreal.EditorAssetLibrary.does_asset_exist(dest+'/T_StartEtherOath') else None
if texture is None:
    task = unreal.AssetImportTask()
    task.set_editor_properties(dict(filename=str(root/'ContentSource/Start/T_StartEtherOath.png'),
        destination_path=dest, destination_name='T_StartEtherOath', automated=True,
        replace_existing=True, save=True))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(dest+'/T_StartEtherOath')
    texture.set_editor_properties(dict(lod_group=unreal.TextureGroup.TEXTUREGROUP_UI,
        compression_settings=unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        never_stream=True, mip_gen_settings=unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS))
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
mat = unreal.load_asset(dest+'/M_StartEtherOathLogo')
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_StartEtherOathLogo', dest,
        unreal.Material, unreal.MaterialFactoryNew())
existing = [x for x in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if x.get_outer() == mat]
if existing:
    existing[0].set_editor_property('code', (root/'ContentSource/Start/EtherOathLogo.ush').read_text())
else:
    mat.set_editor_properties(dict(material_domain=unreal.MaterialDomain.MD_UI,
        blend_mode=unreal.BlendMode.BLEND_TRANSLUCENT))
    uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -500, 0)
    logo = lib.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -500, 100)
    logo.set_editor_properties(dict(parameter_name='Logo', texture=texture))
    elapsed = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 200)
    elapsed.set_editor_properties(dict(parameter_name='Elapsed', default_value=0.0))
    strength = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 300)
    strength.set_editor_properties(dict(parameter_name='Strength', default_value=1.0))
    gem = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 400)
    gem.set_editor_properties(dict(parameter_name='GemCenter', default_value=unreal.LinearColor(.507, .714, 0, 0)))
    node = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, 0, 0)
    inputs = []
    sources = dict(UV=uv, Logo=logo, Elapsed=elapsed, Strength=strength, GemCenter=gem)
    for name in sources:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    node.set_editor_properties(dict(code=(root/'ContentSource/Start/EtherOathLogo.ush').read_text(),
        inputs=inputs, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4))
    for name, source in sources.items():
        lib.connect_material_expressions(source, '', node, name)
    for alpha in (False, True):
        mask = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 300, 150 if alpha else 0)
        mask.set_editor_properties(dict(r=not alpha, g=not alpha, b=not alpha, a=alpha))
        lib.connect_material_expressions(node, '', mask, '')
        lib.connect_material_property(mask, '', unreal.MaterialProperty.MP_OPACITY if alpha else unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('START_ETHER_OATH_IMPORTED')
