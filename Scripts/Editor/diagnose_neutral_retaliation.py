import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'ReleaseFeedback'
ue=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_2')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.time(),samples=[],phase='start');refs={}
def finish(error=None):
 state['error']=error;(out/'neutral_runtime_before.json').write_text(json.dumps(state,indent=2,default=str),encoding='utf-8')
 unreal.unregister_slate_post_tick_callback(handle)
 level.editor_request_end_play()
 def end(dt):
  unreal.unregister_slate_post_tick_callback(end_handle);unreal.SystemLibrary.quit_editor()
 global end_handle
 end_handle=unreal.register_slate_post_tick_callback(end)
def tick(dt):
 try:
  if time.time()-state['start']>100:raise Exception('timeout '+state['phase'])
  w=ue.get_game_world()
  if not w:return
  gs=unreal.GameplayStatics.get_game_state(w)
  if not isinstance(gs,unreal.SFGameState):return
  if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:unreal.GameplayStatics.get_game_mode(w).start_stage()
  if not gs.is_stage_playing():return
  if state['phase']=='start':
   for intro in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFStageIntroWidget,False):intro.deactivate_widget()
   for obj in unreal.ObjectIterator(unreal.SFTutorialSubsystem):
    if obj.get_outer()==w:obj.cancel_guide();obj.cancel_objective_wait()
   unreal.GameplayStatics.set_game_paused(w,False)
   slimes=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SFNeutralSlime)
   assert slimes
   s=slimes[0];p=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(gs.get_nexus(0),'Brawler')
   assert p
   p.set_actor_location(s.get_actor_location()+unreal.Vector(200,0,40),False,True);p.set_actor_tick_enabled(False)
   refs.update(s=s,p=p)
   s.get_component_by_class(unreal.CombatTargetComponent).apply_combat_damage(1,p)
   state.update(phase='observe',since=time.time(),next=0)
  elif state['phase']=='observe':
   elapsed=time.time()-state['since']
   if elapsed>=state['next']:
    s,p=refs['s'],refs['p'];target=s.get_editor_property('current_target')
    state['samples'].append(dict(t=elapsed,health=p.get_health_component().get_current_health(),slime_health=s.get_health_component().get_current_health(),slime_pos=str(s.get_actor_location()),player_pos=str(p.get_actor_location()),target=str(target),provoked=s.is_provoked_by_team(0),controller=str(s.get_controller()),tick=s.is_actor_tick_enabled()))
    state['next']+=1
   if elapsed>=8:finish()
 except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
level.editor_request_begin_play()
