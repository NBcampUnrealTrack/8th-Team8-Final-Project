"""Configure existing campaign maps; preserve scenery and back up edited packages.
Run after building StrikeForgeEditor with -run=pythonscript -script=<this file>.
"""
import json, shutil, sys
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved/Stage67'
OUT.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(Path(__file__).parent))
import ambush_layout as ambush

for name in ['Content/Core/DataTable/DT_SFStage.uasset', 'Content/Core/UI/WBP_Lobby.uasset',
             'Content/Game/Stage/PT_SFStageAmbush.umap', 'Content/Game/Stage/PT_SFStageFinal.umap']:
    dest = OUT / ('before_' + Path(name).name)
    if not dest.exists(): shutil.copy2(ROOT / name, dest)

TAG = unreal.Name('SF_Stage67_Setup')
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
anchor_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Structure/Buildings/BuildingSpawner/BP_BuildingSpawn')
roles = dict(Nexus=('NEXUS', 'Nexus'), House=('HOUSE', 'House'),
             Barracks=('BARRACKS', 'Barrack'), Ranged=('RANGED_PRODUCTION', 'Ranged'),
             Mineral=('MINERAL_MINE', 'MineralMine'), Ether=('ETHER_MINE', 'EtherMine'))

def place(cls, label, p):
    a = sub.spawn_actor_from_class(cls, p)
    assert a, label
    a.set_actor_label(label)
    a.set_folder_path('Gameplay/Stage Setup')
    a.set_editor_property('tags', [TAG])
    return a

def building(label, team, race, role, p, tier=0):
    a = place(anchor_class, label, p)
    definition = unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/' + race + '/DA_' + race + roles[role][1])
    assert definition
    for key, val in dict(team_index=team, building_type=getattr(unreal.SFBuildingType, roles[role][0]),
                         initial_tier_index=tier, preview_definition=definition).items(): a.set_editor_property(key, val)
    a.set_actor_rotation(unreal.Rotator(yaw=90), False)
    a.set_actor_location(p, False, False)
    return a

def reset_generated():
    for a in sub.get_all_level_actors():
        if TAG in a.tags: assert sub.destroy_actor(a)

# Stage 06: reuse the three authored forest approaches, replace their autonomous
# pairs with exactly one wave-controlled spawner per direction.
MAP6 = '/Game/Game/Stage/PT_SFStageAmbush'
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP6)
reset_generated()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = sub.get_all_level_actors()
labels = {a.get_actor_label(): a for a in actors}
lanes = [labels['Ambush_Lane_' + str(i)] for i in range(3)]
for a in actors:
    if isinstance(a, unreal.UnitSpawner) and a.get_actor_label().startswith('Ambush_Spawner_'): assert sub.destroy_actor(a)
def ground6(x, y): return unreal.Vector(x, y, ambush.height(*ambush.world_uv(x, y)) + 1)

# Keep the existing Nexus actor, moving it onto the buildable defensive plateau.
nexus = next(a for a in actors if isinstance(a, unreal.SFBuildingSpawnAnchor) and a.get_editor_property('building_type') == unreal.SFBuildingType.NEXUS)
nexus.set_actor_location(ground6(-3000, 0), False, False)
nexus.set_editor_property('initial_tier_index', 1)
building('Stage06_Barracks', 0, 'Human', 'Barracks', ground6(-1000, -1500), 1)
building('Stage06_Ranged', 0, 'Human', 'Ranged', ground6(-1000, 1500), 1)
for i, y in enumerate([-1000, 1000]): building('Stage06_House_' + str(i), 0, 'Human', 'House', ground6(-3000, y))
for role, label in [('Mineral', 'Ambush_Mineral_0_0'), ('Ether', 'Ambush_Ether_0_0')]:
    building('Stage06_' + role + 'Mine', 0, 'Human', role, labels[label].get_actor_location())
for a in actors:
    if isinstance(a, unreal.ResourceBase):
        # Existing eight deposits become useful reserves rather than 22/50-unit props.
        a.set_editor_property('resource_deposit', 3000 if 'Mineral' in a.get_actor_label() else 1800)

spawners = []
for i, lane in enumerate(lanes):
    spline = lane.get_editor_property('spline')
    points = [spline.get_location_at_spline_point(j, unreal.SplineCoordinateSpace.WORLD) for j in range(spline.get_number_of_spline_points())]
    # The map's hills were lowered after the original lane was authored.
    points = [ground6(p.x, p.y) + unreal.Vector(0, 0, 20) for p in points]
    endpoint = ground6(-2100, 0) + unreal.Vector(0, 0, 20)
    if (points[-1] - endpoint).length() > 10: points.append(endpoint)
    spline.clear_spline_points(False)
    for p in points: spline.add_spline_point(p, unreal.SplineCoordinateSpace.WORLD, False)
    for j in range(len(points)): spline.set_spline_point_type(j, unreal.SplinePointType.LINEAR, False)
    spline.update_spline()
    s = place(unreal.UnitSpawner, 'Stage06_WaveSpawner_' + str(i), points[0])
    for key, value in dict(team_index=1, wave_controlled=True, start_active=False, spawning_enabled=False,
            stop_when_owner_building_destroyed=False, require_owner_building=False,
            max_alive=0, max_total=0, spawn_scatter_radius=120.0, lane=lane).items(): s.set_editor_property(key, value)
    spawners.append(s)

