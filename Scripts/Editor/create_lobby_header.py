"""Import the new header art and create its alpha-masked hover material only."""
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
dest='/Game/Core/UI/LobbyAtmosphere'
name='T_LobbyHeaderAtlas'
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
lib=unreal.MaterialEditingLibrary
mat=unreal.load_asset(dest+'/M_LobbyHeader')
code=(root/'ContentSource/Lobby/LobbyHeader.ush').read_text()
if mat:
    custom=next(x for x in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if x.get_outer()==mat)
    custom.set_editor_property('code',code)
else:
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_LobbyHeader',dest,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    def node(cls,x,y): return lib.create_material_expression(mat,cls,x,y)
    uv=node(unreal.MaterialExpressionTextureCoordinate,-700,0)
    atlas=node(unreal.MaterialExpressionTextureObjectParameter,-700,100)
    atlas.set_editor_properties(dict(parameter_name='Atlas',texture=texture))
    region=node(unreal.MaterialExpressionVectorParameter,-700,200)
    region.set_editor_properties(dict(parameter_name='Region',default_value=unreal.LinearColor(0,0,1,1)))
    region_rgba=node(unreal.MaterialExpressionAppendVector,-450,200)
    assert lib.connect_material_expressions(region,'',region_rgba,'A')
    assert lib.connect_material_expressions(region,'A',region_rgba,'B')
    sources=[uv,atlas,region_rgba]
    names=['UV','Atlas','Region','Elapsed','Hover','Press','Title']
    for i,name in enumerate(names[3:]):
        scalar=node(unreal.MaterialExpressionScalarParameter,-700,300+i*100)
        scalar.set_editor_property('parameter_name',name)
        sources.append(scalar)
    custom=node(unreal.MaterialExpressionCustom,-200,0)
    inputs=[]
    for name in names:
        value=unreal.CustomInput();value.set_editor_property('input_name',name);inputs.append(value)
    custom.set_editor_properties(dict(code=code,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,inputs=inputs))
    for source,name in zip(sources,names):
        assert lib.connect_material_expressions(source,'',custom,name),name
    rgb=node(unreal.MaterialExpressionComponentMask,100,0)
    rgb.set_editor_properties(dict(r=True,g=True,b=True,a=False))
    alpha=node(unreal.MaterialExpressionComponentMask,100,150)
    alpha.set_editor_properties(dict(r=False,g=False,b=False,a=True))
    lib.connect_material_expressions(custom,'',rgb,'')
    lib.connect_material_expressions(custom,'',alpha,'')
    lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('LOBBY_HEADER_SAVED '+mat.get_path_name())
