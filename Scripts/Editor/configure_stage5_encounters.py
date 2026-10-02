"""Add three production outposts and local sentries to the authored Stage 05.

The three outposts correspond to the user's black circles. The two red-X
side cuts are exclusions. Terrain, both Nexuses, player route and resources
are preserved; only SF_Stage05_Encounters actors are replaced on a rerun.
"""
import json
import math
import shutil
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).parent))
from hero_organic_layout import GAPS,divider_y,height

ROOT=Path(unreal.Paths.project_dir())
OUT=ROOT/'Saved/Stage05Encounters'
OUT.mkdir(parents=True,exist_ok=True)
MAP='/Game/Game/Stage/PT_Stage_Hero'
TAG=unreal.Name('SF_Stage05_Encounters')
for source,name in [('Content/Game/Stage/PT_Stage_Hero.umap','PT_Stage_Hero.before_encounters.umap'),
                    ('Content/Core/DataTable/DT_SFStage.uasset','DT_SFStage.before_encounters.uasset')]:
    if not (OUT/name).exists():
        shutil.copy2(ROOT/source,OUT/name)
current=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not current or current.get_name()!='PT_Stage_Hero':
    assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
nav_system=unreal.NavigationSystemV1.get_navigation_system(world)
actors=sub.get_all_level_actors()
labels={a.get_actor_label():a for a in actors}
frame=labels['Hero_OrganicGround'].get_actor_transform()
anchor_class=labels['BP_BuildingSpawn4'].get_class()
player_lane=labels['Hero_PlayerRoute']
player_spline=player_lane.get_editor_property('spline')
route=[player_spline.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD)
       for i in range(player_spline.get_number_of_spline_points())]
assert len(route)==265, 'The authored route changed; review checkpoint placement'

def signature(a):
    p,r,s=a.get_actor_location(),a.get_actor_rotation(),a.get_actor_scale3d()
    record=[a.get_class().get_name(),[p.x,p.y,p.z],[r.pitch,r.yaw,r.roll],[s.x,s.y,s.z],[str(t) for t in a.tags]]
    if isinstance(a,unreal.SFBuildingSpawnAnchor):
        record += [a.get_editor_property('team_index'),str(a.get_editor_property('building_type')),
                   a.get_editor_property('initial_tier_index')]
    if isinstance(a,unreal.ResourceBase):
        record.append(a.get_editor_property('resource_deposit'))
    if isinstance(a,unreal.LanePath):
        spline=a.get_editor_property('spline')
        record.append([[p.x,p.y,p.z] for p in (spline.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD)
            for i in range(spline.get_number_of_spline_points()))])
    return record

baseline={a.get_name():signature(a) for a in actors if TAG not in a.tags}
(OUT/'untouched_actors.json').write_text(json.dumps(baseline,ensure_ascii=False),encoding='utf-8')
excluded=[]
for divider,gaps in GAPS.items():
    for x,_ in gaps:
        p=frame.transform_location(unreal.Vector(x,divider_y(divider,x),0))
        excluded.append(dict(x=p.x,y=p.y,radius=1400.0))

def outside_exclusions(x,y,margin=0):
    return all(math.hypot(x-c['x'],y-c['y'])>c['radius']+margin for c in excluded)

def ground_z(x,y):
    p=frame.inverse_transform_location(unreal.Vector(x,y,0))
    return frame.transform_location(unreal.Vector(p.x,p.y,height(p.x,p.y))).z

def nav(x,y,extent=240):
    p=nav_system.project_point_to_navigation(world,unreal.Vector(x,y,ground_z(x,y)+80),
        None,None,unreal.Vector(extent,extent,400))
    assert p is not None, ('No navigation',x,y)
    assert math.hypot(p.x-x,p.y-y)<=extent+1, ('Navigation moved too far',x,y,str(p))
    return p

