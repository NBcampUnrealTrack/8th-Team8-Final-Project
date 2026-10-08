import unreal, time
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/NeutralSlimes'
out.mkdir(parents=True,exist_ok=True)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_2')
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
cam=sub.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(1300,4800,35000))
cam.set_actor_rotation(unreal.Rotator(pitch=-90,yaw=0),False)
c=cam.get_component_by_class(unreal.SceneCaptureComponent2D)
rt=unreal.RenderingLibrary.create_render_target2d(w,1600,1400,unreal.TextureRenderTargetFormat.RTF_RGBA8)
rt.set_editor_property('target_gamma',2.2)
c.set_editor_properties(dict(texture_target=rt,projection_type=unreal.CameraProjectionMode.ORTHOGRAPHIC,ortho_width=26000.,capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,capture_every_frame=False,capture_on_movement=False))
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.time(),captured=False)
def tick(dt):
    if time.time()-state['start']<20:return
    if not state['captured']:
        c.capture_scene();state.update(start=time.time()-17,captured=True);return
    unreal.RenderingLibrary.export_render_target(w,rt,str(out),'Stage02Overview.png')
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
