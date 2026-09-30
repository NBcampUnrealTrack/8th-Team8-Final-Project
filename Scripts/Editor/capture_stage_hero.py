"""Render actual Editor views after shader/streaming warmup; no PIE spawning."""
import time
from pathlib import Path
import unreal

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/Stage_Hero')
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.EditorLevelLibrary.get_editor_world()
unreal.SystemLibrary.execute_console_command(world,'RebuildNavigation')
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
verify=Path(__file__).with_name('verify_stage_hero.py')
exec(compile(verify.read_text(encoding='utf-8'),str(verify),'exec'),{'__file__':str(verify)})
world=unreal.EditorLevelLibrary.get_editor_world()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
for cmd in ['r.ShaderPipelineCache.Enabled 0','r.TextureStreaming 0','r.ScreenPercentage 100','r.AntiAliasingMethod 2']:
    unreal.SystemLibrary.execute_console_command(world,cmd)
folder=Path(unreal.Paths.project_saved_dir(),'StageHeroPreviews')
folder.mkdir(exist_ok=True)
shots=[('Overview',(0,-28500,44000),(0,0,0),58),
       ('ForestLane',(-7500,12700,5200),(-4500,5200,0),65)]
for name,_,_,_ in shots:
    (folder/(name+'.png')).unlink(missing_ok=True)
camera=sub.spawn_actor_from_class(unreal.CameraActor,unreal.Vector(0,0,1000))
camera.set_actor_label('Hero_PreviewCamera_Temporary')
state={'index':0,'start':time.time(),'task':None,'phase':'prepare'}
def setup():
    name,loc,target,fov=shots[state['index']]
    v=unreal.Vector(*loc)
    rot=unreal.MathLibrary.find_look_at_rotation(v,unreal.Vector(*target))
    camera.set_actor_location_and_rotation(v,rot,False,False)
    camera.camera_component.set_editor_property('field_of_view',fov)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(v,rot)
    state['start']=time.time()
setup()
def tick(delta):
    try:
        if state['phase']=='prepare' and time.time()-state['start']>20:
            state['task']=unreal.AutomationLibrary.take_high_res_screenshot(1920,1440,str(folder/(shots[state['index']][0]+'.png')),camera=camera,delay=2.0)
            state['phase']='capture'
            state['start']=time.time()
        elif state['phase']=='capture' and time.time()-state['start']>12:
            if not (folder/(shots[state['index']][0]+'.png')).exists():
                if time.time()-state['start']<90:return
                raise RuntimeError('Screenshot did not finish')
            state['index']+=1
            if state['index']==len(shots):
                sub.destroy_actor(camera)
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.SystemLibrary.quit_editor()
            else:
                state['phase']='prepare'
                setup()
    except Exception as exc:
        (folder/'capture_error.txt').write_text(str(exc))
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
