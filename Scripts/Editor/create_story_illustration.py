"""Create dedicated story Niagara/UI assets; reuse existing faction art without modifying it."""
from pathlib import Path
import unreal

dest = '/Game/Core/UI/Story'
lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()

def distribution(obj, name, low, high, channels):
    value = obj.get_editor_property(name)
    value.set_editor_property('Min', low)
    value.set_editor_property('Max', high)
    value.set_editor_property('ChannelConstantsAndRanges', channels)
    obj.set_editor_property(name, value)

for suffix in ['Ether', 'Warm']:
    name = 'NS_Story'+suffix
    system = unreal.load_asset(dest+'/'+name)
    if not system:
        source = unreal.load_asset('/Game/Core/UI/LobbyAtmosphere/NS_StartSideMotes'+suffix)
        system = assets.duplicate_asset(name, dest, source)
    emitter = unreal.find_object(system, 'Fountain_1')
    spawns = emitter.get_editor_property('SpawnInfos')
    spawn = spawns[0]
    distribution(spawn, 'Rate', 18., 18., [18.])
    spawns[0] = spawn
    emitter.set_editor_property('SpawnInfos', spawns)
    emitter.set_editor_property('FixedBounds', unreal.Box(min=unreal.Vector(-220,-220,-220),max=unreal.Vector(220,220,220)))
    for module in emitter.get_editor_property('Modules'):
        name = module.get_name()
        if 'InitializeParticle' in name:
            distribution(module, 'LifetimeDistribution', 3., 5., [3.,5.])
            tint = unreal.LinearColor(.1,2.2,1.7,.85) if suffix=='Ether' else unreal.LinearColor(2.4,1.1,.2,.85)
            distribution(module, 'ColorDistribution', tint, tint, [tint.r,tint.g,tint.b,tint.a])
            size = module.get_editor_property('SpriteSizeDistribution')
            for field, number in [('Min',2.5),('Max',5.)]:
                vec=size.get_editor_property(field)
                vec.set_editor_property('X',number); vec.set_editor_property('Y',number)
                size.set_editor_property(field,vec)
            size.set_editor_property('ChannelConstantsAndRanges',[2.5,5.])
            module.set_editor_property('SpriteSizeDistribution',size)
        elif 'ShapeLocation' in name:
            module.set_editor_property('SphereMax',90.)
        elif 'AddVelocity' in name:
            distribution(module,'ConeVelocityDistribution',8.,16.,[8.,16.])
    emitter.set_editor_property('RandomSeed',1921 if suffix=='Ether' else 3451)
    unreal.EditorAssetLibrary.save_loaded_asset(system)
    unreal.log('STORY_ASSET '+system.get_path_name())

name='M_StoryCaptureUI'
mat=unreal.load_asset(dest+'/'+name)
if not mat:
    mat=assets.create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-600,0)
    tex=lib.create_material_expression(mat,unreal.MaterialExpressionTextureObjectParameter,-600,150)
    tex.set_editor_property('parameter_name','Scene')
    tex.set_editor_property('texture',unreal.load_asset('/Engine/EngineResources/Black'))
    custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom,-200,0)
    inputs=[]
    for input_name in ['UV','Scene']:
        value=unreal.CustomInput(); value.set_editor_property('input_name',input_name); inputs.append(value)
    custom.set_editor_properties(dict(inputs=inputs,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4))
    lib.connect_material_expressions(uv,'',custom,'UV')
    lib.connect_material_expressions(tex,'',custom,'Scene')
    for alpha in [False,True]:
        mask=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask,100,150 if alpha else 0)
        mask.set_editor_properties(dict(r=not alpha,g=not alpha,b=not alpha,a=alpha))
        lib.connect_material_expressions(custom,'',mask,'')
        lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY if alpha else unreal.MaterialProperty.MP_EMISSIVE_COLOR)
else:
    custom=next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer()==mat)
# HDR capture stores inverse opacity. Additive Niagara sprites contribute RGB instead.
custom.set_editor_property('code','float4 c=Texture2DSample(Scene,SceneSampler,UV); float a=saturate(1-c.a+max(c.r,max(c.g,c.b))*.6); float edge=1-smoothstep(.45,.49,length(UV-.5)); return float4(c.rgb, a*edge);')
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat)

# Export existing UI art for inspection, leaving the original assets intact.
folder=Path(unreal.Paths.project_saved_dir())/'StoryArtInspection'
folder.mkdir(exist_ok=True)
for name,path in {
    'BeastNexus':'/Game/Game/IN_UI/Image/Building/BeastBuilding/T_SFBeastNexus1T',
    'EtherNexus':'/Game/Game/IN_UI/Image/Building/EtherianBuilding/T_SFHumanNexus1T',
    'EtherNexus3':'/Game/Game/IN_UI/Image/Building/EtherianBuilding/T_SFHumanNexus3T',
    'Ether':'/Game/Game/IN_UI/Image/Building/Resource/T_SFEther',
}.items():
    task=unreal.AssetExportTask()
    task.set_editor_properties(dict(object=unreal.load_asset(path),filename=str(folder/(name+'.png')),automated=True,prompt=False,replace_identical=True,exporter=unreal.TextureExporterPNG()))
    unreal.Exporter.run_asset_export_task(task)
unreal.log('STORY_ASSETS_READY')
