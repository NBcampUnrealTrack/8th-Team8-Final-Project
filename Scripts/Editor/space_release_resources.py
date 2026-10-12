"""Space mine sites on the 100 cm grid; keep small satellite ores with their deposit.
Run without -SFApplyResourceSpacing for a read-only plan. No landscape or scenery edits.
"""
import unreal,json,math,shutil,traceback,time
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/ReleaseFeedback';sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
apply='-SFApplyResourceSpacing' in unreal.SystemLibrary.get_command_line()
baseline=json.loads((out/'levels.json').read_text(encoding='utf-8'))[:7]
report=[]
def collide(a,b,gap=100):
 return abs(a[0]-b[0]) < a[2]+b[2]+gap-.1 and abs(a[1]-b[1]) < a[3]+b[3]+gap-.1
def process(entry,w):
 path=entry['map']
 actors=list(sub.get_all_level_actors());resources=[a for a in actors if isinstance(a,unreal.ResourceBase)]
 by_label={a.get_actor_label():a for a in resources};rows=entry['resources']
 major=[r for r in rows if r['grid']!=[1,1]];groups={r['name']:[r] for r in major}
 for r in rows:
  if r in major:continue
  nearest=min((a for a in major if ('Ether' in a['cls'])==('Ether' in r['cls'])),key=lambda a:math.dist(a['pos'][:2],r['pos'][:2]),default=None)
  if nearest and math.dist(nearest['pos'][:2],r['pos'][:2])<=360:groups[nearest['name']].append(r)
  else:major.append(r);groups[r['name']]=[r]
 anchors=[]
 for a in actors:
  if isinstance(a,unreal.SFBuildingSpawnAnchor):
   p=a.get_actor_location();anchors.append((p.x,p.y,500,500))
 placed=[];changes=[];unresolved=[]
 offsets=sorted([(x,y) for x in range(-2000,2001,100) for y in range(-2000,2001,100)],key=lambda p:p[0]*p[0]+p[1]*p[1])
 def ground(x,y,z):
  hit=unreal.SystemLibrary.line_trace_single_for_objects(w,unreal.Vector(x,y,z+700),unreal.Vector(x,y,z-900),[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1],False,resources,unreal.DrawDebugTrace.NONE,True)
  if not hit:return None
  parts=hit.to_tuple()
  if not parts[0]:return None
  normal=parts[7];p=parts[5]
  return p.z if normal.z>.85 and abs(p.z-z)<180 else None
 for center in major:
  g=groups[center['name']];cx,cy,cz=center['pos']
  # Use a square envelope so 90-degree mining-building rotations also fit.
  hx=max([300]+[abs(r['pos'][0]-cx)+r['grid'][0]*50 for r in g])
  hy=max([300]+[abs(r['pos'][1]-cy)+r['grid'][1]*50 for r in g])
  candidate=None
  for dx,dy in offsets:
   box=(cx+dx,cy+dy,hx,hy)
   if any(collide(box,b) for b in placed):continue
   # Do not move a deposit closer into a pre-existing base footprint.
   if any(collide(box,b,0) and not collide((cx,cy,hx,hy),b,0) for b in anchors):continue
   if dx==0 and dy==0:candidate=(dx,dy,[r['pos'][2] for r in g]);break
   heights=[ground(r['pos'][0]+dx,r['pos'][1]+dy,r['pos'][2]) for r in g]
   if any(z is None for z in heights):continue
   corners=[ground(cx+dx+sx*280,cy+dy+sy*280,cz) for sx,sy in [(-1,-1),(-1,1),(1,-1),(1,1)]]
   if any(z is None for z in corners) or max(corners)-min(corners)>110:continue
   nav=unreal.NavigationSystemV1.project_point_to_navigation(w,unreal.Vector(cx+dx,cy+dy,heights[0]+60),None,None,unreal.Vector(80,80,200))
   if nav is None:continue
   candidate=(dx,dy,heights);break
  if candidate is None:
   unresolved.append(center['name']);placed.append((cx,cy,hx,hy));continue
  dx,dy,heights=candidate;placed.append((cx+dx,cy+dy,hx,hy))
  if dx or dy:
   for r,z in zip(g,heights):
    changes.append(dict(name=r['name'],before=r['pos'],after=[r['pos'][0]+dx,r['pos'][1]+dy,z],site=center['name']))
 record=dict(map=path,sites=len(major),actors=len(rows),changes=changes,unresolved=unresolved)
 report.append(record)
 (out/'spacing_plan.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
 if apply:
  assert not unresolved,(path,unresolved)
  if changes:
   src=root/('Content/'+path.removeprefix('/Game/')+'.umap');dst=out/'before'/src.relative_to(root);dst.parent.mkdir(parents=True,exist_ok=True)
   if not dst.exists():shutil.copy2(src,dst)
   for c in changes:by_label[c['name']].set_actor_location(unreal.Vector(*c['after']),False,False)
   assert unreal.EditorLoadingAndSavingUtils.save_current_level()
 unreal.log('RESOURCE_SPACING '+path+' moved='+str(len(changes))+' unresolved='+str(unresolved))

state={'index':0,'phase':'load','busy':False}
def tick(dt):
 if state['busy']:return
 state['busy']=True
 try:
  if state['phase']=='load':
   w=unreal.EditorLoadingAndSavingUtils.load_map(baseline[state['index']]['map'])
   unreal.SystemLibrary.execute_console_command(w,'RebuildNavigation')
   state.update(world=w,phase='wait',at=time.time());return
  if time.time()-state['at']<10 or unreal.NavigationSystemV1.is_navigation_being_built(state['world']):return
  process(baseline[state['index']],state['world'])
  state['index']+=1
  if state['index']<len(baseline):state['phase']='load'
  else:
   (out/('spacing_applied.json' if apply else 'spacing_dryrun.json')).write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
   unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
 except Exception:
  (out/'spacing_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
  unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
 finally:state['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
