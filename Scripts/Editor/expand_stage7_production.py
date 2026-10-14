"""Add five factories per faction and mineral reserves to Stage07, preserving existing actors."""
import unreal,json,math,shutil,time,traceback
from pathlib import Path

root=Path(unreal.Paths.project_dir());out=root/'Saved/Stage7Expansion';out.mkdir(parents=True,exist_ok=True)
map_path='/Game/Game/Stage/PT_SFStageFinal'
backup=out/'before_expansion.umap'
if not backup.exists():shutil.copy2(root/'Content/Game/Stage/PT_SFStageFinal.umap',backup)
w=unreal.EditorLoadingAndSavingUtils.load_map(map_path)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
labels={a.get_actor_label():a for a in sub.get_all_level_actors()}
tag=unreal.Name('SF_Stage7_Expansion')
terrain=labels['Final_Terrain']
ignore_ground=[a for a in labels.values() if a!=terrain]
nav=labels['Final_NavMeshBounds'];origin,extent=nav.get_actor_bounds(False);scale=nav.get_actor_scale3d()
nav.set_actor_scale3d(unreal.Vector(scale.x*8312.489852905273/extent.x,scale.y*9685.241012573242/extent.y,scale.z))
unreal.SystemLibrary.execute_console_command(w,'RebuildNavigation')
trees=[]
for a in labels.values():
 if isinstance(a,unreal.StaticMeshActor) and a.static_mesh_component.static_mesh:
  if any(k in a.static_mesh_component.static_mesh.get_name().lower() for k in ('tree','pine','birch','oak')):
   o,e=a.get_actor_bounds(False);trees.append((o.x,o.y,e.x,e.y))
report={'buildings':[],'minerals':[]};state={'phase':'nav','at':time.time()}
occupied=[]
for a in labels.values():
 if tag in a.tags:continue
 if isinstance(a,(unreal.SFBuildingSpawnAnchor,unreal.ResourceBase)):
  p=a.get_actor_location();occupied.append((p.x,p.y,450 if isinstance(a,unreal.SFBuildingSpawnAnchor) else 250))
offsets=sorted([(x,y) for x in range(-700,701,100) for y in range(-700,701,100)],key=lambda p:p[0]**2+p[1]**2)

def ground(x,y):
 hit=unreal.SystemLibrary.line_trace_single_for_objects(w,unreal.Vector(x,y,2500),unreal.Vector(x,y,-500),[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1],True,ignore_ground,unreal.DrawDebugTrace.NONE,True)
 if not hit:return None
 parts=hit.to_tuple()
 return parts[5].z if parts[0] and parts[7].z>.90 and parts[5].z>-20 else None

def navpoint(p,extent=100):
 q=unreal.NavigationSystemV1.project_point_to_navigation(w,p,None,None,unreal.Vector(extent,extent,300))
 return q if q and math.hypot(q.x-p.x,q.y-p.y)<extent else None

def site(x,y,space=500):
 for dx,dy in offsets:
  cx,cy=round((x+dx)/100)*100,round((y+dy)/100)*100
  if any(abs(cx-tx)<ex+360 and abs(cy-ty)<ey+360 for tx,ty,ex,ey in trees):continue
  if any(abs(cx-ox)<space+r and abs(cy-oy)<space+r for ox,oy,r in occupied):continue
  zs=[ground(cx+sx*350,cy+sy*350) for sx,sy in [(0,0),(-1,-1),(-1,1),(1,-1),(1,1)]]
  if any(z is None for z in zs) or max(zs)-min(zs)>110:continue
  if space==750 and any(ground(cx+dx,cy+dy) is None for dx,dy in [(150,-220),(150,220),(-150,-220),(-150,220)]):continue
  if any(not navpoint(unreal.Vector(cx+sx*450,cy+sy*450,zs[0]+70),80) for sx,sy in [(-1,-1),(-1,1),(1,-1),(1,1)]):continue
  occupied.append((cx,cy,space));return unreal.Vector(cx,cy,zs[0]+1)
 raise RuntimeError('No clear, level and reachable site near '+str((x,y)))

def place(cls,label,p):
 unreal.log('STAGE7_EXPANSION_PLACE '+label+' '+str(p))
 a=labels.get(label)
 if not a:
  a=sub.spawn_actor_from_class(cls,p);assert a,label
  a.set_actor_label(label);a.tags=list(a.tags)+[tag];labels[label]=a
 a.set_folder_path('Gameplay/Stage07 Expansion');a.set_actor_location(p,False,False)
 return a

