import json
from pathlib import Path
import unreal

out = {}
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_2')
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in sub.get_all_level_actors():
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    if not c or not c.static_mesh:
        continue
    m = c.static_mesh
    name = m.get_name()
    if not any(w.lower() in name.lower() for w in ['tree','oak','pine','bush','rock','ruined','grass','cliff']):
        continue
    if name in out:
        out[name]['count'] += 1
        continue
    b = m.get_bounds()
    s = a.get_actor_scale3d()
    out[name] = dict(path=m.get_path_name(), count=1, scale=[s.x,s.y,s.z],
                     origin=[b.origin.x,b.origin.y,b.origin.z],
                     extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z],
                     materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())])
Path(unreal.Paths.project_saved_dir(),'hero_scenery_assets.json').write_text(json.dumps(out,indent=2))
docs = {n:getattr(unreal.AutomationLibrary,n).__doc__ for n in dir(unreal.AutomationLibrary) if 'screenshot' in n}
docs['viewport']=unreal.EditorLevelLibrary.set_level_viewport_camera_info.__doc__
Path(unreal.Paths.project_saved_dir(),'hero_capture_api.json').write_text(json.dumps(docs,indent=2))
