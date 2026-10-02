"""Author the Beast RTS base on Stage 04 without changing the player layout.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
Only actors tagged SF_Stage04_EnemyBase are replaced on a rerun.
"""
import json
import math
import shutil
from pathlib import Path
import unreal

MAP = '/Game/Game/Stage/PT_Stage_4'
TABLE = '/Game/Core/DataTable/DT_SFStage'
TAG = unreal.Name('SF_Stage04_EnemyBase')
ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved/Stage04Enemy'
OUT.mkdir(parents=True, exist_ok=True)
for source, name in (('Content/Game/Stage/PT_Stage_4.umap', 'PT_Stage_4.before_enemy.umap'),
                     ('Content/Core/DataTable/DT_SFStage.uasset', 'DT_SFStage.before_enemy.uasset')):
    backup = OUT / name
    if not backup.exists():
        shutil.copy2(ROOT / source, backup)

assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
original = sub.get_all_level_actors()
by_label = {a.get_actor_label(): a for a in original}

def signature(a):
    p, r, s = a.get_actor_location(), a.get_actor_rotation(), a.get_actor_scale3d()
    value = [a.get_class().get_name(), [p.x,p.y,p.z], [r.pitch,r.yaw,r.roll], [s.x,s.y,s.z], [str(t) for t in a.tags]]
    if isinstance(a, unreal.SFBuildingSpawnAnchor):
        value.extend([a.get_editor_property('team_index'), str(a.get_editor_property('building_type')),
                      a.get_editor_property('initial_tier_index')])
    if isinstance(a, unreal.ResourceBase):
        value.append(a.get_editor_property('resource_deposit'))
    return value

untouched = {a.get_name(): signature(a) for a in original if TAG not in a.tags}
(OUT/'untouched_actors.json').write_text(json.dumps(untouched,ensure_ascii=False,indent=2),encoding='utf-8')
anchor_class = by_label['BP_BuildingSpawn'].get_class()
ether_class = next(a.get_class() for a in original if a.get_class().get_name() == 'BP_Ether_C')
definitions = {key: unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/Animal/DA_Animal'+suffix)
               for key,suffix in [('Barracks','Barrack'),('Academy','Ranged'),('House','House'),
                                  ('Mineral','MineralMine'),('Ether','EtherMine')]}
assert all(definitions.values())
unit_rows = {r['Name']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(
    unreal.load_asset('/Game/Core/DataTable/DT_Units')))}

# One factory per combat role and tier. Frequencies and alive caps both use 5:3:1.
# The 225-second period produces 5 / 3 / 1 units after every tier is unlocked.
PRODUCTION = [
    ('Barracks',0,'Brawler',    -2850, 8850, 60, 45,10,'Center'),
    ('Academy', 0,'Hunter',     -1600, 8850, 75, 45,10,'Center'),
    ('Barracks',1,'Berserker',  -2850, 9850,240, 75, 6,'West'),
    ('Academy', 1,'Stalker',    -1600, 9850,255, 75, 6,'West'),
    ('Barracks',2,'Predator',   -2850,10850,480,225, 2,'East'),
    ('Academy', 2,'ApexHunter', -1600,10850,495,225, 2,'East'),
]
ROUTES = {
    'Center': [(0,7800),(0,4600),(0,0),(0,-4600),(0,-7800),(1900,-9900)],
    'West': [(0,7800),(-4800,3600),(-4800,0),(-4800,-3600),(0,-7800),(1900,-9900)],
    'East': [(0,7800),(3600,6000),(5400,0),(3600,-6000),(0,-7800),(1900,-9900)],
}

def nav_point(x,y):
    p = unreal.NavigationSystemV1.project_point_to_navigation(world, unreal.Vector(x,y,0), None, None,
        query_extent=unreal.Vector(450,450,1500))
    assert p is not None, ('No navigation at', x,y)
    assert math.hypot(p.x-x,p.y-y) < 500, ('Navigation moved too far', x,y,str(p))
    return p

paths = {}
for role,tier,row,x,y,delay,interval,cap,approach in PRODUCTION:
    assert row in unit_rows
    points = [nav_point(x,y-500),nav_point(x,7800)] + [nav_point(*p) for p in ROUTES[approach]]
    # Store the actual navigable polyline so ramps and authored valley heights are respected.
    route = [points[0]]
    for a,b in zip(points,points[1:]):
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world,a,b)
        assert path and path.is_valid() and not path.is_partial(), ('Disconnected route', row,str(a),str(b))
        route.extend(path.get_editor_property('path_points')[1:])
    paths[row] = route

for a in original:
    if TAG in a.tags:
        assert sub.destroy_actor(a)

