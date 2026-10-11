"""Stage-local placement orientation: front entrances face the east-facing player camera."""
import unreal,json,shutil
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Ambush/Revision5';OUT.mkdir(parents=True,exist_ok=True)
MAP='/Game/Game/Stage/PT_SFStageAmbush';GM='/Game/Game/Stage/AmbushArt/BP_AmbushGameMode'
for p in ['Content/Game/Stage/PT_SFStageAmbush.umap','Content/Game/Stage/AmbushArt/BP_AmbushGameMode.uasset']:
 dest=OUT/('Before_'+Path(p).name)
 if not dest.exists():shutil.copy2(ROOT/p,dest)
w=unreal.EditorLoadingAndSavingUtils.load_map(MAP);assert w
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not any(isinstance(a,unreal.SFBuildingSpawnAnchor) for a in sub.get_all_level_actors()),'Keep the user-deleted starting buildings absent'
bp=unreal.load_asset(GM);cls=unreal.EditorAssetLibrary.load_blueprint_class(GM);cdo=unreal.get_default_object(cls)
before=cdo.get_editor_property('player_building_placement_yaw');cdo.set_editor_property('player_building_placement_yaw',180.)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
w.get_world_settings().set_editor_property('default_game_mode',unreal.EditorAssetLibrary.load_blueprint_class(GM))
assert unreal.EditorLoadingAndSavingUtils.save_map(w,MAP)
(OUT/'applied.json').write_text(json.dumps({'before':before,'after':180,'map':MAP,'starting_buildings':0},indent=2))
