"""Create the approved terrain-only final map, two nexus anchors and four lanes.

Run via UnrealEditor-Cmd -run=pythonscript -script=<this file> -NullRHI.
Existing levels are preserved. This script refuses to overwrite Stage_Final.
"""
import json
import math
import sys
from pathlib import Path
import unreal

sys.path.insert(0,str(Path(__file__).parent))
import final_map_layout as layout

MAP,ART = layout.MAP,layout.ART
updating='-FinalUpdate' in unreal.SystemLibrary.get_command_line()
if unreal.EditorAssetLibrary.does_asset_exist(MAP) and not updating:
    raise RuntimeError('Stage_Final already exists; refusing to replace authored content.')
source = Path(unreal.Paths.project_dir())/'ContentSource/Stage_Final'
layout.generate_sources(source)
unreal.EditorAssetLibrary.make_directory(ART)

# Explicit legacy OBJ import keeps these authored centimetre/Z-up vertices.
def import_mesh(filename):
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(source/filename))
    task.set_editor_property('destination_path',ART)
    task.set_editor_property('destination_name',Path(filename).stem)
    task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',updating)
    task.set_editor_property('save',True)
    options=unreal.FbxImportUI()
    options.set_editor_property('import_mesh',True)
    options.set_editor_property('import_materials',False)
    options.set_editor_property('import_textures',False)
    options.set_editor_property('import_as_skeletal',False)
    options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property('automated_import_should_detect_type',False)
    settings=options.get_editor_property('static_mesh_import_data')
    settings.set_editor_property('combine_meshes',True)
    settings.set_editor_property('auto_generate_collision',False)
    settings.set_editor_property('convert_scene',False)
    settings.set_editor_property('convert_scene_unit',False)
    task.set_editor_property('options',options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths=task.get_editor_property('imported_object_paths')
    meshes=[unreal.load_asset(p) for p in paths if isinstance(unreal.load_asset(p),unreal.StaticMesh)]
    if len(meshes)!=1:
        raise RuntimeError('Expected one mesh from '+filename+': '+str(paths))
    mesh=meshes[0]
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh

ground=import_mesh('SM_Final_Ground.obj')
block_meshes={name:import_mesh('SM_Final_'+name+'.obj') for name in layout.BLOCKERS}
ground_bounds=ground.get_bounding_box()
if abs((ground_bounds.max.z-ground_bounds.min.z)-200.)>1.:
    raise RuntimeError('OBJ importer changed Z-up orientation: '+str(ground_bounds))
for name,polygon in layout.BLOCKERS.items():
    expected=[layout.world(p) for p in polygon]
    bounds=block_meshes[name].get_bounding_box()
    for axis in ('x','y'):
        j=0 if axis=='x' else 1
        assert abs(getattr(bounds.min,axis)-min(p[j] for p in expected))<1,(name,axis,'minimum')
        assert abs(getattr(bounds.max,axis)-max(p[j] for p in expected))<1,(name,axis,'maximum')

ml=unreal.MaterialEditingLibrary
def material(name,code):
    mat=unreal.load_asset(ART+'/'+name)
    if mat:
        ml.delete_all_material_expressions(mat)
    else:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ART,unreal.Material,unreal.MaterialFactoryNew())
    def node(cls,**props):
        result=ml.create_material_expression(mat,cls)
        for key,value in props.items(): result.set_editor_property(key,value)
        return result
    position=node(unreal.MaterialExpressionWorldPosition)
    input_=unreal.CustomInput()
    input_.set_editor_property('input_name','P')
    color=node(unreal.MaterialExpressionCustom,code=code,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,inputs=[input_])
    ml.connect_material_expressions(position,'',color,'P')
    ml.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=node(unreal.MaterialExpressionConstant,r=.94)
    ml.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    spec=node(unreal.MaterialExpressionConstant,r=.12)
    ml.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    ml.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

segments=[]
for lane in layout.LANES:
    for a,b in zip(lane,lane[1:]):
        segments.append('d=min(d, length(p-(float2(%.3f,%.3f)+clamp(dot(p-float2(%.3f,%.3f),float2(%.3f,%.3f))/%.3f,0,1)*float2(%.3f,%.3f))));'%(a[0],a[1],a[0],a[1],b[0]-a[0],b[1]-a[1],(b[0]-a[0])**2+(b[1]-a[1])**2,b[0]-a[0],b[1]-a[1]))
