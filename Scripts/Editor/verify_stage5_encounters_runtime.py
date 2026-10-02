"""Disposable PIE validation. No assets or player save data are changed."""
import json,math,time,traceback
from pathlib import Path
import unreal
OUT=Path(unreal.Paths.project_dir())/'Saved/Stage05Encounters'
CONFIG=json.loads((OUT/'configuration_verified.json').read_text(encoding='utf-8'))
ue=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map(CONFIG['map'])
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state=dict(phase='starting',wall_start=time.time(),checks=[],samples=[])
refs={}
def actors(w,c):return unreal.GameplayStatics.get_all_actors_of_class(w,c)
def xyz(a):
 p=a.get_actor_location();return [p.x,p.y,p.z]
def distance(a,b):return math.hypot(a[0]-b[0],a[1]-b[1])
def finish(error=None):
 state.update(phase='failed' if error else 'passed',passed=not bool(error))
 if error:state['error']=error;unreal.log_error('STAGE05_RUNTIME_FAILED: '+error)
 else:unreal.log('STAGE05_RUNTIME_VERIFIED: local sentries, production, winding lanes, owner destruction, Nexus victory')
 (OUT/'runtime_verified.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
 level.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle)
 def quit_tick(delta):
  unreal.unregister_slate_post_tick_callback(quit_handle);unreal.SystemLibrary.quit_editor()
 global quit_handle
 quit_handle=unreal.register_slate_post_tick_callback(quit_tick)
def teleport_probe(w,index,offset=0):
 p=refs['player_lane'].get_editor_property('spline').get_location_at_spline_point(index,unreal.SplineCoordinateSpace.WORLD)
 nav=unreal.NavigationSystemV1.get_navigation_system(w).project_point_to_navigation(w,p,None,None,unreal.Vector(250,250,500));assert nav
 refs['probe'].set_actor_location(unreal.Vector(nav.x,nav.y+offset,nav.z+100),False,True)
 refs['probe'].command_hold_ground()
