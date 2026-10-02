"""Render the saved Stage 05 without persisting preview/camera changes."""
import time
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'output/Stage05';OUT.mkdir(parents=True,exist_ok=True)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_Hero')
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in sub.get_all_level_actors():
 if isinstance(a,unreal.SFBuildingSpawnAnchor):
  a.get_editor_property('preview_mesh_component').set_hidden_in_game(False)
  a.get_editor_property('grid_preview_component').set_visibility(False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
camera=sub.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(0,0,40000));capture=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
target=unreal.RenderingLibrary.create_render_target2d(world,1920,1280,unreal.TextureRenderTargetFormat.RTF_RGBA8);target.set_editor_property('target_gamma',2.2)
for k,v in dict(texture_target=target,capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,capture_every_frame=False,capture_on_movement=False,fov_angle=65.0).items():capture.set_editor_property(k,v)
unreal.SystemLibrary.execute_console_command(world,'r.TextureStreaming 0')
shots=[('Overview',0,0,40000,True),('LowerOutpost',-8050,1500,3800,False),('WestOutpost',-850,-12300,3800,False),('UpperOutpost',2500,12300,4200,False)]
state=dict(start=time.time(),phase='wait',index=0)
def tick(delta):
 if state['phase']=='wait' and time.time()-state['start']>25:
  name,x,y,z,ortho=shots[state['index']]
  camera.set_actor_location(unreal.Vector(x if ortho else x-2500,y,z),False,False)
  camera.set_actor_rotation(unreal.Rotator(pitch=-90,yaw=0,roll=0) if ortho else unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(x,y,0)),False)
  capture.set_editor_property('projection_type',unreal.CameraProjectionMode.ORTHOGRAPHIC if ortho else unreal.CameraProjectionMode.PERSPECTIVE)
  capture.set_editor_property('ortho_width',30000.0)
  capture.capture_scene();state.update(phase='export',start=time.time())
 elif state['phase']=='export' and time.time()-state['start']>2:
  unreal.RenderingLibrary.export_render_target(world,target,str(OUT),shots[state['index']][0]+'.png')
  state['index']+=1
  if state['index']==len(shots):
   unreal.log('STAGE05_CAPTURED');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
  else:state.update(phase='wait',start=time.time()-23)
handle=unreal.register_slate_post_tick_callback(tick)
