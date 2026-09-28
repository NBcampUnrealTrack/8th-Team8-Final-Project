import copy
import json
import re
import unreal

TAG = 'Tutorial.Stage02.Completed'
tag = unreal.GameplayTag()
tag.import_text('(TagName="' + TAG + '")')
assert TAG in tag.export_text()
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
before = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
after = copy.deepcopy(before)
row = next(r for r in after if r['Name'] == 'Stage02')
assert len(row['Nodes']) == 33 and 'NodeType=End,' in row['Nodes'][32]
assert 'NextNodeIndex=32,' in row['Nodes'][31]
row['Nodes'][32], count = re.subn(r'ObjectiveTag=\(TagName="[^"]*"\)',
                                 'ObjectiveTag=(TagName="' + TAG + '")', row['Nodes'][32])
assert count == 1
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(after, ensure_ascii=False))
loaded = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
quoted = r'"(?:\\.|[^"\\])*"'
def semantic(value):
    return re.sub(r'NSLOCTEXT\(' + quoted + r',\s*' + quoted + r',\s*(' + quoted + r')\)', lambda m: m.group(1), value)
assert semantic(json.dumps(after, ensure_ascii=False)) == semantic(json.dumps(loaded, ensure_ascii=False)) or all(
    semantic(a) == semantic(b)
    for old, new in zip(after, loaded) for a, b in zip(old['Nodes'], new['Nodes']))
assert unreal.EditorAssetLibrary.save_loaded_asset(table)
assert unreal.DataTableFunctionLibrary.export_data_table_to_json_file(table, 'C:/FinalPro/Finalproject/Saved/Stage2SpawnGateTable.json')

path = '/Game/Game/Stage/PT_Stage_2'
assert unreal.EditorLoadingAndSavingUtils.load_map(path)
subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
spawners = [a for a in subsystem.get_all_level_actors() if isinstance(a, unreal.UnitSpawner) and a.get_editor_property('team_index') == 1]
assert len(spawners) == 5
old_state = {}
def unchanged(actor):
    return (actor.get_actor_transform().export_text(),) + tuple(str(actor.get_editor_property(p)) for p in (
        'team_index', 'initial_delay', 'spawn_interval', 'unit_row_name', 'max_alive', 'max_total', 'spawn_count_per_cycle', 'lane'))
for spawner in spawners:
    old_state[spawner.get_actor_label()] = unchanged(spawner)
    spawner.set_editor_property('start_active', False)
    spawner.set_editor_property('activation_objective_tag', tag)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
assert unreal.EditorLoadingAndSavingUtils.load_map(path)
spawners = [a for a in subsystem.get_all_level_actors() if isinstance(a, unreal.UnitSpawner) and a.get_editor_property('team_index') == 1]
assert len(spawners) == 5
for spawner in spawners:
    assert not spawner.get_editor_property('start_active')
    assert TAG in spawner.get_editor_property('activation_objective_tag').export_text()
    assert unchanged(spawner) == old_state[spawner.get_actor_label()]
    unreal.log('SPAWNER_GATE_VERIFIED ' + spawner.get_actor_label())
unreal.log('STAGE2_SPAWN_GATE_SAVED_OK')
