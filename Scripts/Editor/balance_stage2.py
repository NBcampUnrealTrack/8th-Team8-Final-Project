"""Balance Stage 02 enemy barracks and fill the western enemy flank.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
New actors carry SF_Stage02WestEconomy so reruns replace only this set.
"""
import shutil
from pathlib import Path
import unreal

MAP = '/Game/Game/Stage/PT_Stage_2'
TAG = unreal.Name('SF_Stage02WestEconomy')
root = Path(unreal.Paths.project_dir())
backup = root / 'Saved/Stage02Balance/PT_Stage_2.before_balance.umap'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(root / 'Content/Game/Stage/PT_Stage_2.umap', backup)

if not unreal.EditorLoadingAndSavingUtils.load_map(MAP):
    raise RuntimeError('Could not load ' + MAP)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}

# The five barracks are already objective gated. Keep their unit, team, gate,
# lane, and owner-building settings; spread the waves out after that gate opens.
timing = {
    'BP_UnitSpawner':  (35.0, 90.0),
    'BP_UnitSpawner2': (55.0, 105.0),
    'BP_UnitSpawner3': (75.0, 120.0),
    'BP_UnitSpawner4': (95.0, 135.0),
    'BP_UnitSpawner5': (115.0, 150.0),
}
for label, (delay, interval) in timing.items():
    spawner = by_label.get(label)
    if not spawner or 'UnitSpawner' not in spawner.get_class().get_name():
        raise RuntimeError('Expected Stage 02 spawner: ' + label)
    if spawner.get_editor_property('team_index') != 1 or spawner.get_editor_property('start_active'):
        raise RuntimeError('Unexpected team or activation gate on ' + label)
    spawner.set_editor_property('initial_delay', delay)
    spawner.set_editor_property('spawn_interval', interval)
    spawner.set_editor_property('spawn_count_per_cycle', 1)
    spawner.set_editor_property('max_alive', 2)
    spawner.set_editor_property('max_total', 5)

# Anchored buildings inherit their actor yaw at runtime. Player-built
# buildings use 90 degrees in SFInGamePlayerController. The enemy Nexus
# keeps its authored facing.
for actor in actors:
    if (actor.get_class().get_name() == 'BP_BuildingSpawn_C'
            and actor.get_editor_property('team_index') == 1
            and actor.get_editor_property('building_type') != unreal.SFBuildingType.NEXUS):
        actor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=90, roll=0), False)

for actor in actors:
    if TAG in actor.tags:
        sub.destroy_actor(actor)

anchor_class = by_label['Stage02_Start_House'].get_class()
mineral_class = by_label['BP_Mineral2'].get_class()
ether_class = by_label['BP_Ether2'].get_class()

def place(label, cls, x, y, deposit=None):
    actor = sub.spawn_actor_from_class(cls, unreal.Vector(x, y, 0), unreal.Rotator(0, 0, 0))
    if not actor:
        raise RuntimeError('Could not spawn ' + label)
    actor.set_actor_label(label)
    actor.set_editor_property('tags', [TAG])
    actor.set_folder_path('Stage 02/Enemy West Economy')
    if deposit is not None:
        actor.set_editor_property('resource_deposit', deposit)
    return actor

# House anchors use the same race-aware building bootstrap as existing barracks.
for label, x, y in (
    ('Stage02_EnemyWest_House_A', -3500, 11500),
    ('Stage02_EnemyWest_House_B', -1300, 11700),
):
    house = place(label, anchor_class, x, y)
    house.set_editor_property('team_index', 1)
    house.set_editor_property('building_type', unreal.SFBuildingType.HOUSE)
    house.set_editor_property('initial_tier_index', 0)
    house.set_actor_rotation(unreal.Rotator(pitch=0, yaw=90, roll=0), False)

# Resource nodes match the existing Stage 02 deposits and stay accessible to
# either team if the player eventually reaches this side of the map.
for label, cls, x, y, amount in (
    ('Stage02_EnemyWest_Mineral_A', mineral_class, -2800, 8200, 999),
    ('Stage02_EnemyWest_Mineral_B', mineral_class, -2500, 9100, 999),
    ('Stage02_EnemyWest_Mineral_C', mineral_class, -3600, 9700, 999),
    ('Stage02_EnemyWest_Ether_A', ether_class, -3500, 10600, 300),
    ('Stage02_EnemyWest_Ether_B', ether_class, -1900, 10300, 300),
):
    place(label, cls, x, y, amount)

if not unreal.EditorLoadingAndSavingUtils.save_current_level():
    raise RuntimeError('Failed to save ' + MAP)
unreal.log('STAGE02_BALANCE_SAVED 5 spawners, 2 houses, 3 minerals, 2 ether')
