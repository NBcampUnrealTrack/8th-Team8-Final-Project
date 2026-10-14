"""Disposable PIE: voice serialization, repeated build selection, HUD and proximity combat."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'ReleaseFeedback'
ue=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_2')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(start=time.time(),phase='start',checks=[],voices=[]);refs={}
def widget(w,cls):return unreal.WidgetLibrary.get_all_widgets_of_class(w,cls,False)[0]
def finish(error=None):
 state['passed']=error is None;state['error']=error;(out/'runtime_verified.json').write_text(json.dumps(state,indent=2,ensure_ascii=False,default=str),encoding='utf-8')
 unreal.unregister_slate_post_tick_callback(handle);level.editor_request_end_play()
 def end(dt):
  unreal.unregister_slate_post_tick_callback(end_handle);unreal.SystemLibrary.quit_editor()
 global end_handle
 end_handle=unreal.register_slate_post_tick_callback(end)
def tick(dt):
 try:
  if time.time()-state['start']>160:raise Exception('timeout '+state['phase'])
  w=ue.get_game_world()
  if not w:return
  gs=unreal.GameplayStatics.get_game_state(w)
  if not isinstance(gs,unreal.SFGameState):return
  if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:unreal.GameplayStatics.get_game_mode(w).start_stage()
  if not gs.is_stage_playing():return
  elapsed=time.time()-state.get('since',time.time())
  if state['phase']=='start':
   for intro in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.SFStageIntroWidget,False):
    # End the tutorial normally so its deliberate mission locks do not affect this test.
    intro.start_stage_intro_at(unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro'),'Stage02',32)
   for obj in unreal.ObjectIterator(unreal.SFTutorialSubsystem):
    if obj.get_outer()==w:
     obj.cancel_guide();obj.cancel_objective_wait()
   unreal.GameplayStatics.set_game_paused(w,False)
   pc=unreal.GameplayStatics.get_player_controller(w,0);hud=widget(w,unreal.SFInGameHUDWidget)
   gs.add_resource(0,unreal.ResourceType.MINERAL,100000);gs.add_resource(0,unreal.ResourceType.ETHER,100000)
   p=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(gs.get_nexus(0),'Brawler',750,900)
   if not p:return
   hud.select_actor(p)
   voice=next(o for o in unreal.ObjectIterator(unreal.SFUnitVoiceSubsystem) if o.get_outer()==w)
   first=voice.get_active_voice();assert first and first.is_playing(),'First voice failed'
   p.command_move_to(p.get_actor_location()+unreal.Vector(100,0,0))
   assert voice.get_active_voice()==first,'Move response interrupted the selected unit'
   refs.update(pc=pc,hud=hud,p=p,voice=voice,first=first)
   state.update(phase='voice',since=time.time())
  elif state['phase']=='voice':
   current=refs['voice'].get_active_voice()
   if current and current!=refs['first']:
    assert not refs['first'].is_playing(),'Voices overlap'
    state['checks'].append('Selection finishes before queued move response starts')
    state['voice_elapsed']=elapsed
    refs['hud'].open_building_mode();state.update(phase='build',since=time.time(),round=0)
   elif elapsed>16:raise Exception('Queued voice did not start')
  elif state['phase']=='build' and elapsed>1:
   pc,hud=refs['pc'],refs['hud']
   unreal.SystemLibrary.execute_console_command(w,'sf.Tutorial.UnlockBuilding')
   assert pc.is_build_mode_input_enabled(),'Build shortcuts disabled'
   assert hud.request_building_by_slot(0),'Catalog request failed'
   assert pc.get_building_placement_component().is_placement_mode_active(),'No placement ghost'
   pc.clear_spawned_building_ref();pc.set_sf_control_mode(unreal.SFControlMode.GAMEPLAY)
   assert pc.is_build_mode_input_enabled(),'Shortcuts not restored on gameplay return'
   state['round']+=1;state['since']=time.time()
   if state['round']==3:
    state['checks'].append('Three successive build requests restore catalog shortcut mapping')
    hud.close_building_mode()
    panel=widget(w,unreal.SFResourcePanelWidget)
    panel.update_mineral(999999,123);panel.update_ether(999999,123);panel.update_population(999,999)
    unreal.SystemLibrary.execute_console_command(w,'SF.Menu.SceneShot release_resource_frame hold 1')
    state.update(phase='capture',since=time.time())
  elif state['phase']=='capture':
   panel=widget(w,unreal.SFResourcePanelWidget)
   panel.update_mineral(999999,123);panel.update_ether(999999,123);panel.update_population(999,999)
   if elapsed<=3:return
   state['checks'].append('Resource HUD captured with 6-digit values and 3-digit population')
   s=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SFNeutralSlime)[0]
   p=refs['p'];p.set_actor_location(s.get_actor_location()+unreal.Vector(200,0,50),False,True);p.set_actor_tick_enabled(False)
   refs['s']=s;state['health_before']=p.get_health_component().get_current_health();state.update(phase='combat',since=time.time())
  elif state['phase']=='combat' and elapsed>2.5:
   p=refs['p'];state['health_after']=p.get_health_component().get_current_health()
   assert state['health_after']<state['health_before'],'Neutral proximity did not deal damage'
   assert refs['s'].is_provoked_by_team(0),'Camp not provoked'
   state['checks'].append('Neutral monsters attack on approach without being hit first')
   finish()
 except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
level.editor_request_begin_play()
