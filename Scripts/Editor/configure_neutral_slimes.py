"""Place the five screenshot camps and extra Stage 05/06 mineable deposits.
Only actors tagged SF_NeutralSlimes are replaced; back up the user's current maps.
"""
import unreal, json, shutil, sys, math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import hero_organic_layout as hero
import ambush_layout as ambush
root=Path(unreal.Paths.project_dir());out=root/'Saved/NeutralSlimes';out.mkdir(parents=True,exist_ok=True)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tag=unreal.Name('SF_NeutralSlimes')
report={}

def load(name):
    source=root/'Content/Game/Stage'/(name+'.umap');backup=out/(name+'.before_slimes.umap')
    if not backup.exists():shutil.copy2(source,backup)
    current=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not current or current.get_name()!=name:
        assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/'+name)
    for a in sub.get_all_level_actors():
        if tag in a.tags:assert sub.destroy_actor(a)
    w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    return w

def place(cls,label,p):
    a=sub.spawn_actor_from_class(cls,p);assert a,label
    a.set_actor_label(label);a.set_folder_path('Gameplay/Neutral Slimes and Resources');a.set_editor_property('tags',[tag])
    return a

def nav(w,p,extent=180):
    system=unreal.NavigationSystemV1.get_navigation_system(w)
    q=system.project_point_to_navigation(w,p,None,None,unreal.Vector(extent,extent,600))
    assert q is not None and math.hypot(q.x-p.x,q.y-p.y)<=extent+1,('No walkable ground',str(p),str(q))
    return q

def camp(w,label,p,large,grant_clear_reward=True):
    center=nav(w,p,250)
    c=place(unreal.SFNeutralSlimeCamp,label,center)
    c.set_editor_property('grant_clear_reward',grant_clear_reward)
    members=[]
    offsets=[(-160,-200,False),(-160,200,False)] if not large else [(150,0,True),(-180,-220,False),(-180,220,False)]
    for i,(dx,dy,big) in enumerate(offsets):
        q=nav(w,center+unreal.Vector(dx,dy,0),120)
        s=place(unreal.SFNeutralSlime,label+('_Large' if big else '_Small')+str(i),q+unreal.Vector(0,0,108 if big else 60))
        s.set_editor_property('large',big);s.set_editor_property('camp',c)
        # Property edit reconstructs the character so the editor shows its final size.
        s.set_actor_location(q+unreal.Vector(0,0,108 if big else 60),False,False)
        assert s.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==3
        members.append(s)
    c.set_editor_property('members',members)
    return dict(label=label,center=[center.x,center.y,center.z],large=int(large),small=2)

def resources(w,positions,ground):
    actors=sub.get_all_level_actors()
    mineral=next(a for a in actors if isinstance(a,unreal.ResourceBase) and a.get_editor_property('resource_type')==unreal.ResourceType.MINERAL and '_S' not in a.get_actor_label())
    ether=next(a for a in actors if isinstance(a,unreal.ResourceBase) and a.get_editor_property('resource_type')==unreal.ResourceType.ETHER and '_S' not in a.get_actor_label())
    result=[]
    for kind,points in positions.items():
        template=mineral if kind=='Mineral' else ether
        for i,(x,y) in enumerate(points):
            q=nav(w,ground(x,y)+unreal.Vector(0,0,80),260)
            # Use terrain height, not the navigation mesh's vertical voxel offset.
            p=ground(q.x,q.y)
            a=place(template.get_class(),'SlimeUpdate_'+kind+'_'+str(i),p)
            a.set_editor_property('resource_deposit',3000 if kind=='Mineral' else 1800)
            result.append(dict(label=a.get_actor_label(),p=[p.x,p.y,p.z],deposit=a.get_editor_property('resource_deposit')))
    return result

case=globals().get('SLIME_STAGE',2)
if case==2:
    w=load('PT_Stage_2')
    report['Stage02']=[camp(w,'SlimeCamp_West',unreal.Vector(2600,1500,40),True),
        camp(w,'SlimeCamp_East',unreal.Vector(300,9300,40),True),
        camp(w,'SlimeCamp_North',unreal.Vector(8000,4500,40),False,False),
        camp(w,'SlimeCamp_South',unreal.Vector(-4800,5700,40),False,False)]
    assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Game/Stage/'+w.get_name())

if case==5:
    w=load('PT_Stage_Hero')
    frame=next(a for a in sub.get_all_level_actors() if a.get_actor_label()=='Hero_OrganicGround').get_actor_transform()
    p=frame.transform_location(unreal.Vector(-4800,hero.divider_y(0,-4800),hero.height(-4800,hero.divider_y(0,-4800))))
    report['Stage05Camps']=[camp(w,'SlimeCamp_ForestCut',p+unreal.Vector(0,0,60),True)]
    def ground5(x,y):
        q=frame.inverse_transform_location(unreal.Vector(x,y,0))
        return frame.transform_location(unreal.Vector(q.x,q.y,hero.height(q.x,q.y)+2))
    positions={'Mineral':[],'Ether':[]}
    for x,row,delta in [(-9500,0,850),(-1500,2,-700),(7000,3,-900)]:
        for kind,dx in [('Mineral',-550),('Ether',550)]:
            for extra in [-230,230]:
                p=frame.transform_location(unreal.Vector(x+dx+extra,hero.row_y(row,x)+delta,0))
                positions[kind].append((p.x,p.y))
    report['Stage05Resources']=resources(w,positions,ground5)
    assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Game/Stage/'+w.get_name())

if case==6:
    w=load('PT_SFStageAmbush')
    def ground6(x,y):return unreal.Vector(x,y,ambush.height(*ambush.world_uv(x,y))+2)
    positions={'Mineral':[(x,y) for x in [-2800,-3400,-4000] for y in [-1900,-2400]],
               'Ether':[(x,y) for x in [-2800,-3400,-4000] for y in [1900,2400]]}
    report['Stage06Resources']=resources(w,positions,ground6)
    assert unreal.EditorLoadingAndSavingUtils.save_map(w,'/Game/Game/Stage/'+w.get_name())
(out/('configured_'+str(case)+'.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('NEUTRAL_SLIMES_CONFIGURED')
unreal.SystemLibrary.quit_editor()
