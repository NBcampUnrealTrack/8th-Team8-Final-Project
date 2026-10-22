"""Copy only Nexus anchors into PVP 1/2/4/7. Preserve all other PVP map edits."""
from pathlib import Path
import json
import re
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/PVPRTS/CampaignBaseBackups'
out.mkdir(parents=True, exist_ok=True)
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(
    unreal.load_asset('/Game/Core/DataTable/DT_SFStage')))
report = {}
for stage in (1, 2, 4, 7):
    row = next(r for r in rows if r['Name'] == f'Stage{stage:02d}')
    source_path = re.search(r'/Game/[^\s\'\"]+', row['StageLevel']).group(0).split('.')[0]
    target = f'/Game/PVP/Maps/PVP_Stage{stage:02d}'
    backup = out / f'PVP_Stage{stage:02d}.umap'
    if not backup.exists():
        shutil.copy2(root / f'Content/PVP/Maps/PVP_Stage{stage:02d}.umap', backup)
    world = unreal.EditorLoadingAndSavingUtils.load_map(target)
    # Intentionally use an uninitialized source world: the native helper must also
    # support load_asset, as used by the existing resource preparation script.
    source = unreal.load_asset(source_path)
    count = unreal.SFPVPEditorLibrary.restore_battlefield_bases(world, source)
    assert count == 2, (stage, count)
    markers = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        tags = [str(t) for t in actor.tags]
        if any(t in tags for t in ('PVP_Base0', 'PVP_Base1')):
            location = actor.get_actor_location()
            rotation = actor.get_actor_rotation()
            markers.append({'team': 0 if 'PVP_Base0' in tags else 1,
                            'location': [location.x, location.y, location.z],
                            'rotation': [rotation.pitch, rotation.yaw, rotation.roll]})
    assert len(markers) == 2
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, target), target
    report[str(stage)] = {'source': source_path, 'markers': markers}
    (root / 'Saved/PVPRTS/campaign_bases_synced.json').write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('PVP campaign Nexus anchors restored: ' + str(report))