def tick(delta):
 try:
  if time.time()-state['wall_start']>240:raise RuntimeError('Timed out '+state['phase'])
  w=ue.get_game_world()
  if not w:return
  gs=unreal.GameplayStatics.get_game_state(w)
  if not isinstance(gs,unreal.SFGameState):return
  sec=unreal.GameplayStatics.get_time_seconds(w)
  if state['phase']=='starting':
   if gs.get_stage_state()==unreal.SFStageState.READY_TO_START:assert unreal.GameplayStatics.get_game_mode(w).start_stage()
   assert gs.get_stage_state()!=unreal.SFStageState.INITIALIZATION_FAILED
   if not gs.is_stage_playing():return
   if 'ready_wall' not in state:state['ready_wall']=time.time()
   if time.time()-state['ready_wall']<6:return
   assert str(gs.get_current_stage_row_name())=='Stage05'
   assert 'Animal' in gs.get_team_building_catalog(1).get_path_name()
   spawners=actors(w,unreal.UnitSpawner);guards=[u for u in actors(w,unreal.UnitBase) if unreal.Name('SF_Stage05_Encounters') in u.tags]
   assert len(spawners)==3 and len(guards)==27
   owners=[s.get_owner_building() for s in spawners];assert all(owners) and len({o.get_path_name() for o in owners})==3
   buildings=actors(w,unreal.SFBaseBuilding);enemy=[b for b in buildings if b.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==1]
   assert len(enemy)==10,(len(enemy),'enemy buildings')
   for b in buildings:
    if b.get_editor_property('building_definition').get_editor_property('building_type')!=unreal.SFBuildingType.NEXUS:assert abs(b.get_actor_rotation().yaw-90)<.001
   for u in guards:
    assert u.get_advance_mode()==unreal.UnitAdvanceMode.HOLD_POSITION
    assert u.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==1
    assert u.get_component_by_class(unreal.CombatTargetComponent).get_sight_radius()==900
    assert u.get_editor_property('assigned_lane') is None
    assert u.get_current_attack_power()>0
   assert gs.get_mineral_income_rate(1)>0 and gs.get_ether_income_rate(1)>0
   refs.update(spawners=spawners,guards=guards,owners=owners,player_lane=next(a for a in actors(w,unreal.LanePath) if a.get_editor_property('team_index')==0))
   state['guard_initial']={u.get_name():xyz(u) for u in guards}
   state['checks'].append(dict(stage='Stage05',enemy_buildings=10,guards=27,production_owners=3,non_nexus_yaw=90,initialized_guard_stats=True))
   for a in actors(w,unreal.Actor):
    target=a.get_component_by_class(unreal.CombatTargetComponent)
    if target:target.set_damage_multiplier(0.0)
   unreal.GameplayStatics.set_global_time_dilation(w,10.0)
   state.update(phase='idle',phase_start=sec)
  elif state['phase']=='idle' and sec-state['phase_start']>=12:
   drift=max(distance(xyz(u),state['guard_initial'][u.get_name()]) for u in refs['guards'])
   assert drift<160,('Sentries advanced without target',drift)
   assert all(u.get_editor_property('current_target') is None for u in refs['guards'])
   state['checks'].append(dict(idle_guard_max_drift=drift,no_automatic_base_attack=True))
   player=next(b for b in actors(w,unreal.SFBaseBuilding) if b.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==0)
   probe=unreal.SFUnitSpawnLibrary.spawn_unit_from_row(player,unreal.Name('Fighter'),300.0,0.0,None);assert probe
   probe.get_component_by_class(unreal.CombatTargetComponent).set_damage_multiplier(0.0)
   refs['probe']=probe
   guard=min(refs['guards'],key=lambda u:distance(xyz(u),CONFIG['guards'][0]['xyz']))
   refs['guard']=guard;teleport_probe(w,12)
   assert distance(xyz(probe),xyz(guard))>1100
   state.update(phase='far',phase_start=sec)
  elif state['phase']=='far' and sec-state['phase_start']>=4:
   guard=refs['guard'];assert guard.get_editor_property('current_target') is None
   assert not guard.has_line_of_sight(refs['probe'])
   teleport_probe(w,17)
   assert guard.has_line_of_sight(refs['probe']), 'Nearby target has no line of sight'
   state.update(phase='near',phase_start=sec)
  elif state['phase']=='near' and sec-state['phase_start']>=4:
   guard=refs['guard'];assert guard.get_editor_property('current_target')==refs['probe'],'Visible target did not activate sentry'
   assert guard.get_combat_state() in (unreal.UnitCombatState.CHASING,unreal.UnitCombatState.ATTACKING)
   state['checks'].append(dict(outside_sight_idle=True,inside_sight_attacks=True,guard_movement=distance(xyz(guard),state['guard_initial'][guard.get_name()])))
   teleport_probe(w,4);state.update(phase='return',phase_start=sec)
  elif state['phase']=='return' and sec-state['phase_start']>=12:
   guard=refs['guard'];assert guard.get_editor_property('current_target') is None
   drift=distance(xyz(guard),state['guard_initial'][guard.get_name()]);assert drift<180,('Guard did not return',drift)
   state['checks'].append(dict(lost_target_returns=True,return_distance=drift))
   refs['probe'].destroy_actor();unreal.GameplayStatics.set_global_time_dilation(w,20.0)
   state.update(phase='production')
  elif state['phase']=='production':
   counts={str(s.get_editor_property('unit_row_name')):s.get_editor_property('total_spawned') for s in refs['spawners']}
   for s in refs['spawners']:
    delay=s.get_editor_property('initial_delay');row=str(s.get_editor_property('unit_row_name'))
    if sec<delay-1:assert counts[row]==0,('Spawned early',row,sec)
    if sec>delay+5:assert counts[row]>0,('Not spawning',row,sec)
   for checkpoint in (80,105,165,255,330):
    if sec>=checkpoint and not any(a['checkpoint']==checkpoint for a in state['samples']):state['samples'].append(dict(checkpoint=checkpoint,time=sec,counts=counts))
   children=[u for u in actors(w,unreal.UnitBase) if unreal.Name('SF_Stage05_Encounters') not in u.tags and u.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==1]
   for u in children:
    lane=u.get_editor_property('assigned_lane');assert lane in [s.get_editor_property('lane') for s in refs['spawners']]
    assert u.get_advance_mode()==unreal.UnitAdvanceMode.PUSH_TO_CORE
    p=u.get_actor_location();assert unreal.NavigationSystemV1.get_navigation_system(w).project_point_to_navigation(w,p,None,None,unreal.Vector(200,200,500))
    nearest=lane.get_editor_property('spline').find_location_closest_to_world_location(p,unreal.SplineCoordinateSpace.WORLD)
    deviation=math.hypot(p.x-nearest.x,p.y-nearest.y)
    state['lane_max_deviation']=max(state.get('lane_max_deviation',0),deviation)
    assert deviation<900,('Unit left the winding lane',u.get_name(),deviation)
   if sec>=330:
    assert children and any(u.get_editor_property('lane_alpha')>.02 for u in children), 'Lane units did not move'
    state['checks'].append(dict(production_counts=counts,lane_units=len(children),assigned_lane_movement=True,all_units_on_navigation=True))
    state['before_destroy']=counts;refs['children']=children
    for s,o in zip(refs['spawners'],refs['owners']):
     o.get_component_by_class(unreal.CombatTargetComponent).set_damage_multiplier(1.0)
     o.get_component_by_class(unreal.CombatTargetComponent).apply_combat_damage(100000.0,None)
     assert s.is_shut_down_by_owner_building();s.activate_spawner();s.set_spawning_enabled(True);assert s.spawn_one() is None
    assert all(unreal.SystemLibrary.is_valid(u) for u in children)
    state.update(phase='destroyed',phase_start=sec)
  elif state['phase']=='destroyed':
   assert {str(s.get_editor_property('unit_row_name')):s.get_editor_property('total_spawned') for s in refs['spawners']}==state['before_destroy']
   if sec-state['phase_start']>=185:
    state['checks'].append(dict(all_three_factories_stop_on_destruction=True,spawns_stopped_seconds=sec-state['phase_start'],existing_units_preserved=True))
    nexus=next(b for b in actors(w,unreal.SFBaseBuilding) if 'Nexus' in b.get_editor_property('building_definition').get_name() and b.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==1)
    target=nexus.get_component_by_class(unreal.CombatTargetComponent);target.set_damage_multiplier(1.0);target.apply_combat_damage(100000.0,None)
    assert gs.get_stage_result()==unreal.SFStageResult.VICTORY
    state['checks'].append(dict(enemy_nexus_destroyed_result='Victory'));finish()
 except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
editor_started=time.time()
def begin_when_navigation_ready(delta):
 if time.time()-editor_started<20:return
 unreal.unregister_slate_post_tick_callback(begin_handle)
 level.editor_request_begin_play()
begin_handle=unreal.register_slate_post_tick_callback(begin_when_navigation_ready)
