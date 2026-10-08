"""PIE narrative/camera regression: -Stage67StoryCase=6, Human, or Beast. No asset writes."""
import json,time,traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());out=root/'Saved/Stage67Story';out.mkdir(exist_ok=True)
source=json.loads((root/'ContentSource/Stage67Story_KO.json').read_text(encoding='utf-8'))
portraits=json.loads((root/'ContentSource/Stage67StoryPortraits.json').read_text(encoding='utf-8'))
cmd=unreal.SystemLibrary.get_command_line()
case='Beast' if '-Stage67StoryCase=Beast' in cmd else 'Human' if '-Stage67StoryCase=Human' in cmd else '6'
row='Stage06' if case=='6' else 'Stage07_'+case
table=unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
common_camera=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class('/Game/Game/IN_UI/JSYBluePrints/BP_SFRTSCameraPawn'))
common_camera_distance=common_camera.get_editor_property('initial_camera_distance')
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_SFStageAmbush' if case=='6' else '/Game/Game/Stage/PT_SFStageFinal')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(case=case,phase='opening',started=time.time(),checks=[])

def widgets(w,cls):return unreal.WidgetLibrary.get_all_widgets_of_class(w,cls,True)
def check_portraits(intro,expected,index):
 visual=portraits[expected][index]
 for faction,field in [('etherian','left_character_image'),('beast','right_character_image')]:
  portrait=intro.get_editor_property(field)
  if faction in visual:
   assert portrait.get_visibility() not in [unreal.SlateVisibility.COLLAPSED,unreal.SlateVisibility.HIDDEN]
   texture=portrait.get_editor_property('brush').get_editor_property('resource_object')
   prefix='T_EtherCommander_' if faction=='etherian' else 'Beast_Commander_'
   assert texture and texture.get_name()==prefix+visual[faction], 'Wrong commander image'
  else:
   assert portrait.get_visibility() in [unreal.SlateVisibility.COLLAPSED,unreal.SlateVisibility.HIDDEN]
def finish(error=None):
 state.update(passed=error is None,error=error,phase='done')
 (out/('runtime_'+case+'.json')).write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
 if error:unreal.log_error(error)
 else:unreal.log('STAGE67_STORY_RUNTIME_VERIFIED '+case)
 level.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle)
 def quit_tick(delta):
  unreal.unregister_slate_post_tick_callback(quit_handle);unreal.SystemLibrary.quit_editor()
 global quit_handle
 quit_handle=unreal.register_slate_post_tick_callback(quit_tick)

def check_story(w,expected):
 intros=widgets(w,unreal.SFStageIntroWidget)
 assert len(intros)==1 and intros[0].is_activated()
 assert unreal.GameplayStatics.is_game_paused(w)
 body=str(intros[0].get_editor_property('dialogue_text').get_text())
 assert source[expected][0].startswith(body), 'Wrong story: '+expected+' / '+body
 if body!=source[expected][0]:return None  # Let the real typewriter finish while paused.
 check_portraits(intros[0],expected,0)
 if '-Stage67Capture' in cmd:
  if state.get('capture_story')!=expected:
   unreal.SystemLibrary.execute_console_command(w,'Shot SHOWUI filename='+str(out/(expected+'_portraits.png')))
   state.update(capture_story=expected,capture_at=time.time());return None
  if time.time()-state['capture_at']<1:return None
 assert not widgets(w,unreal.ResultWidget)
 state['checks'].append(dict(story=expected,correct_text=True,paused=True,commander_images=True))
 for index in range(1,len(source[expected])):
  assert intros[0].start_stage_intro_at(table,expected,index)
  check_portraits(intros[0],expected,index)
 return intros[0]

