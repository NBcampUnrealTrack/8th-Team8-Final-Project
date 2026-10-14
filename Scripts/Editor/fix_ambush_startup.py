"""Persist Nanite usage and opt the anchor-free ambush map into unit-only startup."""
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/Ambush/Revision6'
out.mkdir(parents=True, exist_ok=True)
art = '/Game/Game/Stage/AmbushArt'
for name in ['M_AmbushNaturalRock', 'M_AmbushGround', 'BP_AmbushGameMode']:
    source = root / ('Content/Game/Stage/AmbushArt/' + name + '.uasset')
    backup = out / ('Before_' + source.name)
    if not backup.exists():
        shutil.copy2(source, backup)

report = {'materials': {}}
for name in ['M_AmbushNaturalRock', 'M_AmbushGround']:
    material = unreal.load_asset(art + '/' + name)
    material.set_editor_property('used_with_nanite', True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    report['materials'][name] = material.get_editor_property('used_with_nanite')

bp = unreal.load_asset(art + '/BP_AmbushGameMode')
cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(art + '/BP_AmbushGameMode'))
cdo.set_editor_property('allow_stage_without_initial_buildings', True)
assert cdo.get_editor_property('player_building_placement_yaw') == 180
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
shared = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Blueprints/GameMode/BP_SFGameMode'))
assert not shared.get_editor_property('allow_stage_without_initial_buildings')
report['unit_only_stage'] = True
report['shared_mode_requires_anchors'] = True

# The saved navigation brush ends in the ravine. Include the far landing so
# enabling the repaired bridge can rebuild a complete retreat path at runtime.
map_path = '/Game/Game/Stage/PT_SFStageAmbush'
map_file = root / 'Content/Game/Stage/PT_SFStageAmbush.umap'
backup = out / ('Before_' + map_file.name)
if not backup.exists():
    shutil.copy2(map_file, backup)
world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
assert not any(isinstance(a, unreal.SFBuildingSpawnAnchor) for a in actors)
nav = next(a for a in actors if isinstance(a, unreal.NavMeshBoundsVolume))
center, extent = nav.get_actor_bounds(False)
minimum_x = min(center.x - extent.x, -10400.)
maximum_x = center.x + extent.x
target = unreal.Vector((minimum_x + maximum_x) * .5, center.y, center.z)
scale = nav.get_actor_scale3d()
nav.set_actor_scale3d(unreal.Vector(scale.x * (maximum_x - minimum_x) * .5 / extent.x, scale.y, scale.z))
actual_center, _ = nav.get_actor_bounds(False)
nav.set_actor_location(nav.get_actor_location() + target - actual_center, False, False)
assert unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
report['retreat_nav_minimum_x'] = minimum_x
report['starting_anchors'] = 0
(out / 'applied.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
