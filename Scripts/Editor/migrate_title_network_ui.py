"""Move Start's audited UI-only level script into the local title controller."""
from pathlib import Path
import shutil
import unreal

root = Path(unreal.Paths.project_dir())
backup = root / 'Saved/PVPRevision/Start.before_network_ui.umap'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(root / 'Content/Core/Levels/Start.umap', backup)
world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/Core/Levels/Start')
assert unreal.SFPVPEditorLibrary.migrate_title(world), 'Unexpected title world'
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False), 'Title save failed'
assert not unreal.SFPVPEditorLibrary.describe_level_script(world).strip(), 'Legacy title UI nodes remain'
unreal.log('Start now uses SFTitleMode; UI and camera are local-player owned.')