# Fixed production is deliberately sparse for one player. The upper outposts
# send their units down the entire winding lane, not through the side cuts.
OUTPOSTS=[
    dict(name='Lower',role='Barracks',tier=0,row='Brawler',xy=(-8050,1250),exit=(650,0),
         delay=90,interval=90,cap=2,house=(-8450,700),mine_role='Ether',mine=(-8400,2300)),
    dict(name='WestTurn',role='Academy',tier=1,row='Stalker',xy=(-1000,-12150),exit=(0,650),
         delay=150,interval=120,cap=2,house=(-450,-12200),mine_role='Mineral',mine=(-1000,-13100)),
    dict(name='UpperTurn',role='Barracks',tier=2,row='Predator',xy=(2650,12050),exit=(0,-650),
         delay=240,interval=180,cap=1,house=(3250,12000),mine_role='Mineral',mine=(1300,12900)),
]
GUARD_GROUPS=[
    ('01_FirstRoad',16,['Brawler','Brawler','Hunter']),
    ('02_LowerOutpost',38,['Brawler','Brawler','Hunter']),
    ('03_FirstBend',58,['Brawler','Hunter','Berserker']),
    ('04_ReturnRoad',88,['Berserker','Berserker','Stalker']),
    ('05_WestBend',132,['Berserker','Berserker','Stalker']),
    ('06_UpperRoad',170,['Berserker','Stalker','Predator']),
    ('07_UpperBend',199,['Berserker','Stalker','Predator']),
    ('08_FinalRoad',233,['Berserker','Predator','ApexHunter']),
    ('09_CoreGuard',262,['Predator','Predator','ApexHunter']),
]
paths={}
for outpost in OUTPOSTS:
    x,y=outpost['xy'];dx,dy=outpost['exit']
    origin=nav(x+dx,y+dy)
    index=min(range(len(route)),key=lambda i:math.hypot(route[i].x-origin.x,route[i].y-origin.y))
    points=[origin]+list(reversed(route[:index+1]))
    for a,b in zip(points,points[1:]):
        path=unreal.NavigationSystemV1.find_path_to_location_synchronously(world,a,b)
        assert path and path.is_valid() and not path.is_partial(), ('Disconnected lane',outpost['name'],str(a),str(b))
    paths[outpost['name']]=points
    outpost['route_index']=index

for a in actors:
    if TAG in a.tags:
        assert sub.destroy_actor(a)

