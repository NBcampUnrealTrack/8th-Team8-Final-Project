"""Apply scoped tutorial and Stage03 resource fixes, preserving other rows/maps."""
import json
import re
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/PlaytestFeedback'
out.mkdir(parents=True, exist_ok=True)
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
before = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
if not (out/'intro_before.json').exists():
    (out/'intro_before.json').write_text(before, encoding='utf-8')
rows = json.loads(before)
stage1 = next(r for r in rows if r['Name'] == 'Stage01')
stage3 = next(r for r in rows if r['Name'] == 'Stage03')

def body(node, key, text):
    value = 'NSLOCTEXT("SFPlaytest", ' + json.dumps(key) + ', ' + json.dumps(text, ensure_ascii=False) + ')'
    return re.sub(r'BodyText=.*?(?=,LeftCharacterImage=)', lambda _: 'BodyText=' + value, node)

if not any('SFPlaytest_Synergy' in n for n in stage1['Nodes']):
    start = len(stage1['Nodes'])
    template = stage1['Nodes'][37]
    lessons = [
        ('SFPlaytest_Synergy', '건물을 가까이 배치하면 시너지 효과를 받을 수 있습니다.\n\n병영은 주변 아카데미와 성소의 생산 속도를 높이고, 아카데미는 주변 성소의 생산 속도를 높입니다.\n\n건설할 때 표시되는 효과 범위와 생산 속도 보너스를 확인하세요.'),
        ('SFPlaytest_Controls', '유닛을 선택한 뒤 A 키를 누르고 지형이나 미니맵을 좌클릭하면, 목적지로 이동하며 만나는 적을 공격합니다.\n\nCtrl 키를 누른 채 숫자 키 1~6 중 하나를 누르면 부대가 지정됩니다. 지정한 숫자 키를 누르면 해당 부대가 선택되고, 두 번 누르면 부대 위치로 화면이 이동합니다.'),
        ('SFPlaytest_Menu', '마지막으로, F10 키를 누르면 메뉴 창이 열립니다.\n\n메뉴에서 ‘계속하기’를 누르면 전장으로 돌아옵니다.\n\n이제 병력을 모아 적 넥서스를 파괴하세요!')]
    for index, (key, text) in enumerate(lessons):
        node = body(template, key, text)
        node = re.sub(r'NextNodeIndex=-?\d+', 'NextNodeIndex=' + str(start + index + 1 if index < 2 else 38), node)
        stage1['Nodes'].append(node)
    stage1['Nodes'][37] = re.sub(r'NextNodeIndex=\d+', 'NextNodeIndex=' + str(start), template)

# Keep the existing barracks at its snapped centre (750,550). Both receiving
# footprints intersect its military zone; the sanctuary also intersects the
# academy zone. The 75 cm tolerance excludes adjacent 100 cm grid positions.
for i in (4, 5):
    node = stage3['Nodes'][i]
    node = node.replace('WorldLocation=(X=1300.000000,Y=500.000000,Z=0.000000)',
                        'WorldLocation=(X=1200.000000,Y=550.000000,Z=0.000000)')
    node = node.replace('WorldRadius=450.000000', 'WorldRadius=75.000000')
    stage3['Nodes'][i] = node
for i in (7, 8, 10):
    node = stage3['Nodes'][i]
    # Only world-space steps; screen-space guide anchors remain unchanged.
    node = re.sub(r'(TargetKind=WorldLocation,.*?WorldLocation=)\(X=0.000000,Y=0.000000,Z=0.000000\)',
                  lambda m: m[1] + '(X=1200.000000,Y=50.000000,Z=0.000000)', node)
    node = node.replace('WorldRadius=550.000000', 'WorldRadius=75.000000')
    node = node.replace('맵 중앙에 특수 병영을 건설하세요.', '아카데미 바로 옆에 성소(특수 병영)를 건설하세요.')
    stage3['Nodes'][i] = node
stage3['Nodes'][6] = body(stage3['Nodes'][6], 'Stage03_SynergyPlacement',
    '아카데미가 완성되었습니다.\n\n이제 병영과 아카데미에 인접한 표시 위치에 성소(특수 병영)를 건설해 주십시오.\n\n두 건물의 시너지 효과로 성소의 생산 속도가 빨라집니다.')
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(table)
saved = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
assert len(next(r for r in saved if r['Name']=='Stage01')['Nodes']) == len(stage1['Nodes'])

map_file = root/'Content/Game/Stage/PT_StageDefence.umap'
backup = out/'PT_StageDefence.before_feedback.umap'
if not backup.exists(): shutil.copy2(map_file, backup)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_StageDefence')
changes=[]
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not isinstance(actor, unreal.ResourceBase): continue
    mineral = isinstance(actor, unreal.Mineral)
    actor.set_editor_property('resource_deposit', max(actor.get_editor_property('resource_deposit'), 1500 if mineral else 900))
    if actor.get_actor_label() == 'BP_Mineral3':
        pos=actor.get_actor_location()
        pos.z=0.
        actor.set_actor_location(pos, False, True)
    changes.append(dict(actor=actor.get_actor_label(), deposit=actor.get_editor_property('resource_deposit'), location=str(actor.get_actor_location())))
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
(out/'applied_assets.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('PLAYTEST_ASSETS_APPLIED')
