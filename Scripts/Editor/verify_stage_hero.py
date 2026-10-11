"""Read-only structural verification for Stage_Hero."""
import json
import re
import sys
import math
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).parent))
from hero_curve_layout import ROUTE, SECTIONS

MAP = '/Game/Game/Stage/PT_Stage_Hero'
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
table = unreal.load_asset('/Game/Core/DataTable/DT_SFStage')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
hero = next(row for row in rows if row['Name'] == 'StageHero')

assert hero['PlayerRace'] == 'Human', hero
assert hero['TeamSetups'] == ['(TeamIndex=1,Race=Beast)'], hero
assert hero['StageLevel'] == MAP + '.PT_Stage_Hero', hero
assert len([r for r in rows if r['StageLevel'] == hero['StageLevel']]) == 1
assert hero['StageOrder'] == 5
assert any('BuildingId="EtherMining",bCanBuild=True' in rule for rule in hero['BuildingRules'])
spline=by_label['Hero_PlayerRoute'].get_editor_property('spline')
assert spline.get_number_of_spline_points() == len(ROUTE)
for i,(x,y) in enumerate(ROUTE):
    p=spline.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD)
    assert abs(p.x-x)<.1 and abs(p.y-y)<.1
    assert spline.get_spline_point_type(i)==unreal.SplinePointType.CURVE

groups = []
for label, actor in by_label.items():
    if label.startswith('Hero_Enemy_'):
        target = actor.get_component_by_class(unreal.CombatTargetComponent)
        assert target and target.get_editor_property('team_index') == 1, label
        assert len(actor.get_editor_property('auto_advance_teams')) == 0, label
        groups.append(dict(label=label, row=str(actor.get_editor_property('unit_row_name')),
                           team=target.get_editor_property('team_index')))
assert len(groups) == 17, groups
assert len([a for a in actors if a.get_actor_label().startswith('Hero_Road_')]) >= 40
assert len([a for a in actors if a.get_actor_label().startswith('Hero_Ground_')]) == 90
assert len([a for a in actors if a.get_actor_label().startswith('Hero_Divider_')]) == 3
assert by_label['Hero_Ground_00_00'].static_mesh_component.get_material(0).get_name() == 'M_HeroForestFloor'
assert by_label['Hero_Divider_North'].get_editor_property('static_mesh_component').get_material(0).get_path_name().endswith('/texture_material/Modular_Map_M.Modular_Map_M')
resource_counts = {}
for label, actor in by_label.items():
    if label.startswith('Hero_Resource_'):
        kind = actor.get_class().get_name()
        resource_counts[kind] = resource_counts.get(kind, 0) + 1
assert resource_counts == {}, resource_counts
assert not any('BuildingSpawn' in a.get_class().get_name() for a in actors)
scenery = [a for a in actors if unreal.Name('SF_StageHeroScenery') in a.tags]
assert len(scenery) > 1000
for a in scenery:
    rot = a.get_actor_rotation()
    assert abs(rot.pitch) < .01 and abs(rot.roll) < .01, a.get_actor_label()
    origin, extent = a.get_actor_bounds(False)
    assert -12 <= origin.z - extent.z <= 1, a.get_actor_label()
assert not any(a.get_class().get_name() in ['BP_Mineral_C','BP_Mineral_S_C','BP_Ether_C','BP_Ether_S_C'] for a in actors)
baseline = json.loads(Path(unreal.Paths.project_saved_dir(), 'hero_terrain_before_dressing.json').read_text())
for label, transform in baseline.items():
    actual = re.sub(r'0x[0-9A-Fa-f]+', 'ADDRESS', str(by_label[label].get_actor_transform()))
    assert actual == re.sub(r'0x[0-9A-Fa-f]+', 'ADDRESS', transform), label

world = unreal.EditorLevelLibrary.get_editor_world()
nav_origin, nav_extent = by_label['Hero_NavMeshBounds'].get_actor_bounds(False)
assert round(nav_extent.x) == 19000 and round(nav_extent.y) == 13500
path_checks = []
for section in SECTIONS:
    points=section[::3]
    if points[-1]!=section[-1]:points.append(section[-1])
    valid=True
    for a,b in zip(points,points[1:]):
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
            world, unreal.Vector(a[0],a[1],100), unreal.Vector(b[0],b[1],100))
        valid=valid and bool(path and path.is_valid() and not path.is_partial())
    path_checks.append(valid)
assert all(path_checks), path_checks
start,end=ROUTE[0],ROUTE[-1]
full_path=unreal.NavigationSystemV1.find_path_to_location_synchronously(world,unreal.Vector(*start,100),unreal.Vector(*end,100))
assert full_path and full_path.is_valid() and not full_path.is_partial()
path_points=full_path.get_editor_property('path_points')
full_distance=sum(math.hypot(b.x-a.x,b.y-a.y) for a,b in zip(path_points,path_points[1:]))
assert full_distance>spline.get_spline_length()*.75, 'Unexpected shortcut through curved dividers'
result = dict(actor_count=len(actors), stage=hero, groups=groups,
              resources=resource_counts,
              game_mode=str(world.get_world_settings().get_editor_property('default_game_mode')),
              navigation=[a.get_actor_label() for a in actors if a.get_class().get_name() == 'NavMeshBoundsVolume'],
              nav_origin=[nav_origin.x,nav_origin.y,nav_origin.z],
              nav_extent=[nav_extent.x,nav_extent.y,nav_extent.z],
              path_checks=path_checks,route_points=len(ROUTE),full_route_distance=full_distance)
Path(unreal.Paths.project_saved_dir(), 'stage_hero_verified.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
