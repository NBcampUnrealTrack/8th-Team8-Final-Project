"""Disposable Stage03 PIE: real synergy, spawn geometry and off-screen combat."""
import json
import re
import time
import traceback
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir())/'PlaytestFeedback'
ue=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_StageDefence')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(phase='starting',start=time.time(),checks=[])
refs={}
opponent_class=unreal.load_class(None,'/Game/Core/Units/Etherial_Human/BP_Human_Fighter.BP_Human_Fighter_C')

def actors(world,cls): return unreal.GameplayStatics.get_all_actors_of_class(world,cls)
def subsystem(world,cls): return next(o for o in unreal.ObjectIterator(cls) if o.get_outer()==world)
def finish(error=None):
    state['passed']=error is None
    if error: state['error']=error
    (out/'runtime_verified.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('PLAYTEST_RUNTIME_PASSED' if not error else 'PLAYTEST_RUNTIME_FAILED: '+error)
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(delta):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle=unreal.register_slate_post_tick_callback(quit_tick)

def tick(delta):
    try:
        if time.time()-state['start']>100: raise RuntimeError('PIE timed out: '+state['phase'])
        world=ue.get_game_world()
        if not world:return
        gs=unreal.GameplayStatics.get_game_state(world)
        if not isinstance(gs,unreal.SFGameState):return
        if state['phase']=='starting':
            if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
                unreal.GameplayStatics.get_game_mode(world).start_stage()
            if not gs.is_stage_playing():return
            intro=unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
            tutorial=subsystem(world,unreal.SFTutorialSubsystem)
            tutorial.cancel_guide()
            tutorial.cancel_objective_wait()
            for w in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.SFStageIntroWidget,False):
                w.start_stage_intro_at(intro,'Stage03',12)
            unreal.GameplayStatics.set_game_paused(world,False)
            stage = next(row for row in json.loads(
                unreal.DataTableFunctionLibrary.export_data_table_to_json_string(intro)) if row['Name'] == 'Stage03')
            for name, node in [('AnimalRanged', 5), ('AnimalSpecial', 8)]:
                target = re.search(r'TargetKind=WorldLocation,.*?WorldLocation=\(X=([^,]+),Y=([^,]+),Z=([^\)]+)\)', stage['Nodes'][node])
                assert target, 'Missing current Stage03 construction target'
                xyz = tuple(float(value) for value in target.groups())
                definition=unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/Animal/DA_'+name)
                transform=unreal.Transform(location=unreal.Vector(*xyz),rotation=unreal.Rotator(),scale=unreal.Vector(1,1,1))
                building=unreal.SFBuildingSpawnLibrary.spawn_building_from_definition(world,definition,0,transform,unreal.SFBuildingSpawnMode.VIEW_PORT_PLACED_MODE,0)
                assert building, 'Could not create '+name
                refs[name]=building
            state.update(phase='synergy',phase_start=unreal.GameplayStatics.get_time_seconds(world))
        elif state['phase']=='synergy':
            if unreal.GameplayStatics.get_time_seconds(world)-state['phase_start']<3:return
            synergy=subsystem(world,unreal.SFBuildingSynergySubsystem)
            bonuses={name:synergy.get_production_bonus_percent(refs[name]) for name in ('AnimalRanged','AnimalSpecial')}
            assert bonuses['AnimalRanged']>0 and bonuses['AnimalSpecial']>bonuses['AnimalRanged'],bonuses
            state['checks'].append(dict(synergy=bonuses))
            beast=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(refs['AnimalSpecial'],'WarBeast')
            assert beast, 'No safe WarBeast spawn'
            state['checks'].append(dict(beast_spawn=str(beast.get_actor_location())))
            beast.set_actor_location(unreal.Vector(-500,-300,190),False,True)
            # Give the opponent enough health to survive several real montage cycles.
            enemy=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(refs['AnimalSpecial'],'WarBeast',300,200,opponent_class)
            assert enemy, 'No safe opponent spawn'
            enemy.set_actor_location(unreal.Vector(-250,-300,100),False,True)
            enemy.get_component_by_class(unreal.CombatTargetComponent).set_team_index(1)
            beast.command_attack_target(enemy)
            enemy.command_attack_target(beast)
            refs.update(beast=beast,enemy=enemy)
            state.update(phase='combat',phase_start=unreal.GameplayStatics.get_time_seconds(world),
                beast_before=beast.get_component_by_class(unreal.HealthComponent).get_current_health(),
                enemy_before=enemy.get_component_by_class(unreal.HealthComponent).get_current_health())
        elif state['phase']=='combat':
            if unreal.GameplayStatics.get_time_seconds(world)-state['phase_start']<9:return
            health={name:refs[name].get_component_by_class(unreal.HealthComponent).get_current_health() for name in ('beast','enemy')}
            state['checks'].append(dict(combat_health=health))
            assert health['enemy']<state['enemy_before'], 'WarBeast did not deal damage'
            assert health['beast']<state['beast_before'], 'WarBeast did not receive damage'
            if '-SFPlaytestCapture' in unreal.SystemLibrary.get_command_line():
                camera=unreal.GameplayStatics.get_player_pawn(world,0)
                pos=camera.get_actor_location()
                camera.set_actor_location(unreal.Vector(-1500,0,pos.z),False,True)
                commander=actors(world,unreal.SFCommander)[0]
                commander.set_actor_location(unreal.Vector(-1500,0,100),False,True)
                state.update(phase='capture_warmup',capture_start=time.time())
                return
            finish()
        elif state['phase']=='capture_warmup' and time.time()-state['capture_start']>1:
            unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI')
            state.update(phase='capture',capture_start=time.time())
        elif state['phase']=='capture' and time.time()-state['capture_start']>3:
            finish()
    except Exception:
        finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
level.editor_request_begin_play()
