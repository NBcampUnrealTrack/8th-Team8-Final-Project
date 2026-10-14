"""Fresh asset reload plus disposable PIE selection/move playback for every unit and commander."""
import collections,json,time,traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir());OUT=Path(globals().get('VOICE_WORK',ROOT/'Saved/VoiceExpansion/v3'))
OUT.mkdir(parents=True,exist_ok=True)
manifest=json.loads((ROOT/'ContentSource/UnitVoices/manifest.json').read_text(encoding='utf-8'))
state={'started':time.time(),'phase':'start','checks':[],'played':[]}
refs={}
expected=collections.defaultdict(list)
for row in manifest['entries']:expected[(row['faction'],row['unit'],row['kind'])].append(row)
for (faction,unit,kind),entries in expected.items():
    path=f'/Game/Sound/Voices/{faction}/{unit}/{kind}/SC_{unit}_{kind}'
    cue=unreal.load_asset(path);assert isinstance(cue,unreal.SoundCue),path
    random=cue.get_editor_property('first_node');assert isinstance(random,unreal.SoundNodeRandom)
    children=random.get_editor_property('child_nodes')
    assert len(children)==len(entries),(path,len(children),len(entries))
    assert random.get_editor_property('randomize_without_replacement')
    wave_names={f'S_{unit}_{kind}_{row["index"]:02}' for row in entries}
    actual_names=set()
    for node in children:
        assert isinstance(node,unreal.SoundNodeWavePlayer) and not node.get_editor_property('looping')
        assert node.get_editor_property('sound_wave_asset_ptr'),path
        actual_names.add(node.get_editor_property('sound_wave_asset_ptr').get_name())
    assert actual_names==wave_names,(path,actual_names,wave_names)
    assert cue.get_editor_property('sound_class_object')==unreal.load_asset('/Game/Sound/SC_SFX')
state['checks'].append(f"Fresh reload: {len(manifest['entries'])} waves and {len(expected)} non-looping random response cue graphs")
table=unreal.load_asset('/Game/Core/DataTable/DT_Units')
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
before=json.loads((OUT/'units_before.json').read_text(encoding='utf-8'))
omit={'SelectionSound','MoveCommandSound'}
assert [{k:v for k,v in r.items() if k not in omit} for r in rows]==[{k:v for k,v in r.items() if k not in omit} for r in before]
assert next(r for r in rows if r['Name']=='Brawler')==next(r for r in before if r['Name']=='Brawler')
state['checks'].append('Brawler unchanged; all non-voice unit data unchanged')
for row in rows:
    for field in ('SelectionSound','MoveCommandSound'):
        assert row[field]!='None' and unreal.load_asset(row[field]),(row['Name'],field)
    if row['Name']!='Brawler':
        faction=next(k[0] for k in expected if k[1]==row['Name'])
        for kind,field in [('Selection','SelectionSound'),('Move','MoveCommandSound')]:
            cue=unreal.load_asset(f'/Game/Sound/Voices/{faction}/{row["Name"]}/{kind}/SC_{row["Name"]}_{kind}')
            assert unreal.load_asset(row[field])==cue,(row['Name'],field)
assert len(rows)==16,('Expected 16 playable unit rows',len(rows))
commanders={}
for faction,unit,bp in [('Beast','BeastCommander','/Game/Game/Race_Beast/Character/Unit_Commander/BP_Beast_Commander'),('Etherian','EtherianCommander','/Game/Game/Race_Etherial/Character/Unit_Commander/BP_Ether_Commander')]:
    cdo=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(bp))
    assigned={}
    for kind,field in [('Selection','selection_sound'),('Move','move_command_sound')]:
        cue=unreal.load_asset(f'/Game/Sound/Voices/{faction}/{unit}/{kind}/SC_{unit}_{kind}')
        assert cue and cdo.get_editor_property(field)==cue,(unit,field)
        assigned[kind]=cue.get_path_name()
    commanders[unit]=assigned
(OUT/'coverage.json').write_text(json.dumps(dict(units=[{k:row[k] for k in ['Name','SelectionSound','MoveCommandSound']} for row in rows],commanders=commanders,missing=[],playable_characters=18),indent=2),encoding='utf-8')
refs['rows']={r['Name']:r for r in rows}
refs['queue']=[r['Name'] for r in rows if r['SelectionSound']!='None']+['PlayerCommander','OtherCommander']
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_2')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)

