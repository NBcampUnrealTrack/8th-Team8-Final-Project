"""Import generated portraits/icons and extend Stage01 after unit movement."""
import json
import re
import runpy
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/CommanderContent'
out.mkdir(parents=True, exist_ok=True)
manifest = json.loads((root/'ContentSource/CommanderImagePrompts.json').read_text(encoding='utf-8'))
tasks = []
for entry in manifest['assets']:
    source = root/entry['destination']
    assert source.exists(), source
    package = '/Game/' + str(source.parent.relative_to(root/'Content')).replace('\\', '/')
    task = unreal.AssetImportTask()
    for name, value in dict(filename=str(source), destination_path=package,
                            destination_name=entry['name'], automated=True,
                            replace_existing=True, save=True).items():
        task.set_editor_property(name, value)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
textures = {}
for entry, task in zip(manifest['assets'], tasks):
    paths = task.get_editor_property('imported_object_paths')
    assert len(paths) == 1, entry['name']
    texture = unreal.load_asset(paths[0])
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb', True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
    textures[entry['name']] = texture

for faction, race, blueprint in [
    ('Beast', 'Beast', 'BP_Beast_Commander'),
    ('Ether', 'Etherial', 'BP_Ether_Commander'),
]:
    path = f'/Game/Game/Race_{race}/Character/Unit_Commander/{blueprint}'
    bp = unreal.load_asset(path)
    default = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
    default.set_editor_property('skill_icons', [textures[f'T_Skill_{faction}_{i}'] for i in range(1, 4)])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp)

runpy.run_path(str(root/'Scripts/Editor/configure_stage45_story.py'))
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
before = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
if not (out/'intro_before_skills.json').exists():
    (out/'intro_before_skills.json').write_text(before, encoding='utf-8')
rows = json.loads(before)
stage = next(r for r in rows if r['Name'] == 'Stage01')
lessons = json.loads((root/'ContentSource/CommanderSkillTutorial_KO.json').read_text(encoding='utf-8'))
steps = []
for tier, lesson in enumerate(lessons, 1):
    text = 'NSLOCTEXT("SFCommanderSkills", "Tier%d", %s)' % (tier, json.dumps(lesson, ensure_ascii=False))
    steps.append('(InstructionText=' + text + ',TargetKind=ScreenRegion,ScreenAnchor=(X=0.18,Y=0.88),'
                 'ScreenExtent=(X=0.035,Y=0.06),CommanderSkillTier=%d,Completion=Click,' % tier +
                 'AutoAdvanceSeconds=0,bDimBackground=False,bShowHighlight=True)')
node = '(NodeType=InteractiveGuide,GuideSteps=(' + ','.join(steps) + '),NextNodeIndex=37,PreviousNodeIndex=-1,YesNodeIndex=-1,NoNodeIndex=-1)'
indices = [i for i, n in enumerate(stage['Nodes']) if 'SFCommanderSkills' in n]
assert len(indices) <= 1
index = indices[0] if indices else len(stage['Nodes'])
if indices:
    stage['Nodes'][index] = node
else:
    stage['Nodes'].append(node)
stage['Nodes'][36] = re.sub(r'NextNodeIndex=-?\d+', f'NextNodeIndex={index}', stage['Nodes'][36])
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows, ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(table)
after = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
saved = next(r for r in after if r['Name']=='Stage01')
assert f'NextNodeIndex={index},' in saved['Nodes'][36]
assert saved['Nodes'][index].count('CommanderSkillTier=') == 3
assert 'NextNodeIndex=37,' in saved['Nodes'][index]
assert {r['Name']:r for r in after if r['Name']!='Stage01'} == {r['Name']:r for r in rows if r['Name']!='Stage01'}
(out/'intro_after.json').write_text(json.dumps(after,ensure_ascii=False,indent=2),encoding='utf-8')

bindings = []
for name in ('IMC_SFBase', 'IMC_SFGameplay'):
    context = unreal.load_asset('/Game/Game/IN_UI/Input/IMC/' + name)
    for binding in context.get_editor_property('mappings'):
        action = binding.get_editor_property('action')
        if action and 'Skill' in action.get_name():
            bindings.append(dict(action=action.get_name(), key=str(unreal.InputLibrary.key_get_display_name(binding.get_editor_property('key')))))
(out/'installed.json').write_text(json.dumps(dict(passed=True,portrait_count=6,skill_icon_count=6,
    guide_node=index,guide_tiers=[1,2,3],bindings=bindings),ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('COMMANDER_CONTENT_INSTALLED')

runpy.run_path(str(root/'Scripts/Editor/polish_tutorial_presentation.py'))
