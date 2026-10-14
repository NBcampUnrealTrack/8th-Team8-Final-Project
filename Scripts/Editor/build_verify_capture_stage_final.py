"""Build final-map navigation and verify actual editor geometry, then capture.

Run in the full Unreal Editor using -ExecutePythonScript=<this file>.
Only adds missing SF_StageFinal navigation modifiers; saves the resulting level.
"""
import json
import math
import sys
import time
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).parent))
import final_map_layout as layout

folder=Path(unreal.Paths.project_saved_dir(),'StageFinal')
folder.mkdir(exist_ok=True)
preview=Path(unreal.Paths.project_dir(),'output/StageFinal')
preview.mkdir(parents=True,exist_ok=True)
assert unreal.EditorLoadingAndSavingUtils.load_map(layout.MAP)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.EditorLevelLibrary.get_editor_world()
actors=sub.get_all_level_actors()
by_label={a.get_actor_label():a for a in actors}
for name in layout.BLOCKERS:
    label='Final_NoWalk_'+name
    if label in by_label: continue
    bounds=by_label['Final_Impassable_'+name].static_mesh_component.static_mesh.get_bounding_box()
    center=unreal.Vector((bounds.min.x+bounds.max.x)/2,(bounds.min.y+bounds.max.y)/2,1300.)
    null=sub.spawn_actor_from_class(unreal.NavModifierVolume,center)
    null.set_actor_label(label)
    null.set_folder_path('Stage Final/Navigation')
    null.set_editor_property('tags',[unreal.Name('SF_StageFinal')])
    null.set_editor_property('area_class',unreal.NavArea_Null)
    null.set_actor_scale3d(unreal.Vector((bounds.max.x-bounds.min.x)/200,(bounds.max.y-bounds.min.y)/200,8))
unreal.SystemLibrary.execute_console_command(world,'RebuildNavigation')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
for cmd in ('r.TextureStreaming 0','r.ScreenPercentage 100','r.AntiAliasingMethod 2','r.BloomQuality 0'):
    unreal.SystemLibrary.execute_console_command(world,cmd)

state={'phase':'build','start':time.time(),'shot':0}
shots=[
    ('ForestOverview.png',(-22000,0,40000),(-4000,0,0),65.),
    ('EtherianBase.png',(-4600,7800,4600),(-9700,12600,200),60.),
    ('BeastBase.png',(4000,-7500,4600),(10000,-12600,200),60.),
    ('LaneView.png',(-5000,-1800,6000),(4500,-2500,100),60.),
]
camera=sub.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(0,0,36000),unreal.Rotator(pitch=-90,yaw=0,roll=0))
camera.set_actor_label('Final_Temporary_PreviewCamera')
capture=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
target=unreal.RenderingLibrary.create_render_target2d(world,1920,1440,unreal.TextureRenderTargetFormat.RTF_RGBA8)
target.set_editor_property('target_gamma',2.2)
capture.set_editor_property('texture_target',target)
capture.set_editor_property('projection_type',unreal.CameraProjectionMode.PERSPECTIVE)
capture.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture.set_editor_property('capture_every_frame',False)
capture.set_editor_property('capture_on_movement',False)
def set_shot(index):
    name,eye,look,fov=shots[index]
    eye=unreal.Vector(*eye)
    rotation=unreal.MathLibrary.find_look_at_rotation(eye,unreal.Vector(*look))
    camera.set_actor_location_and_rotation(eye,rotation,False,False)
    capture.set_editor_property('fov_angle',fov)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(eye,rotation)
set_shot(0)

def path_ok(a,b):
    path=unreal.NavigationSystemV1.find_path_to_location_synchronously(world,unreal.Vector(a[0],a[1],80),unreal.Vector(b[0],b[1],80))
    return bool(path and path.is_valid() and not path.is_partial())

