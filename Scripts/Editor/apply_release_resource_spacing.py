"""Apply the terrain/navigation-checked plan, preserving every resource actor and amount."""
import unreal,json,shutil,time,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/ReleaseFeedback'
plan=json.loads((out/'spacing_dryrun.json').read_text(encoding='utf-8'))
assert len(plan)==7 and all(not r['unresolved'] for r in plan)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
state={'index':0,'phase':'load','busy':False};done=[]
def tick(dt):
 if state['busy']:return
 state['busy']=True
 try:
  entry=plan[state['index']];path=entry['map']
  if state['phase']=='load':
   w=unreal.EditorLoadingAndSavingUtils.load_map(path)
   resources=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.ResourceBase)]
   assert len(resources)==entry['actors'],(path,len(resources),entry['actors'])
   lookup={a.get_actor_label():a for a in resources}
   if entry['changes']:
    src=root/('Content/'+path.removeprefix('/Game/')+'.umap');backup=out/'before'/src.relative_to(root)
    backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists():shutil.copy2(src,backup)
    for change in entry['changes']:
     a=lookup[change['name']];pos=a.get_actor_location()
     assert min((pos-unreal.Vector(*change[k])).length() for k in ('before','after'))<2,(path,change['name'],'unexpected edit')
     a.set_actor_location(unreal.Vector(*change['after']),False,False)
    unreal.SystemLibrary.execute_console_command(w,'RebuildNavigation')
   state.update(world=w,phase='save',at=time.time());return
  if time.time()-state['at']<8 or unreal.NavigationSystemV1.is_navigation_being_built(state['world']):return
  if entry['changes']:assert unreal.EditorLoadingAndSavingUtils.save_map(state['world'],path)
  done.append(entry);(out/'spacing_applied.json').write_text(json.dumps(done,ensure_ascii=False,indent=2),encoding='utf-8')
  state['index']+=1
  if state['index']==len(plan):unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
  else:state['phase']='load'
 except Exception:
  (out/'spacing_apply_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
  unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
 finally:state['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
