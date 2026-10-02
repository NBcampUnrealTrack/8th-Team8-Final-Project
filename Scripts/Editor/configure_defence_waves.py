"""Configure the existing Stage03 defence map; run with Unreal's Python commandlet.

Only replaces actors tagged SF_DefenceWaves. Existing scenery and player buildings
are preserved. Saved/DefenceWaves contains a backup and verification report.
"""
import json
import shutil
from pathlib import Path
import unreal

MAP = '/Game/Game/Stage/PT_StageDefence'
TAG = unreal.Name('SF_DefenceWaves')
root = Path(unreal.Paths.project_dir())
folder = root / 'Saved' / 'DefenceWaves'
folder.mkdir(parents=True, exist_ok=True)
backup = folder / 'PT_StageDefence.before_waves.umap'
if not backup.exists():
    shutil.copy2(root / 'Content/Game/Stage/PT_StageDefence.umap', backup)

assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.EditorLevelLibrary.get_editor_world()
actors = sub.get_all_level_actors()
assert not any(isinstance(a, unreal.UnitSpawner) and TAG not in a.tags for a in actors), \
    'Unexpected non-wave spawners: inspect before changing the map'
pool = []
for row in ('Fighter', 'Warrior', 'Guardian', 'Shooter', 'Sharpshooter', 'Judicator'):
    cls = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Units/Etherial_Human/BP_Human_' + row)
    assert cls, 'Missing unit class: ' + row
    entry = unreal.SFDefenceUnitEntry()
    entry.set_editor_property('row_name', unreal.Name(row))
    entry.set_editor_property('unit_class', cls)
    entry.set_editor_property('spawn_weight', {'Fighter': 5, 'Warrior': 3, 'Guardian': 1,
                                              'Shooter': 5, 'Sharpshooter': 3, 'Judicator': 1}[row])
    pool.append(entry)

# These are the centres of the map's three open forest approaches. Use the baked
# navigation path for each lane, ensuring that every enemy can reach the base.
routes = []
for name, x, y in [('North', -4330., -2500.), ('East', 0., 5000.), ('South', 4330., -2500.)]:
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
        world, unreal.Vector(x, y, 50), unreal.Vector(-700, 500, 50))
    assert path and path.is_valid() and not path.is_partial(), 'No complete route: ' + name
    routes.append((name, list(path.path_points)))

for actor in actors:
    if TAG in actor.tags:
        sub.destroy_actor(actor)

spawners = []
report = []
for name, points in routes:
    lane = sub.spawn_actor_from_class(unreal.LanePath, points[0])
    lane.set_actor_label('Defence_' + name + '_Lane')
    lane.set_folder_path('Defence/Wave Routes')
    lane.set_editor_property('tags', [TAG])
    lane.set_editor_property('team_index', 1)
    lane.set_editor_property('lane_half_width', 300.)
    spline = lane.get_editor_property('spline')
    spline.clear_spline_points(False)
    for point in points:
        spline.add_spline_point(point, unreal.SplineCoordinateSpace.WORLD, False)
    for index in range(len(points)):
        spline.set_spline_point_type(index, unreal.SplinePointType.LINEAR, False)
    spline.update_spline()

    rotation = unreal.MathLibrary.find_look_at_rotation(points[0], points[-1])
    spawner = sub.spawn_actor_from_class(unreal.UnitSpawner, points[0], rotation)
    spawner.set_actor_label('Defence_' + name + '_Spawner')
    spawner.set_folder_path('Defence/Enemy Spawners')
    spawner.set_editor_property('tags', [TAG])
    for prop, value in dict(team_index=1, wave_controlled=True, start_active=False,
                            spawning_enabled=False, stop_when_owner_building_destroyed=False,
                            require_owner_building=False, max_alive=0, max_total=0,
                            spawn_scatter_radius=150., lane=lane).items():
        spawner.set_editor_property(prop, value)
    spawners.append(spawner)
    report.append(dict(direction=name, points=[[p.x, p.y, p.z] for p in points]))

controller = sub.spawn_actor_from_class(unreal.SFDefenceWaveController, unreal.Vector(0, 0, 0))
controller.set_actor_label('Defence_WaveController')
controller.set_folder_path('Defence')
controller.set_editor_property('tags', [TAG])
for prop, value in dict(spawners=spawners, unit_pool=pool, total_waves=5,
                        initial_preparation_duration=60., preparation_duration=180.,
                        wave_duration=300., spawn_interval=4., first_wave_copies_per_type=6,
                        additional_copies_per_wave=3, spawn_count_multiplier=0.4 / 3.).items():
    controller.set_editor_property(prop, value)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()

# Reload the saved map to verify serialized actor references and exclusions.
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.EditorLevelLibrary.get_editor_world()
saved = sub.get_all_level_actors()
controllers = [a for a in saved if isinstance(a, unreal.SFDefenceWaveController)]
assert len(controllers) == 1
controller = controllers[0]
assert len([a for a in saved if isinstance(a, unreal.UnitSpawner)]) == 3
assert len(controller.get_editor_property('spawners')) == 3
assert controller.get_editor_property('preparation_duration') == 180.
assert controller.get_editor_property('initial_preparation_duration') == 60.
assert controller.get_editor_property('total_waves') == 5
assert controller.get_editor_property('wave_duration') == 300.
assert abs(controller.get_editor_property('spawn_count_multiplier') - 0.4 / 3.) < 0.000001
assert [e.spawn_weight for e in controller.get_editor_property('unit_pool')] == [5, 3, 1, 5, 3, 1]
assert [str(e.row_name) for e in controller.get_editor_property('unit_pool')] == [
    'Fighter', 'Warrior', 'Guardian', 'Shooter', 'Sharpshooter', 'Judicator']
for spawner in controller.get_editor_property('spawners'):
    assert spawner.get_editor_property('wave_controlled')
    assert not spawner.get_editor_property('spawning_enabled')
    assert not spawner.get_editor_property('stop_when_owner_building_destroyed')
    assert not spawner.get_editor_property('require_owner_building')
    assert spawner.get_editor_property('team_index') == 1
    assert spawner.get_editor_property('lane')
    lane = spawner.get_editor_property('lane')
    spline = lane.get_editor_property('spline')
    for i in range(spline.get_number_of_spline_points() - 1):
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world,
            spline.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD),
            spline.get_location_at_spline_point(i + 1, unreal.SplineCoordinateSpace.WORLD))
        assert path and path.is_valid() and not path.is_partial()
assert not any(isinstance(a, unreal.SFBuildingSpawnAnchor) and a.get_editor_property('team_index') == 1
               for a in saved), 'Defence enemies must have no production buildings'
(folder / 'configuration.json').write_text(json.dumps(dict(
    map=MAP, initial_preparation_seconds=60, preparation_seconds=180, wave_seconds=300,
    total_waves=5, wave_totals=[14, 22, 29, 36, 43],
    first_wave_total=14, second_wave_total=22, spawn_count_multiplier=0.4 / 3.,
    tier_weights=[5, 3, 1], routes=report,
    units=['Fighter', 'Warrior', 'Guardian', 'Shooter', 'Sharpshooter', 'Judicator']), indent=2))
unreal.log('DEFENCE_WAVES_VERIFIED: 3 standalone spawners, 5 waves, preparation 60/180s, tier weights 5:3:1')
