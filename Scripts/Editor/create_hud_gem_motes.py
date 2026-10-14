"""Dedicated sparse HUD Niagara; original lobby/story systems remain untouched."""
import unreal
dest='/Game/Game/IN_UI/HUDMotes'
tools=unreal.AssetToolsHelpers.get_asset_tools()
def distribution(obj,prop,low,high,channels):
    value=obj.get_editor_property(prop)
    value.set_editor_property('Min',low);value.set_editor_property('Max',high)
    value.set_editor_property('ChannelConstantsAndRanges',channels);obj.set_editor_property(prop,value)
system=unreal.load_asset(dest+'/NS_HUDGemMotes') if unreal.EditorAssetLibrary.does_asset_exist(dest+'/NS_HUDGemMotes') else tools.duplicate_asset('NS_HUDGemMotes',dest,unreal.load_asset('/Game/Core/UI/Story/NS_StoryEther'))
emitter=unreal.find_object(system,'Fountain_1')
spawns=emitter.get_editor_property('SpawnInfos');spawn=spawns[0]
distribution(spawn,'Rate',9.,9.,[9.]);spawns[0]=spawn;emitter.set_editor_property('SpawnInfos',spawns)
emitter.set_editor_property('FixedBounds',unreal.Box(min=unreal.Vector(-100,-100,-100),max=unreal.Vector(100,100,100)))
for module in emitter.get_editor_property('Modules'):
    name=module.get_name()
    if 'InitializeParticle' in name:
        distribution(module,'LifetimeDistribution',1.5,2.8,[1.5,2.8])
        tint=unreal.LinearColor(.22,1.5,1.15,.8)
        distribution(module,'ColorDistribution',tint,tint,[tint.r,tint.g,tint.b,tint.a])
        size=module.get_editor_property('SpriteSizeDistribution')
        for field,number in [('Min',1.4),('Max',3.6)]:
            vec=size.get_editor_property(field);vec.set_editor_property('X',number);vec.set_editor_property('Y',number);size.set_editor_property(field,vec)
        size.set_editor_property('ChannelConstantsAndRanges',[1.4,3.6]);module.set_editor_property('SpriteSizeDistribution',size)
    elif 'ShapeLocation' in name:module.set_editor_property('SphereMax',28.)
    elif 'AddVelocity' in name:distribution(module,'ConeVelocityDistribution',4.,9.,[4.,9.])
emitter.set_editor_property('RandomSeed',4217)
assert unreal.EditorAssetLibrary.save_loaded_asset(system)
mat=unreal.load_asset(dest+'/M_HUDGemMotes') if unreal.EditorAssetLibrary.does_asset_exist(dest+'/M_HUDGemMotes') else tools.duplicate_asset('M_HUDGemMotes',dest,unreal.load_asset('/Game/Core/UI/Story/M_StoryCaptureUI'))
custom=next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer()==mat)
custom.set_editor_property('code','float4 c=Texture2DSample(Scene,SceneSampler,UV); float edge=(1-smoothstep(.35,.49,abs(UV.x-.5)))*(1-smoothstep(.30,.49,abs(UV.y-.5))); float light=max(c.r,max(c.g,c.b)); return float4(c.rgb,saturate(light*.85)*edge);')
unreal.MaterialEditingLibrary.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('HUD_GEM_NIAGARA_READY')
