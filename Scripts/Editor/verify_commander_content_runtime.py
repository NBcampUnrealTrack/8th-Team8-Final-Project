"""Disposable Stage01 PIE: skill guide progression, live HUD assets, tier gates."""
import json
import time
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root/'Saved/CommanderContent'
table = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
guide_node = json.loads((out/'installed.json').read_text(encoding='utf-8'))['guide_node']
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/ProtoType')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(phase='starting', started=time.time(), checks=[])

def finish(error=None):
    state['passed'] = error is None
    if error:
        state['error'] = error
        unreal.log_error(error)
    (out/'runtime_verified.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(delta):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle = unreal.register_slate_post_tick_callback(quit_tick)

def tick(delta):
    try:
        if time.time()-state['started'] > 180: raise RuntimeError('Timed out '+state['phase'])
        world = editor.get_game_world()
        if not world: return
        gs = unreal.GameplayStatics.get_game_state(world)
        if not isinstance(gs,unreal.SFGameState): return
        intros = unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.SFStageIntroWidget,True)
        if state['phase']=='starting':
            if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
                assert unreal.GameplayStatics.get_game_mode(world).start_stage()
            if not gs.is_stage_playing() or not intros: return
            assert str(gs.get_current_stage_row_name())=='Stage01'
            assert intros[0].start_stage_intro_at(table,'Stage01',guide_node)
            state.update(phase='guide',at=time.time(),step=0)
        elif state['phase']=='guide' and time.time()-state['at']>3:
            step=state['step']
            name='skill_tier_'+str(step+1)
            unreal.SystemLibrary.execute_console_command(world,'SF.Commander.CheckContent '+name)
            report=(out/(name+'.txt')).read_text(encoding='utf-8-sig')
            assert report.startswith('PASSED'),report
            state['checks'].append(dict(step=step+1,probe=True))
            state.update(phase='advance',at=time.time())
        elif state['phase']=='advance' and time.time()-state['at']>1:
            if state['step']==0:
                # The guide is paused in gameplay, but its UI shimmer must continue animating.
                unreal.SystemLibrary.execute_console_command(world,'SF.Commander.CheckContent skill_pulse1')
                report=(out/'skill_pulse1.txt').read_text(encoding='utf-8-sig')
                first=(out/'skill_tier_1.txt').read_text(encoding='utf-8-sig')
                assert report.startswith('PASSED')
                assert next(line for line in report.splitlines() if line.startswith('edge_alpha')) != next(line for line in first.splitlines() if line.startswith('edge_alpha'))
                state['checks'].append(dict(skill_highlight_animates_while_guiding=True))
            unreal.SystemLibrary.execute_console_command(world,'SF.Commander.AdvanceGuide')
            state['step']+=1
            state.update(phase='returned' if state['step']==3 else 'guide',at=time.time())
        elif state['phase']=='returned' and time.time()-state['at']>2:
            assert len(intros)==1 and intros[0].is_activated()
            state['checks'].append(dict(resumed_original_tutorial=True))
            finish()
    except Exception:
        finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
def begin(delta):
    if time.time()-state['started']<15:return
    unreal.unregister_slate_post_tick_callback(begin_handle)
    level.editor_request_begin_play()
begin_handle=unreal.register_slate_post_tick_callback(begin)
