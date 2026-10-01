"""Read Stage 04 gameplay layout and race catalogs without saving assets."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'Saved/Stage04Enemy'
out.mkdir(parents=True, exist_ok=True)
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_4')
assert world
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
records = []
for a in sub.get_all_level_actors():
    cls = a.get_class().get_name()
    label = a.get_actor_label()
    if not (any(s in cls for s in ('BuildingSpawn', 'Spawner', 'LanePath', 'Mineral', 'Ether', 'NavMesh', 'PlayerStart'))
            or label.startswith(('GF_Zone', 'GF_Route'))):
        continue
    p, r, s = a.get_actor_location(), a.get_actor_rotation(), a.get_actor_scale3d()
    item = dict(name=a.get_name(), label=label, cls=cls, xyz=[p.x,p.y,p.z], yaw=r.yaw,
                scale=[s.x,s.y,s.z], tags=[str(t) for t in a.tags])
    for key in ('team_index','building_type','initial_tier_index','preview_definition', 'unit_row_name',
                'spawn_interval','initial_delay','start_active','activation_objective_tag','lane','resource_deposit'):
        try:
            value = a.get_editor_property(key)
            item[key] = value if isinstance(value,(int,float,bool)) else str(value)
        except Exception:
            pass
    if isinstance(a, unreal.LanePath):
        spline = a.get_editor_property('spline')
        item['points'] = [[v.x,v.y,v.z] for v in (spline.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD)
                          for i in range(spline.get_number_of_spline_points()))]
    records.append(item)

units = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/Core/DataTable/DT_Units')))
catalogs = {}
for path in unreal.EditorAssetLibrary.list_assets('/Game/Core/Primary/Buildings/Definitions/Animal',recursive=True):
    asset = unreal.load_asset(path)
    try:
        catalogs[path] = dict(grid=str(asset.get_editor_property('grid_size')), tiers=str(asset.get_editor_property('tiers')))
    except Exception:
        pass
tables = {}
for path in unreal.EditorAssetLibrary.list_assets('/Game',recursive=True):
    if 'DT_Stage' in path or 'DT_SFStage' in path:
        asset = unreal.load_asset(path)
        if isinstance(asset,unreal.DataTable):
            tables[path]=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(asset))
(out/'inspection.json').write_text(json.dumps(dict(actors=records,units=units,catalogs=catalogs,tables=tables),ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('STAGE04_ENEMY_INSPECTED: '+str(len(records)))
