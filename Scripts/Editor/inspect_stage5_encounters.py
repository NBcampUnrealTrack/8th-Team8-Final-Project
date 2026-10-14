"""Read the current Stage 05 serpentine map and gameplay actors; never save."""
import json
from pathlib import Path
import unreal

out=Path(unreal.Paths.project_dir())/'Saved/Stage05Encounters'
out.mkdir(parents=True,exist_ok=True)
table=unreal.load_asset('/Game/Core/DataTable/DT_SFStage')
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
stage=next(r for r in rows if r['Name']=='Stage05')
map_path=stage['StageLevel'].split('.')[0]
assert unreal.EditorLoadingAndSavingUtils.load_map(map_path)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
records=[]
for a in sub.get_all_level_actors():
    p,r=a.get_actor_location(),a.get_actor_rotation()
    center,extent=a.get_actor_bounds(False)
    item=dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),
              xyz=[p.x,p.y,p.z],yaw=r.yaw,tags=[str(t) for t in a.tags],
              bounds_center=[center.x,center.y,center.z],bounds_extent=[extent.x,extent.y,extent.z])
    for key in ('team_index','building_type','initial_tier_index','preview_definition','unit_row_name',
                'auto_advance_teams','advance_mode','return_to_hold_location','leash_radius','aggro_radius',
                'spawn_interval','initial_delay','start_active','lane','require_owner_building','resource_deposit'):
        try:
            v=a.get_editor_property(key)
            item[key]=v if isinstance(v,(int,float,bool)) else str(v)
        except Exception:
            pass
    target=a.get_component_by_class(unreal.CombatTargetComponent)
    if target:
        item['combat_team']=target.get_team_index()
        item['sight_radius']=target.get_sight_radius()
    if isinstance(a,unreal.LanePath):
        spline=a.get_editor_property('spline')
        item['points']=[[v.x,v.y,v.z] for v in (spline.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD)
                        for i in range(spline.get_number_of_spline_points()))]
    records.append(item)
(out/'inspection.json').write_text(json.dumps(dict(map=map_path,stage=stage,actors=records),ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('STAGE05_INSPECTED: '+map_path+' actors='+str(len(records)))
