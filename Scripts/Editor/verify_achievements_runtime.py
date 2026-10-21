"""Disposable campaign slot: lobby interactions, responsive journal and real victory toast."""
import json
import time
import traceback
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_dir()) / 'output/qa/achievements-2026-10-21'
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Core/Levels/Lobby')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(start=time.time(), phase='start', checks=[])

def widgets(world, cls):
    return [w for w in unreal.WidgetLibrary.get_all_widgets_of_class(world, cls, False) if w.is_in_viewport()]

def check(value, label):
    state['checks'].append(dict(check=label, passed=bool(value)))
    if not value:
        raise AssertionError(label)

def command(world, text):
    unreal.SystemLibrary.execute_console_command(world, text)

def shot(world, name):
    command(world, 'Shot SHOWUI -nosuffix filename=' + str(out / (name + '.png')).replace('\\', '/'))

def phase(name):
    state.update(phase=name, since=time.time())

def finish(error=None):
    state['passed'] = error is None and all(c['passed'] for c in state['checks'])
    state['error'] = error
    (out / 'report.json').write_text(json.dumps(state, indent=2, ensure_ascii=False), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    def end(dt):
        unreal.unregister_slate_post_tick_callback(end_handle)
        unreal.SystemLibrary.quit_editor()
    global end_handle
    end_handle = unreal.register_slate_post_tick_callback(end)

def tick(dt):
    try:
        if time.time() - state['start'] > 200:
            raise RuntimeError('Timed out: ' + state['phase'])
        world = editor.get_game_world()
        if not world:
            return
        elapsed = time.time() - state.get('since', time.time())
        gi = unreal.GameplayStatics.get_game_instance(world)
        if state['phase'] == 'start':
            if not widgets(world, unreal.LobbyWidget):
                return
            check(gi.get_highest_cleared_stage_order() == 0, 'Isolated QA slot starts fresh')
            check(len(gi.get_pve_achievements()) == 7, 'Seven stage achievements')
            command(world, 'DisableAllScreenMessages')
            command(world, 'SF.Menu.PauseInput resize 1920 1080')
            phase('lobby')
        elif state['phase'] == 'lobby' and elapsed > 4:
            shot(world, 'lobby_1920')
            phase('open_journal')
        elif state['phase'] == 'open_journal' and elapsed > .5:
            command(world, 'SF.Menu.PauseInput click AchievementsButton')
            phase('journal')
        elif state['phase'] == 'journal' and elapsed > 1:
            check(len(widgets(world, unreal.SFAchievementsWidget)) == 1, 'Lobby mouse click opens journal')
            shot(world, 'journal_fresh_1920')
            phase('close_journal')
        elif state['phase'] == 'close_journal' and elapsed > .5:
            command(world, 'SF.Menu.PauseInput Escape')
            phase('closed')
        elif state['phase'] == 'closed' and elapsed > .5:
            check(not widgets(world, unreal.SFAchievementsWidget), 'Escape closes journal')
            for stage in range(1,4):
                gi.mark_stage_cleared('Stage%02d' % stage)
            command(world, 'SF.Menu.PauseInput click AchievementsButton')
            phase('partial')
        elif state['phase'] == 'partial' and elapsed > 1:
            check(sum(e.unlocked for e in gi.get_pve_achievements()) == 3, 'Three cleared stages unlock exactly three achievements')
            shot(world, 'journal_partial_1920')
            phase('resize_small')
        elif state['phase'] == 'resize_small' and elapsed > .5:
            command(world, 'SF.Menu.PauseInput resize 1280 720')
            phase('small')
        elif state['phase'] == 'small' and elapsed > 1:
            shot(world, 'journal_partial_1280')
            phase('open_preview')
        elif state['phase'] == 'open_preview' and elapsed > .5:
            command(world, 'SF.Menu.PauseInput Escape')
            command(world, 'SF.Achievements.Preview 7 1.2')
            phase('preview')
        elif state['phase'] == 'preview' and elapsed > 1:
            shot(world, 'toast_1280')
            phase('resize_large')
        elif state['phase'] == 'resize_large' and elapsed > .5:
            command(world, 'SF.Menu.PauseInput resize 1920 1080')
            phase('preview_large')
        elif state['phase'] == 'preview_large' and elapsed > 1:
            shot(world, 'toast_1920')
            phase('travel')
        elif state['phase'] == 'travel' and elapsed > .5:
            command(world, 'SF.Achievements.Preview clear')
            check(gi.select_stage('Stage04'), 'Select next campaign stage')
            check(gi.move_to_stage(), 'Travel to real PvE stage')
            phase('stage')
        elif state['phase'] == 'stage':
            gs = unreal.GameplayStatics.get_game_state(world)
            if not isinstance(gs, unreal.SFGameState):
                return
            if gs.get_stage_state() == unreal.SFStageState.READY_TO_START:
                for loading in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.LoadingWidget, False):
                    loading.finish_loading_screen()
            if not gs.is_stage_playing():
                return
            for obj in unreal.ObjectIterator(unreal.SFTutorialSubsystem):
                if obj.get_outer() == world:
                    obj.cancel_guide()
                    obj.cancel_objective_wait()
            for intro in unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.SFStageIntroWidget, False):
                intro.remove_from_parent()
            unreal.GameplayStatics.set_game_paused(world, False)
            check(unreal.GameplayStatics.get_game_mode(world).process_victory(), 'Real victory succeeds')
            check(gi.get_highest_cleared_stage_order() == 4, 'Victory saves achievement progress')
            phase('victory')
        elif state['phase'] == 'victory' and elapsed > 3:
            toasts = widgets(world, unreal.SFAchievementToastWidget)
            check(len(toasts) == 1, 'First victory shows exactly one toast')
            check(unreal.GameplayStatics.is_game_paused(world), 'Toast runs while result screen pauses game')
            check(toasts[0].get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE, 'Toast does not block input')
            shot(world, 'victory_toast')
            phase('expiry')
        elif state['phase'] == 'expiry' and elapsed > 4:
            check(not widgets(world, unreal.SFAchievementToastWidget), 'Toast expires using real time while paused')
            gi.mark_stage_cleared('Stage04')
            check(not widgets(world, unreal.SFAchievementToastWidget), 'Replay never repeats toast')
            gi.move_to_lobby()
            phase('return')
        elif state['phase'] == 'return' and elapsed > 5:
            check(bool(widgets(world, unreal.LobbyWidget)), 'Return to lobby')
            command(world, 'SF.Menu.PauseInput click AchievementsButton')
            phase('final')
        elif state['phase'] == 'final' and elapsed > 1:
            check(len(widgets(world, unreal.SFAchievementsWidget)) == 1, 'Journal reopens after victory')
            check(sum(e.unlocked for e in gi.get_pve_achievements()) == 4, 'Journal reflects new victory')
            shot(world, 'journal_after_victory')
            phase('done')
        elif state['phase'] == 'done' and elapsed > 1:
            finish()
    except Exception:
        finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
command(editor.get_editor_world(), 'SF.Menu.PauseStart')