pool = []
for row, weight in [('Brawler',5), ('Berserker',3), ('Predator',1), ('Hunter',5), ('Stalker',3), ('ApexHunter',1)]:
    e = unreal.SFDefenceUnitEntry()
    e.set_editor_property('row_name', row)
    e.set_editor_property('unit_class', unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Units/Beast_Fox/BP_Fox_' + row))
    e.set_editor_property('spawn_weight', weight)
    pool.append(e)
c = place(unreal.SFDefenceWaveController, 'Stage06_WaveController', unreal.Vector())
for key, value in dict(spawners=spawners, unit_pool=pool, total_waves=5,
        initial_preparation_duration=60., preparation_duration=60., wave_duration=300., require_full_wave_duration=True, end_wave_on_enemy_clear=True,
        spawn_interval=4., first_wave_copies_per_type=6, additional_copies_per_wave=3, spawn_count_multiplier=0.4/3.).items(): c.set_editor_property(key,value)
for a in actors:
    if isinstance(a, unreal.SFAmbushDefenseDirector): a.set_editor_property('spawners', [])
assert unreal.EditorLoadingAndSavingUtils.save_current_level()

# Stage 07: fixed faction bases. The game mode maps the chosen base to team 0.
MAP7 = '/Game/Game/Stage/PT_SFStageFinal'
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP7)
reset_generated()
labels = {a.get_actor_label():a for a in sub.get_all_level_actors()}
for a in list(labels.values()):
    if isinstance(a, unreal.UnitSpawner) and a.get_actor_label().startswith('Final_BeastWave_'): assert sub.destroy_actor(a)

for team, race, origin, factories, houses, resources in [
    (0, 'Human', 'Final_EtherNexus', [(-5300,8300),(-7000,6500)], [(-6200,9400),(-6800,9400)], ['Final_Mineral_0_0','Final_Ether_0_0']),
    (1, 'Animal', 'Final_BeastNexus', [(7400,-6600),(5300,-8700)], [(6800,-9500),(6200,-9500)], ['Final_Mineral_2_0','Final_Ether_1_0'])]:
    labels[origin].set_editor_property('initial_tier_index', 1)
    for i, (x,y) in enumerate(houses): building('Stage07_%d_House%d'%(team,i), team, race, 'House', unreal.Vector(x,y,34))
    for role, label in zip(['Mineral','Ether'], resources): building('Stage07_%d_%sMine'%(team,role), team, race, role, labels[label].get_actor_location())
    for i, ((x,y), role) in enumerate(zip(factories, ['Barracks','Ranged'])):
        b = building('Stage07_%d_%s'%(team,role), team, race, role, unreal.Vector(x,y,34), 1)
        lane = labels['Final_Lane_%d_Team%d'%(1 if i==0 else 3,team)]
        entry = lane.get_editor_property('spline').get_location_at_spline_point(0,unreal.SplineCoordinateSpace.WORLD)
        # Native spawners have zero rotation, so the offset is in world XY.
        s = place(unreal.UnitSpawner, 'Stage07_%d_AI_%s'%(team,role), b.get_actor_location())
        row = (['Warrior','Sharpshooter'] if team==0 else ['Berserker','Stalker'])[i]
        unit = '/Game/Core/Units/' + ('Etherial_Human/BP_Human_' if team==0 else 'Beast_Fox/BP_Fox_') + row
        for key,value in dict(team_index=team,unit_row_name=row,unit_class=unreal.EditorAssetLibrary.load_blueprint_class(unit),
                initial_delay=60.0+20*i,spawn_interval=30.0+10*i,spawn_count_per_cycle=1,max_alive=6,max_total=0,
                spawn_offset=entry-b.get_actor_location(),spawn_scatter_radius=100.,lane=lane,
                start_active=True,spawning_enabled=True,stop_when_owner_building_destroyed=True,
                require_owner_building=True,owner_building_search_radius=200.,owner_building_resolve_timeout=120.).items(): s.set_editor_property(key,value)
for a in sub.get_all_level_actors():
    if isinstance(a, unreal.ResourceBase): a.set_editor_property('resource_deposit', 3000 if 'Mineral' in a.get_actor_label() else 1800)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()

dt = unreal.load_asset('/Game/Core/DataTable/DT_SFStage')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt))
for r in rows:
    if r['Name']=='Stage06':
        r.update(StartingMineral=2000, StartingEther=800, PlayerRace='Human', TeamSetups=['(TeamIndex=1,Race=Beast)'])
        r['BuildingRules']=[v for v in r['BuildingRules'] if 'BuildingId=""' not in v]
    if r['Name']=='Stage07': r.update(StartingMineral=1500, StartingEther=600, PlayerRace='None', bUseFactionBases=True)
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt,json.dumps(rows,ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(dt,False)
bp = unreal.load_asset('/Game/Core/UI/WBP_Lobby')
cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/UI/WBP_Lobby'))
cdo.set_editor_property('stage_row_names',['Stage%02d'%i for i in range(1,8)])
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
(OUT/'configured.json').write_text(json.dumps(dict(stage6=dict(waves=5,preparation=60,defence=300,
    totals=[14,22,29,36,43],starting_mineral=2000,starting_ether=800),stage7=dict(faction_choice=True,
    buildings_per_faction=7,starting_mineral=1500,starting_ether=600)),indent=2),encoding='utf-8')
unreal.log('STAGE67_CONFIGURED')