def place(label,cls,x,y,z,yaw=90,margin=0):
    assert outside_exclusions(x,y,margin), ('Red-X exclusion',label,x,y)
    a=sub.spawn_actor_from_class(cls,unreal.Vector(x,y,z),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
    assert a,label
    a.set_actor_location(unreal.Vector(x,y,z),False,False)
    a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
    a.set_actor_label('Stage05_'+label)
    a.set_folder_path('Stage 05/Enemy Encounters')
    a.set_editor_property('tags',[TAG])
    return a

definitions={key:unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/Animal/DA_Animal'+suffix)
             for key,suffix in [('Barracks','Barrack'),('Academy','Ranged'),('House','House'),('Mineral','MineralMine'),('Ether','EtherMine')]}
types={'Barracks':unreal.SFBuildingType.BARRACKS,'Academy':unreal.SFBuildingType.RANGED_PRODUCTION,
       'House':unreal.SFBuildingType.HOUSE,'Mineral':unreal.SFBuildingType.MINERAL_MINE,'Ether':unreal.SFBuildingType.ETHER_MINE}
assert all(definitions.values())

def building(label,role,tier,xy):
    x,y=xy
    a=place(label,anchor_class,x,y,ground_z(x,y)+1,margin=400)
    a.set_actor_rotation(unreal.SFBuildingSpawnLibrary.get_construction_rotation(),False)
    a.set_editor_property('team_index',1)
    a.set_editor_property('building_type',types[role])
    a.set_editor_property('initial_tier_index',tier)
    a.set_editor_property('preview_definition',definitions[role])
    a.set_actor_location(unreal.Vector(x,y,ground_z(x,y)+1),False,False)
    return a

for outpost in OUTPOSTS:
    name=outpost['name'];x,y=outpost['xy'];dx,dy=outpost['exit']
    building(name+'_Factory',outpost['role'],outpost['tier'],outpost['xy'])
    building(name+'_House','House',0,outpost['house'])
    building(name+'_Mine',outpost['mine_role'],0,outpost['mine'])
    lane=place(name+'_AttackLane',unreal.LanePath,x+dx,y+dy,paths[name][0].z,yaw=0)
    lane.set_editor_property('team_index',1)
    lane.set_editor_property('lane_half_width',450.0)
    spline=lane.get_editor_property('spline')
    spline.clear_spline_points(False)
    for p in paths[name]:
        spline.add_spline_point(p,unreal.SplineCoordinateSpace.WORLD,False)
    for i in range(len(paths[name])):
        spline.set_spline_point_type(i,unreal.SplinePointType.LINEAR,False)
    spline.update_spline()
    spawner=place(name+'_Spawner',unreal.UnitSpawner,x,y,ground_z(x,y)+1)
    for key,value in dict(team_index=1,unit_row_name=unreal.Name(outpost['row']),
            unit_class=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Units/Beast_Fox/BP_Fox_'+outpost['row']),
            initial_delay=float(outpost['delay']),spawn_interval=float(outpost['interval']),spawn_count_per_cycle=1,
            max_alive=outpost['cap'],max_total=0,spawn_scatter_radius=100.0,
            spawn_offset=unreal.Vector(dy,-dx,0),lane=lane,start_active=True,spawning_enabled=True,
            wave_controlled=False,stop_when_owner_building_destroyed=True,require_owner_building=True,
            owner_building_search_radius=200.0,owner_building_resolve_timeout=60.0).items():
        spawner.set_editor_property(key,value)

guard_report=[]
for group,index,rows in GUARD_GROUPS:
    center=route[index];a,b=route[index-1],route[index+1]
    dx,dy=b.x-a.x,b.y-a.y
    length=math.hypot(dx,dy);tx,ty=dx/length,dy/length
    for i,(row,(forward,side)) in enumerate(zip(rows,[(-160,-170),(-160,170),(170,0)]),1):
        x=center.x+forward*tx-side*ty;y=center.y+forward*ty+side*tx
        p=nav(x,y,120)
        guard=place('Guard_'+group+'_'+str(i),unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Units/Beast_Fox/BP_Fox_'+row),
                    p.x,p.y,p.z+90,yaw=math.degrees(math.atan2(-ty,-tx)),margin=90)
        for key,value in dict(unit_row_name=unreal.Name(row),auto_advance_teams=set(),
                return_to_hold_location=True,leash_radius=900.0,aggro_radius=900.0,ally_assist_radius=0.0,
                require_line_of_sight=True,require_line_of_sight_for_buildings=True).items():
            guard.set_editor_property(key,value)
        target=guard.get_component_by_class(unreal.CombatTargetComponent)
        target.set_editor_property('team_index',1)
        target.set_editor_property('sight_radius',900.0)
        guard_report.append(dict(label=guard.get_actor_label(),row=row,group=group,route_index=index,xyz=[p.x,p.y,p.z+90]))

table=unreal.load_asset('/Game/Core/DataTable/DT_SFStage')
before=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
(OUT/'stage_table_before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
rows=json.loads(json.dumps(before));stage=next(r for r in rows if r['Name']=='Stage05')
stage['TeamSetups']=['(TeamIndex=1,Race=Beast)']
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(table,False)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,MAP)

assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
saved=sub.get_all_level_actors();saved_labels={a.get_actor_label():a for a in saved}
assert {a.get_name():signature(a) for a in saved if TAG not in a.tags}==baseline, 'Existing map actors changed'
assert len([a for a in saved if isinstance(a,unreal.UnitSpawner)])==3
assert len([a for a in saved if isinstance(a,unreal.UnitBase)])==27
for a in saved:
    if TAG not in a.tags:
        continue
    p=a.get_actor_location()
    assert outside_exclusions(p.x,p.y,400 if isinstance(a,unreal.SFBuildingSpawnAnchor) else 90)
    if isinstance(a,unreal.SFBuildingSpawnAnchor):
        assert abs(a.get_actor_rotation().yaw-90)<.001
        color=a.get_editor_property('grid_preview_component').get_editor_property('shape_color')
        assert color.g>color.r, ('Invalid building grid',a.get_actor_label(),str(color))
    if isinstance(a,unreal.UnitBase):
        assert not a.get_editor_property('auto_advance_teams')
        assert a.get_editor_property('return_to_hold_location')
        assert a.get_editor_property('require_line_of_sight')
        assert a.get_editor_property('require_line_of_sight_for_buildings')
        assert a.get_component_by_class(unreal.CombatTargetComponent).get_team_index()==1
for outpost in OUTPOSTS:
    spawner=saved_labels['Stage05_'+outpost['name']+'_Spawner']
    factory=saved_labels['Stage05_'+outpost['name']+'_Factory']
    assert math.dist([getattr(spawner.get_actor_location(),k) for k in ('x','y','z')],
                     [getattr(factory.get_actor_location(),k) for k in ('x','y','z')])<1
    assert spawner.get_editor_property('require_owner_building')
    assert spawner.get_editor_property('stop_when_owner_building_destroyed')
    assert spawner.get_editor_property('lane').get_editor_property('spline').get_number_of_spline_points()==len(paths[outpost['name']])
after=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
assert [r for r in before if r['Name']!='Stage05']==[r for r in after if r['Name']!='Stage05']
report=dict(passed=True,map=MAP,spawners=3,guards=guard_report,outposts=OUTPOSTS,enemy_buildings=10,
            exclusions=excluded,untouched_actors=len(baseline),stage=next(r for r in after if r['Name']=='Stage05'),
            lane_points={name:[[p.x,p.y,p.z] for p in points] for name,points in paths.items()})
(OUT/'configuration_verified.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('STAGE05_ENCOUNTERS_VERIFIED: 3 owner-gated spawners, 27 sight-gated guards, two empty side cuts')
