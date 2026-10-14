"""Disposable PIE: six real commander casts populate the merged buff HUD."""
import json, time, traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root/'Saved/MergeFeatureRestore'
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_4')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(started=time.time(), phase='start', checks=[])
refs = {}
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
last = len(next(r for r in rows if r['Name']=='Stage04')['Nodes'])-1

def actors(w, cls): return unreal.GameplayStatics.get_all_actors_of_class(w, cls)
def combat(a): return a.get_component_by_class(unreal.CombatTargetComponent)
def buffs(a):
    c = a.get_component_by_class(unreal.SFBuffComponent)
    return list(c.get_editor_property('buff_list')) if c else []
def finish(error=None):
    state.update(passed=error is None, error=error)
    (out/'buff_ui_runtime.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    if error: unreal.log_error(error)
    unreal.unregister_slate_post_tick_callback(handle)
    # Python references to BuffData otherwise keep the PIE world alive at teardown.
    refs.clear()
    import gc
    gc.collect()
    level.editor_request_end_play()
    def quit_tick(dt):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle = unreal.register_slate_post_tick_callback(quit_tick)

def tick(dt):
    try:
        if time.time()-state['started']>160: raise RuntimeError('Timeout '+state['phase'])
        w = editor.get_game_world()
        if not w: return
        gs = unreal.GameplayStatics.get_game_state(w)
        if not isinstance(gs,unreal.SFGameState): return
        if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
            unreal.GameplayStatics.get_game_mode(w).start_stage(); return
        intros = unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFStageIntroWidget,True)
        if intros:
            intros[0].start_stage_intro_at(table,'Stage04',last); return
        if not gs.is_stage_playing(): return
        sec = unreal.GameplayStatics.get_time_seconds(w)
        if state['phase']=='start':
            pc = unreal.GameplayStatics.get_player_controller(w,0)
            ether = pc.get_my_commander(); assert ether
            for s in actors(w,unreal.UnitSpawner): s.set_spawning_enabled(False)
            for u in actors(w,unreal.UnitBase): u.command_hold_ground()
            unreal.SystemLibrary.execute_console_command(w,'SF.Commander.SpawnRespawnFixture')
            beast = next(c for c in actors(w,unreal.SFCommander) if c.actor_has_tag('RespawnFixture'))
            combat(beast).set_team_index(0)
            beast.set_actor_location(ether.get_actor_location()+unreal.Vector(220,0,0),False,True)
            victim = unreal.SFUnitSpawnLibrary.spawn_unit_from_row(gs.get_nexus(0),'Brawler',400,500)
            assert victim
            victim.set_actor_location(ether.get_actor_location()+unreal.Vector(350,0,0),False,True)
            combat(victim).set_team_index(1); victim.command_hold_ground()
            for a in actors(w,unreal.Actor):
                if combat(a): combat(a).set_damage_multiplier(0.)
            gs.get_nexus(0).set_editor_property('current_tier',2)
            hud = unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFInGameHUDWidget,True)[0]
            panel = unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFSelectionPanelWidget,False)[0]
            refs.update(ether=ether,beast=beast,victim=victim,hud=hud,panel=panel,
                queue=[(beast,1,'SpeedUp'),(beast,2,'AtkDefUp'),(beast,3,'ReceiveDmgUp'),
                       (ether,1,'HealUp'),(ether,2,'ProdSpeedUp'),(ether,3,'ReceiveDmgDown')])
            unreal.SystemLibrary.execute_console_command(w,'SF.HUD.QA resize 1280 720')
            state.update(phase='cast',since=sec)
        elif state['phase']=='cast':
            if not refs['queue']:
                state.update(phase='expire',since=sec); return
            c,tier,key = refs['queue'].pop(0)
            refs.update(caster=c,tier=tier,key=key)
            c.call_method('ActiveCommanderSkill', (tier,))
            assert c.get_editor_property('mesh').get_anim_instance().is_any_montage_playing(), (key,'cast rejected')
            state.update(phase='effect',since=sec)
        elif state['phase']=='effect':
            matches=[(a,b) for a in actors(w,unreal.Actor) for b in buffs(a)
                if b.get_editor_property('key_name')==refs['key'] and b.get_time_rate()>0]
            if not matches:
                assert sec-state['since']<6, ('No skill effect',refs['key']); return
            # Prefer the caster to check commander selection, except enemy debuffs/building buffs.
            target,buff = next((m for m in matches if m[0]==refs['caster']),matches[0])
            assert buff.get_editor_property('image') and buff.get_editor_property('description')
            assert buff.get_editor_property('is_buff') == (refs['key']!='ReceiveDmgUp')
            refs.update(target=target,buff=buff)
            refs['hud'].select_actor(target)
            state.update(phase='ui',since=sec)
        elif state['phase']=='ui' and sec-state['since']>.45:
            lv = refs['panel'].get_editor_property('BuffListView')
            items = lv.get_list_items()
            expected_name = str(refs['buff'].get_editor_property('display_name'))
            item = next((i for i in items if str(i.get_editor_property('Name'))==expected_name),None)
            assert item, (expected_name,'missing in buff list',len(items))
            assert item.get_editor_property('Image') == refs['buff'].get_editor_property('image')
            assert str(item.get_editor_property('Description'))
            entries = lv.get_displayed_entry_widgets()
            assert entries, 'Buff entry has no rendered widget'
            state['checks'].append(dict(skill=expected_name,key=refs['key'],
                target=refs['target'].get_name(),ui_items=len(items),rendered_entries=len(entries),
                description=str(item.get_editor_property('Description')),icon=item.get_editor_property('Image').get_path_name()))
            unreal.SystemLibrary.execute_console_command(w,'Shot showui -nosuffix filename="'+str(out/(refs['key']+'.png')).replace('\\','/')+'"')
            state.update(phase='next',since=sec)
        elif state['phase']=='next' and sec-state['since']>1.5:
            state.update(phase='cast',since=sec)
        elif state['phase']=='expire' and sec-state['since']>32:
            refs['hud'].select_actor(refs['ether'])
            state.update(phase='expired_ui',since=sec)
        elif state['phase']=='expired_ui' and sec-state['since']>.5:
            assert refs['panel'].get_editor_property('BuffListView').get_num_items()==0
            state['checks'].append(dict(expired_buffs_removed=True))
            finish()
    except Exception: finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'SF.HUD.QA start 1280 720')
