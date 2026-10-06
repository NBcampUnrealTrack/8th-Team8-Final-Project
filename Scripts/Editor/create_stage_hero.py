"""Author Stage_Hero and its stage-table row in Unreal Editor Python.

Run with UnrealEditor-Cmd -run=pythonscript -script=<absolute path>.
The level follows the supplied sketch from the upper-left Human base to the
lower-left Beast core. Existing terrain is protected; use dress_stage_hero.py
to update scenery in a level that has already been authored.
After creation, run dress_stage_hero.py, then build_stage_hero_nav.py in the full Editor.
"""
import json
import math
import unreal

MAP = '/Game/Game/Stage/PT_Stage_Hero'
TAG = unreal.Name('SF_StageHero')
ROOT = '/Game/Core'

route = [
    (-13500, 8500), (13500, 8500), (13500, 3500),
    (-13500, 3500), (-13500, -2000), (13500, -2000),
    (13500, -7500), (-13500, -7500),
]

def blueprint(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not cls:
        raise RuntimeError('Missing Blueprint: ' + path)
    return cls


units = {
    'Brawler': blueprint(ROOT + '/Units/Beast_Fox/BP_Fox_Brawler'),
    'Hunter': blueprint(ROOT + '/Units/Beast_Fox/BP_Fox_Hunter'),
    'Berserker': blueprint(ROOT + '/Units/Beast_Fox/BP_Fox_Berserker'),
}
cube = unreal.load_asset('/Engine/BasicShapes/Cube')
if not cube:
    raise RuntimeError('Engine cube mesh unavailable')

def material(path):
    result = unreal.load_asset(path)
    if not result or not isinstance(result, unreal.MaterialInterface):
        raise RuntimeError('Missing material: ' + path)
    return result

grass = material('/Game/Game/Stage/Props/texture/greengrass/Grass005_1K-PNG_Color_Mat')
road = material('/Game/Game/Stage/Props/texture/brownmud/brown_mud_leaves_01_diff_1k_Mat')
stone = material('/Game/Game/Stage/Props/asset/texture_material/Modular_Map_M')
cliff = unreal.load_asset('/Game/Game/Stage/Props/asset/mountain/SM_Cliffs_15')
if not cliff:
    raise RuntimeError('Stage cliff mesh is missing')

if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError('Stage_Hero already exists. Run dress_stage_hero.py to preserve its terrain.')
else:
    if not unreal.EditorLevelLibrary.new_level(MAP):
        raise RuntimeError('Could not create Stage_Hero')

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in sub.get_all_level_actors():
    if TAG in actor.tags:
        sub.destroy_actor(actor)

world = unreal.EditorLevelLibrary.get_editor_world()
game_mode = blueprint(ROOT + '/Blueprints/GameMode/BP_SFGameMode')
world.get_world_settings().set_editor_property('default_game_mode', game_mode)

def spawn(cls, label, xyz, folder, yaw=0):
    actor = sub.spawn_actor_from_class(
        cls, unreal.Vector(*xyz), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    if not actor:
        raise RuntimeError('Could not spawn ' + label)
    actor.set_actor_label(label)
    actor.set_folder_path('Stage Hero/' + folder)
    actor.set_editor_property('tags', [TAG])
    return actor

def box(label, xyz, size, mat, folder):
    actor = spawn(unreal.StaticMeshActor, label, xyz, folder)
    mesh = actor.get_editor_property('static_mesh_component')
    mesh.set_editor_property('static_mesh', cube)
    mesh.set_material(0, mat)
    actor.set_actor_scale3d(unreal.Vector(size[0]/100, size[1]/100, size[2]/100))
    return actor

# Use repeated surface tiles so the existing stage textures stay legible over
# this much larger arena instead of stretching one UV across the full map.
for gx in range(10):
    for gy in range(9):
        box('Hero_Ground_%02d_%02d' % (gx,gy),
            (-17100 + gx*3800, -12000 + gy*3000, -55),
            (3800, 3000, 100), grass, 'Terrain')
for i, ((x0,y0),(x1,y1)) in enumerate(zip(route, route[1:]), 1):
    segment_length = math.hypot(x1-x0, y1-y0)
    count = math.ceil(segment_length / 2800)
    yaw = math.degrees(math.atan2(y1-y0,x1-x0))
    for j in range(count):
        alpha = (j+0.5)/count
        actor = box('Hero_Road_%02d_%02d' % (i,j+1),
                    (x0+(x1-x0)*alpha, y0+(y1-y0)*alpha, 1),
                    (segment_length/count+20, 1800, 12), road, 'Route')
        actor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=yaw, roll=0), False)

