"""Configure Stage 02 enemy production and three attack approaches.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
The three newly created lanes carry SF_Stage02AttackRoute for safe reruns.
"""
import json
import unreal

MAP = '/Game/Game/Stage/PT_Stage_2'
TAG = unreal.Name('SF_Stage02AttackRoute')
if not unreal.EditorLoadingAndSavingUtils.load_map(MAP):
    raise RuntimeError('Could not load Stage 02')
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
by_name = {a.get_name(): a for a in actors}

spawner_labels = ('BP_UnitSpawner', 'BP_UnitSpawner2', 'BP_UnitSpawner3',
                  'BP_UnitSpawner4', 'BP_UnitSpawner5')
for label in spawner_labels:
    if by_label[label].get_class().get_name() != 'BP_UnitSpawner_C':
        raise RuntimeError('Unexpected spawner ' + label)

# The Human catalog provides a ranged factory and a second barracks tier.
barracks = unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/Human/DA_HumanBarrack')
ranged_factory = unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/Human/DA_HumanRanged')
if not barracks or len(barracks.get_editor_property('tiers')) < 2:
    raise RuntimeError('Human barracks Tier 2 is missing')
if not ranged_factory or not ranged_factory.get_editor_property('tiers'):
    raise RuntimeError('Human ranged factory is missing')
unit_table = unreal.load_asset('/Game/Core/DataTable/DT_Units')
rows = {r['Name'] for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unit_table))}
if not {'Fighter', 'Shooter', 'Warrior'} <= rows:
    raise RuntimeError('Stage 02 unit rows are missing')

# Each lane begins at its own spawner. The western and central paths lead to
# different sides of the player base; the eastern path keeps the existing flank.
routes = {
    'BP_UnitSpawner': (
        'Stage02_WestNorth', 'LanePath_1',
        [(3734, 12642), (1000, 11600), (-2000, 10000), (-4200, 7900),
         (-4900, 4800), (-5000, 1200), (-4700, -1600)]),
    'BP_UnitSpawner2': (
        'Stage02_CenterNorth', 'LanePath_3',
        [(3734, 11110), (2200, 8500), (1000, 6000), (-1800, 8000),
         (-3000, 4940), (-1000, 2500), (-2000, 900), (-3500, -1700)]),
    'BP_UnitSpawner3': (
        'Stage02_WestFlank', None,
        [(-160, 8327), (-2200, 8000), (-4200, 6500), (-5100, 3500),
         (-5000, -300), (-4700, -1600)]),
    'BP_UnitSpawner4': (
        'Stage02_CenterRanged', None,
        [(4308, 8883), (2900, 7000), (1400, 5100), (-1000, 7000),
         (-3000, 4940), (-1000, 2500), (-2000, 900), (-3500, -1700)]),
    'BP_UnitSpawner5': (
        'Stage02_EastWarrior', None,
        [(7186, 10585), (6300, 7800), (5000, 5800), (4600, 2100),
         (1700, 200), (-2300, -1700)]),
}

world = unreal.EditorLevelLibrary.get_editor_world()
for label, (_, _, points) in routes.items():
    for p, q in zip(points, points[1:]):
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
            world, unreal.Vector(p[0], p[1], 0), unreal.Vector(q[0], q[1], 0))
        if not path or not path.is_valid() or path.is_partial():
            raise RuntimeError('No full navigation path on %s: %s -> %s' % (label, p, q))

for actor in actors:
    if TAG in actor.tags:
        sub.destroy_actor(actor)

for label, (lane_label, existing_name, points) in routes.items():
    lane = by_name[existing_name] if existing_name else sub.spawn_actor_from_class(
        unreal.LanePath, unreal.Vector(points[0][0], points[0][1], 0),
        unreal.Rotator(pitch=0, yaw=0, roll=0))
    if not lane or lane.get_class().get_name() != 'LanePath':
        raise RuntimeError('Could not configure lane ' + lane_label)
    lane.set_actor_label(lane_label)
    lane.set_folder_path('Stage 02/Enemy Attack Routes')
    lane.set_editor_property('team_index', 1)
    lane.set_editor_property('lane_half_width', 300.0)
    if not existing_name:
        lane.set_editor_property('tags', [TAG])
    spline = lane.get_editor_property('spline')
    spline.clear_spline_points(False)
    for x, y in points:
        spline.add_spline_point(unreal.Vector(x, y, 0), unreal.SplineCoordinateSpace.WORLD, False)
    for i in range(len(points)):
        spline.set_spline_point_type(i, unreal.SplinePointType.LINEAR, False)
    spline.update_spline()
    by_label[label].set_editor_property('lane', lane)

# Keep the first three factories on Tier 1 Fighters. One ranged factory
# produces Shooters and the last barracks starts at Tier 2 for Warriors.
by_label['BP_BuildingSpawn8'].set_editor_property('building_type', unreal.SFBuildingType.RANGED_PRODUCTION)
by_label['BP_BuildingSpawn8'].set_editor_property('initial_tier_index', 0)
by_label['BP_BuildingSpawn9'].set_editor_property('building_type', unreal.SFBuildingType.BARRACKS)
by_label['BP_BuildingSpawn9'].set_editor_property('initial_tier_index', 1)
for label, row, asset in (
    ('BP_UnitSpawner4', 'Shooter', '/Game/Core/Units/Etherial_Human/BP_Human_Shooter'),
    ('BP_UnitSpawner5', 'Warrior', '/Game/Core/Units/Etherial_Human/BP_Human_Warrior'),
):
    unit_class = unreal.EditorAssetLibrary.load_blueprint_class(asset)
    if not unit_class:
        raise RuntimeError('Missing unit blueprint ' + asset)
    spawner = by_label[label]
    spawner.set_editor_property('unit_class', unit_class)
    spawner.set_editor_property('unit_row_name', unreal.Name(row))

for label in spawner_labels:
    spawner = by_label[label]
    spawner.set_editor_property('stop_when_owner_building_destroyed', True)
    spawner.set_editor_property('require_owner_building', True)
    spawner.set_editor_property('owner_building_search_radius', 200.0)
    spawner.set_editor_property('owner_building_resolve_timeout', 60.0)

if not unreal.EditorLoadingAndSavingUtils.save_current_level():
    raise RuntimeError('Could not save Stage 02')

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
saved = {a.get_actor_label(): a for a in sub.get_all_level_actors()}
assert saved['BP_BuildingSpawn8'].get_editor_property('building_type') == unreal.SFBuildingType.RANGED_PRODUCTION
assert saved['BP_BuildingSpawn9'].get_editor_property('initial_tier_index') == 1
for label, (lane_label, _, points) in routes.items():
    spawner = saved[label]
    lane = spawner.get_editor_property('lane')
    assert lane and lane.get_actor_label() == lane_label
    assert lane.get_editor_property('spline').get_number_of_spline_points() == len(points)
    assert spawner.get_editor_property('require_owner_building')
    assert spawner.get_editor_property('stop_when_owner_building_destroyed')
assert str(saved['BP_UnitSpawner4'].get_editor_property('unit_row_name')) == 'Shooter'
assert str(saved['BP_UnitSpawner5'].get_editor_property('unit_row_name')) == 'Warrior'
unreal.log('STAGE02_ENEMY_CONFIG_VERIFIED 5 owner-gated spawners, Shooter, Warrior, 5 attack lanes')
