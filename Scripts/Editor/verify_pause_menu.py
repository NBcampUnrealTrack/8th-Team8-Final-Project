"""Disposable PIE visual / interaction check for the real F10 menu. Saves no game assets."""
import json
import time
import traceback
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_dir()) / 'output/qa/pause-menu-2026-10-19'
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_2')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(start=time.time(), phase='start', checks=[])
refs = {}

def widgets(world, cls):
    return unreal.WidgetLibrary.get_all_widgets_of_class(world, cls, False)

def check(condition, label):
    state['checks'].append(dict(check=label, passed=bool(condition)))
    if not condition:
        raise AssertionError(label)

def command(world, text):
    unreal.SystemLibrary.execute_console_command(world, text)

def shot(world, name):
    active = menu(world)
    if active:
        command(world, 'SF.Menu.PauseInput inspect ' + name)
        options = next((w for w in widgets(world, unreal.SettingWidget) if w.is_visible()), None)
        layout_dir = out.parent / 'settings-2026-10-19' if options else out
        layout = (layout_dir / (name + '.txt')).read_text(encoding='utf-8-sig')
        state.setdefault('geometry', []).append(dict(frame=name, layout=layout))
        state['checks'].append(dict(check=name + ': all buttons fit viewport', passed='FAIL' not in layout))
    command(world, 'Shot SHOWUI -nosuffix filename=' + str(out / (name + '.png')).replace('\\', '/'))

def phase(name):
    state.update(phase=name, since=time.time())

def menu(world):
    return next((w for w in widgets(world, unreal.SFSettingsWidget) if w.is_activated()), None)

def finish(error=None):
    state['passed'] = error is None and all(c['passed'] for c in state['checks'])
    state['error'] = error
    (out / 'report.json').write_text(json.dumps(state, indent=2, ensure_ascii=False, default=str), encoding='utf-8')
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
        elapsed = time.time() - state.get('since', time.time())
        if state['phase'] == 'start':
            gs = unreal.GameplayStatics.get_game_state(world)
            if not isinstance(gs, unreal.SFGameState):
                return
            if gs.get_stage_state() == unreal.SFStageState.READY_TO_START:
                for loading in widgets(world, unreal.LoadingWidget):
                    loading.finish_loading_screen()
            if not gs.is_stage_playing():
                return
            huds = widgets(world, unreal.SFInGameHUDWidget)
            if not huds:
                return
            for intro in widgets(world, unreal.SFStageIntroWidget):
                intro.start_stage_intro_at(unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro'), 'Stage02', 32)
            for obj in unreal.ObjectIterator(unreal.SFTutorialSubsystem):
                if obj.get_outer() == world:
                    obj.cancel_guide()
                    obj.cancel_objective_wait()
            unreal.GameplayStatics.set_game_paused(world, False)
            refs['hud'] = huds[0]
            command(world, 'DisableAllScreenMessages')
            command(world, 'SF.HUD.QA resize 1600 900')
            phase('open')
        elif state['phase'] == 'open' and elapsed > 2:
            refs['hud'].toggle_settings()
            phase('capture')
        elif state['phase'] == 'capture' and elapsed > 2:
            active = menu(world)
            check(active is not None, 'F10 HUD stack activates redesigned menu')
            check(unreal.GameplayStatics.is_game_paused(world), 'Opening menu pauses gameplay')
            refs['menu'] = active
            state['widgets'] = []
            for obj in unreal.ObjectIterator(unreal.Widget):
                outer = obj.get_outer()
                while outer and outer != active:
                    outer = outer.get_outer()
                if outer == active:
                    state['widgets'].append(dict(name=obj.get_name(), type=obj.get_class().get_name()))
            shot(world, 'menu_1600x900')
            phase('settings')
        elif state['phase'] == 'settings' and elapsed > 1:
            command(world, 'SF.Menu.PauseInput click SettingButton')
            phase('settings_open')
        elif state['phase'] == 'settings_open' and elapsed > 1:
            options = next((w for w in widgets(world, unreal.SettingWidget) if w.get_name() == 'SettingContent'), None)
            refs['options'] = options
            check(options and options.is_visible(), 'Settings button opens existing options controls')
            check(unreal.GameplayStatics.is_game_paused(world), 'Options keep gameplay paused')
            shot(world, 'settings_open')
            command(world, 'SF.Menu.PauseInput click BackUi_Btn')
            phase('settings_closed')
        elif state['phase'] == 'settings_closed' and elapsed > 1:
            check(not refs['options'].is_visible(), 'Options back button returns to menu')
            check(menu(world) is not None, 'Menu stays open after closing options')
            phase('continue')
        elif state['phase'] == 'continue' and elapsed > 1:
            command(world, 'SF.Menu.PauseInput click ContinueButton')
            phase('continued')
        elif state['phase'] == 'continued' and elapsed > 1:
            check(menu(world) is None, 'Routed mouse click on Continue closes menu')
            check(not unreal.GameplayStatics.is_game_paused(world), 'Continue resumes gameplay')
            refs['hud'].toggle_settings()
            phase('reopened')
        elif state['phase'] == 'reopened' and elapsed > 1:
            check(menu(world) is not None, 'Menu reopens correctly from CommonUI pool')
            command(world, 'SF.HUD.QA resize 1280 720')
            phase('small')
        elif state['phase'] == 'small' and elapsed > 2:
            shot(world, 'menu_1280x720')
            phase('large_resize')
        elif state['phase'] == 'large_resize' and elapsed > 1:
            command(world, 'SF.HUD.QA resize 1920 1080')
            phase('large')
        elif state['phase'] == 'large' and elapsed > 2:
            shot(world, 'menu_1920x1080')
            phase('escape')
        elif state['phase'] == 'escape' and elapsed > 1:
            command(world, 'SF.Menu.PauseInput Escape')
            phase('escaped')
        elif state['phase'] == 'escaped' and elapsed > 1:
            check(menu(world) is None, 'Escape closes menu through Slate focus routing')
            check(not unreal.GameplayStatics.is_game_paused(world), 'Escape resumes gameplay')
            refs['hud'].toggle_settings()
            phase('f10')
        elif state['phase'] == 'f10' and elapsed > 1:
            command(world, 'SF.Menu.PauseInput F10')
            phase('f10_closed')
        elif state['phase'] == 'f10_closed' and elapsed > 1:
            check(menu(world) is None, 'F10 closes menu through Slate focus routing')
            check(not unreal.GameplayStatics.is_game_paused(world), 'F10 resumes gameplay')
            refs['hud'].toggle_settings()
            phase('hover_primary')
        elif state['phase'] == 'hover_primary' and elapsed > 1:
            command(world, 'SF.Menu.PauseInput hover ContinueButton')
            phase('hover_capture')
        elif state['phase'] == 'hover_capture' and elapsed > .6:
            shot(world, 'menu_hover')
            phase('hover')
        elif state['phase'] == 'hover' and elapsed > 1:
            # A real pointer down/up is routed by the existing QA command.
            command(world, 'SF.Menu.PauseInput click ExitButton')
            phase('lobby')
        elif state['phase'] == 'lobby' and elapsed > 5:
            check('Lobby' in world.get_name(), 'Routed mouse click on Exit travels to Lobby')
            shot(world, 'returned_to_lobby')
            phase('done')
        elif state['phase'] == 'done' and elapsed > 1:
            finish()
    except Exception:
        finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'SF.Menu.PauseStart')
