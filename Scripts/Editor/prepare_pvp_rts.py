"""Restore resource nodes and base markers only in the four competitive level copies."""
from pathlib import Path
import json
import re
import shutil
import unreal

root=Path(unreal.Paths.project_dir())
out=root/'Saved/PVPRTS'
out.mkdir(parents=True,exist_ok=True)
table=unreal.load_asset('/Game/Core/DataTable/DT_SFStage')
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
report_path=out/'map_resources.json'
report=json.loads(report_path.read_text(encoding='utf-8')) if report_path.exists() else {}
for stage in globals().get('PVP_STAGES',(1,2,4,7)):
    row=next(r for r in rows if r['Name']==f'Stage{stage:02d}')
    source_path=re.search(r'/Game/[^\s\'\"]+',row['StageLevel']).group(0).split('.')[0]
    target=f'/Game/PVP/Maps/PVP_Stage{stage:02d}'
    backup=out/'MapBackups'/f'PVP_Stage{stage:02d}.umap'
    backup.parent.mkdir(exist_ok=True)
    if not backup.exists():shutil.copy2(root/f'Content/PVP/Maps/PVP_Stage{stage:02d}.umap',backup)
    world=unreal.EditorLoadingAndSavingUtils.load_map(target)
    source=unreal.load_asset(source_path)
    result=unreal.SFPVPEditorLibrary.restore_battlefield_resources(world,source)
    assert result.startswith('resources='),(stage,result)
    starter_resources=0
    if stage==1:
        # The campaign prototype has no Ether and only one distant mineral node.
        # Both PVP seats need access to mining when they start with zero Ether.
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if 'PVP_StarterResource' in [str(tag) for tag in actor.tags]:
                unreal.EditorLevelLibrary.destroy_actor(actor)
        for resource_type,asset,locations in (
            (unreal.ResourceType.MINERAL,'/Game/Core/Structure/Resources/BP_Mineral',((-1400,-300,0),(1600,4800,10))),
            (unreal.ResourceType.ETHER,'/Game/Core/Structure/Resources/BP_Ether',((1400,-300,0),(-1200,4800,10))),
        ):
            resource_class=unreal.EditorAssetLibrary.load_blueprint_class(asset)
            assert resource_class,asset
            for location in locations:
                actor=unreal.EditorLevelLibrary.spawn_actor_from_class(resource_class,unreal.Vector(*location))
                assert actor,location
                assert actor.get_editor_property('resource_type')==resource_type,asset
                actor.set_editor_property('resource_deposit',1500)
                actor.set_replicates(True)
                actor.tags=[unreal.Name('PVP_StarterResource')]
                starter_resources+=1
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,target),target
    report[str(stage)]={'source':source_path,'result':result,'starter_resources':starter_resources}
    report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('Competitive resources restored: '+str(report))
