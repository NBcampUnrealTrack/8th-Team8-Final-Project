"""Disable clear rewards for only the two small camps at Stage 02's top/bottom."""
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
source = root / 'Content/Game/Stage/PT_Stage_2.umap'
backup = root / 'Saved/NeutralSlimes/PT_Stage_2.before_reward_exclusion.umap'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(source, backup)
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_2')
assert world
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
camps = {a.get_actor_label(): a for a in actors if isinstance(a, unreal.SFNeutralSlimeCamp)}
excluded = {'SlimeCamp_North', 'SlimeCamp_South'}
assert excluded.issubset(camps)
for label in excluded:
    camp = camps[label]
    members = camp.get_editor_property('members')
    assert len(members) == 2 and all(not s.get_editor_property('large') for s in members)
    camp.set_editor_property('grant_clear_reward', False)
assert all(c.get_editor_property('grant_clear_reward') for label, c in camps.items() if label not in excluded)
assert unreal.EditorLoadingAndSavingUtils.save_map(world, '/Game/Game/Stage/PT_Stage_2')
exec(compile((root / 'Scripts/Editor/verify_neutral_slime_assets.py').read_text(encoding='utf-8'), 'verify_neutral_slime_assets.py', 'exec'))
