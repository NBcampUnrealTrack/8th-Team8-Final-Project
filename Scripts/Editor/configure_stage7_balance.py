"""Idempotent Stage07-only setup; preserve scenery and back up the map before saving."""
import json
import shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
out=root/'Saved/Stage7Balance'
out.mkdir(parents=True,exist_ok=True)
backup=out/'before_balance_PT_SFStageFinal.umap'
if not backup.exists(): shutil.copy2(root/'Content/Game/Stage/PT_SFStageFinal.umap',backup)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_SFStageFinal')
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
labels={a.get_actor_label():a for a in sub.get_all_level_actors()}
anchor_class=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Structure/Buildings/BuildingSpawner/BP_BuildingSpawn')
tag=unreal.Name('SF_Stage7_Balance')
unit_data={r['Name']:r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(unreal.load_asset('/Game/Core/DataTable/DT_Units')))}

def place(cls,label,p):
    a=labels.get(label)
    if not a:
        a=sub.spawn_actor_from_class(cls,p)
        assert a,label
        a.set_actor_label(label)
        a.set_folder_path('Gameplay/Stage Setup')
        a.tags=list(a.tags)+[tag]
        labels[label]=a
    a.set_actor_location(p,False,False)
    return a

def building(label,team,race,role,p):
    a=place(anchor_class,label,p)
    suffix,kind={'Special':('Special',unreal.SFBuildingType.SPECIAL_PRODUCTION)}[role]
    a.set_editor_property('team_index',team)
    a.set_editor_property('building_type',kind)
    a.set_editor_property('preview_definition',unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/'+race+'/DA_'+race+suffix))
    a.set_editor_property('initial_tier_index',0)
    a.set_actor_rotation(unreal.SFBuildingSpawnLibrary.get_construction_rotation(),False)
    return a

# Mirror the starter economy and factory spacing around the authored faction Nexuses.
# The old Beast ether deposit was almost four thousand cm from its Nexus.
human_base=labels['Final_EtherNexus'].get_actor_location()
beast_base=labels['Final_BeastNexus'].get_actor_location()
def mirror(p): return unreal.Vector(beast_base.x-(p.x-human_base.x),beast_base.y-(p.y-human_base.y),33.25)
for suffix in ['House0','House1','Barracks','Ranged']:
    labels['Stage07_1_'+suffix].set_actor_location(mirror(labels['Stage07_0_'+suffix].get_actor_location()),False,False)
for i in range(4):
    for source,dest in [('Final_Mineral_0_','Final_Mineral_2_'),('Final_Ether_0_','Final_Ether_1_'),('Final_Mineral_1_','Stage07_BeastReserveMineral_')]:
        src=labels[source+str(i)]
        a=place(src.get_class(),dest+str(i),mirror(src.get_actor_location()))
        a.set_actor_scale3d(src.get_actor_scale3d())
        a.set_actor_rotation(src.get_actor_rotation(),False)
        a.set_editor_property('resource_deposit',5000 if 'Mineral' in dest else 3500)
for team, mineral,ether in [(0,'Final_Mineral_0_0','Final_Ether_0_0'),(1,'Final_Mineral_2_0','Final_Ether_1_0')]:
    for role,resource in [('Mineral',mineral),('Ether',ether)]:
        labels['Stage07_%d_%sMine'%(team,role)].set_actor_location(labels[resource].get_actor_location(),False,False)
for a in labels.values():
    if isinstance(a,unreal.ResourceBase):
        a.set_editor_property('resource_deposit',5000 if 'Mineral' in a.get_actor_label() else 3500)
    if isinstance(a,unreal.SFBuildingSpawnAnchor):
        a.set_editor_property('initial_tier_index',2 if a.get_editor_property('building_type')==unreal.SFBuildingType.NEXUS else 0)

roles={
    'Human':{'Barracks':['Fighter','Warrior','Guardian'],'Ranged':['Shooter','Sharpshooter','Judicator'],'Special':['Assassin','Colossus']},
    'Animal':{'Barracks':['Brawler','Berserker','Predator'],'Ranged':['Hunter','Stalker','ApexHunter'],'Special':['SpiritShaman','WarBeast']}}
summary=[]
for team,race in [(0,'Human'),(1,'Animal')]:
    special_position=unreal.Vector(-8500,8100,33.25) if team==0 else mirror(unreal.Vector(-8500,8100,33.25))
    building('Stage07_%d_Special'%team,team,race,'Special',special_position)
    for index,role in enumerate(['Barracks','Ranged','Special']):
        factory=labels['Stage07_%d_%s'%(team,role)]
        s=place(unreal.UnitSpawner,'Stage07_%d_AI_%s'%(team,role),factory.get_actor_location())
        # Opposing ends use the matching nearby entrance rather than spawning across the Nexus.
        lane_number=([1,3,4] if team==0 else [3,1,2])[index]
        lane=labels['Final_Lane_%d_Team%d'%(lane_number,team)]
        entry=lane.get_editor_property('spline').get_location_at_spline_point(0,unreal.SplineCoordinateSpace.WORLD)
        rows=roles[race][role]
        classes=[unreal.load_class(None,unit_data[row]['UnitClass']) for row in rows]
        assert all(classes),(race,role)
        phases=[]
        weights=[(240,[5,0]),(360,[4,1])] if role=='Special' else [(0,[10,0,0]),(120,[7,3,0]),(240,[4,5,1]),(360,[3,5,2])]
        for seconds,mix in weights:
            pool=[]
            for row,cls,weight in zip(rows,classes,mix):
                if weight==0: continue
                e=unreal.SFSpawnPoolEntry()
                e.set_editor_property('row_name',row)
                e.set_editor_property('unit_class',cls)
                e.set_editor_property('weight',weight)
                pool.append(e)
            p=unreal.SFSpawnPhase()
            p.set_editor_property('start_seconds',seconds)
            p.set_editor_property('unit_pool',pool)
            phases.append(p)
        interval=120. if role=='Special' else ([32.,38.] if race=='Human' else [26.,32.])[index]
        delay=[90.,105.,240.][index]
        for key,value in dict(team_index=team,unit_row_name=rows[0],unit_class=classes[0],spawn_phases=phases,
            initial_delay=delay,spawn_interval=interval,spawn_count_per_cycle=1,max_alive=[10,8,2][index],max_total=0,
            spawn_offset=entry-factory.get_actor_location(),spawn_scatter_radius=160.,lane=lane,
            start_active=True,spawning_enabled=True,wave_controlled=False,stop_when_owner_building_destroyed=True,
            require_owner_building=True,owner_building_search_radius=200.,owner_building_resolve_timeout=120.).items():
            s.set_editor_property(key,value)
        s.set_actor_rotation(unreal.Rotator(),False)
        summary.append(dict(team=team,role=role,delay=delay,interval=interval,cap=[10,8,2][index],phases=weights))
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
(out/'configured.json').write_text(json.dumps(dict(buildings=16,nexus_tier=3,production_tier=1,
    special_per_team=1,mineral_per_deposit=5000,ether_per_deposit=3500,spawners=summary),indent=2),encoding='utf-8')
unreal.log('STAGE7_BALANCE_CONFIGURED')
