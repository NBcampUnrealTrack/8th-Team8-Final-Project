"""Update only the existing Stage03 wave controller and verify the saved settings."""
import json
import shutil
from pathlib import Path
import unreal

MAP = '/Game/Game/Stage/PT_StageDefence'
WEIGHTS = {'Fighter': 5, 'Warrior': 3, 'Guardian': 1,
           'Shooter': 5, 'Sharpshooter': 3, 'Judicator': 1}
MULTIPLIER = 0.28 / 3.0
root = Path(unreal.Paths.project_dir())
folder = root / 'Saved' / 'DefenceWaves'
folder.mkdir(parents=True, exist_ok=True)
backup = folder / 'PT_StageDefence.before_central_balance.umap'
if not backup.exists():
    shutil.copy2(root / 'Content/Game/Stage/PT_StageDefence.umap', backup)

assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
controllers = [a for a in sub.get_all_level_actors() if isinstance(a, unreal.SFDefenceWaveController)]
assert len(controllers) == 1
controller = controllers[0]
previous_multiplier = controller.get_editor_property('spawn_count_multiplier')
pool = controller.get_editor_property('unit_pool')
assert len(pool) == len(WEIGHTS) and {str(e.row_name) for e in pool} == set(WEIGHTS)
updated_pool = []
for source in pool:
    entry = unreal.SFDefenceUnitEntry()
    entry.set_editor_property('row_name', source.row_name)
    entry.set_editor_property('unit_class', source.unit_class)
    entry.set_editor_property('spawn_weight', WEIGHTS[str(source.row_name)])
    updated_pool.append(entry)
controller.set_editor_property('unit_pool', updated_pool)
controller.set_editor_property('spawn_count_multiplier', MULTIPLIER)
controller.set_editor_property('total_waves', 5)
controller.set_editor_property('initial_preparation_duration', 60.)
controller.set_editor_property('preparation_duration', 180.)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()

assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
controller = next(a for a in sub.get_all_level_actors() if isinstance(a, unreal.SFDefenceWaveController))
assert abs(controller.get_editor_property('spawn_count_multiplier') - MULTIPLIER) < 0.000001
saved_weights = {str(e.row_name): e.spawn_weight for e in controller.get_editor_property('unit_pool')}
assert saved_weights == WEIGHTS, str(saved_weights)
assert len(controller.get_editor_property('spawners')) == 3
assert controller.get_editor_property('preparation_duration') == 180.
assert controller.get_editor_property('initial_preparation_duration') == 60.
assert controller.get_editor_property('total_waves') == 5
assert controller.get_editor_property('wave_duration') == 300.
assert controller.get_editor_property('first_wave_copies_per_type') == 6
assert controller.get_editor_property('additional_copies_per_wave') == 3
wave_totals = [int(6 * (6 + wave * 3) * 3 * controller.get_editor_property('spawn_count_multiplier') + 0.5)
               for wave in range(5)]
assert wave_totals == [10, 15, 20, 25, 30]
report = dict(map=MAP, first_wave_total=10, second_wave_total=15,
              wave_totals=wave_totals, total_waves=5, previous_multiplier=previous_multiplier,
              initial_preparation_seconds=60, preparation_seconds=180,
              spawn_count_multiplier=MULTIPLIER, unit_weights=WEIGHTS)
(folder / 'weighted_configuration.json').write_text(json.dumps(report, indent=2))
configuration = folder / 'configuration.json'
current_report = json.loads(configuration.read_text()) if configuration.exists() else {}
current_report.update(report)
configuration.write_text(json.dumps(current_report, indent=2))
unreal.log('DEFENCE_BALANCE_VERIFIED: solo 5 waves (10/15/20/25/30), initial preparation 60s, later 180s; saved map reloaded')
