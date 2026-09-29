"""Create the world-space mote and localized menu-shade materials, without editing the camera or level."""
import unreal
dest='/Game/Core/UI/LobbyAtmosphere'
lib=unreal.MaterialEditingLibrary
def create(name,ui,code):
    mat=unreal.load_asset(dest+'/'+name)
    if mat:
        node=next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer()==mat)
        node.set_editor_property('code',code)
    else:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
        mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI if ui else unreal.MaterialDomain.MD_SURFACE)
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if ui else unreal.BlendMode.BLEND_ADDITIVE)
        if not ui:
            mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
            mat.set_editor_property('two_sided',True)
            mat.set_editor_property('used_with_instanced_static_meshes',True)
        uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-500,0)
        tint=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-500,150)
        tint.set_editor_properties(dict(parameter_name='Tint',default_value=unreal.LinearColor(1,.67,.23,1)))
        node=lib.create_material_expression(mat,unreal.MaterialExpressionCustom,-150,0)
        inputs=[]
        for name in ['UV','Tint']:
            value=unreal.CustomInput();value.set_editor_property('input_name',name);inputs.append(value)
        node.set_editor_properties(dict(code=code,inputs=inputs,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4))
        lib.connect_material_expressions(uv,'',node,'UV');lib.connect_material_expressions(tint,'',node,'Tint')
        for alpha in [False,True]:
            mask=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask,150,150 if alpha else 0)
            mask.set_editor_properties(dict(r=not alpha,g=not alpha,b=not alpha,a=alpha))
            lib.connect_material_expressions(node,'',mask,'')
            lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY if alpha else unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
create('M_StartWorldMote',False,'float2 d=(UV-.5)*2; float r=dot(d,d); float glow=exp(-r*8)*.26+exp(-r*65); return float4(Tint*2.2,saturate(glow)*(1-smoothstep(.65,1,r)));')
create('M_StartMenuShade',True,'float a=smoothstep(.58,1,UV.x)*smoothstep(.25,.72,UV.y)*.34; return float4(.014,.028,.024,a);')
unreal.log('START_SCENE_AMBIENCE_SAVED')
