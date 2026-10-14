"""Read back saved deposits and tutorial coordinates after the grid-spacing migration."""
import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'ReleaseFeedback'
baseline=json.loads((out/'levels.json').read_text(encoding='utf-8'))[:7]
plan=json.loads((out/'spacing_applied.json').read_text(encoding='utf-8'))
report=[];sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for original,entry in zip(baseline,plan):
 w=unreal.EditorLoadingAndSavingUtils.load_map(entry['map'])
 actors={a.get_actor_label():a for a in sub.get_all_level_actors() if isinstance(a,unreal.ResourceBase)}
 expected={r['name']:r['pos'] for r in original['resources']}
 expected.update({r['name']:r['after'] for r in entry['changes']})
 assert len(actors)==len(expected)
 for name,pos in expected.items():assert (actors[name].get_actor_location()-unreal.Vector(*pos)).length()<.1,(entry['map'],name)
 report.append(dict(map=entry['map'],actors=len(actors),moved=len(entry['changes'])))
table=unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
(out/'stage_intro_after.json').write_text(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table),encoding='utf-8')
(out/'spacing_verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
