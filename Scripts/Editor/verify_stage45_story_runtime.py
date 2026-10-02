"""Verify opening stories and deferred victory in disposable PIE (no asset saves)."""
import json
import time
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/Stage45Story'
source = json.loads((root / 'ContentSource/Stage45Story_KO.json').read_text(encoding='utf-8'))
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_Hero')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(phase='starting', started=time.time(), checks=[])


def widgets(world, cls):
    return unreal.WidgetLibrary.get_all_widgets_of_class(world, cls, True)


def finish(error=None):
    state.update(passed=error is None, phase='failed' if error else 'passed')
    if error:
        state['error'] = error
        unreal.log_error('STAGE45_STORY_RUNTIME_FAILED: ' + error)
    else:
        unreal.log('STAGE45_STORY_RUNTIME_VERIFIED: opening stories and epilogue before victory')
    (out / 'runtime_verified.json').write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(delta):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle = unreal.register_slate_post_tick_callback(quit_tick)


def tick(delta):
    try:
        if time.time() - state['started'] > 180:
            raise RuntimeError('Timed out: ' + state['phase'])
        world = editor.get_game_world()
        if not world:
            return
        gs = unreal.GameplayStatics.get_game_state(world)
        if not isinstance(gs, unreal.SFGameState):
            return
        if state['phase'] == 'starting':
            assert gs.get_stage_state() != unreal.SFStageState.INITIALIZATION_FAILED
            if gs.get_stage_state() == unreal.SFStageState.READY_TO_START:
                assert unreal.GameplayStatics.get_game_mode(world).start_stage()
            intros = widgets(world, unreal.SFStageIntroWidget)
            huds = widgets(world, unreal.SFInGameHUDWidget)
            if not gs.is_stage_playing() or not intros or not huds:
                return
            assert str(gs.get_current_stage_row_name()) == 'Stage05'
            intro = intros[0]
            assert intro.is_activated()
            assert unreal.GameplayStatics.is_game_paused(world)
            # Use the real widget's normal End path, including pause/HUD restoration.
            intro.start_stage_intro_at(table, 'Stage05', len(source['Stage05']))
            assert not unreal.GameplayStatics.is_game_paused(world)
            state['checks'].append(dict(stage05_auto_open=True, pause_restored=True))
            # Campaign progress lives only in this disposable PIE GameInstance.
            assert unreal.GameplayStatics.get_game_mode(world).process_victory()
            state['phase'] = 'epilogue'
        elif state['phase'] == 'epilogue':
            epilogues = widgets(world, unreal.SFStageIntroWidget)
            assert len(epilogues) == 1
            epilogue = epilogues[0]
            assert epilogue.is_activated()
            assert not widgets(world, unreal.ResultWidget)
            assert unreal.GameplayStatics.is_game_paused(world)
            assert epilogue.start_stage_intro_at(table, 'Stage05Clear', len(source['Stage05Clear']))
            state['checks'].append(dict(epilogue_before_victory=True))
            state['phase'] = 'result'
        elif state['phase'] == 'result':
            assert not widgets(world, unreal.SFStageIntroWidget)
            results = widgets(world, unreal.ResultWidget)
            assert len(results) == 1 and results[0].is_in_viewport()
            state['checks'].append(dict(single_victory_screen_after_epilogue=True))
            finish()
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)
started = time.time()


def begin(delta):
    if time.time() - started < 15:
        return
    unreal.unregister_slate_post_tick_callback(begin_handle)
    level.editor_request_begin_play()


begin_handle = unreal.register_slate_post_tick_callback(begin)