def tick(delta):
 try:
  if time.time()-state['started']>180:raise RuntimeError('Timed out: '+state['phase'])
  w=editor.get_game_world()
  if not w:return
  gs=unreal.GameplayStatics.get_game_state(w)
  if not isinstance(gs,unreal.SFGameState):return
  assert gs.get_stage_state()!=unreal.SFStageState.INITIALIZATION_FAILED
  if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:
   assert unreal.GameplayStatics.get_game_mode(w).start_stage();return
  if state['phase']=='opening':
   if not gs.is_stage_playing() or not widgets(w,unreal.SFStageIntroWidget):return
   if case!='6' and not state.get('selected'):
    # Select through the same GameInstance flow as the lobby, then travel.
    intro=widgets(w,unreal.SFStageIntroWidget)[0]
    assert intro.start_stage_intro_at(table,'Stage07_Human',len(source['Stage07_Human']))
    gi=unreal.GameplayStatics.get_game_instance(w)
    for order in range(1,7):assert gi.mark_stage_cleared('Stage%02d'%order)
    assert gi.select_stage('Stage07')
    assert gi.select_race(unreal.SFRace.BEAST if case=='Beast' else unreal.SFRace.HUMAN)
    assert gi.move_to_stage()
    state['selected']=True;return
   intro=check_story(w,row)
   if not intro:return
   if case=='6':
    pawn=unreal.GameplayStatics.get_player_pawn(w,0)
    assert abs(pawn.get_actor_rotation().yaw)<.01
    arm=pawn.get_component_by_class(unreal.SpringArmComponent)
    distance=common_camera_distance
    assert abs(arm.get_editor_property('target_arm_length')-distance)<1
    controller=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SFDefenceWaveController)[0]
    assert controller.get_phase()==unreal.SFDefencePhase.WAITING_FOR_TUTORIAL
    state['checks'].append(dict(front_camera=True,camera_distance=distance,waves_wait_for_story=True))
   if case=='Beast':
    unreal.SystemLibrary.execute_console_command(w,'SF.Story.PressVictoryKey')
    assert gs.get_stage_result()==unreal.SFStageResult.VICTORY
    state['checks'].append(dict(k_victory_while_story_paused=True))
    state['phase']='epilogue';return
   assert intro.start_stage_intro_at(table,row,len(source[row]))
   assert not unreal.GameplayStatics.is_game_paused(w)
   state['phase']='playing'
  elif state['phase']=='playing':
   if case=='6':
    controller=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SFDefenceWaveController)[0]
    if controller.get_phase()!=unreal.SFDefencePhase.PREPARATION:return
    assert controller.get_remaining_seconds()>50
   # Exercise the normal victory handler and its deferred result screen.
   unreal.SystemLibrary.execute_console_command(w,'SF.Story.PressVictoryKey')
   assert gs.get_stage_result()==unreal.SFStageResult.VICTORY
   state['checks'].append(dict(k_victory_while_playing=True))
   if case=='6':
    assert controller.get_phase()==unreal.SFDefencePhase.STOPPED
    assert not widgets(w,unreal.SFDefenceTimerWidget)
    state['checks'].append(dict(waves_and_timer_stopped=True))
   state['phase']='epilogue'
  elif state['phase']=='epilogue':
   intro=check_story(w,row+'Clear')
   if not intro:return
   unreal.SystemLibrary.execute_console_command(w,'SF.Story.PressVictoryKey')
   assert widgets(w,unreal.SFStageIntroWidget)==[intro]
   assert not widgets(w,unreal.ResultWidget)
   assert intro.start_stage_intro_at(table,row+'Clear',len(source[row+'Clear']))
   state['phase']='result'
  elif state['phase']=='result':
   assert not widgets(w,unreal.SFStageIntroWidget)
   results=widgets(w,unreal.ResultWidget)
   assert len(results)==1 and results[0].is_in_viewport()
   state['checks'].append(dict(single_result_after_epilogue=True));finish()
 except Exception:finish(traceback.format_exc())

handle=unreal.register_slate_post_tick_callback(tick)
started=time.time()
def begin(delta):
 if time.time()-started<15:return
 unreal.unregister_slate_post_tick_callback(begin_handle);level.editor_request_begin_play()
begin_handle=unreal.register_slate_post_tick_callback(begin)
