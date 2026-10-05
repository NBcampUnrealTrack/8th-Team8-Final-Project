"""Review display text only, hide narration portraits, and update skill lessons."""
import json
import re
import runpy
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/TutorialPresentation'
out.mkdir(parents=True, exist_ok=True)
runpy.run_path(str(root / 'Scripts/Editor/configure_stage45_story.py'))
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
raw = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
rows = json.loads(raw)
if not (out / 'before.json').exists():
    (out / 'before.json').write_text(raw, encoding='utf-8')
quoted = r'"(?:\\.|[^"\\])*"'
pattern = re.compile(r'(\w+)=(NSLOCTEXT\(' + quoted + r',\s*' + quoted + r',\s*' + quoted + r'\)|INVTEXT\(' + quoted + r'\)|' + quoted + ')')
fields = {'TitleText', 'BodyText', 'YesText', 'NoText', 'InstructionText'}
unsafe = '\"\'“”‘’「」『』'
lessons = json.loads((root / 'ContentSource/CommanderSkillTutorial_KO.json').read_text(encoding='utf-8'))
changes, narrations = [], []
checked = 0

def clean(text):
    return text.translate(str.maketrans('', '', unsafe)).replace('…', '...').replace('·', ', ').replace('\r\r\n', '\n').replace('\r\n', '\n')

for row in rows:
    for index, node in enumerate(row['Nodes']):
        step = [0]
        def revise(match):
            global checked
            if match[1] not in fields:
                return match[0]
            checked += 1
            strings = list(re.finditer(quoted, match[2]))
            source = strings[-1]
            before = json.loads(source[0])
            after = clean(before)
            if 'SFCommanderSkills' in node and match[1] == 'InstructionText':
                after = lessons[step[0]]
                step[0] += 1
            if before == after:
                return match[0]
            changes.append(dict(row=row['Name'], node=index, field=match[1], before=before, after=after))
            return match[1] + '=' + match[2][:source.start()] + json.dumps(after, ensure_ascii=False) + match[2][source.end():]
        node = pattern.sub(revise, node)
        title = next((json.loads(re.findall(quoted, m[2])[-1]) for m in pattern.finditer(node) if m[1] == 'TitleText'), '')
        if 'NodeType=Dialogue' in node and title.strip() in ('', '독백', '나레이션', '내레이션'):
            for field in ('LeftCharacterImage', 'RightCharacterImage', 'PortraitFrameImage'):
                node = re.sub(field + r'=(?:"[^"\n]*"|None)', field + '=None', node)
            node = re.sub(r'SpeakerSide=\w+', 'SpeakerSide=None', node)
            narrations.append(dict(row=row['Name'], node=index))
        row['Nodes'][index] = node

assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False))
saved = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
for row in saved:
    for node in row['Nodes']:
        for match in pattern.finditer(node):
            if match[1] in fields:
                text = json.loads(re.findall(quoted, match[2])[-1])
                assert not any(char in text for char in unsafe + '…·'), (row['Name'], text)
for entry in narrations:
    node = next(row for row in saved if row['Name'] == entry['row'])['Nodes'][entry['node']]
    assert all(field + '=None' in node for field in ('LeftCharacterImage', 'RightCharacterImage', 'PortraitFrameImage'))
assert unreal.EditorAssetLibrary.save_loaded_asset(table, False)
(out / 'after.json').write_text(json.dumps(saved, ensure_ascii=False, indent=2), encoding='utf-8')
(out / 'verified.json').write_text(json.dumps(dict(passed=True, checked_text_fields=checked,
    text_changes=changes, narration_nodes=narrations), ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log(f'TUTORIAL_PRESENTATION_VERIFIED: {checked} fields checked, {len(changes)} text edits, {len(narrations)} narration nodes')
