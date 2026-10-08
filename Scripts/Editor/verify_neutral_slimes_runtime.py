"""PIE integration: actual map camps, passive AI, rewards, production and HUD."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/NeutralSlimes'
ue=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_Hero')
gi_cdo=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Blueprints/GameInstance/BP_SFGameInstance'))
assert gi_cdo.select_race(unreal.SFRace.HUMAN)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.time(),phase='waiting',checks=[]);refs={}
story=unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
rows={r['Name']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(story))}
def actors(w,cls):return unreal.GameplayStatics.get_all_actors_of_class(w,cls)
def finish(error=None):
    state.update(passed=error is None,error=error)
    (out/'runtime.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    if error:unreal.log_error(error)
    else:unreal.log('NEUTRAL_RUNTIME_VERIFIED')
    level.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(dt):
        unreal.unregister_slate_post_tick_callback(quit_handle);unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle=unreal.register_slate_post_tick_callback(quit_tick)
def tick(dt):
    try:
        if time.time()-state['start']>180:raise RuntimeError('Timeout '+state['phase'])
        w=ue.get_game_world()
        if not w:return
        gs=unreal.GameplayStatics.get_game_state(w)
        if not isinstance(gs,unreal.SFGameState):return
        if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
            assert unreal.GameplayStatics.get_game_mode(w).start_stage();return
        if not gs.is_stage_playing():return
        intros=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFStageIntroWidget,True)
        if intros:
            key=str(gs.get_current_stage_row_name());assert intros[0].start_stage_intro_at(story,key,len(rows[key]['Nodes'])-1);return
        sec=unreal.GameplayStatics.get_time_seconds(w)
        if state['phase']=='waiting':
            camps=actors(w,unreal.SFNeutralSlimeCamp);slimes=actors(w,unreal.SFNeutralSlime)
            assert len(camps)==1 and len(slimes)==3
            assert sum(s.get_editor_property('large') for s in slimes)==1
            assert all(s.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==3 for s in slimes)
            nodes=[a for a in actors(w,unreal.ResourceBase) if a.actor_has_tag('SF_NeutralSlimes')]
            assert len(nodes)==12
            for s in actors(w,unreal.UnitSpawner):s.set_spawning_enabled(False)
            player=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(gs.get_nexus(0),'Warrior',300.,0.)
            commander=player
            assert player
            refs.update(camp=camps[0],slimes=slimes,commander=commander,player=player)
            state.update(phase='settle',since=sec)
        elif state['phase']=='settle' and sec-state['since']>1:
            refs['positions']=[s.get_actor_location() for s in refs['slimes']]
            state.update(phase='idle',since=sec)
        elif state['phase']=='idle' and sec-state['since']>5:
            for s,p in zip(refs['slimes'],refs['positions']):
                assert (s.get_actor_location()-p).length()<5
                assert not s.is_provoked_by_team(0)
            state['checks'].append('All three slimes remain at their authored locations before damage')
            c=refs['commander'];p=refs['slimes'][0].get_actor_location()
            c.set_actor_location(p+unreal.Vector(0,-300,50),False,True)
            c.command_hold_ground()
            refs['slimes'][0].get_component_by_class(unreal.CombatTargetComponent).apply_combat_damage(1,c)
            assert all(s.is_provoked_by_team(0) for s in refs['slimes'])
            state['checks'].append('One hit provokes only the attacked camp')
            buffs=unreal.SFStageBuffSubsystem.get_stage_buffs(w)
            before=buffs.get_buffs(0)
            state.update(attack_before=refs['player'].get_current_attack_power(),health_before=refs['player'].get_health_component().get_max_health())
            mineral=gs.get_mineral(0);ether=gs.get_ether(0)
            for s in refs['slimes']:s.get_component_by_class(unreal.CombatTargetComponent).apply_combat_damage(100000,c)
            b=buffs.get_buffs(0)
            assert b.resource_rewards+b.attack_stacks+b.health_stacks==1
            assert gs.get_mineral(0)-mineral==400+1000*b.resource_rewards
            assert gs.get_ether(0)-ether==1000*b.resource_rewards
            assert refs['camp'].is_cleared()
            state.update(phase='hud',since=sec,reward=dict(resources=b.resource_rewards,attack=b.attack_stacks,health=b.health_stacks))
        elif state['phase']=='hud' and sec-state['since']>1:
            b=unreal.SFStageBuffSubsystem.get_stage_buffs(w).get_buffs(0)
            assert abs(refs['player'].get_current_attack_power()-state['attack_before']*(1+.1*b.attack_stacks))<.01
            assert abs(refs['player'].get_health_component().get_max_health()-state['health_before']*(1+.1*b.health_stacks))<.01
            new=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(gs.get_nexus(0),'Warrior',300.,0.)
            assert new and abs(new.get_current_attack_power()-refs['player'].get_current_attack_power())<.01
            assert abs(new.get_health_component().get_max_health()-refs['player'].get_health_component().get_max_health())<.01
            hud=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFInGameHUDWidget,False)[0]
            panel=hud.get_editor_property('stage_buff_panel')
            text=hud.get_editor_property('stage_buff_text')
            state['hud_objects']=dict(panel=panel.get_path_name() if panel else None,text=text.get_path_name() if text else None)
            assert panel and panel.is_visible() and text and str(text.get_text())
            state['hud_text']=str(text.get_text())
            state['checks']+=['100/100/200 minerals and exactly one reward','Existing and production-spawned units match','Reward is visible in the gameplay HUD','12 additional mineable nodes']
            finish()
    except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
started=time.time()
def begin(dt):
    if time.time()-started<20:return
    unreal.unregister_slate_post_tick_callback(begin_handle);level.editor_request_begin_play()
begin_handle=unreal.register_slate_post_tick_callback(begin)