def configure():
 # Two later-added ores occupy the original Human sanctuary's grid footprint.
 # Keep the ores, but move them into the open reserve beside that factory.
 special=labels['Stage07_0_Special'].get_actor_location()
 for label,x,y in [('Final_Mineral_1_4',-8500,9400),('Final_Mineral_1_5',-9000,9400)]:
  ore=labels.get(label)
  if not ore:continue
  old=ore.get_actor_location()
  if math.hypot(old.x-special.x,old.y-special.y)>=650:continue
  z=ground(x,y);assert z is not None,label
  ore.set_actor_location(unreal.Vector(x,y,z+1),False,False)
  occupied.remove((old.x,old.y,250));occupied.append((x,y,250))
  report.setdefault('relocated_conflicting_minerals',[]).append({'label':label,'position':[x,y,z+1]})
 anchor_cls=labels['Stage07_0_Barracks'].get_class()
 copies=['team_index','unit_row_name','unit_class','spawn_phases','spawn_interval','spawn_count_per_cycle','max_alive','max_total',
  'start_active','spawning_enabled','wave_controlled','stop_when_owner_building_destroyed','require_owner_building',
  'owner_building_search_radius','owner_building_resolve_timeout']
 plans_by_team={
  0:[('Barracks',0,-3400,8000,1,120),('Ranged',0,-5400,6800,2,135),('Barracks',1,-600,8900,1,150),('Ranged',1,-6800,5200,4,165),('Special',0,-7200,2400,4,270)],
  1:[('Barracks',0,4100,-8500,4,120),('Ranged',0,7900,-4100,1,135),('Barracks',1,2300,-8700,4,150),('Ranged',1,7700,-2300,1,165),('Special',0,1100,-6800,3,270)]}
 human=labels['Final_EtherNexus'].get_actor_location();beast=labels['Final_BeastNexus'].get_actor_location()
 def mirror(x,y):return (beast.x-(x-human.x),beast.y-(y-human.y))
 for team in [0,1]:
  for role,index,x,y,lane_number,delay in plans_by_team[team]:
   p=site(x,y)
   template=labels['Stage07_%d_%s'%(team,role)]
   prefix='Stage07_Expansion_%d_%s%d'%(team,role,index)
   b=place(anchor_cls,prefix,p)
   for key in ['team_index','building_type','preview_definition','initial_tier_index']:b.set_editor_property(key,template.get_editor_property(key))
   b.set_actor_rotation(unreal.SFBuildingSpawnLibrary.get_construction_rotation(),False)
   b.set_actor_location(p,False,False)
   lane=labels['Final_Lane_%d_Team%d'%(lane_number,team)]
   entry=lane.get_editor_property('spline').get_location_at_spline_point(0,unreal.SplineCoordinateSpace.WORLD)
   angle=math.atan2(entry.y-p.y,entry.x-p.x);gate=None
   for turn in [0,45,-45,90,-90,135,-135,180]:
    theta=angle+math.radians(turn)
    q=navpoint(unreal.Vector(p.x+800*math.cos(theta),p.y+800*math.sin(theta),p.z+60),200)
    if not q:continue
    path=unreal.NavigationSystemV1.find_path_to_location_synchronously(w,q,entry)
    if path and path.is_valid() and not path.is_partial():gate=q;break
   assert gate,('No factory exit',prefix)
   s=place(unreal.UnitSpawner,prefix+'_Spawner',p)
   source=labels['Stage07_%d_AI_%s'%(team,role)]
   for key in copies:s.set_editor_property(key,source.get_editor_property(key))
   # Forward factories join their route here instead of walking back to the base.
   source_spline=lane.get_editor_property('spline')
   route=place(unreal.LanePath,prefix+'_Route',unreal.Vector())
   route.set_editor_property('team_index',team)
   route.set_editor_property('lane_half_width',lane.get_editor_property('lane_half_width'))
   spline=route.get_editor_property('spline')
   alpha=lane.get_alpha_at_location(gate)
   points=[gate]
   length=source_spline.get_spline_length()
   distance=min(length,alpha*length+450)
   while distance<length:
    points.append(source_spline.get_location_at_distance_along_spline(distance,unreal.SplineCoordinateSpace.WORLD));distance+=350
   points.append(source_spline.get_location_at_spline_point(source_spline.get_number_of_spline_points()-1,unreal.SplineCoordinateSpace.WORLD))
   spline.set_spline_points(points,unreal.SplineCoordinateSpace.WORLD,True)
   s.set_editor_property('initial_delay',float(delay));s.set_editor_property('lane',route)
   s.set_editor_property('spawn_offset',gate-p);s.set_editor_property('spawn_scatter_radius',100.)
   s.set_actor_rotation(unreal.Rotator(),False)
   report['buildings'].append({'label':prefix,'team':team,'role':role,'position':[p.x,p.y,p.z],
    'gate':[gate.x,gate.y,gate.z],'lane':lane_number,'delay':delay,'interval':s.get_editor_property('spawn_interval')})
  for cluster,(x,y) in enumerate({0:[(-2300,8100),(-7200,3900)],1:[(6300,-6900),(7100,-5300)]}[team]):
   p=site(x,y,400)
   for i,(dx,dy) in enumerate([(150,-220),(150,220),(-150,-220),(-150,220)]):
    source=labels['Final_Mineral_0_'+str(i)]
    px,py=p.x+dx,p.y+dy;z=ground(px,py);assert z is not None
    label='Stage07_Expansion_%d_Mineral_%d_%d'%(team,cluster,i)
    ore=place(source.get_class(),label,unreal.Vector(px,py,z+1))
    ore.set_actor_scale3d(source.get_actor_scale3d());ore.set_actor_rotation(source.get_actor_rotation(),False)
    ore.set_editor_property('resource_deposit',5000)
    report['minerals'].append({'label':label,'position':[px,py,z+1],'amount':5000})
 assert len(report['buildings'])==10 and len(report['minerals'])==16
 unreal.SystemLibrary.execute_console_command(w,'RebuildNavigation')

def tick(dt):
 if state.get('busy'):return
 state['busy']=True
 try:
  if time.time()-state['at']<8 or unreal.NavigationSystemV1.is_navigation_being_built(w):return
  if state['phase']=='nav':
   configure();state.update(phase='save',at=time.time());return
  for item in report['buildings']:
   gate=unreal.Vector(*item['gate']);assert navpoint(gate,150),item
  assert unreal.EditorLoadingAndSavingUtils.save_map(w,map_path)
  report['saved']=True
  (out/'applied.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
  unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
 except:
  report['error']=traceback.format_exc()
  (out/'failed.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
  unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
 finally:state['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
