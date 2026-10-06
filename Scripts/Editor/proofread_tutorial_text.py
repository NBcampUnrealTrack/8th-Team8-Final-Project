"""Export or apply reviewed Korean tutorial copy without changing tutorial flow."""
import json
import re
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/TutorialProofreading'
out.mkdir(parents=True, exist_ok=True)
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
raw = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
rows = json.loads(raw)
quoted = r'"(?:\\.|[^"\\])*"'
pattern = re.compile(r'(\w+)=(' + r'NSLOCTEXT\(' + quoted + r',\s*' + quoted + r',\s*' + quoted + r'\)|INVTEXT\(' + quoted + r'\)|' + quoted + ')')

def entries(rows):
    result = []
    for row in rows:
        for index, node in enumerate(row.get('Nodes', [])):
            for occurrence, match in enumerate(pattern.finditer(node)):
                value = match[2]
                strings = re.findall(quoted, value)
                text = json.loads(strings[-1])
                if not text or not re.search('[가-힣]', text):
                    continue
                result.append(dict(id=f'{row["Name"]}/{index}/{occurrence}', field=match[1], text=text))
    return result

(out / 'current_table.json').write_text(raw, encoding='utf-8')
(out / 'current_text.json').write_text(json.dumps(entries(rows), ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('TUTORIAL_COPY_EXPORTED: ' + str(len(entries(rows))))

if '-ApplyTutorialCopy' in unreal.SystemLibrary.get_command_line():
    edits = json.loads((root / 'ContentSource/TutorialCopyEdits_KO.json').read_text(encoding='utf-8'))
    backup = out / 'before_table.json'
    if not backup.exists(): backup.write_text(raw, encoding='utf-8')
    original = json.loads(raw)
    changes = []
    found = set()
    text_fields = {'TitleText', 'BodyText', 'YesText', 'NoText', 'InstructionText'}
    for row in rows:
        for index, node in enumerate(row.get('Nodes', [])):
            occurrence = [-1]
            def revise(match):
                occurrence[0] += 1
                identity = f'{row["Name"]}/{index}/{occurrence[0]}'
                if identity not in edits: return match[0]
                assert match[1] in text_fields, identity
                found.add(identity)
                strings = list(re.finditer(quoted, match[2]))
                source = strings[-1]
                before = json.loads(source[0])
                after = edits[identity]
                if before == after: return match[0]
                changes.append(dict(id=identity, before=before, after=after))
                value = match[2][:source.start()] + json.dumps(after, ensure_ascii=False) + match[2][source.end():]
                return match[1] + '=' + value
            row['Nodes'][index] = pattern.sub(revise, node)
    assert found == set(edits), set(edits) - found
    def structural(data):
        result = json.loads(json.dumps(data))
        for row in result:
            row['Nodes'] = [pattern.sub(lambda m: m[1] + '=<TEXT>' if m[1] in text_fields else m[0], n) for n in row['Nodes']]
        return result
    assert structural(original) == structural(rows), 'A non-text field changed'
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False))
    saved_raw = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
    saved = json.loads(saved_raw)
    assert structural(original) == structural(saved), 'Import changed tutorial flow or placement'
    actual = {item['id']: item['text'] for item in entries(saved)}
    assert all(actual.get(key) == value for key, value in edits.items())
    assert unreal.EditorAssetLibrary.save_loaded_asset(table, False)
    (out / 'after_table.json').write_text(saved_raw, encoding='utf-8')
    (out / 'changes.json').write_text(json.dumps(changes, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log(f'TUTORIAL_COPY_VERIFIED: {len(changes)} corrected; flow and placement unchanged')
