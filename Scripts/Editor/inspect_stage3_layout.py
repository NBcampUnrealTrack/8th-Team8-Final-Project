"""Export the authored Stage03 layout and tutorial targets without changing assets."""
import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'Stage03Layout'
out.mkdir(parents=True, exist_ok=True)
for name, path in [('intro', '/Game/Game/IN_UI/DT/DT_SFStageIntro'),
                   ('stages', '/Game/Core/DataTable/DT_SFStage')]:
    table = unreal.load_asset(path)
    (out / (name + '.json')).write_text(
        unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table), encoding='utf-8')
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_StageDefence')
result = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    p = actor.get_actor_location()
    item = dict(label=actor.get_actor_label(), cls=actor.get_class().get_name(),
                location=[p.x, p.y, p.z])
    for prop in ('building_type', 'team_index', 'resource_deposit', 'initial_tier_index', 'tags'):
        try:
            item[prop] = str(actor.get_editor_property(prop))
        except Exception:
            pass
    result.append(item)
(out / 'actors_before.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
unreal.log('STAGE03_INSPECTION_COMPLETE')
