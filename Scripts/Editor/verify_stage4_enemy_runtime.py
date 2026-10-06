"""Disposable PIE integration check of the saved Stage 04 enemy base.

Run in UnrealEditor with -ExecutePythonScript=<this file> -nullrhi.
Accelerates the clock and shields allied targets only in PIE to test the full
production schedule. No asset or save-game changes are made by this script.
"""
import json
import math
import time
import traceback
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_dir())/'Saved/Stage04Enemy'
CONFIG = json.loads((OUT/'configuration_verified.json').read_text(encoding='utf-8'))
ue = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_4')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(phase='starting',wall_start=time.time(),samples=[],checks=[],snapshots={})
ORIENTATION_ONLY = '-Stage04OrientationOnly' in unreal.SystemLibrary.get_command_line()
anchors=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
         if isinstance(a,unreal.SFBuildingSpawnAnchor)]
state['saved_nexus_yaws']={str(a.get_editor_property('team_index')):a.get_actor_rotation().yaw for a in anchors
    if a.get_editor_property('building_type')==unreal.SFBuildingType.NEXUS}
if ORIENTATION_ONLY:
    for a in anchors:
        if a.get_editor_property('building_type')!=unreal.SFBuildingType.NEXUS:
            assert abs(a.get_actor_rotation().yaw-90)<.001, ('Saved facing differs',a.get_actor_label())
    # Reverse one factory in this disposable world to exercise normalization.
    probe=next(a for a in anchors if a.get_actor_label()=='Stage04_Enemy_Barracks_T1')
    probe.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
    state['temporary_reversed_factory']=probe.get_actor_label()

def targets(world,cls):
    return unreal.GameplayStatics.get_all_actors_of_class(world,cls)

def save():
    data = {k:v for k,v in state.items() if k not in ('spawners','owners','game_state','world','victim','children')}
    name='orientation_verified.json' if ORIENTATION_ONLY else 'runtime_verified.json'
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')

def finish(error=None):
    state['phase']='failed' if error else 'passed'
    state['passed']=not bool(error)
    if error:
        state['error']=error
        unreal.log_error('STAGE04_RUNTIME_FAILED: '+error)
    else:
        unreal.log('STAGE04_ORIENTATION_VERIFIED: construction facing, Nexus facing preserved' if ORIENTATION_ONLY
                   else 'STAGE04_RUNTIME_VERIFIED: buildings, tier timing, teams, navigation, owner destruction')
    save()
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    # Quit on the next editor tick, after EndPIE has released the world.
    def quit_tick(delta):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle=unreal.register_slate_post_tick_callback(quit_tick)

def sample(world,seconds):
    counts = {row:s.get_editor_property('total_spawned') for row,s in state['spawners'].items()}
    state['samples'].append(dict(time=seconds,counts=counts))
    unreal.log('STAGE04_RUNTIME_SAMPLE: '+json.dumps(state['samples'][-1]))
    return counts

