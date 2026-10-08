"""PIE smoke tests: -Stage67Case=6, Human, or Beast. No asset writes."""
import json, time, traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT/'Saved/Stage67'
cmd = unreal.SystemLibrary.get_command_line()
case = 'Beast' if '-Stage67Case=Beast' in cmd else 'Human' if '-Stage67Case=Human' in cmd else '6'
stage = 'Stage06' if case=='6' else 'Stage07'
map_path = '/Game/Game/Stage/PT_SFStageAmbush' if case=='6' else '/Game/Game/Stage/PT_SFStageFinal'
ue = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map(map_path)
gi_cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Blueprints/GameInstance/BP_SFGameInstance'))
assert gi_cdo.select_race(unreal.SFRace.BEAST if case=='Beast' else unreal.SFRace.HUMAN)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(case=case, phase='waiting', started=time.time(), checks=[])
refs = {}
story_table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
story_rows = {r['Name']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(story_table))}

def actors(w,cls): return unreal.GameplayStatics.get_all_actors_of_class(w,cls)
def team(a): return a.get_component_by_class(unreal.CombatTargetComponent).get_team_index()
def finish(error=None):
    state.update(passed=error is None, error=error, phase='done')
    (OUT/('runtime_'+case+'.json')).write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    if error: unreal.log_error(error)
    else: unreal.log('STAGE67_RUNTIME_VERIFIED '+case)
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(delta):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle=unreal.register_slate_post_tick_callback(quit_tick)

