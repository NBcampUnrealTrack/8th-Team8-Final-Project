"""Build sparse warm/ether Niagara streams for the transient Start ambience actor."""
import unreal
DEST='/Game/Core/UI/LobbyAtmosphere'
SOURCE='/Niagara/DefaultAssets/Templates/Systems/FountainLightweight'
def distribution(obj, prop, low, high, channels):
    value=obj.get_editor_property(prop)
    value.set_editor_property('Min',low)
    value.set_editor_property('Max',high)
    value.set_editor_property('ChannelConstantsAndRanges',channels)
    obj.set_editor_property(prop,value)
for suffix,tint in [('Warm',unreal.LinearColor(1.4,.65,.14,.4)),('Ether',unreal.LinearColor(.10,1.35,1.05,.4))]:
    path=DEST+'/NS_StartSideMotes'+suffix
    system=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('NS_StartSideMotes'+suffix,DEST,unreal.load_asset(SOURCE))
    assert system
    emitter=unreal.find_object(system,'Fountain_1')
    spawns=emitter.get_editor_property('SpawnInfos')
    spawn=spawns[0]
    distribution(spawn,'Rate',6.,6.,[6.])
    spawns[0]=spawn
    emitter.set_editor_property('SpawnInfos',spawns)
    emitter.set_editor_property('FixedBounds',unreal.Box(min=unreal.Vector(-250,-250,-250),max=unreal.Vector(250,250,250)))
    for module in emitter.get_editor_property('Modules'):
        name=module.get_name()
        if 'InitializeParticle' in name:
            distribution(module,'LifetimeDistribution',2.8,4.4,[2.8,4.4])
            distribution(module,'ColorDistribution',tint,tint,[tint.r,tint.g,tint.b,tint.a])
            size=module.get_editor_property('SpriteSizeDistribution')
            for field,number in [('Min',.6),('Max',1.5)]:
                vec=size.get_editor_property(field)
                vec.set_editor_property('X',number);vec.set_editor_property('Y',number)
                size.set_editor_property(field,vec)
            size.set_editor_property('ChannelConstantsAndRanges',[.6,1.5])
            module.set_editor_property('SpriteSizeDistribution',size)
        elif 'AddVelocity' in name:
            distribution(module,'ConeVelocityDistribution',4.,9.,[4.,9.])
            module.set_editor_property('ConeAngle',70.)
        elif 'ShapeLocation' in name:
            module.set_editor_property('SphereMax',14.)
        elif 'GravityForce' in name or 'Drag' in name:
            module.set_editor_property('bModuleEnabled',False)
    # Notify the owning emitter after nested module edits to rebuild lightweight runtime data.
    emitter.set_editor_property('RandomSeed',791 if suffix=='Warm' else 1571)
    unreal.EditorAssetLibrary.save_loaded_asset(system)
    unreal.log('START_SIDE_FX_SAVED '+path)