def tick(delta):
    try:
        if not state.get('tick_started'):
            state['tick_started']=True
            unreal.log('STAGE04_RUNTIME_TICK_STARTED')
        if time.time()-state['wall_start'] > 240:
            raise RuntimeError('PIE integration check timed out: '+state['phase'])
        world = ue.get_game_world()
        if world and not state.get('world_seen'):
            state['world_seen']=world.get_path_name()
            unreal.log('STAGE04_RUNTIME_WORLD: '+state['world_seen'])
        if not world:
            return
        gs = unreal.GameplayStatics.get_game_state(world)
        if not isinstance(gs,unreal.SFGameState):
            return
        seconds = unreal.GameplayStatics.get_time_seconds(world)
        if state['phase']=='starting':
            if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
                # NullRHI does not render the loading-screen outro that calls this.
                assert unreal.GameplayStatics.get_game_mode(world).start_stage()
            if gs.get_stage_state()==unreal.SFStageState.INITIALIZATION_FAILED:
                raise RuntimeError('Stage bootstrap failed')
            if not gs.is_stage_playing():
                return
            assert str(gs.get_current_stage_row_name())=='Stage04'
            spawners = targets(world,unreal.UnitSpawner)
            assert len(spawners)==6
            state['spawners']={str(s.get_editor_property('unit_row_name')):s for s in spawners}
            state['owners']={row:s.get_owner_building() for row,s in state['spawners'].items()}
            assert all(state['owners'].values()), 'A factory was not linked'
            assert len({a.get_path_name() for a in state['owners'].values()})==6
            assert not targets(world,unreal.SFDefenceWaveController), 'RTS map contains a defence controller'
            buildings = targets(world,unreal.SFBaseBuilding)
            orientation=[]
            for b in buildings:
                definition=b.get_editor_property('building_definition')
                if definition.get_editor_property('building_type')==unreal.SFBuildingType.NEXUS:
                    team=str(b.get_component_by_class(unreal.CombatTargetComponent).get_team_index())
                    assert abs(b.get_actor_rotation().yaw-state['saved_nexus_yaws'][team])<.001, ('Nexus facing changed',team)
                    continue
                r=b.get_actor_rotation()
                assert abs(r.pitch)<.001 and abs(r.roll)<.001 and abs(r.yaw-90)<.001, ('Wrong building facing',b.get_name(),str(r))
                orientation.append(b.get_name())
            state['checks'].append(dict(non_nexus_buildings_facing_construction=len(orientation),yaw=90))
            enemy = [b for b in buildings if b.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==1]
            assert len(enemy)==16, ('Missing enemy buildings',len(enemy))
            assert gs.get_team_building_catalog(1).get_path_name().lower().find('animal')>=0
            assert gs.get_team_building_catalog(0).get_path_name().lower().find('human')>=0
            for expected in CONFIG['production']:
                owner = state['owners'][expected['row']]
                assert isinstance(owner,unreal.SFProductionBuilding)
                assert owner.get_editor_property('current_tier')==expected['tier']-1
                assert 'DA_Animal' in owner.get_editor_property('building_definition').get_path_name()
            assert gs.get_mineral_income_rate(1)>0 and gs.get_ether_income_rate(1)>0
            state['checks'].append(dict(enemy_buildings=len(enemy),owners_linked=6,enemy_catalog=gs.get_team_building_catalog(1).get_path_name(),
                mineral_income=gs.get_mineral_income_rate(1),ether_income=gs.get_ether_income_rate(1),playing_at=seconds))
            if ORIENTATION_ONLY:
                finish()
                return
            # Protect allied targets only in this disposable simulation so the
            # later-tier timers can be verified independently of solo balance.
            for a in targets(world,unreal.Actor):
                target=a.get_component_by_class(unreal.CombatTargetComponent)
                if target and target.get_team_index()==0:
                    target.set_damage_multiplier(0.0)
            unreal.GameplayStatics.set_global_time_dilation(world,20.0)
            state.update(phase='timing',start_time=seconds)
            sample(world,seconds)
        elif state['phase']=='timing':
            counts = {row:s.get_editor_property('total_spawned') for row,s in state['spawners'].items()}
            for expected in CONFIG['production']:
                if seconds < expected['delay']-1:
                    assert counts[expected['row']]==0, ('Tier spawned early',expected['row'],seconds)
                if seconds > expected['delay']+3:
                    assert counts[expected['row']]>0, ('Tier did not unlock',expected['row'],seconds)
            for checkpoint in (55,90,230,280,470,525):
                if seconds>=checkpoint and str(checkpoint) not in state['snapshots']:
                    state['snapshots'][str(checkpoint)]=sample(world,seconds)
            # Validate actual unit teams, positions and movement before destroying factories.
            if seconds>=525:
                # Pawn possession changes Actor.Owner to the AI controller.
                # This map has one producer for each of these six unit rows.
                children=[u for u in targets(world,unreal.UnitBase)
                          if str(u.get_unit_row_name()) in state['spawners']]
                assert children, 'No spawned units in the live world'
                assert all(u.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==1 for u in children)
                # At least one early unit must have left its production base.
                assert any(u.get_actor_location().y<7000 for u in children), 'Attack units never left the base'
                nav_ok=[]
                for u in children:
                    p=u.get_actor_location()
                    nav=unreal.NavigationSystemV1.project_point_to_navigation(world,p,None,None,unreal.Vector(200,200,500))
                    assert nav is not None, ('Unit is off navigation',u.get_name())
                    nav_ok.append(u.get_class().get_name())
                state['checks'].append(dict(spawned_units=len(children),teams_correct=True,movement=True,classes=sorted(set(nav_ok))))
                victim=state['spawners']['Brawler']
                state['victim']=victim
                state['before_destroy']=victim.get_editor_property('total_spawned')
                state['children']=[u for u in children if str(u.get_unit_row_name())=='Brawler']
                state['children_at_destroy']=len(state['children'])
                owner=state['owners']['Brawler']
                owner.get_component_by_class(unreal.CombatTargetComponent).apply_combat_damage(100000.0,None)
                assert victim.is_shut_down_by_owner_building(), 'Combat destruction did not stop production'
                victim.activate_spawner()
                victim.set_spawning_enabled(True)
                assert victim.spawn_one() is None, 'Destroyed factory resumed spawning'
                assert all(unreal.SystemLibrary.is_valid(u) for u in state['children']), 'Existing units were removed'
                state.update(phase='destroyed',destroy_time=seconds)
                sample(world,seconds)
        elif state['phase']=='destroyed':
            victim=state['victim']
            assert victim.get_editor_property('total_spawned')==state['before_destroy'], 'Timer spawned after owner destruction'
            if seconds-state['destroy_time']>=100:
                # Also exercise direct removal for the other five production buildings.
                for row,s in state['spawners'].items():
                    if row=='Brawler':
                        continue
                    state['owners'][row].destroy_actor()
                    assert s.is_shut_down_by_owner_building(), ('Direct destruction failed',row)
                    assert s.spawn_one() is None
                state['checks'].append(dict(combat_destruction_stops=True,cannot_reactivate=True,existing_units_kept=True,
                    timer_stopped_for_seconds=seconds-state['destroy_time'],other_factory_removal_stops=True))
                sample(world,seconds)
                # Killing the enemy core must produce the standard RTS victory.
                nexus=next(b for b in targets(world,unreal.SFBaseBuilding)
                    if 'Nexus' in b.get_editor_property('building_definition').get_name()
                    and b.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==1)
                nexus.get_component_by_class(unreal.CombatTargetComponent).apply_combat_damage(100000.0,None)
                assert gs.get_stage_result()==unreal.SFStageResult.VICTORY
                state['checks'].append(dict(enemy_nexus_destroyed_result='Victory'))
                finish()
    except Exception:
        finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
level.editor_request_begin_play()
