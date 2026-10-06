"""One-time 10% XY expansion. Keep elevation and gameplay actor scales unchanged."""
import unreal, json, re, shutil
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir())
OUT=ROOT/'Saved/Ambush/Revision3';OUT.mkdir(parents=True,exist_ok=True)
assert not (OUT/'enlarged.json').exists(), 'Expansion already applied'
MAP='/Game/Game/Stage/PT_SFStageAmbush';S=1.1
for path in ('Content/Game/Stage/PT_SFStageAmbush.umap','Content/Core/DataTable/DT_SFStage.uasset','Content/Game/Stage/AmbushArt/M_AmbushGround.uasset'):
 shutil.copy2(ROOT/path,OUT/Path(path).name)
w=unreal.EditorLoadingAndSavingUtils.load_map(MAP);assert w
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=list(sub.get_all_level_actors())
assert not any(a.get_attach_parent_actor() for a in actors), 'Attached actor needs separate handling'
preserved=[];scaled=0;lanes=[]
for a in actors:
 if isinstance(a,(unreal.DirectionalLight,unreal.SkyLight,unreal.SkyAtmosphere,unreal.RecastNavMesh)):continue
 p=a.get_actor_location();s=a.get_actor_scale3d();points=[]
 if isinstance(a,unreal.LanePath):
  sp=a.get_editor_property('spline');points=[sp.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD) for i in range(sp.get_number_of_spline_points())]
 a.set_actor_location(unreal.Vector(p.x*S,p.y*S,p.z),False,False)
 if isinstance(a,(unreal.Pawn,unreal.PlayerStart,unreal.ResourceBase,unreal.SFBuildingSpawnAnchor,unreal.UnitSpawner)):
  preserved.append({'label':a.get_actor_label(),'scale':[s.x,s.y,s.z]})
 else:
  a.set_actor_scale3d(unreal.Vector(s.x*S,s.y*S,s.z));scaled+=1
 if points:
  error=max((sp.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD)-unreal.Vector(q.x*S,q.y*S,q.z)).length() for i,q in enumerate(points))
  assert error<.1,error;lanes.append(error)
# The authored brush can have an offset pivot; fit its actual bounds to the full retreat.
nav=next(a for a in actors if isinstance(a,unreal.NavMeshBoundsVolume))
c,e=nav.get_actor_bounds(False);s=nav.get_actor_scale3d()
nav.set_actor_scale3d(unreal.Vector(s.x*10500*S/e.x,s.y*11600*S/e.y,s.z*3000/e.z))
c,e=nav.get_actor_bounds(False)
nav.set_actor_location(nav.get_actor_location()+unreal.Vector(0,0,300)-c,False,False)
m=unreal.load_asset('/Game/Game/Stage/AmbushArt/M_AmbushGround');changed=0
for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
 if n.get_outer()==m and 'float2 p=P.xy;' in n.get_editor_property('code'):
  n.set_editor_property('code',n.get_editor_property('code').replace('float2 p=P.xy;','float2 p=P.xy/1.1; /* AMBUSH_XY_SCALE */'));changed+=1
assert changed>=1,changed
unreal.MaterialEditingLibrary.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
t=unreal.load_asset('/Game/Core/DataTable/DT_SFStage');rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(t));before=json.loads(json.dumps(rows))
row=next(r for r in rows if r['Name']=='Stage06');assert 'PT_SFStageAmbush' in row['StageLevel']
row['Minimap']=re.sub(r'(?P<key>\b[XY]|CaptureHeight|BoundsPadding)=(?P<num>-?\d+(?:\.\d+)?)',lambda m:m['key']+'='+str(float(m['num'])*S),row['Minimap'])
assert all(a==b for a,b in zip(before,rows) if a['Name']!='Stage06')
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(t,json.dumps(rows,ensure_ascii=False))
assert unreal.EditorAssetLibrary.save_loaded_asset(t)
assert unreal.EditorLoadingAndSavingUtils.save_map(w,MAP)
(OUT/'enlarged.json').write_text(json.dumps({'factor_xy':S,'factor_z':1,'scaled_actors':scaled,'preserved_gameplay_scales':preserved,'spline_errors':lanes,'minimap':row['Minimap']},indent=2),encoding='utf-8')
unreal.log('AMBUSH_EXPANDED_SUCCESS')