def tick(delta):
    try:
        if time.time()-state['started']>180: raise RuntimeError('Timed out: '+state['phase'])
        w=ue.get_game_world()
        if not w: return
        gs=unreal.GameplayStatics.get_game_state(w)
        if not isinstance(gs,unreal.SFGameState): return
        assert gs.get_stage_state()!=unreal.SFStageState.INITIALIZATION_FAILED, 'Stage bootstrap failed'
        if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
            # NullRHI does not paint/tick UMG fades. Assets must still reach Ready normally.
            assert unreal.GameplayStatics.get_game_mode(w).start_stage()
            return
        if not gs.is_stage_playing(): return
        intros = unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFStageIntroWidget,True)
        if intros:
            # Narrative behavior is covered separately; this harness exercises combat.
            opening = str(gs.get_current_stage_row_name())
            race = unreal.GameplayStatics.get_game_instance(w).get_selected_race()
            variant = opening + ('_Beast' if race==unreal.SFRace.BEAST else '_Human')
            if variant in story_rows: opening = variant
            assert intros[0].start_stage_intro_at(story_table,opening,len(story_rows[opening]['Nodes'])-1)
            return
        if case!='6' and not state.get('selected'):
            gi=unreal.GameplayStatics.get_game_instance(w)
            for order in range(1,7): assert gi.mark_stage_cleared('Stage%02d'%order)
            assert gi.select_stage('Stage07')
            assert gi.get_selected_race()==unreal.SFRace.NONE
            assert gi.select_race(unreal.SFRace.BEAST if case=='Beast' else unreal.SFRace.HUMAN)
            assert gi.move_to_stage()
            state['selected']=True
            state['travelling_world']=w.get_path_name()
            return
        if state.get('travelling_world'):
            # Travel is applied after this callback; wait for the next world's setup.
            state.pop('travelling_world')
            return
        sec=unreal.GameplayStatics.get_time_seconds(w)
        if state['phase']=='waiting':
            gi=unreal.GameplayStatics.get_game_instance(w)
            for order in range(1,6 if case=='6' else 7): assert gi.mark_stage_cleared('Stage%02d'%order)
            assert str(gs.get_current_stage_row_name())==stage
            buildings=actors(w,unreal.SFBaseBuilding)
            expected=7 if case=='6' else 16
            assert len(buildings)==expected, ('buildings', len(buildings), expected)
            assert gs.get_nexus(0)
            assert gs.get_mineral_income_rate(0)>0 and gs.get_ether_income_rate(0)>0
            assert ('Animal' if case=='Beast' else 'Human') in gs.get_team_building_catalog(0).get_path_name()
            assert ('Human' if case=='Beast' else 'Animal') in gs.get_team_building_catalog(1).get_path_name()
            for a in buildings: a.get_component_by_class(unreal.CombatTargetComponent).set_damage_multiplier(0.0)
            spawners=actors(w,unreal.UnitSpawner)
            refs.update(spawners=spawners)
            if case=='6':
                c=actors(w,unreal.SFDefenceWaveController)[0]
                refs['controller']=c
                assert len(spawners)==3
                assert c.get_phase()==unreal.SFDefencePhase.PREPARATION
                assert c.get_remaining_seconds()>50
                assert gs.get_mineral(0)>=2000 and gs.get_ether(0)>=800
                # Validate complete navigation from all entrances to the defensive plateau.
                for s in spawners:
                    nav=unreal.NavigationSystemV1.get_navigation_system(w)
                    path=nav.find_path_to_location_synchronously(w,s.get_actor_location(),unreal.Vector(-2100,0,90))
                    assert path and path.is_valid() and not path.is_partial(), 'Disconnected Stage06 entrance'
            else:
                assert len(spawners)==6
                for s in spawners:
                    assert s.get_owner_building(), 'Unbound AI factory'
                    if s.get_editor_property('team_index')==0:
                        s.activate_spawner();s.set_spawning_enabled(True)
                        assert s.spawn_one() is None, 'Player factory auto-spawned'
                n=gs.get_nexus(0).get_actor_location()
                assert (n.x>0)==(case=='Beast'), ('wrong base',str(n))
                p=unreal.GameplayStatics.get_player_pawn(w,0).get_actor_location()
                assert abs(p.x-n.x)<100 and abs(p.y-n.y)<100, ('camera',str(p),str(n))
                commanders=actors(w,unreal.SFCommander)
                assert len(commanders)==1, ('commander count',len(commanders))
                commander=commanders[0]
                expected_class='BP_Beast_Commander_C' if case=='Beast' else 'BP_Ether_Commander_C'
                assert commander.get_class().get_name()==expected_class, commander.get_class().get_name()
                assert commander.get_editor_property('race')==(unreal.Race.BEAST if case=='Beast' else unreal.Race.ETHERIAN)
                assert commander.get_editor_property('my_commander') and team(commander)==0
                pc=unreal.GameplayStatics.get_player_controller(w,0)
                assert pc.get_my_commander()==commander, 'Commander not registered for player control'
                c=commander.get_actor_location()
                direction=gs.get_nexus(1).get_actor_location()-n
                distance=(direction.x**2+direction.y**2)**0.5
                forward=unreal.Vector(direction.x/distance,direction.y/distance,0)
                offset=c-n
                assert 300<(offset.x**2+offset.y**2)**0.5<1800, ('commander too far from Nexus',str(c),str(n))
                assert offset.x*forward.x+offset.y*forward.y>300, 'Commander behind Nexus'
                assert commander.get_controller(), 'Commander has no movement controller'
                assert commander.get_component_by_class(unreal.HealthComponent).get_current_health()>0
                nav=unreal.NavigationSystemV1.get_navigation_system(w)
                destination=c+forward*400
                path=nav.find_path_to_location_synchronously(w,c,destination)
                assert path and path.is_valid() and not path.is_partial(), 'Commander cannot leave base'
                state['checks'].append(dict(commander_class=expected_class,commander_position=str(c),nexus_position=str(n),player_control=True,walkable_front=True))
            state['checks'].append(dict(buildings=len(buildings),income=True,correct_races=True))
            unreal.GameplayStatics.set_global_time_dilation(w,20.0)
            state.update(phase='spawning',playing_start=sec)
        elif state['phase']=='spawning':
            for a in actors(w,unreal.UnitBase): a.get_component_by_class(unreal.CombatTargetComponent).set_damage_multiplier(0.0)
            if case=='6':
                c=refs['controller']
                if c.get_wave_number()<1 or c.get_remaining_spawn_count()>0: return
                assert c.get_phase()==unreal.SFDefencePhase.WAVE and c.get_remaining_seconds()>200
                assert all(s.get_alive_count()>0 for s in refs['spawners'])
                spawned=[u for u in actors(w,unreal.UnitBase) if not isinstance(u,unreal.SFCommander)]
                assert spawned and all(team(a)==1 for a in spawned)
                state['checks'].append(dict(three_directions_spawned=True,full_wave_time=True))
                # Accelerate only the verification fixture; the saved map stays 60/300.
                c.set_editor_property('preparation_duration',1.)
                c.set_editor_property('wave_duration',2.)
                state.update(phase='waves')
            else:
                enemy=[s for s in refs['spawners'] if s.get_editor_property('team_index')==1]
                if not all(s.get_alive_count()>0 for s in enemy): return
                assert all(s.get_editor_property('total_spawned')==0 for s in refs['spawners'] if s.get_editor_property('team_index')==0)
                units=[u for u in actors(w,unreal.UnitBase) if not isinstance(u,unreal.SFCommander)]
                assert units and all(team(u)==1 for u in units), [(u.get_name(),team(u)) for u in units]
                expected_rows=['Fighter','Warrior','Guardian','Shooter','Sharpshooter','Judicator','Assassin','Colossus'] if case=='Beast' else ['Brawler','Berserker','Predator','Hunter','Stalker','ApexHunter','SpiritShaman','WarBeast']
                assert all(str(u.get_unit_row_name()) in expected_rows for u in units)
                for s in enemy:
                    target=s.get_owner_building().get_component_by_class(unreal.CombatTargetComponent)
                    target.set_damage_multiplier(1.);target.apply_combat_damage(100000.,None)
                    assert s.is_shut_down_by_owner_building()
                state['checks'].append(dict(enemy_production=True,player_auto_production=False,factory_destruction_stops=True))
                target=gs.get_nexus(1).get_component_by_class(unreal.CombatTargetComponent)
                target.set_damage_multiplier(1.);target.apply_combat_damage(100000.,None)
                assert gs.get_stage_result()==unreal.SFStageResult.VICTORY
                assert unreal.GameplayStatics.get_game_instance(w).get_highest_cleared_stage_order()==7
                finish()
        elif state['phase']=='waves':
            c=refs['controller']
            # The legacy ambush director must not finish the stage after 240 seconds.
            if c.get_wave_number()<5: return
            state.update(phase='victory')
        elif state['phase']=='victory': pass
    except Exception: finish(traceback.format_exc())

def victory_tick(delta):
    if state['phase'] not in ['waves','victory']: return
    w=ue.get_game_world()
    if not w:return
    gs=unreal.GameplayStatics.get_game_state(w)
    if gs and gs.is_stage_finished():
        try:
            assert gs.get_stage_result()==unreal.SFStageResult.VICTORY
            assert refs['controller'].get_wave_number()==5
            assert unreal.GameplayStatics.get_game_instance(w).get_highest_cleared_stage_order()==6
            state['checks'].append(dict(five_wave_victory=True,stage7_unlocked=True))
            unreal.unregister_slate_post_tick_callback(victory_handle)
            finish()
        except Exception: finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
victory_handle=unreal.register_slate_post_tick_callback(victory_tick)
editor_start=time.time()
def begin(delta):
    if time.time()-editor_start<15:return
    unreal.unregister_slate_post_tick_callback(begin_handle)
    level.editor_request_begin_play()
begin_handle=unreal.register_slate_post_tick_callback(begin)
