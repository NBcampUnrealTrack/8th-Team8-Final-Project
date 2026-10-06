"""Disposable PIE checks for portrait visibility, skill guides and cooldown behavior."""
import json
import time
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/TutorialPresentation'
ether = '-EtherCooldownQA' in unreal.SystemLibrary.get_command_line()
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_Hero' if ether else '/Game/Game/Stage/ProtoType')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(phase='starting', started=time.time(), checks=[], faction='Ether' if ether else 'Beast')

def probe(world, name):
    unreal.SystemLibrary.execute_console_command(world, 'SF.Presentation.Check ' + name)
    report = (out / (name + '.txt')).read_text(encoding='utf-8-sig')
    assert report.startswith('PASSED'), report
    state['checks'].append(name)
    return report

def phase(name):
    state.update(phase=name, at=time.time())

def finish(error=None):
    state['passed'] = error is None
    if error:
        state['error'] = error
        unreal.log_error(error)
    (out / ('ether_runtime_verified.json' if ether else 'runtime_verified.json')).write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(delta):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle = unreal.register_slate_post_tick_callback(quit_tick)

def tick(delta):
    try:
        if time.time() - state['started'] > 240: raise RuntimeError('Timed out ' + state['phase'])
        world = editor.get_game_world()
        if not world: return
        gs = unreal.GameplayStatics.get_game_state(world)
        if not isinstance(gs, unreal.SFGameState): return
        intros = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.SFStageIntroWidget, True)
        if state['phase'] == 'starting':
            if gs.get_stage_state() == unreal.SFStageState.READY_TO_START:
                assert unreal.GameplayStatics.get_game_mode(world).start_stage()
            if not gs.is_stage_playing() or not intros: return
            phase('narration')
        elif time.time() - state['at'] < 3:
            return
        elif state['phase'] == 'narration':
            probe(world, 'narration')
            phase('open_pair')
        elif state['phase'] == 'open_pair':
            assert intros[0].start_stage_intro_at(table, 'Stage04', 6)
            phase('portraits')
        elif state['phase'] == 'portraits':
            probe(world, 'portraits')
            phase('end_intro' if ether else 'guide')
        elif state['phase'] == 'guide':
            assert intros[0].start_stage_intro_at(table, 'Stage01', 47)
            phase('guide_capture')
        elif state['phase'] == 'guide_capture':
            unreal.SystemLibrary.execute_console_command(world, 'SF.Commander.CheckContent skill_tier_1')
            assert (root / 'Saved/CommanderContent/skill_tier_1.txt').read_text(encoding='utf-8-sig').startswith('PASSED')
            phase('advance_guide')
        elif state['phase'] == 'advance_guide':
            unreal.SystemLibrary.execute_console_command(world, 'SF.Commander.AdvanceGuide')
            unreal.SystemLibrary.execute_console_command(world, 'SF.Commander.AdvanceGuide')
            unreal.SystemLibrary.execute_console_command(world, 'SF.Commander.AdvanceGuide')
            phase('end_intro')
        elif state['phase'] == 'end_intro':
            assert intros[0].start_stage_intro_at(table, 'Stage05' if ether else 'Stage01', 6 if ether else 38)
            unreal.GameplayStatics.set_game_paused(world, False)
            phase('cast')
        elif state['phase'] == 'cast':
            probe(world, 'cast')
            state['cast_game_time'] = unreal.GameplayStatics.get_time_seconds(world)
            phase('recast')
        elif state['phase'] == 'recast':
            probe(world, 'recast')
            unreal.GameplayStatics.set_game_paused(world, True)
            state['paused_report'] = probe(world, 'paused')
            phase('paused')
        elif state['phase'] == 'paused':
            assert probe(world, 'paused') == state['paused_report'], 'Cooldown advanced while paused'
            unreal.GameplayStatics.set_game_paused(world, False)
            phase('expire')
        elif state['phase'] == 'expire':
            if unreal.GameplayStatics.get_time_seconds(world) - state['cast_game_time'] < 30.1: return
            probe(world, 'expired')
            phase('visual')
        elif state['phase'] == 'visual':
            probe(world, 'visual')
            unreal.GameplayStatics.set_game_paused(world, True)
            phase('finish')
        elif state['phase'] == 'finish':
            finish()
    except Exception:
        finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
def begin(delta):
    if time.time() - state['started'] < 15: return
    unreal.unregister_slate_post_tick_callback(begin_handle)
    level.editor_request_begin_play()
begin_handle = unreal.register_slate_post_tick_callback(begin)
