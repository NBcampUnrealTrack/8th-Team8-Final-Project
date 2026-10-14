import unreal,json
from pathlib import Path
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={}
for name,camps,slimes,nodes in [('PT_Stage_2',4,10,0),('PT_Stage_Hero',1,3,12),('PT_SFStageAmbush',0,0,12)]:
    assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/'+name)
    actors=sub.get_all_level_actors()
    groups=[a for a in actors if isinstance(a,unreal.SFNeutralSlimeCamp)]
    units=[a for a in actors if isinstance(a,unreal.SFNeutralSlime)]
    deposits=[a for a in actors if isinstance(a,unreal.ResourceBase) and a.actor_has_tag('SF_NeutralSlimes')]
    assert len(groups)==camps and len(units)==slimes and len(deposits)==nodes
    records=[]
    for c in groups:
        members=c.get_editor_property('members')
        small_only=c.get_actor_label() in ['SlimeCamp_North','SlimeCamp_South']
        assert c.get_editor_property('grant_clear_reward') == (not small_only)
        assert len(members)==(2 if small_only else 3)
        assert sum(s.get_editor_property('large') for s in members)==(0 if small_only else 1)
        for s in members:
            assert s.get_editor_property('camp')==c
            assert s.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==3
            scale=s.get_component_by_class(unreal.SkeletalMeshComponent).get_editor_property('relative_scale3d')
            assert abs(scale.x-(4.5 if s.get_editor_property('large') else 2.5))<.01,(s.get_actor_label(),str(scale))
        records.append(dict(label=c.get_actor_label(),members=len(members),large=not small_only,grant_clear_reward=c.get_editor_property('grant_clear_reward')))
    assert sum(d.get_editor_property('resource_type')==unreal.ResourceType.MINERAL for d in deposits)==nodes//2
    for d in deposits:assert d.get_editor_property('resource_deposit')==(3000 if d.get_editor_property('resource_type')==unreal.ResourceType.MINERAL else 1800)
    report[name]=dict(camps=records,slimes=len(units),additional_resources=len(deposits))
out=Path(unreal.Paths.project_dir())/'Saved/NeutralSlimes'
(out/'assets_verified.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('NEUTRAL_ASSETS_VERIFIED')
