import unreal,time,traceback
from pathlib import Path
cmd=unreal.SystemLibrary.get_command_line()
case=5 if '-SlimeStage=5' in cmd else 6 if '-SlimeStage=6' in cmd else 2
maps={2:'PT_Stage_2',5:'PT_Stage_Hero',6:'PT_SFStageAmbush'}
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/'+maps[case])
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started=time.time()
def tick(dt):
    if time.time()-started<25:return
    unreal.unregister_slate_post_tick_callback(handle)
    try:
        p=Path(unreal.Paths.project_dir())/'Scripts/Editor/configure_neutral_slimes.py'
        exec(compile(p.read_text(),str(p),'exec'),dict(__file__=str(p),SLIME_STAGE=case))
    except Exception:unreal.log_error(traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
