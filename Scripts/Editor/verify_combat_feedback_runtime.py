"""Disposable Stage04 PIE: Nexus commander, compact damage bars, real melee/ranged attacks."""
import json, time, traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
out=root/'Saved/CombatFeedback'
out.mkdir(parents=True,exist_ok=True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_4')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(phase='start',started=time.time(),checks=[])
refs={}
table=unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
last=len(next(r for r in rows if r['Name']=='Stage04')['Nodes'])-1

def actors(w,cls):return unreal.GameplayStatics.get_all_actors_of_class(w,cls)
def hp(a):return a.get_component_by_class(unreal.HealthComponent)
def combat(a):return a.get_component_by_class(unreal.CombatTargetComponent)
def dist(a,b):return ((a.x-b.x)**2+(a.y-b.y)**2)**.5
def finish(error=None):
    state.update(passed=not error,error=error)
    (out/'runtime.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    if error:unreal.log_error(error)
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(dt):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle=unreal.register_slate_post_tick_callback(quit_tick)

def tick(dt):
    try:
        if time.time()-state['started']>180:raise RuntimeError('Timeout '+state['phase'])
        w=editor.get_game_world()
        if not w:return
        gs=unreal.GameplayStatics.get_game_state(w)
        if not isinstance(gs,unreal.SFGameState):return
        assert gs.get_stage_state()!=unreal.SFStageState.INITIALIZATION_FAILED
        if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
            unreal.GameplayStatics.get_game_mode(w).start_stage();return
        intros=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFStageIntroWidget,True)
        if intros:
            intros[0].start_stage_intro_at(table,'Stage04',last);return
        if not gs.is_stage_playing():return
        sec=unreal.GameplayStatics.get_time_seconds(w)
        if state['phase']=='start':
            pc=unreal.GameplayStatics.get_player_controller(w,0)
            c=pc.get_my_commander()
            camera=unreal.GameplayStatics.get_player_pawn(w,0)
            camera.set_actor_tick_enabled(False)
            movement=camera.get_component_by_class(unreal.FloatingPawnMovement)
            if movement:movement.stop_movement_immediately()
            assert c and len(actors(w,unreal.SFCommander))==1
            n=gs.get_nexus(0).get_actor_location();enemy=gs.get_nexus(1).get_actor_location()
            p=c.get_actor_location();direction=enemy-n;length=dist(enemy,n)
            forward=unreal.Vector(direction.x/length,direction.y/length,0)
            assert 300<dist(p,n)<1800,(str(p),str(n))
            assert (p-n).x*forward.x+(p-n).y*forward.y>300
            camera.set_actor_location(unreal.Vector(p.x,p.y,camera.get_actor_location().z),False,True)
            pc.set_view_target_with_blend(camera,0.)
            state['checks'].append(dict(stage4_commander_front=True,commander=str(p),nexus=str(n)))
            for s in actors(w,unreal.UnitSpawner):s.set_spawning_enabled(False)
            for b in actors(w,unreal.SFBaseBuilding):combat(b).set_damage_multiplier(0.)
            # Production uses the real unit data rows and their attack montages/notifies.
            units=[]
            for row in ['Warrior','Sharpshooter','Berserker','Hunter']:
                unit=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(c,row,350.,0.)
                assert unit,row
                unit.command_hold_ground()
                unit.tags=list(unit.tags)+[unreal.Name('CombatFeedback')]
                units.append(unit)
            refs.update(c=c,units=units,forward=forward,origin=p)
            state.update(phase='bars',since=sec)
        elif state['phase']=='bars' and sec-state['since']>1:
            for a in [refs['c']]+refs['units']:
                bar=a.get_component_by_class(unreal.SFUnitHealthBarComponent)
                assert bar and not bar.is_visible(),a.get_name()
                before=hp(a).get_current_health()
                combat(a).apply_combat_damage(1.,None)
                assert abs(hp(a).get_current_health()-(before-1))<.01
                assert bar.is_visible(),a.get_name()
                assert bar.get_user_widget_object(),a.get_name()
                size=bar.get_draw_size()
                assert 22<=size.x<=64 and size.y==6,str(size)
            state['checks'].append(dict(commander_and_four_units_show_bar_on_one_damage=True))
            state.update(phase='combat_setup',since=sec)
            unreal.SystemLibrary.execute_console_command(w,'Shot showui')
        elif state['phase']=='combat_setup' and sec-state['since']>1:
            # Give each attacker its own stationary target on a verified walking route.
            nav=unreal.NavigationSystemV1.get_navigation_system(w)
            targets=[]
            for i,unit in enumerate(refs['units']):
                start=unit.get_actor_location()
                end=start+refs['forward']*600
                path=nav.find_path_to_location_synchronously(w,start,end)
                assert path and path.is_valid() and not path.is_partial(),'No fixture route'
                victim=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(unit,'Warrior',50.,0.)
                assert victim
                victim.set_actor_location(end+unreal.Vector(0,0,80),False,True)
                victim.set_actor_tick_enabled(False)
                combat(victim).set_team_index(1)
                combat(victim).set_damage_multiplier(.1)
                unit.command_attack_target(victim)
                targets.append(victim)
            refs['targets']=targets
            refs['before']=[hp(v).get_current_health() for v in targets]
            refs['start_positions']=[u.get_actor_location() for u in refs['units']]
            state.update(phase='combat',since=sec)
        elif state['phase']=='combat':
            if sec-state['since']>8 and not state.get('diagnosed'):
                unreal.SystemLibrary.execute_console_command(w,'SF.Combat.Diagnose')
                state['diagnosed']=True
            damaged=[hp(v).get_current_health()<before for v,before in zip(refs['targets'],refs['before'])]
            if all(damaged):
                state['checks'].append(dict(four_real_units_move_and_apply_attack_damage=True,damage=[before-hp(v).get_current_health() for v,before in zip(refs['targets'],refs['before'])]))
                for unit in refs['units']:unit.command_stop()
                refs['automatic_before']=sum(hp(v).get_current_health() for v in refs['targets'])
                state.update(phase='automatic',since=sec)
            elif sec-state['since']>30:
                raise AssertionError(str([(u.get_name(),str(u.get_combat_state()),damaged[i],str(u.get_actor_location()),str(refs['targets'][i].get_actor_location())) for i,u in enumerate(refs['units'])]))
        elif state['phase']=='automatic' and sec-state['since']>5:
            assert sum(hp(v).get_current_health() for v in refs['targets'])<refs['automatic_before'],'Automatic combat did not deal damage'
            assert all(u.get_combat_state()==unreal.UnitCombatState.ATTACKING for u in refs['units'])
            state['checks'].append(dict(four_units_resume_automatic_attack=True))
            finish()
    except Exception:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
def begin(dt):
    if time.time()-state['started']<20:return
    unreal.unregister_slate_post_tick_callback(begin_handle)
    level.editor_request_begin_play()
begin_handle=unreal.register_slate_post_tick_callback(begin)
