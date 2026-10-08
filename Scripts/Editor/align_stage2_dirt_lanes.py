"""Author Stage 02 lanes along the dirt roads; validate navigation before saving.

Use -ExecutePythonScript with -ApplyDirtLanes to save, otherwise only inspect.
The image-space landmarks refer to the 1600x1400 orthographic Stage 02 capture
(camera 1300,4800,35000, yaw 0, width 26000). Spawners remain unchanged.
"""
import json
import math
import shutil
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved/Stage02Routes/DirtRoads'
OUT.mkdir(parents=True, exist_ok=True)
MAP = '/Game/Game/Stage/PT_Stage_2'

# Long straight road sections share identical points across lanes.
SOUTH = [(1200, 1030), (970, 1030), (810, 1015), (650, 1030), (450, 1040)]
EAST = [(1250, 620), (1250, 965), (1235, 1010)] + SOUTH
CENTER = [(760, 400), (760, 440), (748, 610), (735, 840), (720, 920), (710, 985),
          (650, 1030), (450, 1040)]
ROADS = {
    'Stage02_WestNorth': [(1250, 580)] + EAST,
    'Stage02_CenterNorth': [(1220, 580), (1250, 620)] + EAST[1:],
    'Stage02_WestFlank': [(995, 335), (760, 335)] + CENTER,
    'Stage02_CenterRanged': [(895, 335), (760, 335)] + CENTER,
    'Stage02_EastWarrior': [(1100, 335), (430, 335), (375, 365),
                           (350, 420), (350, 890), (375, 920)],
}

def world_point(pixel):
    u, v = pixel
    return unreal.Vector(1300 + (700-v)*16.25, 4800 + (u-800)*16.25, 0)

def distance(a, b):
    return math.hypot(a.x-b.x, a.y-b.y)

def dense(points):
    result = [points[0]]
    for a, b in zip(points, points[1:]):
        count = max(1, math.ceil(distance(a, b)/350))
        for i in range(1, count+1):
            t = i/count
            result.append(unreal.Vector(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, 0))
    return result

assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started = time.time()

def run():
    actors = sub.get_all_level_actors()
    labels = {a.get_actor_label(): a for a in actors}
    prepared, report, failures = [], {}, []
    for label, pixels in ROADS.items():
        lane = labels[label]
        spline = lane.get_editor_property('spline')
        start = spline.get_location_at_spline_point(0, unreal.SplineCoordinateSpace.WORLD)
        end = spline.get_location_at_spline_point(spline.get_number_of_spline_points()-1,
                                                unreal.SplineCoordinateSpace.WORLD)
        points = dense([start] + [world_point(p) for p in pixels] + [end])
        issues = []
        for index, (a, b) in enumerate(zip(points, points[1:])):
            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, a, b)
            valid = bool(path and path.is_valid() and not path.is_partial())
            route = list(path.path_points) if path else []
            length = sum(distance(p,q) for p,q in zip(route,route[1:]))
            snap = max(distance(a,route[0]),distance(b,route[-1])) if route else 99999
            if not valid or length > distance(a,b)*1.15+20 or snap > 90:
                issues.append(dict(segment=index, a=[a.x,a.y], b=[b.x,b.y],
                                   valid=valid, length=length, direct=distance(a,b), snap=snap))
        report[label] = dict(points=[[p.x,p.y,p.z] for p in points], issues=issues)
        failures.extend(issues)
        prepared.append((lane, points))
    (OUT/'navigation.json').write_text(json.dumps(report,indent=2))
    if failures:
        raise RuntimeError('Dirt route navigation failed: %d segments; see navigation.json' % len(failures))
    if '-ApplyDirtLanes' not in unreal.SystemLibrary.get_command_line():
        unreal.log('STAGE02_DIRT_LANES_PROBE_OK')
        return
    backup = OUT/'PT_Stage_2.before_dirt_lanes.umap'
    if not backup.exists():
        shutil.copy2(ROOT/'Content/Game/Stage/PT_Stage_2.umap', backup)
    for lane, points in prepared:
        spline = lane.get_editor_property('spline')
        spline.clear_spline_points(False)
        for point in points:
            spline.add_spline_point(point, unreal.SplineCoordinateSpace.WORLD, False)
        for index in range(len(points)):
            spline.set_spline_point_type(index, unreal.SplinePointType.LINEAR, False)
        spline.update_spline()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    saved = {a.get_actor_label(): a for a in sub.get_all_level_actors()}
    for label, row in report.items():
        spline = saved[label].get_editor_property('spline')
        assert spline.get_number_of_spline_points() == len(row['points'])
        for i, expected in enumerate(row['points']):
            assert distance(spline.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD),
                            unreal.Vector(*expected)) < 0.1
    unreal.log('STAGE02_DIRT_LANES_SAVED_AND_VERIFIED')
    (OUT/'error.txt').unlink(missing_ok=True)
    return True

def tick(dt):
    if time.time()-started < 15:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        if run():
            capture = ROOT/'Scripts/Editor/capture_stage2_dirt_lanes.py'
            exec(compile(capture.read_text(encoding='utf-8-sig'), str(capture), 'exec'),
                 {'__file__': str(capture)})
            return
    except Exception:
        (OUT/'error.txt').write_text(traceback.format_exc())
        unreal.log_error(traceback.format_exc())
    unreal.SystemLibrary.quit_editor()

handle = unreal.register_slate_post_tick_callback(tick)
