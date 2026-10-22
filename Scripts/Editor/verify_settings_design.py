"""Exercise the lobby's real settings modal in disposable standalone PIE."""
import json
import time
import traceback
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_dir()) / 'output/qa/settings-2026-10-19'
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Core/Levels/Lobby')
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

def input(world, text):
    command(world, 'SF.Menu.PauseInput ' + text)

def phase(name):
    state.update(phase=name, since=time.time())

def control(name):
    owner = refs['settings']
    for obj in unreal.ObjectIterator(unreal.Widget):
        outer = obj.get_outer()
        while outer and outer != owner:
            outer = outer.get_outer()
        if outer == owner and obj.get_name().lower() == name.lower():
            return obj
    raise RuntimeError('Missing settings control: ' + name)

def shot(world, name, inspect=True):
    if inspect:
        input(world, 'inspect ' + name)
        layout = (out / (name + '.txt')).read_text(encoding='utf-8-sig')
        check('FAIL' not in layout and layout.count('PASS') == 7, name + ': all seven controls fit')
    command(world, 'Shot SHOWUI -nosuffix filename=' + str(out / (name + '.png')).replace('\\', '/'))

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
        if time.time() - state['start'] > 120:
            raise RuntimeError('Timed out: ' + state['phase'])
        world = editor.get_game_world()
        if not world:
            return
        elapsed = time.time() - state.get('since', time.time())
        if state['phase'] == 'start':
            if not widgets(world, unreal.LobbyWidget):
                return
            command(world, 'DisableAllScreenMessages')
            input(world, 'resize 1600 900')
            phase('lobby')
        elif state['phase'] == 'lobby' and elapsed > 2:
            input(world, 'click Setting_Btn')
            phase('opened')
        elif state['phase'] == 'opened' and elapsed > 1:
            refs['settings'] = next((w for w in widgets(world, unreal.SettingWidget) if w.is_visible()), None)
            check(refs['settings'] is not None, 'Lobby settings button opens real modal')
            check(str(control('SettingsTitle').get_text()) == '설정', 'Native Korean title replaces old raster')
            shot(world, 'settings_1600x900')
            phase('hover')
        elif state['phase'] == 'hover' and elapsed > 1:
            input(world, 'hover MasterVolumeSlider')
            phase('hover_capture')
        elif state['phase'] == 'hover_capture' and elapsed > .6:
            shot(world, 'settings_hover')
            phase('dropdown')
        elif state['phase'] == 'dropdown' and elapsed > 1:
            input(world, 'click FullScreenComboBox')
            phase('dropdown_open')
        elif state['phase'] == 'dropdown_open' and elapsed > 1:
            check(control('FullScreenComboBox').is_open(), 'Screen mode dropdown opens through pointer input')
            shot(world, 'screen_mode_dropdown', False)
            input(world, 'Escape')
            phase('slider')
        elif state['phase'] == 'slider' and elapsed > 1:
            check(refs['settings'].is_visible(), 'Escape dismisses dropdown without dismissing settings')
            refs['master_before'] = control('MasterVolumeSlider').get_value()
            input(world, 'click MasterVolumeSlider 0.72')
            phase('slider_changed')
        elif state['phase'] == 'slider_changed' and elapsed > 1:
            value = control('MasterVolumeSlider').get_value()
            check(abs(value - refs['master_before']) > .01, 'Pointer input changes master volume')
            check(str(control('MasterVolumeLabelValue').get_text()) == str(round(value * 100)) + '%', 'Volume readout follows slider')
            input(world, 'click MouseSensitivitySlider 0.45')
            phase('sensitivity')
        elif state['phase'] == 'sensitivity' and elapsed > 1:
            value = control('MouseSensitivitySlider').get_value()
            check(str(control('SensitivityLabelValue').get_text()) == f'{value:.2f}x', 'Sensitivity readout follows slider')
            input(world, 'resize 1280 720')
            phase('small')
        elif state['phase'] == 'small' and elapsed > 2:
            shot(world, 'settings_1280x720')
            phase('large_resize')
        elif state['phase'] == 'large_resize' and elapsed > 1:
            input(world, 'resize 1920 1080')
            phase('large')
        elif state['phase'] == 'large' and elapsed > 2:
            shot(world, 'settings_1920x1080')
            phase('back')
        elif state['phase'] == 'back' and elapsed > 1:
            input(world, 'click BackUi_Btn')
            phase('closed')
        elif state['phase'] == 'closed' and elapsed > 1:
            check(not refs['settings'].is_visible(), 'Full back button closes modal')
            input(world, 'click Setting_Btn')
            phase('reopened')
        elif state['phase'] == 'reopened' and elapsed > 1:
            check(refs['settings'].is_visible(), 'Lobby can reopen settings')
            input(world, 'Escape')
            phase('escaped')
        elif state['phase'] == 'escaped' and elapsed > 1:
            check(not refs['settings'].is_visible(), 'Escape closes settings through Slate focus')
            finish()
    except Exception:
        finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'SF.Menu.PauseStart')
