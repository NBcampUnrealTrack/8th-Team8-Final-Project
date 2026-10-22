"""Move Stage03 production to the centre and add three mineable deposits.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
Reruns reuse tagged deposits. Only the defence map and Stage03 intro row change.
"""
import copy
import json
import re
import shutil
from pathlib import Path
import unreal

MAP = '/Game/Game/Stage/PT_StageDefence'
INTRO = '/Game/Game/IN_UI/DT/DT_SFStageIntro'
TAG = unreal.Name('SF_Stage03CentralEconomy')
BARRACKS = (-250, 250, 0)
ACADEMY = (200, 250, 0)
SANCTUARY = (200, -250, 0)
MINERALS = {
    'Stage03_Central_Mineral_A': (-1700, -800, 0),
    'Stage03_Central_Mineral_B': (-1000, -700, 0),
    'Stage03_Central_Mineral_C': (1100, -800, 0),
}
root = Path(unreal.Paths.project_dir())
out = root / 'Saved/Stage03Layout'
out.mkdir(parents=True, exist_ok=True)
for relative in ('Content/Game/Stage/PT_StageDefence.umap',
                 'Content/Game/IN_UI/DT/DT_SFStageIntro.uasset'):
    backup = out / (Path(relative).name + '.before_layout')
    if not backup.exists():
        shutil.copy2(root / relative, backup)

table = unreal.load_asset(INTRO)
before = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
rows = copy.deepcopy(before)
stage = next(row for row in rows if row['Name'] == 'Stage03')
targets = {4: ACADEMY, 5: ACADEMY, 7: SANCTUARY, 8: SANCTUARY, 10: SANCTUARY}
for index, xyz in targets.items():
    location = '(X=%.6f,Y=%.6f,Z=%.6f)' % xyz
    node, count = re.subn(r'(TargetKind=WorldLocation,.*?WorldLocation=)\([^)]*\)',
                          lambda match: match[1] + location, stage['Nodes'][index])
    assert count == 1, ('Unexpected world target count', index, count)
    stage['Nodes'][index] = node
assert all(a == b for a, b in zip(before, rows) if a['Name'] != 'Stage03')

assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
barracks = by_label['Stage03_Start_Barracks']
assert barracks.get_editor_property('team_index') == 0
assert barracks.get_editor_property('building_type') == unreal.SFBuildingType.BARRACKS
barracks.set_actor_location(unreal.Vector(*BARRACKS), False, True)
# Clear two small decorative props from the new production footprints.
for label, xyz in {'RF_WallRock_090': (1192, 413, None),
                   'RF_CampStump_1': (1088, -170, None)}.items():
    actor = by_label[label]
    actor.set_actor_location(unreal.Vector(xyz[0], xyz[1], actor.get_actor_location().z), False, True)

mineral_class = by_label['BP_Mineral'].get_class()
for label, xyz in MINERALS.items():
    actor = by_label.get(label)
    if actor:
        assert TAG in actor.tags and isinstance(actor, unreal.Mineral), label
    else:
        actor = sub.spawn_actor_from_class(mineral_class, unreal.Vector(*xyz))
        assert actor, label
        actor.set_actor_label(label)
        actor.set_editor_property('tags', [TAG])
    actor.set_actor_location(unreal.Vector(*xyz), False, True)
    actor.set_folder_path('Stage 03/Central Economy')
    actor.set_editor_property('resource_deposit', 1500)

assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(table)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
serialized = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
(out / 'intro_expected.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
(out / 'intro_saved.json').write_text(json.dumps(serialized, ensure_ascii=False, indent=2), encoding='utf-8')
# The importer gives empty FTexts localization identities until the package reloads.
def canonical(value):
    if isinstance(value, str):
        return re.sub(r'NSLOCTEXT\("[^"\n]*", "[^"\n]*", ""\)', '""', value)
    if isinstance(value, list):
        return [canonical(item) for item in value]
    if isinstance(value, dict):
        return {key: canonical(item) for key, item in value.items()}
    return value
assert canonical(serialized) == canonical(rows)
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
saved = sub.get_all_level_actors()
by_label = {a.get_actor_label(): a for a in saved}
assert by_label['Stage03_Start_Barracks'].get_actor_location().equals(unreal.Vector(*BARRACKS))
for label, xyz in MINERALS.items():
    assert by_label[label].get_actor_location().equals(unreal.Vector(*xyz))
    assert by_label[label].get_editor_property('resource_deposit') == 1500
assert len([a for a in saved if TAG in a.tags]) == len(MINERALS)
report = dict(barracks=BARRACKS, academy=ACADEMY, sanctuary=SANCTUARY,
              minerals=MINERALS, mineral_count=sum(isinstance(a, unreal.Mineral) for a in saved),
              total_mineral=sum(a.get_editor_property('resource_deposit') for a in saved if isinstance(a, unreal.Mineral)),
              tutorial_nodes=list(targets), other_intro_rows_unchanged=True)
(out / 'configuration_verified.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('STAGE03_LAYOUT_SAVED ' + json.dumps(report))
