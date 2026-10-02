import unreal,time,traceback
from pathlib import Path
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_Hero')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
started=time.time()
def tick(delta):
 if time.time()-started<20:return
 unreal.unregister_slate_post_tick_callback(handle)
 try:
  path=Path(unreal.Paths.project_dir())/'Scripts/Editor/configure_stage5_encounters.py'
  exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'),dict(__file__=str(path),__name__='__main__'))
 except Exception:unreal.log_error(traceback.format_exc())
 unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)

