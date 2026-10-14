"""Import the reviewed response pack and wire only voice fields; back up existing assets first."""
import collections,hashlib,json,shutil,traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir())
WORK=Path(globals().get('VOICE_WORK',ROOT/'Saved/VoiceExpansion/v3'));WORK.mkdir(parents=True,exist_ok=True)
IMPORT_FACTION=globals().get('VOICE_FACTION')
MANIFEST=ROOT/'ContentSource/UnitVoices/manifest.json'
BACKUP=WORK/'before';BACKUP.mkdir(exist_ok=True)
TABLE_PATH='/Game/Core/DataTable/DT_Units'
COMMANDERS={
    'BeastCommander':'/Game/Game/Race_Beast/Character/Unit_Commander/BP_Beast_Commander',
    'EtherianCommander':'/Game/Game/Race_Etherial/Character/Unit_Commander/BP_Ether_Commander'}

def backup(package):
    src=ROOT/'Content'/Path(package.removeprefix('/Game/')+'.uasset')
    dst=BACKUP/src.relative_to(ROOT)
    if src.exists() and not dst.exists():
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)

try:
    data=json.loads(MANIFEST.read_text(encoding='utf-8'))
    rows=data['entries'];assert len(rows)==data.get('expected_entries',40),('Incomplete voice pack',len(rows))
    groups=collections.defaultdict(list)
    for row in rows:
        p=ROOT/row['file'];assert p.is_file()
        assert hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256'],str(p)
        groups[(row['faction'],row['unit'],row['kind'])].append(row)
    assert len(groups)==data.get('expected_groups',18),len(groups)
    if IMPORT_FACTION:
        groups={key:entries for key,entries in groups.items() if key[0]==IMPORT_FACTION}
        assert groups,IMPORT_FACTION
    COMMANDERS={unit:path for unit,path in COMMANDERS.items() if any(key[1]==unit for key in groups)}
    table=unreal.load_asset(TABLE_PATH);assert table
    original=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    if not (WORK/'units_before.json').exists():
        (WORK/'units_before.json').write_text(json.dumps(original,ensure_ascii=False,indent=2),encoding='utf-8')
    backup(TABLE_PATH)
    for p in COMMANDERS.values():backup(p)
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    sfx=unreal.load_asset('/Game/Sound/SC_SFX')
    cues={};report={'waves':[],'cues':[],'commanders':[]}
    for (faction,unit,kind),entries in sorted(groups.items()):
        package=f'/Game/Sound/Voices/{faction}/{unit}/{kind}'
        tasks=[]
        for row in sorted(entries,key=lambda r:r['index']):
            name=f'S_{unit}_{kind}_{row["index"]:02}'
            backup(package+'/'+name)
            task=unreal.AssetImportTask()
            for key,value in {'filename':str(ROOT/row['file']),'destination_path':package,'destination_name':name,'automated':True,'replace_existing':True,'save':True}.items():task.set_editor_property(key,value)
            tasks.append((task,row,name))
        tools.import_asset_tasks([t[0] for t in tasks])
        waves=[]
        for task,row,name in tasks:
            wave=unreal.load_asset(package+'/'+name);assert isinstance(wave,unreal.SoundWave),name
            wave.set_editor_property('looping',False)
            if sfx:wave.set_editor_property('sound_class_object',sfx)
            assert unreal.EditorAssetLibrary.save_loaded_asset(wave)
            waves.append(wave);report['waves'].append(wave.get_path_name())
        cue_name=f'SC_{unit}_{kind}'
        backup(package+'/'+cue_name)
        cue=unreal.load_asset(package+'/'+cue_name) if unreal.EditorAssetLibrary.does_asset_exist(package+'/'+cue_name) else tools.create_asset(cue_name,package,unreal.SoundCue,unreal.SoundCueFactoryNew())
        assert unreal.SFVoiceAssetLibrary.configure_response_cue(cue,waves)
        if sfx:cue.set_editor_property('sound_class_object',sfx)
        assert unreal.EditorAssetLibrary.save_loaded_asset(cue)
        random=cue.get_editor_property('first_node')
        assert isinstance(random,unreal.SoundNodeRandom)
        assert len(random.get_editor_property('child_nodes'))==len(waves)
        assert random.get_editor_property('randomize_without_replacement')
        cues[(unit,kind)]=cue
        report['cues'].append({'unit':unit,'kind':kind,'path':cue.get_path_name(),'count':len(waves)})
    updated=[]
    for row in original:
        new=dict(row)
        if (row['Name'],'Selection') in cues:
            new['SelectionSound']=cues[(row['Name'],'Selection')].get_path_name()
            new['MoveCommandSound']=cues[(row['Name'],'Move')].get_path_name()
        updated.append(new)
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(updated,ensure_ascii=False))
    assert unreal.EditorAssetLibrary.save_loaded_asset(table)
    after=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    fields={'SelectionSound','MoveCommandSound'}
    assert [{k:v for k,v in r.items() if k not in fields} for r in original]==[{k:v for k,v in r.items() if k not in fields} for r in after]
    assert next(r for r in original if r['Name']=='Brawler')==next(r for r in after if r['Name']=='Brawler')
    for unit,path in COMMANDERS.items():
        bp=unreal.load_asset(path)
        cls=unreal.EditorAssetLibrary.load_blueprint_class(path)
        cdo=unreal.get_default_object(cls)
        cdo.set_editor_property('selection_sound',cues[(unit,'Selection')])
        cdo.set_editor_property('move_command_sound',cues[(unit,'Move')])
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
        cdo=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
        assert cdo.get_selection_sound()==cues[(unit,'Selection')]
        assert cdo.get_editor_property('move_command_sound')==cues[(unit,'Move')]
        report['commanders'].append(unit)
    report.update(passed=True,preserved_brawler=True,non_voice_unit_fields_unchanged=True)
    (WORK/'installed.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log(f'Installed {sum(len(entries) for entries in groups.values())} clips / {len(cues)} random response cues.')
except Exception:
    (WORK/'install_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
    raise
