"""Disposable PIE: both commander races/teams die, count down, and revive at their Nexus."""
import json
import time
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/CommanderRespawn'
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_4')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(phase='start', started=time.time(), checks=[])
refs = {}
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
last = len(next(r for r in rows if r['Name'] == 'Stage04')['Nodes']) - 1

def actors(w, cls): return unreal.GameplayStatics.get_all_actors_of_class(w, cls)
def hp(a): return a.get_component_by_class(unreal.HealthComponent)
def combat(a): return a.get_component_by_class(unreal.CombatTargetComponent)
def dist(a, b): return ((a.x-b.x)**2 + (a.y-b.y)**2)**.5

def finish(error=None):
    state.update(passed=error is None, error=error)
    (out/'runtime.json').write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
    if error: unreal.log_error(error)
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(dt):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle = unreal.register_slate_post_tick_callback(quit_tick)

def kill(c):
    combat(c).apply_combat_damage(100000., None)
    assert c.is_awaiting_respawn() and hp(c).get_is_dead()
    assert hp(c).get_current_health() == 0 and not combat(c).is_alive()
    assert not c.get_actor_enable_collision()
    assert 29.9 <= c.get_respawn_time_remaining() <= 30
    combat(c).apply_combat_damage(100000., None)
    c.move_to_location(c.get_actor_location()+unreal.Vector(800, 0, 0))
    assert c.get_component_by_class(unreal.CharacterMovementComponent).movement_mode == unreal.MovementMode.MOVE_NONE