# Each divider closes the middle of the field, leaving an alternating turn.
for label, xmin, xmax, y in (
    ('North', -18600, 11000, 6000),
    ('Middle', -11000, 18600, 750),
    ('South', -18600, 11000, -4750),
):
    box('Hero_Divider_' + label, ((xmin+xmax)/2, y, 400),
        (xmax-xmin, 650, 800), stone, 'Terrain')
    x = xmin + 1800
    index = 0
    while x <= xmax - 1800:
        rock = spawn(unreal.StaticMeshActor, 'Hero_Cliff_%s_%02d' % (label,index),
                     (x,y,300), 'Terrain')
        comp = rock.get_editor_property('static_mesh_component')
        comp.set_editor_property('static_mesh', cliff)
        comp.set_material(0, stone)
        rock.set_actor_scale3d(unreal.Vector(1.1,0.5,1.0))
        x += 3200
        index += 1
for label, x, y, sx, sy in (
    ('West',-18600,0,400,26000),('East',18600,0,400,26000),
    ('North',0,13100,37200,400),('South',0,-13100,37200,400),
):
    box('Hero_Border_' + label, (x,y,450), (sx,sy,900), stone, 'Terrain')

sun = spawn(unreal.DirectionalLight, 'Hero_Sun', (0,0,500), 'Lighting')
sun.set_actor_rotation(unreal.Rotator(pitch=-55, yaw=-35, roll=0), False)
sun.get_editor_property('light_component').set_editor_property('intensity', 5.0)
sky = spawn(unreal.SkyLight, 'Hero_SkyLight', (0,0,800), 'Lighting')
sky.get_editor_property('light_component').set_editor_property('intensity', 1.0)
spawn(unreal.SkyAtmosphere, 'Hero_Atmosphere', (0,0,0), 'Lighting')
spawn(unreal.PlayerStart, 'Hero_PlayerStart', (-15100, 10300, 92), 'Player')

nav = spawn(unreal.NavMeshBoundsVolume, 'Hero_NavMeshBounds', (0,0,0), 'Navigation')
nav.set_actor_scale3d(unreal.Vector(190,135,10))

# Buildings and resource nodes are intentionally omitted during scenery review.

def lane(label, team, points):
    a = spawn(unreal.LanePath, label, (points[0][0],points[0][1],0), 'Routes')
    a.set_editor_property('team_index', team)
    a.set_editor_property('lane_half_width', 900.0)
    spline = a.get_editor_property('spline')
    spline.clear_spline_points(False)
    for x,y in points:
        spline.add_spline_point(unreal.Vector(x,y,0), unreal.SplineCoordinateSpace.WORLD, False)
    for i in range(len(points)):
        spline.set_spline_point_type(i, unreal.SplinePointType.LINEAR, False)
    spline.update_spline()
    return a

lane('Hero_PlayerRoute', 0, route)

# Beast defenders hold positions along successive route checkpoints.
checkpoints = [
    ('01_EastApproach', 'Brawler', (5000,8500), 2),
    ('02_FirstTurn', 'Hunter', (13500,3500), 2),
    ('03_WestReturn', 'Brawler', (-5000,3500), 3),
    ('04_MiddleTurn', 'Berserker', (-13500,-2000), 2),
    ('05_EastPush', 'Hunter', (5000,-2000), 3),
    ('06_LastTurn', 'Berserker', (13500,-7500), 2),
    ('07_CoreGuard', 'Brawler', (-8500,-7500), 3),
]
offsets = [(-180,-150),(180,150),(0,0)]
for name, unit, (x,y), count in checkpoints:
    for i in range(count):
        dx,dy = offsets[i]
        defender = spawn(units[unit], 'Hero_Enemy_' + name + '_%02d' % (i+1),
                         (x+dx,y+dy,92), 'Enemy Units')
        defender.set_editor_property('unit_row_name', unreal.Name(unit))
        defender.set_editor_property('auto_advance_teams', set())
        target = defender.get_component_by_class(unreal.CombatTargetComponent)
        if not target:
            raise RuntimeError('Defender has no CombatTargetComponent: ' + name)
        target.set_editor_property('team_index', 1)

if not unreal.EditorLoadingAndSavingUtils.save_current_level():
    raise RuntimeError('Could not save Stage_Hero')

table = unreal.load_asset('/Game/Core/DataTable/DT_SFStage')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
hero = next((row.copy() for row in rows if row['Name'] in ('StageHero', 'Stage05')), None)
if hero is None:
    hero = next(row.copy() for row in rows if row['Name'] == 'Stage02')
rows = [row for row in rows if row['Name'] not in ('StageHero', 'Stage05', 'Stage_Hero')]
hero.update(Name='StageHero', StageOrder=5, DisplayName='StageHero',
            StageLevel=MAP + '.PT_Stage_Hero', PlayerRace='Human',
            TeamSetups=['(TeamIndex=1,Race=Beast)'],
            Minimap='(bAutoDetectBounds=False,WorldMin=(X=-19000,Y=-13500),WorldMax=(X=19000,Y=13500),CaptureHeight=18000,BoundsPadding=200)')
rows.append(hero)
if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False)):
    raise RuntimeError('Could not update DT_SFStage')
if not unreal.EditorAssetLibrary.save_loaded_asset(table):
    raise RuntimeError('Could not save DT_SFStage')

unreal.log('STAGE_HERO_CREATED: 7 road segments, 17 Beast defenders, no buildings/resources, Human team 0, Beast team 1')