def verify():
    actors=sub.get_all_level_actors()
    by_label={a.get_actor_label():a for a in actors}
    anchors=[a for a in actors if isinstance(a,unreal.SFBuildingSpawnAnchor)]
    assert len(anchors)==2
    assert all(a.get_editor_property('building_type')==unreal.SFBuildingType.NEXUS for a in anchors)
    for race,data in layout.NEXUSES.items():
        nexus=by_label['Final_'+race+'_Nexus']
        assert nexus.get_editor_property('team_index')==data['team']
        assert nexus.get_editor_property('preview_mesh_component').static_mesh
        assert str(nexus.get_editor_property('preview_definition').get_path_name()).startswith(data['definition'])
    assert len([a for a in actors if isinstance(a,unreal.LanePath)])==8
    assert not any(isinstance(a,unreal.ResourceBase) for a in actors)
    assert not any(isinstance(a,unreal.UnitBase) for a in actors)
    assert not any('UnitSpawner' in a.get_class().get_name() for a in actors)
    origin,extent=by_label['Final_NavMeshBounds'].get_actor_bounds(False)
    assert abs(extent.x-14400)<1 and abs(extent.y-18000)<1,(origin,extent)
    nav_extent=[extent.x,extent.y,extent.z]
    lane_checks=[]
    for i,points in enumerate(layout.LANES,1):
        checks=[path_ok(a,b) for a,b in zip(points,points[1:])]
        lane_checks.append(dict(lane=i,segments=checks,full_path=path_ok(points[0],points[-1])))
        assert all(checks),lane_checks
        assert lane_checks[-1]['full_path'],lane_checks
        for team in (0,1):
            actor=by_label['Final_Lane_%02d_%s'%(i,'Beast' if team==1 else 'Etherian')]
            expected=points if team==1 else list(reversed(points))
            spline=actor.get_editor_property('spline')
            assert spline.get_number_of_spline_points()==len(expected)
            assert abs(actor.get_editor_property('lane_half_width')-1100)<.1
            for j,p in enumerate(expected):
                v=spline.get_location_at_spline_point(j,unreal.SplineCoordinateSpace.WORLD)
                assert math.hypot(v.x-p[0],v.y-p[1])<1
    reserve_checks={}
    for race,reserve in layout.RESERVES.items():
        cx,cy,_=reserve['center']
        sx,sy=reserve['size']
        points=[(cx+dx*sx/2,cy+dy*sy/2) for dx in (-.94,0,.94) for dy in (-.94,0,.94)]
        # Recast returns a one-point (invalid) path for identical endpoints.
        # Use a nearby distinct start to verify navigation at the center too.
        checks=[path_ok((cx+100,cy),p) for p in points]
        assert all(checks),(race,checks)
        for point in points:
            assert all(not lane.is_inside_lane(unreal.Vector(*point,0)) for lane in actors if isinstance(lane,unreal.LanePath)),(race,point,'resource space overlaps lane')
        reserve_checks[race]=dict(size_m=[sx/100,sy/100],navigation=checks)
    # Null modifiers must reject path endpoints on each impassable island.
    blocker_checks={}
    for name in layout.BLOCKERS:
        actor=by_label['Final_Impassable_'+name]
        center,extent=actor.get_actor_bounds(False)
        path=unreal.NavigationSystemV1.find_path_to_location_synchronously(world,unreal.Vector(*layout.LANES[0][0],80),unreal.Vector(center.x,center.y,850))
        blocked=not path or not path.is_valid() or path.is_partial()
        assert blocked,name
        blocker_checks[name]=blocked
    table=unreal.load_asset('/Game/Core/DataTable/DT_SFStage')
    rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    stage=next(r for r in rows if r['Name']=='Stage07')
    assert stage['StageLevel']==layout.MAP+'.Stage_Final'
    assert stage['PlayerRace']=='Human' and stage['TeamSetups']==['(TeamIndex=1,Race=Beast)']
    baseline=json.loads((folder/'stage_table_before.json').read_text(encoding='utf-8'))
    assert [r for r in rows if r['Name']!='Stage07']==[r for r in baseline if r['Name']!='Stage07']
    forest=None
    forest_report=folder/'forest_dressing_report.json'
    if forest_report.exists():
        report=json.loads(forest_report.read_text(encoding='utf-8'))
        total=0
        foliage_components=[c for a in actors if isinstance(a,unreal.InstancedFoliageActor) for c in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent)]
        for path,expected in report['foliage_assets'].items():
            ft=unreal.load_asset(path)
            assert ft and ft.get_editor_property('body_instance').get_editor_property('collision_enabled')==unreal.CollisionEnabled.NO_COLLISION
            actual=sum(c.get_instance_count() for c in foliage_components if c.static_mesh==ft.get_editor_property('mesh'))
            assert actual==expected,(path,actual,expected)
            total+=actual
        assert total==report['instances'] and total>500
        for placement in report['placements']:
            x,y=placement['xy']; radius=placement['radius']
            for race,reserve in layout.RESERVES.items():
                cx,cy,_=reserve['center']; sx,sy=reserve['size']
                assert abs(x-cx)>=sx/2+radius+120 or abs(y-cy)>=sy/2+radius+120,(race,placement)
        assert by_label['Final_Playable_Ground'].static_mesh_component.get_material(0).get_name()=='M_FinalForestFloor'
        assert all(not by_label['Final_Impassable_'+name].static_mesh_component.get_editor_property('visible') for name in layout.BLOCKERS)
        forest=dict(instances=total,counts=report['counts'],reserves_clear=True)
    result=dict(passed=True,lanes=lane_checks,resources=reserve_checks,blockers=blocker_checks,nexuses=2,other_buildings=0,resource_nodes=0,nav_bounds_cm=nav_extent,stage=stage,forest=forest)
    (folder/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('STAGE_FINAL_VERIFIED: all four lanes, both resource reserves, six blocked areas')

def finish():
    state['phase']='finished'
    for actor in sub.get_all_level_actors():
        if isinstance(actor,unreal.SFBuildingSpawnAnchor):
            actor.get_editor_property('preview_mesh_component').set_hidden_in_game(True)
    sub.destroy_actor(camera)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,layout.MAP)
    (folder/'verification_error.txt').unlink(missing_ok=True)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    try:
        elapsed=time.time()-state['start']
        if state['phase']=='build' and elapsed>25:
            verify()
            if '-FinalNavOnly' in unreal.SystemLibrary.get_command_line():
                finish()
                return
            for actor in sub.get_all_level_actors():
                if isinstance(actor,unreal.SFBuildingSpawnAnchor):
                    actor.get_editor_property('preview_mesh_component').set_hidden_in_game(False)
            set_shot(0)
            capture.capture_scene()
            state.update(phase='capture',start=time.time())
        elif state['phase']=='capture' and elapsed>8:
            name=shots[state['shot']][0]
            unreal.RenderingLibrary.export_render_target(world,target,str(preview),name)
            assert (preview/name).exists(),name
            state['shot']+=1
            if state['shot']==len(shots): finish()
            else:
                set_shot(state['shot'])
                capture.capture_scene()
                state['start']=time.time()
    except Exception as exc:
        import traceback
        (folder/'verification_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
        unreal.log_error(str(exc))
        sub.destroy_actor(camera)
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
