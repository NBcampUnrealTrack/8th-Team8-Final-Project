import unreal, json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/NeutralSlimes'
out.mkdir(parents=True,exist_ok=True)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={}
for name in ['PT_Stage_2']:
    assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/'+name)
    rows=[]
    for a in sub.get_all_level_actors():
        label=a.get_actor_label()
        if isinstance(a,(unreal.ResourceBase,unreal.LanePath,unreal.SFBuildingSpawnAnchor)) or any(k in label for k in ['Ground','Zone','River','Bridge']):
            p=a.get_actor_location();r=a.get_actor_rotation();s=a.get_actor_scale3d()
            d=dict(label=label,cls=a.get_class().get_path_name(),p=[p.x,p.y,p.z],r=[r.pitch,r.yaw,r.roll],s=[s.x,s.y,s.z])
            if isinstance(a,unreal.ResourceBase):d['deposit']=a.get_editor_property('resource_deposit')
            rows.append(d)
    report[name]=rows
mesh=unreal.load_asset('/Game/ma060_Slime/SkeletalMesh/SK_ma060_Slime')
report['mesh_bounds']=str(mesh.get_bounds())
(out/'inspection_stage2.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('NEUTRAL_INSPECTION_DONE')
