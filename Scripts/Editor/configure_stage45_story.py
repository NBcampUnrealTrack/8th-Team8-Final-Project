"""Install Stage04/05 opening stories and the pre-victory Stage05Clear epilogue.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.
Only DT_SFStageIntro is saved. Existing rows and stage/map settings are preserved.
"""
import copy
import json
import re
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/Stage45Story'
out.mkdir(parents=True, exist_ok=True)
source = json.loads((root / 'ContentSource/Stage45Story_KO.json').read_text(encoding='utf-8'))
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
assert table
before_raw = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
before = json.loads(before_raw)
# Preserve the first pre-edit snapshot even when rerunning the script.
backup = out / 'intro_before.json'
if not backup.exists():
    backup.write_text(before_raw, encoding='utf-8')

template = next(row for row in before if row['Name'] == 'Stage03Clear')
dialogue_template = template['Nodes'][0]
end_template = template['Nodes'][-1]
assert 'NodeType=Dialogue' in dialogue_template and 'NodeType=End' in end_template
agent = '/Game/Game/IN_UI/Image/Intro/StoryUse/Beast/Agent/Beast_Agent_Nomal.Beast_Agent_Nomal'
frame = '/Game/Game/IN_UI/Image/Intro/StoryUse/Flame/T_Curtain.T_Curtain'
assert unreal.load_asset(agent) and unreal.load_asset(frame)


def quoted(value):
    # Unreal's struct text uses C-style escaping inside FText strings.
    return json.dumps(value, ensure_ascii=False)


def localized(row, index, field, value):
    if not value:
        return '""'
    return 'NSLOCTEXT("SFStage45Story", ' + quoted(f'{row}_{index}_{field}') + ', ' + quoted(value) + ')'


def node_text(row, index, data):
    # Replace the text prefix as a unit: commas inside NSLOCTEXT are not field separators.
    suffix = dialogue_template[dialogue_template.index(',LeftCharacterImage='):]
    node = '(NodeType=Dialogue,TitleText=' + localized(row, index, 'Title', data.get('title', ''))
    node += ',BodyText=' + localized(row, index, 'Body', data['body']) + suffix
    if data.get('right') == 'agent':
        node = node.replace('RightCharacterImage=None', 'RightCharacterImage=' + quoted(agent))
        node = node.replace('PortraitFrameImage=None', 'PortraitFrameImage=' + quoted(frame))
        node = node.replace('SpeakerSide=None', 'SpeakerSide=Right')
    return re.sub(r'NextNodeIndex=-?\d+', f'NextNodeIndex={index + 1}', node)


rows = [copy.deepcopy(row) for row in before if row['Name'] not in source]
for name, lines in source.items():
    row = copy.deepcopy(template)
    row.update(Name=name, StartNodeIndex=0,
               Nodes=[node_text(name, index, data) for index, data in enumerate(lines)] + [end_template],
               LockedFeatures='(GameplayTags=)', ObjectiveSuccessSound='None')
    rows.append(row)

assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(
    table, json.dumps(rows, ensure_ascii=False))
after_raw = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
after = json.loads(after_raw)
(out / 'intro_import_diagnostic.json').write_text(after_raw, encoding='utf-8')
def comparable(row):
    result = copy.deepcopy(row)
    # Unreal assigns localization keys to previously empty FText fields on JSON import.
    # Their display text remains empty; all nonempty text identities must match exactly.
    result['Nodes'] = [re.sub(r'NSLOCTEXT\("[^"]*", "[^"]*", ""\)', '""', n)
                       for n in result['Nodes']]
    return result


assert {r['Name']: comparable(r) for r in before if r['Name'] not in source} == {
    r['Name']: comparable(r) for r in after if r['Name'] not in source}

checks = []
for name, lines in source.items():
    saved = next(row for row in after if row['Name'] == name)
    assert saved['StartNodeIndex'] == 0
    assert len(saved['Nodes']) == len(lines) + 1
    assert 'NodeType=End' in saved['Nodes'][-1]
    for index, data in enumerate(lines):
        node = saved['Nodes'][index]
        assert 'NodeType=Dialogue' in node
        assert f'NextNodeIndex={index + 1},' in node
        for paragraph in data['body'].split('\n'):
            assert paragraph in node, (name, index, paragraph)
        for field in ('PreviousNodeIndex', 'YesNodeIndex', 'NoNodeIndex'):
            assert f'{field}=-1' in node
    checks.append(dict(row=name, dialogue_nodes=len(lines), end_node=len(lines), start_node=0))

assert unreal.EditorAssetLibrary.save_loaded_asset(table, False)
(out / 'intro_after.json').write_text(after_raw, encoding='utf-8')
(out / 'configuration_verified.json').write_text(json.dumps(dict(
    passed=True, rows=checks, preserved_rows=[r['Name'] for r in before if r['Name'] not in source],
    epilogue_trigger='Stage05 Victory -> Stage05Clear -> ResultWidget',
), ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('STAGE45_STORY_VERIFIED: Stage04, Stage05, Stage05Clear saved; existing rows preserved')
