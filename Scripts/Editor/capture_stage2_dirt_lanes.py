import unreal,time,json,traceback,hashlib,math
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Stage02Routes/DirtRoads'
out.mkdir(parents=True,exist_ok=True)
mapfile=Path(unreal.Paths.project_dir())/'Content/Game/Stage/PT_Stage_2.umap'
meta={'map':str(mapfile),'map_sha256':hashlib.sha256(mapfile.read_bytes()).hexdigest(),'camera':[1300,4800,35000],'rotation':[-90,0,0],'ortho_width':26000,'resolution':[1600,1400]}
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_2')
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=es.get_all_level_actors()
lanes=[a for a in actors if isinstance(a,unreal.LanePath)]
assert lanes,'No LanePath actors in current Stage 2'
meta['lanes']=[]
for a in lanes:
 spline=a.get_component_by_class(unreal.SplineComponent)
 meta['lanes'].append({'label':a.get_actor_label(),'length':spline.get_spline_length(),'half_width':a.get_lane_half_width(),'points':[[spline.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD).x,spline.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD).y,spline.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD).z] for i in range(spline.get_number_of_spline_points())]})
cam=es.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(1300,4800,35000))
cam.set_actor_rotation(unreal.Rotator(pitch=-90,yaw=0),False)
c=cam.get_component_by_class(unreal.SceneCaptureComponent2D)
rt=unreal.RenderingLibrary.create_render_target2d(world,1600,1400,unreal.TextureRenderTargetFormat.RTF_RGBA8)
rt.set_editor_property('target_gamma',2.2)
c.set_editor_properties(dict(texture_target=rt,projection_type=unreal.CameraProjectionMode.ORTHOGRAPHIC,ortho_width=26000.,capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,capture_every_frame=False,capture_on_movement=False))
for cmd in ['r.TextureStreaming 0','r.ScreenPercentage 100']:
 unreal.SystemLibrary.execute_console_command(world,cmd)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state={'start':time.time(),'phase':'warmup'}
def draw_lanes():
 for a in lanes:
  sp=a.get_component_by_class(unreal.SplineComponent);length=sp.get_spline_length();prev=None
  for i in range(math.ceil(length/150)+1):
   dist=min(length,i*150)
   pos=sp.get_location_at_distance_along_spline(dist,unreal.SplineCoordinateSpace.WORLD)
   right=sp.get_right_vector_at_distance_along_spline(dist,unreal.SplineCoordinateSpace.WORLD)
   norm=max(.0001,math.hypot(right.x,right.y));width=a.get_lane_half_width()
   center=unreal.Vector(pos.x,pos.y,max(pos.z+150,2200))
   l=unreal.Vector(pos.x-right.x/norm*width,pos.y-right.y/norm*width,max(pos.z+150,2200))
   r=unreal.Vector(pos.x+right.x/norm*width,pos.y+right.y/norm*width,max(pos.z+150,2200))
   now=(center,l,r)
   if prev:
    for j,color,thick in [(0,unreal.LinearColor(1,1,0,1),45.),(1,unreal.LinearColor(0,1,1,1),20.),(2,unreal.LinearColor(0,1,1,1),20.)]:
     unreal.SystemLibrary.draw_debug_line(world,prev[j],now[j],color,60.,thick)
   prev=now

def tick(dt):
 try:
  t=time.time()-state['start']
  if state['phase']=='warmup' and t>30:
   draw_lanes();state.update(phase='draw',start=time.time())
  elif state['phase']=='draw' and t>2:
   c.capture_scene();state.update(phase='export',start=time.time())
  elif state['phase']=='export' and t>4:
   unreal.RenderingLibrary.export_render_target(world,rt,str(out),'Stage02_Dirt_LanePaths.png')
   meta['capture_time']=time.strftime('%Y-%m-%d %H:%M:%S');meta['map_sha256_after']=hashlib.sha256(mapfile.read_bytes()).hexdigest()
   (out/'capture_metadata.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf8')
   es.destroy_actor(cam);unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
 except Exception:
  (out/'capture_error.txt').write_text(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)


