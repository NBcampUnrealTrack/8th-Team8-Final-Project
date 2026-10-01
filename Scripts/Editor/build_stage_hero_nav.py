"""Build and save Stage_Hero navigation in the full Unreal Editor."""
import json
import sys
from pathlib import Path
import unreal
sys.path.insert(0,str(Path(__file__).parent))
from hero_organic_layout import SECTIONS,height

MAP = '/Game/Game/Stage/PT_Stage_Hero'
assert unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.EditorLevelLibrary.get_editor_world()
floor = next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
             if a.get_actor_label() == 'Hero_OrganicGround')
frame = floor.get_actor_transform()
unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
checks = []
for section in SECTIONS:
    points=section[::3]
    if points[-1]!=section[-1]:points.append(section[-1])
    valid=True
    for a,b in zip(points,points[1:]):
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(
            world, frame.transform_location(unreal.Vector(a[0],a[1],height(*a)+92)),
            frame.transform_location(unreal.Vector(b[0],b[1],height(*b)+92)))
        valid=valid and bool(p and p.is_valid() and not p.is_partial())
    checks.append(valid)
saved = all(checks) and unreal.EditorLoadingAndSavingUtils.save_current_level()
Path(unreal.Paths.project_saved_dir(), 'stage_hero_nav_build.json').write_text(
    json.dumps(dict(saved=saved, checks=checks)), encoding='utf-8')
unreal.SystemLibrary.quit_editor()
