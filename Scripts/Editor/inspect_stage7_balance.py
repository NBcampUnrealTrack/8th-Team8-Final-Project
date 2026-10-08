import json
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_dir())/'Saved/Stage7Balance'
out.mkdir(parents=True, exist_ok=True)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_SFStageFinal')
records=[]
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not isinstance(a, (unreal.SFBuildingSpawnAnchor, unreal.UnitSpawner, unreal.ResourceBase, unreal.LanePath)): continue
    p=a.get_actor_location()
    r=dict(label=a.get_actor_label(),cls=a.get_class().get_name(),xyz=[p.x,p.y,p.z])
    for key in ['team_index','building_type','initial_tier_index','preview_definition','resource_deposit','unit_row_name','lane','spawn_offset']:
        try: r[key]=str(a.get_editor_property(key))
        except Exception: pass
    if isinstance(a,unreal.LanePath):
        s=a.get_editor_property('spline')
        r['points']=[[p.x,p.y,p.z] for p in (s.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD) for i in range(s.get_number_of_spline_points()))]
    records.append(r)
data=dict(actors=records)
for name,path in [('units','/Game/Core/DataTable/DT_Units'),('stages','/Game/Core/DataTable/DT_SFStage')]:
    data[name]=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset(path)))
data['definitions']={}
for race in ['Human','Animal']:
    for role in ['Nexus','Barrack','Ranged','Special']:
        asset=unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/'+race+'/DA_'+race+role)
        data['definitions'][race+role]=str(asset.get_editor_property('tiers'))
(out/'inspection.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
