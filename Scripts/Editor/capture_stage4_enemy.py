"""Capture the saved Beast base in the editor without saving preview changes."""
import time
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir())
OUT=ROOT/'output/Stage04'
OUT.mkdir(parents=True,exist_ok=True)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_4')
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in sub.get_all_level_actors():
    if isinstance(a,unreal.SFBuildingSpawnAnchor):
        a.get_editor_property('preview_mesh_component').set_hidden_in_game(False)
        a.get_editor_property('grid_preview_component').set_visibility(False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
camera=sub.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(300,4800,6700))
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(300,10100,100)),False)
capture=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
target=unreal.RenderingLibrary.create_render_target2d(world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8)
target.set_editor_property('target_gamma',2.2)
capture.set_editor_property('texture_target',target)
capture.set_editor_property('projection_type',unreal.CameraProjectionMode.PERSPECTIVE)
capture.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture.set_editor_property('capture_every_frame',False)
capture.set_editor_property('capture_on_movement',False)
capture.set_editor_property('fov_angle',70.0)
unreal.SystemLibrary.execute_console_command(world,'r.TextureStreaming 0')
state=dict(start=time.time(),phase='wait')
def tick(delta):
    if state['phase']=='wait' and time.time()-state['start']>25:
        capture.capture_scene()
        state.update(phase='export',start=time.time())
    elif state['phase']=='export' and time.time()-state['start']>2:
        unreal.RenderingLibrary.export_render_target(world,target,str(OUT),'BeastBase.png')
        unreal.log('STAGE04_BASE_CAPTURED: '+str(OUT/'BeastBase.png'))
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
