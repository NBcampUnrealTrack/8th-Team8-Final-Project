"""Use -SFCampaignSaveSlot=SFCampaignRuntimeQA, first -SFCampaignQA=seed then =reload.
Uses a disposable PIE session and saves no game assets or real player progress.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

if '-SFCampaignSaveSlot=SFCampaignRuntimeQA' not in unreal.SystemLibrary.get_command_line():
    raise RuntimeError('A separate -SFCampaignSaveSlot=SFCampaignRuntimeQA is required')

out = Path(unreal.Paths.project_dir()) / 'output/qa/campaign-save-2026-10-21'
out.mkdir(parents=True, exist_ok=True)
reload_run = '-SFCampaignQA=reload' in unreal.SystemLibrary.get_command_line()
run = 'reload' if reload_run else 'seed'
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Core/Levels/Lobby')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(start=time.time(), phase='lobby', since=time.time(), checks=[])


def check(condition, label):
    state['checks'].append(dict(check=label, passed=bool(condition)))
    if not condition:
        raise AssertionError(label)


def command(world, text):
    unreal.SystemLibrary.execute_console_command(world, text)


def widgets(world, cls):
    return unreal.WidgetLibrary.get_all_widgets_of_class(world, cls, False)


def phase(name):
    state.update(phase=name, since=time.time())


def shot(world, name):
    command(world, 'Shot SHOWUI -nosuffix filename=' + str(out / (name + '.png')).replace('\\', '/'))


def finish(error=None):
    state['passed'] = error is None and all(c['passed'] for c in state['checks'])
    state['error'] = error
    (out / (run + '_report.json')).write_text(json.dumps(state, indent=2, ensure_ascii=False), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()

    def end(dt):
        unreal.unregister_slate_post_tick_callback(end_handle)
        unreal.SystemLibrary.quit_editor()

    global end_handle
    end_handle = unreal.register_slate_post_tick_callback(end)


def tick(dt):
    try:
        if time.time() - state['start'] > 150:
            raise RuntimeError('Timed out: ' + state['phase'])
        world = editor.get_game_world()
        if not world:
            return
        age = time.time() - state['since']
        gi = unreal.GameplayStatics.get_game_instance(world)
        if state['phase'] == 'lobby' and age > 6:
            lobbies = widgets(world, unreal.LobbyWidget)
            if not lobbies:
                return
            expected = 1 if reload_run else 0
            check(gi.get_highest_cleared_stage_order() == expected, 'Startup restored the expected campaign progress')
            check(str(gi.get_next_campaign_stage_row_name()) == ('Stage02' if reload_run else 'Stage01'), 'Continue points to the first uncleared stage')
            check(gi.get_stage_progress_state('Stage03') == unreal.SFStageProgressState.LOCKED, 'Later stages remain locked')
            command(world, 'SF.Menu.PauseInput resize 1600 900')
            phase('capture_lobby')
        elif state['phase'] == 'capture_lobby' and age > 2:
            shot(world, run + '_lobby_1600x900')
            phase('click_continue')
        elif state['phase'] == 'click_continue' and age > 1:
            command(world, 'SF.Menu.PauseInput click ContinueCampaignButton')
            phase('stage')
        elif state['phase'] == 'stage':
            gs = unreal.GameplayStatics.get_game_state(world)
            if not isinstance(gs, unreal.SFGameState):
                return
            if gs.get_stage_state() == unreal.SFStageState.READY_TO_START:
                for loading in widgets(world, unreal.LoadingWidget):
                    loading.finish_loading_screen()
            if not gs.is_stage_playing():
                return
            check(str(gi.get_selected_stage_row_name()) == ('Stage02' if reload_run else 'Stage01'), 'A real Continue click travels to the expected stage')
            if reload_run:
                check(gi.get_highest_cleared_stage_order() == 1, 'Entering the next stage does not count as a clear')
                finish()
                return
            mode = unreal.GameplayStatics.get_game_mode(world)
            check(mode.process_victory(), 'The normal PvE victory path succeeds')
            check(gi.get_highest_cleared_stage_order() == 1, 'Victory immediately records Stage01')
            check(unreal.GameplayStatics.does_save_game_exist('SFCampaignRuntimeQA', 0), 'Victory writes a save before leaving the stage')
            gi.move_to_lobby()
            phase('after_victory')
        elif state['phase'] == 'after_victory' and age > 6:
            if not widgets(world, unreal.LobbyWidget):
                return
            check(gi.get_stage_progress_state('Stage01') == unreal.SFStageProgressState.CLEARED, 'The cleared stage is available for replay')
            check(gi.get_stage_progress_state('Stage02') == unreal.SFStageProgressState.UNLOCKED, 'Victory unlocks the next stage')
            command(world, 'SF.Menu.PauseInput resize 1280 720')
            phase('small')
        elif state['phase'] == 'small' and age > 2:
            shot(world, 'after_victory_lobby_1280x720')
            phase('done')
        elif state['phase'] == 'done' and age > 1:
            finish()
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)
command(editor.get_editor_world(), 'SF.Menu.PauseStart')