def tick(dt):
    try:
        if time.time()-state['started'] > 210: raise RuntimeError('Timeout '+state['phase'])
        w = editor.get_game_world()
        if not w: return
        gs = unreal.GameplayStatics.get_game_state(w)
        if not isinstance(gs, unreal.SFGameState): return
        assert gs.get_stage_state() != unreal.SFStageState.INITIALIZATION_FAILED
        if gs.get_stage_state() == unreal.SFStageState.READY_TO_START:
            unreal.GameplayStatics.get_game_mode(w).start_stage()
            return
        intros = unreal.WidgetLibrary.get_all_widgets_of_class(w, unreal.SFStageIntroWidget, True)
        if intros:
            intros[0].start_stage_intro_at(table, 'Stage04', last)
            return
        if not gs.is_stage_playing(): return
        sec = unreal.GameplayStatics.get_time_seconds(w)
        if state['phase'] == 'start':
            pc = unreal.GameplayStatics.get_player_controller(w, 0)
            ally = pc.get_my_commander()
            assert ally
            for s in actors(w, unreal.UnitSpawner): s.set_spawning_enabled(False)
            for b in actors(w, unreal.SFBaseBuilding): combat(b).set_damage_multiplier(0.)
            for u in actors(w, unreal.UnitBase): u.command_hold_ground()
            unreal.SystemLibrary.execute_console_command(w, 'SF.Commander.SpawnRespawnFixture')
            enemy = next(c for c in actors(w, unreal.SFCommander) if c.actor_has_tag('RespawnFixture'))
            refs.update(ally=ally, enemy=enemy, pc=pc, positions=[ally.get_actor_location(), enemy.get_actor_location()])
            kill(ally)
            state.update(phase='stagger', death=sec)
        elif state['phase'] == 'stagger' and sec-state['death'] >= 3:
            kill(refs['enemy'])
            state.update(phase='countdown', enemy_death=sec)
        elif state['phase'] == 'countdown' and sec-state['enemy_death'] >= 2:
            hud = unreal.WidgetLibrary.get_all_widgets_of_class(w, unreal.SFInGameHUDWidget, True)[0]
            panel = hud.get_editor_property('commander_respawn_panel')
            text = hud.get_editor_property('commander_respawn_text')
            label = str(text.get_text())
            assert '아군 커맨더' in label and '적군' not in label and '\n' not in label, label
            assert panel.get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE
            for i, c in enumerate([refs['ally'], refs['enemy']]):
                assert dist(c.get_actor_location(), refs['positions'][i]) < 1
                assert c.is_awaiting_respawn() and hp(c).get_current_health() == 0
            refs.update(panel=panel, text=text)
            state['checks'].append(dict(both_dead=True, blocked_movement=True, hud=label))
            unreal.SystemLibrary.execute_console_command(w, 'Shot showui')
            refs['pause_remaining'] = refs['ally'].get_respawn_time_remaining()
            unreal.GameplayStatics.set_game_paused(w, True)
            state.update(phase='paused', pause_wall=time.time())
        elif state['phase'] == 'paused' and time.time()-state['pause_wall'] >= 2:
            assert abs(refs['ally'].get_respawn_time_remaining()-refs['pause_remaining']) < .05
            unreal.GameplayStatics.set_game_paused(w, False)
            state['checks'].append(dict(pause_freezes_respawn=True))
            state['phase'] = 'wait'
        elif state['phase'] == 'wait':
            if not refs['ally'].is_awaiting_respawn() and sec-state['death'] > 30.2 and refs['enemy'].is_awaiting_respawn():
                assert refs['panel'].get_visibility() == unreal.SlateVisibility.COLLAPSED
                refs['enemy_timer_hidden'] = True
            for key, death in [('ally', state['death']), ('enemy', state['enemy_death'])]:
                c = refs[key]
                if sec-death < 29.95: assert c.is_awaiting_respawn(), 'Early respawn '+key
                if c.is_awaiting_respawn(): continue
                if key+'_revived' in refs: continue
                assert 29.95 <= sec-death < 32, (key, sec-death)
                team = combat(c).get_team_index()
                base = gs.get_nexus(team).get_actor_location()
                other = gs.get_nexus(1-team).get_actor_location()
                pos = c.get_actor_location()
                assert 300 < dist(pos, base) < 2200, (key, str(pos), str(base))
                assert (pos.x-base.x)*(other.x-base.x)+(pos.y-base.y)*(other.y-base.y) > 0
                assert hp(c).get_current_health() == hp(c).get_max_health()
                assert combat(c).is_alive() and c.get_actor_enable_collision()
                assert c.get_respawn_time_remaining() == 0
                combat(c).apply_combat_damage(1., None)
                assert hp(c).get_current_health() == hp(c).get_max_health()-1
                refs[key+'_revived'] = True
                state['checks'].append(dict(team=team, respawn_seconds=sec-death, at_own_nexus=True, damage_after_respawn=True))
            if refs.get('ally_revived') and refs.get('enemy_revived'):
                assert refs['pc'].get_my_commander() == refs['ally']
                assert refs.get('enemy_timer_hidden'), 'Enemy-only countdown must be hidden'
                state['checks'].append(dict(enemy_timer_hidden=True))
                refs['move_starts'] = [c.get_actor_location() for c in [refs['ally'], refs['enemy']]]
                for c in [refs['ally'], refs['enemy']]:
                    c.move_to_location(c.get_actor_location()+c.get_actor_forward_vector()*250)
                state.update(phase='second', since=sec)
        elif state['phase'] == 'second' and sec-state['since'] > 2:
            assert refs['panel'].get_visibility() == unreal.SlateVisibility.COLLAPSED
            for i, c in enumerate([refs['ally'], refs['enemy']]):
                assert dist(c.get_actor_location(), refs['move_starts'][i]) > 10, 'Cannot move after respawn'
            state['checks'].append(dict(both_can_move_after_respawn=True))
            kill(refs['ally'])
            kill(refs['enemy'])
            state.update(phase='second_wait', since=sec)
        elif state['phase'] == 'second_wait' and sec-state['since'] > 30.5:
            for c in [refs['ally'], refs['enemy']]:
                assert not c.is_awaiting_respawn() and combat(c).is_alive()
            state['checks'].append(dict(repeated_death_and_respawn=True, hud_hides_after_revive=True))
            finish()
    except Exception:
        finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
def begin(dt):
    if time.time()-state['started'] < 20: return
    unreal.unregister_slate_post_tick_callback(begin_handle)
    level.editor_request_begin_play()
begin_handle = unreal.register_slate_post_tick_callback(begin)
