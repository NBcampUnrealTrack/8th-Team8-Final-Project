"""PIE verification for each faction: -Stage7Race=Human or Beast. No asset writes."""
import json,time,traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
out=root/'Saved/Stage7Balance'
out.mkdir(parents=True,exist_ok=True)
race='Beast' if '-Stage7Race=Beast' in unreal.SystemLibrary.get_command_line() else 'Human'
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_SFStageFinal')
cdo=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Blueprints/GameInstance/BP_SFGameInstance'))
cdo.select_race(unreal.SFRace.BEAST if race=='Beast' else unreal.SFRace.HUMAN)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
story=unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
stories={r['Name']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(story))}
state=dict(race=race,phase='start',started=time.time(),checks=[],spawned=[])
refs={}
seen=set()

def actors(w,cls):return unreal.GameplayStatics.get_all_actors_of_class(w,cls)
def combat(a):return a.get_component_by_class(unreal.CombatTargetComponent)
def finish(error=None):
    state.update(passed=error is None,error=error)
    (out/('runtime_'+race+'.json')).write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
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
        if time.time()-state['started']>240:raise RuntimeError('Timeout '+state['phase'])
        w=editor.get_game_world()
        if not w:return
        gs=unreal.GameplayStatics.get_game_state(w)
        if not isinstance(gs,unreal.SFGameState):return
        assert gs.get_stage_state()!=unreal.SFStageState.INITIALIZATION_FAILED
        if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
            unreal.GameplayStatics.get_game_mode(w).start_stage();return
        if not gs.is_stage_playing():return
        intros=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFStageIntroWidget,True)
        if intros:
            name='Stage07_'+race
            intros[0].start_stage_intro_at(story,name,len(stories[name]['Nodes'])-1);return
        if not state.get('selected'):
            gi=unreal.GameplayStatics.get_game_instance(w)
            for i in range(1,7):assert gi.mark_stage_cleared('Stage%02d'%i)
            assert gi.select_stage('Stage07')
            assert gi.select_race(unreal.SFRace.BEAST if race=='Beast' else unreal.SFRace.HUMAN)
            assert gi.move_to_stage()
            state.update(selected=True,travelling=True);return
        if state.pop('travelling',False):return
        if state['phase']=='start':
            buildings=actors(w,unreal.SFBaseBuilding)
            assert len(buildings)==26,len(buildings)
            assert ('Animal' if race=='Beast' else 'Human') in gs.get_team_building_catalog(0).get_path_name()
            for team in [0,1]:
                assert gs.get_nexus(team).get_current_tier()==2
                own=[b for b in buildings if combat(b).get_team_index()==team]
                special=[b for b in own if 'Special' in b.get_building_definition().get_name()]
                assert len(own)==13 and len(special)==2
                assert all(b.get_current_tier()==0 for b in own if isinstance(b,unreal.SFProductionBuilding))
                assert gs.get_mineral_income_rate(team)>0 and gs.get_ether_income_rate(team)>0
            for b in buildings:combat(b).set_damage_multiplier(0.)
            spawners=actors(w,unreal.UnitSpawner)
            assert len(spawners)==16
            enemy=[s for s in spawners if s.get_editor_property('team_index')==1]
            player=[s for s in spawners if s.get_editor_property('team_index')==0]
            assert len(enemy)==len(player)==8
            for s in spawners:
                assert s.get_owner_building()
                assert combat(s.get_owner_building()).get_team_index()==s.get_editor_property('team_index')
            for s in player:
                s.activate_spawner();s.set_spawning_enabled(True)
                assert s.spawn_one() is None
            for s in enemy:assert s.get_editor_property('total_spawned')==0
            refs.update(enemy=enemy,player=player,buildings=buildings)
            state['checks'].append(dict(buildings=26,sanctuary_per_team=2,nexus_tier=3,production_tier=1,both_incomes=True,ownership=True))
            camera=unreal.GameplayStatics.get_player_pawn(w,0)
            camera.set_actor_tick_enabled(False)
            camera.get_component_by_class(unreal.FloatingPawnMovement).stop_movement_immediately()
            camera.get_component_by_class(unreal.SpringArmComponent).set_editor_property('target_arm_length',5000.)
            unreal.SystemLibrary.execute_console_command(w,'Shot showui')
            unreal.GameplayStatics.set_global_time_dilation(w,10.)
            state['phase']='progression'
        elif state['phase']=='progression':
            elapsed=refs['enemy'][0].get_editor_property('progression_elapsed_seconds')
            for unit in actors(w,unreal.UnitBase):
                if unit.get_name() in seen:continue
                row=str(unit.get_unit_row_name())
                if combat(unit).get_team_index()!=1:continue
                # Possession changes a Pawn's Owner to its AI controller.
                owner=next((s for s in refs['enemy'] if any(
                    row==str(e.get_editor_property('row_name')) for p in s.get_editor_property('spawn_phases')
                    for e in p.get_editor_property('unit_pool'))),None)
                if owner is None:continue
                seen.add(unit.get_name())
                phase=owner.get_progression_phase_index()
                pools=owner.get_editor_property('spawn_phases')
                allowed=[str(e.get_editor_property('row_name')) for e in pools[phase].get_editor_property('unit_pool')]
                assert row in allowed,(row,allowed)
                assert elapsed>=owner.get_editor_property('initial_delay')-.5
                # Full paths to the opposing base verify the saved spawn entrances and bridges.
                if not state.get('navigation_checked'):
                    dest=unreal.GameplayStatics.get_player_controller(w,0).get_my_commander().get_actor_location()
                    nav=unreal.NavigationSystemV1.get_navigation_system(w)
                    path=nav.find_path_to_location_synchronously(w,unit.get_actor_location(),dest)
                    assert path and path.is_valid() and not path.is_partial(),'No route from enemy base'
                    state['navigation_checked']=True
                state['spawned'].append(dict(row=row,phase=phase,seconds=elapsed))
                unit.command_hold_ground()
                # Sampling composition rather than combat: release the cap after observing each actual spawn.
                unit.destroy_actor()
            assert all(s.get_editor_property('total_spawned')==0 for s in refs['player'])
            if elapsed<410:return
            assert all(s.get_editor_property('total_spawned')>0 for s in refs['enemy'])
            assert {r['phase'] for r in state['spawned']} >= {0,1,2,3}
            assert all(b.get_current_tier()==0 for b in refs['buildings'] if isinstance(b,unreal.SFProductionBuilding))
            for s in refs['enemy']:
                target=combat(s.get_owner_building())
                target.set_damage_multiplier(1.);target.apply_combat_damage(100000.,None)
                assert s.is_shut_down_by_owner_building() and s.spawn_one() is None
            state['checks'].append(dict(timed_pools=True,player_never_auto_spawns=True,production_stays_tier1=True,factory_destruction_stops_spawning=True))
            finish()
    except Exception:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
def begin(dt):
    if time.time()-state['started']<20:return
    unreal.unregister_slate_post_tick_callback(begin_handle)
    level.editor_request_begin_play()
begin_handle=unreal.register_slate_post_tick_callback(begin)
