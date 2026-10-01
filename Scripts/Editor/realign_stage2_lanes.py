"""Realign Stage 02 spawners to production anchors and rebuild navigable lanes.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
This edits only the existing five spawners and their five LanePath actors.
"""
import math
import unreal


MAP = '/Game/Game/Stage/PT_Stage_2'
RALLY_DISTANCE = 650.0  # Start outside the production building's footprint.
POINT_SPACING = 450.0
LANE_HALF_WIDTH = 240.0

# Keep the existing mix of western, central, and eastern approaches.
ROUTES = (
    ('BP_UnitSpawner', 'BP_BuildingSpawn5', 'Stage02_WestNorth', (-4700, -1600)),
    ('BP_UnitSpawner2', 'BP_BuildingSpawn6', 'Stage02_CenterNorth', (-3500, -1700)),
    ('BP_UnitSpawner3', 'BP_BuildingSpawn7', 'Stage02_WestFlank', (-4700, -1600)),
    ('BP_UnitSpawner4', 'BP_BuildingSpawn8', 'Stage02_CenterRanged', (-3500, -1700)),
    ('BP_UnitSpawner5', 'BP_BuildingSpawn9', 'Stage02_EastWarrior', (-2300, -1700)),
)


def xy_distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def point_at(polyline, distance):
    remaining = distance
    for a, b in zip(polyline, polyline[1:]):
        length = xy_distance(a, b)
        if remaining <= length:
            fraction = remaining / length if length else 0.0
            return (a[0] + (b[0] - a[0]) * fraction,
                    a[1] + (b[1] - a[1]) * fraction)
        remaining -= length
    return polyline[-1]


def lane_points(path_points):
    raw = [(p.x, p.y) for p in path_points]
    distances = [0.0]
    for a, b in zip(raw, raw[1:]):
        distances.append(distances[-1] + xy_distance(a, b))
    if distances[-1] <= RALLY_DISTANCE + POINT_SPACING:
        raise RuntimeError('Navigation route is too short for a rally point')

    # Retain every NavMesh corner after the rally point. Interpolate within
    # each straight segment so the runtime unit follows turns closely.
    trimmed = [point_at(raw, RALLY_DISTANCE)]
    trimmed.extend(raw[i] for i in range(1, len(raw))
                   if distances[i] > RALLY_DISTANCE + 1.0)
    result = [trimmed[0]]
    for a, b in zip(trimmed, trimmed[1:]):
        length = xy_distance(a, b)
        segments = max(1, math.ceil(length / POINT_SPACING))
        for index in range(1, segments + 1):
            fraction = index / segments
            result.append((a[0] + (b[0] - a[0]) * fraction,
                           a[1] + (b[1] - a[1]) * fraction))
    return result


def get_path(world, start, end, label):
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(
        world, unreal.Vector(start[0], start[1], 0),
        unreal.Vector(end[0], end[1], 0))
    if not path or not path.is_valid() or path.is_partial() or len(path.path_points) < 2:
        raise RuntimeError('Unreachable Stage 02 route: ' + label)
    return path


if not unreal.EditorLoadingAndSavingUtils.load_map(MAP):
    raise RuntimeError('Could not load Stage 02')
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
world = unreal.EditorLevelLibrary.get_editor_world()
prepared = []

# Validate the entire change before moving a single actor.
for spawner_label, anchor_label, lane_label, destination in ROUTES:
    spawner = by_label[spawner_label]
    anchor = by_label[anchor_label]
    lane = by_label[lane_label]
    if spawner.get_class().get_name() != 'BP_UnitSpawner_C':
        raise RuntimeError('Unexpected spawner ' + spawner_label)
    if anchor.get_class().get_name() != 'BP_BuildingSpawn_C':
        raise RuntimeError('Unexpected building anchor ' + anchor_label)
    if lane.get_class().get_name() != 'LanePath' or spawner.get_editor_property('lane') != lane:
        raise RuntimeError('Spawner lane reference changed: ' + spawner_label)
    if spawner.get_editor_property('team_index') != anchor.get_editor_property('team_index'):
        raise RuntimeError('Spawner/building team mismatch: ' + spawner_label)

    origin = anchor.get_actor_location()
    new_spawner = (origin.x - 24.0, origin.y + 12.0)
    nav_path = get_path(world, new_spawner, destination, spawner_label)
    points = lane_points(nav_path.path_points)
    get_path(world, new_spawner, points[0], spawner_label + ' rally')
    for a, b in zip(points, points[1:]):
        segment = get_path(world, a, b, spawner_label + ' segment')
        nav_length = sum(xy_distance((p.x, p.y), (q.x, q.y))
                         for p, q in zip(segment.path_points, segment.path_points[1:]))
        if nav_length > xy_distance(a, b) * 1.15 + 20.0:
            raise RuntimeError('Lane cuts a navigation obstacle: ' + spawner_label)
    prepared.append((spawner, anchor, lane, new_spawner, points))

for spawner, anchor, lane, new_spawner, points in prepared:
    spawner.set_actor_location(unreal.Vector(new_spawner[0], new_spawner[1],
                                                 anchor.get_actor_location().z), False, False)
    lane.set_actor_location(unreal.Vector(points[0][0], points[0][1], 0), False, False)
    lane.set_editor_property('team_index', 1)
    lane.set_editor_property('lane_half_width', LANE_HALF_WIDTH)
    spline = lane.get_editor_property('spline')
    spline.clear_spline_points(False)
    for x, y in points:
        spline.add_spline_point(unreal.Vector(x, y, 0),
                                unreal.SplineCoordinateSpace.WORLD, False)
    for index in range(len(points)):
        spline.set_spline_point_type(index, unreal.SplinePointType.LINEAR, False)
    spline.update_spline()

if not unreal.EditorLoadingAndSavingUtils.save_current_level():
    raise RuntimeError('Could not save Stage 02')

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
saved = {a.get_actor_label(): a for a in sub.get_all_level_actors()}
for spawner_label, anchor_label, lane_label, destination in ROUTES:
    spawner = saved[spawner_label]
    anchor = saved[anchor_label]
    lane = saved[lane_label]
    assert spawner.get_editor_property('lane') == lane
    assert xy_distance((spawner.get_actor_location().x, spawner.get_actor_location().y),
                       (anchor.get_actor_location().x, anchor.get_actor_location().y)) < 150.0
    assert lane.get_editor_property('spline').get_number_of_spline_points() >= 10
    assert spawner.get_editor_property('require_owner_building')
    assert spawner.get_editor_property('stop_when_owner_building_destroyed')
unreal.log('STAGE02_LANES_REALIGNED 5 spawners, 5 nav-guided attack routes')