base_masks=[]
for n in layout.NEXUSES.values():
    x,y,_=n['xyz']
    base_masks.append('road=max(road,1-smoothstep(2350,2700,length(p-float2(%.1f,%.1f))));'%(x,y))
for reserve in layout.RESERVES.values():
    x,y,_=reserve['center']
    sx,sy=reserve['size']
    base_masks.append('road=max(road,1-smoothstep(-120,180,max(abs(p.x-(%.1f))-%.1f,abs(p.y-(%.1f))-%.1f)));'%(x,sx/2,y,sy/2))
floor_code='float2 p=P.xy; float d=1e8;\n'+'\n'.join(segments)+'\nfloat road=1-smoothstep(980,1160,d);\n'+'\n'.join(base_masks)+'''
float grain=sin(p.x*.021)*sin(p.y*.019)*.012;
float broad=sin(p.x*.00051)*sin(p.y*.00037)*.035;
float3 grass=float3(.18,.245,.115)*(1+broad+grain);
float3 earth=float3(.48,.365,.22)*(1+broad*.3+grain);
return lerp(grass,earth,road);
'''
floor_mat=material('M_Final_Terrain',floor_code)
cliff_mat=material('M_Final_Impassable','float grain=sin(P.x*.013)*sin(P.y*.012)*.015; return float3(.095,.125,.105)*(1+grain);')

if updating:
    assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
else:
    assert unreal.EditorLevelLibrary.new_level(MAP)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.EditorLevelLibrary.get_editor_world()
mode=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Blueprints/GameMode/BP_SFGameMode')
assert mode
world.get_world_settings().set_editor_property('default_game_mode',mode)
TAG=unreal.Name('SF_StageFinal')
def spawn(cls,label,xyz,folder,yaw=0):
    actor=next((a for a in sub.get_all_level_actors() if a.get_actor_label()==label),None) if updating else None
    if actor:
        assert TAG in actor.tags, 'Refusing to change unowned actor '+label
        actor.set_actor_location_and_rotation(unreal.Vector(*xyz),unreal.Rotator(pitch=0,yaw=yaw,roll=0),False,False)
    else:
        actor=sub.spawn_actor_from_class(cls,unreal.Vector(*xyz),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
    if not actor: raise RuntimeError('Failed to spawn '+label)
    actor.set_actor_label(label)
    actor.set_folder_path('Stage Final/'+folder)
    actor.set_editor_property('tags',[TAG])
    return actor
def mesh_actor(label,mesh,mat):
    actor=spawn(unreal.StaticMeshActor,label,(0,0,0),'Terrain')
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_material(0,mat)
    actor.static_mesh_component.set_collision_profile_name('BlockAll')
    return actor
mesh_actor('Final_Playable_Ground',ground,floor_mat)
for name,mesh in block_meshes.items():
    mesh_actor('Final_Impassable_'+name,mesh,cliff_mat)
    bounds=mesh.get_bounding_box()
    center=((bounds.min.x+bounds.max.x)/2,(bounds.min.y+bounds.max.y)/2,1300.)
    null=spawn(unreal.NavModifierVolume,'Final_NoWalk_'+name,center,'Navigation')
    null.set_editor_property('area_class',unreal.NavArea_Null)
    # Affect the cliff tops only. The closed solid collision blocks ground
    # movement; a rectangular modifier reaching the floor would clip corners.
    null.set_actor_scale3d(unreal.Vector((bounds.max.x-bounds.min.x)/200,(bounds.max.y-bounds.min.y)/200,8))

# PreviewDefinition displays the proper racial nexus in-editor; the standard
# GameMode bootstrap creates the functional nexus from this same anchor in play.
anchor_cls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Structure/Buildings/BuildingSpawner/BP_BuildingSpawn')
assert anchor_cls
for race,data in layout.NEXUSES.items():
    definition=unreal.load_asset(data['definition'])
    nexus=spawn(anchor_cls,'Final_'+race+'_Nexus',data['xyz'],'Nexuses',data['yaw'])
    nexus.set_editor_property('team_index',data['team'])
    nexus.set_editor_property('building_type',unreal.SFBuildingType.NEXUS)
    nexus.set_editor_property('initial_tier_index',0)
    nexus.set_editor_property('preview_definition',definition)
    nexus.set_actor_location_and_rotation(unreal.Vector(*data['xyz']),unreal.Rotator(pitch=0,yaw=data['yaw'],roll=0),False,False)
    preview=nexus.get_editor_property('preview_mesh_component')
    if not preview.get_editor_property('static_mesh'):
        tier=definition.get_editor_property('tiers')[0]
        completed=tier.get_editor_property('visual').get_editor_property('completed_mesh')
        preview.set_static_mesh(unreal.load_asset(str(completed).split("'")[1]) if "'" in str(completed) else completed)

for i,points in enumerate(layout.LANES,1):
    for team,route in ((1,points),(0,list(reversed(points)))):
        lane=spawn(unreal.LanePath,'Final_Lane_%02d_%s'%(i,'Beast' if team==1 else 'Etherian'),(*route[0],0),'Attack Lanes')
        lane.set_editor_property('team_index',team)
        lane.set_editor_property('lane_half_width',layout.LANE_WIDTH/2)
        spline=lane.get_editor_property('spline')
        spline.clear_spline_points(False)
        for x,y in route:
            spline.add_spline_point(unreal.Vector(x,y,0),unreal.SplineCoordinateSpace.WORLD,False)
        for j in range(len(route)):
            spline.set_spline_point_type(j,unreal.SplinePointType.LINEAR,False)
        spline.update_spline()

# Editor-only locator actors identify the empty resource areas without placing
# any mineral/ether nodes, units or additional buildings into the actual level.
for race,reserve in layout.RESERVES.items():
    marker=spawn(unreal.TargetPoint,'Final_'+race+'_ResourceReserve_54x36m',reserve['center'],'Resource Reserves (empty)')
    marker.set_editor_property('is_editor_only_actor',True)
    marker.set_editor_property('tags',[TAG,unreal.Name('ResourceReserve_54x36m')])

spawn(unreal.PlayerStart,'Final_PlayerStart_Etherian',(-7000,11500,100),'Player')
nav=spawn(unreal.NavMeshBoundsVolume,'Final_NavMeshBounds',(0,0,300),'Navigation')
nav.set_actor_scale3d(unreal.Vector(layout.HEIGHT/200,layout.WIDTH/200,20))
sun=spawn(unreal.DirectionalLight,'Final_Sun',(0,0,500),'Lighting')
sun.set_actor_rotation(unreal.Rotator(pitch=-65,yaw=-30,roll=0),False)
sun.light_component.set_editor_property('intensity',3.)
sky=spawn(unreal.SkyLight,'Final_SkyLight',(0,0,1000),'Lighting')
sky.light_component.set_editor_property('intensity',.8)
spawn(unreal.SkyAtmosphere,'Final_Atmosphere',(0,0,0),'Lighting')

unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(0,0,45000),unreal.Rotator(pitch=-90,yaw=0,roll=0))
assert unreal.EditorLoadingAndSavingUtils.save_current_level()

