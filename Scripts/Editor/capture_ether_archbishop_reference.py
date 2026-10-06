"""Render the existing in-game commander as an identity reference; saves no level."""
import time
import json
from pathlib import Path
import unreal
out=Path(unreal.Paths.project_saved_dir())/'ArchbishopPortraits'
out.mkdir(parents=True,exist_ok=True)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Engine/Maps/Entry')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for existing in actors.get_all_level_actors(): existing.set_actor_hidden_in_game(True)
actor=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(),unreal.Rotator(yaw=-90))
mesh=actor.get_component_by_class(unreal.SkeletalMeshComponent)
mesh.set_skeletal_mesh_asset(unreal.load_asset('/Game/Game/Race_Etherial/Character/Unit_Commander/Meshes/HumanCommander'))
mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
mesh.set_animation(unreal.load_asset('/Game/Game/Race_Etherial/Character/Unit_Commander/Anim/AS_EtherCommanderIdle'))
mesh.set_position(0.,False)
origin,extent=actor.get_actor_bounds(False)
(out/'bounds.json').write_text(json.dumps(dict(origin=str(origin),extent=str(extent))),encoding='utf-8')
scale=200./max(1.,extent.z*2.)
actor.set_actor_scale3d(unreal.Vector(scale,scale,scale))
actor.set_actor_location(unreal.Vector(-origin.x*scale,-origin.y*scale,-origin.z*scale),False,True)
for pos,power in [(unreal.Vector(230,-160,220),20.),(unreal.Vector(120,170,180),15.)]:
    light=actors.spawn_actor_from_class(unreal.PointLight,pos)
    component=light.get_component_by_class(unreal.PointLightComponent)
    component.set_intensity(power)
    component.set_attenuation_radius(2000.)
camera=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(450,0,20),unreal.Rotator(yaw=180))
capture=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
target=unreal.RenderingLibrary.create_render_target2d(world,1024,1024,unreal.TextureRenderTargetFormat.RTF_RGBA8)
target.set_editor_property('target_gamma',2.2)
for key,value in dict(texture_target=target,projection_type=unreal.CameraProjectionMode.PERSPECTIVE,fov_angle=48.,capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,capture_every_frame=False,capture_on_movement=False).items():
    capture.set_editor_property(key,value)
capture.set_editor_property('primitive_render_mode',unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
capture.show_only_actor_components(actor,False)
pp=unreal.PostProcessSettings()
pp.set_editor_property('override_auto_exposure_method',True)
pp.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
pp.set_editor_property('override_auto_exposure_bias',True)
pp.set_editor_property('auto_exposure_bias',0.)
capture.set_editor_property('post_process_settings',pp)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.time(),phase='wait')
def tick(delta):
    if state['phase']=='wait' and time.time()-state['start']>15:
        capture.capture_scene()
        state.update(phase='save',start=time.time())
    elif state['phase']=='save' and time.time()-state['start']>2:
        unreal.RenderingLibrary.export_render_target(world,target,str(out),'in_game_archbishop.png')
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