def finish(error=None):
    state.update(passed=error is None,error=error,elapsed=time.time()-state['started'])
    (OUT/'runtime_verified.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()
    def quit_tick(dt):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle=unreal.register_slate_post_tick_callback(quit_tick)

def tick(dt):
    try:
        if time.time()-state['started']>360:raise RuntimeError('Voice test timeout: '+state['phase'])
        w=editor.get_game_world()
        if not w:return
        gs=unreal.GameplayStatics.get_game_state(w)
        if not isinstance(gs,unreal.SFGameState):return
        if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
            unreal.GameplayStatics.get_game_mode(w).start_stage()
        if not gs.is_stage_playing():return
        now=time.time()
        if state['phase']=='start':
            if now-state['started']<38:return
            for intro in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFStageIntroWidget,False):
                intro.deactivate_widget()
            for obj in unreal.ObjectIterator(unreal.SFTutorialSubsystem):
                if obj.get_outer()==w:obj.cancel_guide();obj.cancel_objective_wait()
            unreal.GameplayStatics.set_game_paused(w,False)
            pc=unreal.GameplayStatics.get_player_controller(w,0)
            commander=pc.get_my_commander()
            if not commander:return
            for spawner in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.UnitSpawner):spawner.set_spawning_enabled(False)
            for target in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                combat=target.get_component_by_class(unreal.CombatTargetComponent)
                if combat:combat.set_damage_multiplier(0.)
            voice=next(o for o in unreal.ObjectIterator(unreal.SFUnitVoiceSubsystem) if o.get_outer()==w)
            refs.update(pc=pc,commander=commander,voice=voice)
            state.update(phase='next',since=now)
        elif state['phase']=='next':
            if refs['voice'].get_active_voice():return
            if not refs['queue']:
                assert len(state['played'])==18,len(state['played'])
                state['checks'].append('All 16 playable units and both commanders: six busy clicks discarded, no interruption or queue, fresh movement click plays after silence')
                finish();return
            name=refs['queue'][0]
            if name=='PlayerCommander':speaker=refs['commander']
            elif name=='OtherCommander':
                unreal.SystemLibrary.execute_console_command(w,'SF.Commander.SpawnRespawnFixture')
                speaker=next(c for c in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SFCommander) if c.actor_has_tag('RespawnFixture'))
                speaker.get_component_by_class(unreal.CombatTargetComponent).set_team_index(refs['commander'].get_component_by_class(unreal.CombatTargetComponent).get_team_index())
            else:
                speaker=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(gs.get_nexus(0),name,800,1000)
                if not speaker:return
            refs['queue'].pop(0)
            speaker.get_component_by_class(unreal.CombatTargetComponent).set_damage_multiplier(0.)
            selection=speaker.get_selection_sound();assert selection,name+' selection missing'
            move=speaker.get_editor_property('move_command_sound') if isinstance(speaker,unreal.SFCommander) else unreal.load_asset(refs['rows'][name]['MoveCommandSound'])
            refs['pc'].play_selection_sound(speaker)
            first=refs['voice'].get_active_voice();assert first and first.is_playing(),name+' selection did not start'
            assert first.get_editor_property('sound')==selection
            dest=speaker.get_actor_location()+unreal.Vector(120,0,0)
            for repeat in range(6):
                refs['pc'].play_selection_sound(speaker)
                if isinstance(speaker,unreal.SFCommander):speaker.move_to_location(dest)
                else:speaker.command_move_to(dest)
            assert refs['voice'].get_active_voice()==first,name+' selection interrupted'
            refs.update(speaker=speaker,first=first,selection_path=selection.get_path_name(),move=move,name=name)
            state.update(phase='selection',since=now)
        elif state['phase']=='selection':
            current=refs['voice'].get_active_voice()
            assert current is None or current==refs['first'],refs['name']+' busy click was queued'
            if current is None:state.update(phase='silent',since=now)
            elif now-state['since']>12:raise RuntimeError(refs['name']+' selection did not finish')
        elif state['phase']=='silent':
            assert refs['voice'].get_active_voice() is None,refs['name']+' unexpected deferred playback'
            if now-state['since']<1.2:return
            speaker=refs['speaker'];dest=speaker.get_actor_location()+unreal.Vector(220,100,0)
            if isinstance(speaker,unreal.SFCommander):speaker.move_to_location(dest)
            else:speaker.command_move_to(dest)
            current=refs['voice'].get_active_voice()
            assert current and current.is_playing(),refs['name']+' fresh click did not play'
            assert current.get_editor_property('sound')==refs['move'],refs['name']+' incorrect movement cue'
            refs['second']=current;state.update(phase='move',since=now)
        elif state['phase']=='move':
            if refs['voice'].get_active_voice() is None:
                assert refs['voice'].get_active_voice() is None
                state['played'].append({'name':refs['name'],'selection':refs['selection_path'],'move':refs['move'].get_path_name()})
                unreal.log('VOICE VERIFIED '+refs['name'])
                if isinstance(refs['speaker'],unreal.UnitBase):refs['speaker'].destroy_actor()
                state.update(phase='next',since=now)
            elif now-state['since']>12:raise RuntimeError(refs['name']+' movement response looping')
    except Exception:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'SF.HUD.QA start 1280 720')