def place(label,cls,x,y,z=1,yaw=90):
    a = sub.spawn_actor_from_class(cls,unreal.Vector(x,y,z),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
    assert a,label
    a.set_actor_label('Stage04_Enemy_'+label)
    a.set_folder_path('Stage 04/Beast Base')
    a.set_editor_property('tags',[TAG])
    return a

def building(label,role,tier,x,y):
    a = place(label,anchor_class,x,y)
    a.set_actor_rotation(unreal.SFBuildingSpawnLibrary.get_construction_rotation(),False)
    a.set_editor_property('team_index',1)
    a.set_editor_property('building_type', {'Barracks':unreal.SFBuildingType.BARRACKS,
        'Academy':unreal.SFBuildingType.RANGED_PRODUCTION,'House':unreal.SFBuildingType.HOUSE,
        'Mineral':unreal.SFBuildingType.MINERAL_MINE,'Ether':unreal.SFBuildingType.ETHER_MINE}[role])
    a.set_editor_property('initial_tier_index',tier)
    a.set_editor_property('preview_definition',definitions[role])
    # Trigger the native construction preview after all properties have been assigned.
    a.set_actor_location(unreal.Vector(x,y,1),False,False)
    return a

for role,tier,row,x,y,delay,interval,cap,approach in PRODUCTION:
    building(role+'_T'+str(tier+1),role,tier,x,y)
    lane = place('Lane_'+row,unreal.LanePath,x,y-500,yaw=0)
    lane.set_editor_property('team_index',1)
    lane.set_editor_property('lane_half_width',550.0)
    spline = lane.get_editor_property('spline')
    spline.clear_spline_points(False)
    for p in paths[row]:
        spline.add_spline_point(p,unreal.SplineCoordinateSpace.WORLD,False)
    for i in range(spline.get_number_of_spline_points()):
        spline.set_spline_point_type(i,unreal.SplinePointType.LINEAR,False)
    spline.update_spline()
    spawner = place('Spawner_'+row,unreal.UnitSpawner,x,y)
    spawner.set_editor_property('unit_class',unreal.EditorAssetLibrary.load_blueprint_class(
        '/Game/Core/Units/Beast_Fox/BP_Fox_'+row))
    assert spawner.get_editor_property('unit_class')
    for key,value in dict(unit_row_name=unreal.Name(row),team_index=1,initial_delay=float(delay),
            spawn_interval=float(interval),spawn_count_per_cycle=1,max_alive=cap,max_total=0,
            spawn_scatter_radius=130.0,spawn_offset=unreal.Vector(-500,0,0),lane=lane,
            start_active=True,wave_controlled=False,spawning_enabled=True,
            stop_when_owner_building_destroyed=True,require_owner_building=True,
            owner_building_search_radius=200.0,owner_building_resolve_timeout=60.0).items():
        spawner.set_editor_property(key,value)

for i,(x,y) in enumerate([(1950,10800),(1450,10800),(950,10800),(950,11300)],1):
    building('House_'+str(i),'House',0,x,y)
# Existing north-side mineral fields are used; only the missing ether deposits are added.
for i,(x,y) in enumerate([(1600,11600),(2100,11700),(2600,11600)],1):
    building('MineralMine_'+str(i),'Mineral',0,x,y)
for i,(x,y) in enumerate([(1800,9000),(2500,9000)],1):
    node = place('EtherDeposit_'+str(i),ether_class,x,y)
    node.set_editor_property('resource_deposit',300)
    building('EtherMine_'+str(i),'Ether',0,x,y)

table = unreal.load_asset(TABLE)
before = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
(OUT/'stage_table_before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
rows = json.loads(json.dumps(before))
stage = next(r for r in rows if r['Name']=='Stage04')
stage['TeamSetups'] = ['(TeamIndex=1,Race=Beast)']
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(table,False)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()

# Verify the serialized level, not just the objects we edited in memory.
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
saved = sub.get_all_level_actors()
actual = {a.get_name():signature(a) for a in saved if TAG not in a.tags}
assert actual == untouched, 'Unrelated actor changed'
labels = {a.get_actor_label():a for a in saved}
for role,tier,row,x,y,delay,interval,cap,approach in PRODUCTION:
    a = labels['Stage04_Enemy_Spawner_'+row]
    anchor = labels['Stage04_Enemy_'+role+'_T'+str(tier+1)]
    p,q = a.get_actor_location(),anchor.get_actor_location()
    assert math.dist([p.x,p.y,p.z],[q.x,q.y,q.z]) < 1
    assert a.get_editor_property('initial_delay') == delay
    assert a.get_editor_property('spawn_interval') == interval
    assert a.get_editor_property('max_alive') == cap
    assert str(a.get_editor_property('unit_row_name')) == row
    assert a.get_editor_property('require_owner_building')
    assert a.get_editor_property('stop_when_owner_building_destroyed')
    assert not a.get_editor_property('wave_controlled')
    assert a.get_editor_property('lane')
    assert anchor.get_editor_property('initial_tier_index') == tier
    assert anchor.get_editor_property('preview_mesh_component').get_editor_property('static_mesh')
    color = anchor.get_editor_property('grid_preview_component').get_editor_property('shape_color')
    assert color.g > color.r, ('Invalid factory placement',row,str(color))
for a in saved:
    if TAG in a.tags and isinstance(a,unreal.SFBuildingSpawnAnchor):
        r = a.get_actor_rotation()
        assert abs(r.pitch)<.001 and abs(r.roll)<.001 and abs(r.yaw-90)<.001, a.get_actor_label()
        color = a.get_editor_property('grid_preview_component').get_editor_property('shape_color')
        assert color.g > color.r, ('Invalid base placement',a.get_actor_label(),str(color))
after = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
assert [r for r in before if r['Name']!='Stage04'] == [r for r in after if r['Name']!='Stage04']
expected_stage = next(r for r in before if r['Name']=='Stage04').copy()
expected_stage['TeamSetups'] = ['(TeamIndex=1,Race=Beast)']
assert next(r for r in after if r['Name']=='Stage04') == expected_stage
report = dict(passed=True,production=[dict(role=role,tier=tier+1,row=row,xy=[x,y],delay=delay,
    interval=interval,alive_cap=cap,approach=approach) for role,tier,row,x,y,delay,interval,cap,approach in PRODUCTION],
    houses=4,mineral_mines=3,ether_mines=2,new_ether_deposits=2,untouched_actors=len(untouched),
    route_points={row:[[p.x,p.y,p.z] for p in points] for row,points in paths.items()})
(OUT/'configuration_verified.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('STAGE04_ENEMY_CONFIG_VERIFIED: 6 owner-gated factories, tier rates 5:3:1, Beast RTS base')