# Stage07 is the previously unassigned final stage. Keep its order/UI/art and
# building rules while supplying the actual map and its two faction teams.
table=unreal.load_asset('/Game/Core/DataTable/DT_SFStage')
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
backup=Path(unreal.Paths.project_saved_dir(),'StageFinal')
backup.mkdir(exist_ok=True)
if not (backup/'stage_table_before.json').exists():
    (backup/'stage_table_before.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
stage=next(r for r in rows if r['Name']=='Stage07')
if stage['StageLevel'] not in ('None',MAP+'.Stage_Final'):
    raise RuntimeError('Stage07 is already assigned; map saved but stage table preserved.')
stage.update(StageLevel=MAP+'.Stage_Final',PlayerRace='Human',TeamSetups=['(TeamIndex=1,Race=Beast)'],Minimap='(bAutoDetectBounds=False,WorldMin=(X=-14400,Y=-18000),WorldMax=(X=14400,Y=18000),CaptureHeight=22000,BoundsPadding=200)')
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows,ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(table)
report=dict(map=MAP,dimensions_cm=[layout.WIDTH,layout.HEIGHT],lane_width_cm=layout.LANE_WIDTH,physical_lanes=4,team_splines=8,nexuses=layout.NEXUSES,resource_reserves=layout.RESERVES,blockers=list(layout.BLOCKERS),actors=len(sub.get_all_level_actors()),ground_bounds=str(ground_bounds),stage=stage)
(backup/'creation_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('STAGE_FINAL_CREATED: four lanes, two nexuses, two empty 54x36m resource reserves')
