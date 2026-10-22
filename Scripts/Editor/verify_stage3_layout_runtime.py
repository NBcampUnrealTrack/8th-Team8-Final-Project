"""Disposable PIE check: relocated tutorial ghosts/gates, grids, synergy and mining.

Run with -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.LevelEditorPlaySettings]:PlayNetMode=PIE_Standalone
to isolate this check from saved multiplayer PIE preferences.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir()) / 'Stage03Layout'
config = json.loads((out / 'configuration_verified.json').read_text(encoding='utf-8'))
ue = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_StageDefence')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(phase='starting', start=time.time(), checks=[])
refs = {}

def actors(world, cls):
    return unreal.GameplayStatics.get_all_actors_of_class(world, cls)

def subsystem(world, cls):
    return next(o for o in unreal.ObjectIterator(cls) if o.get_outer() == world)

def check(condition, message):
    assert condition, message
    state['checks'].append(message)

def finish(error=None):
    state['passed'] = error is None
    if error:
        state['error'] = error
        unreal.log_error(error)
    (out / 'runtime_verified.json').write_text(json.dumps(state, indent=2), encoding='utf-8')
    unreal.log('STAGE03_LAYOUT_RUNTIME_' + ('FAILED' if error else 'PASSED'))
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(delta):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle = unreal.register_slate_post_tick_callback(quit_tick)

def spawn(world, name, xyz):
    definition = unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/Animal/DA_' + name)
    transform = unreal.Transform(location=unreal.Vector(*xyz), rotation=unreal.Rotator(yaw=90), scale=unreal.Vector(1, 1, 1))
    actor = unreal.SFBuildingSpawnLibrary.spawn_building_from_definition(
        world, definition, 0, transform, unreal.SFBuildingSpawnMode.VIEW_PORT_PLACED_MODE, 0)
    check(bool(actor), name + ' successfully registers its real building grid')
    return actor

def show_target(world, node, xyz, old_xyz):
    tutorial = subsystem(world, unreal.SFTutorialSubsystem)
    tutorial.cancel_guide()
    tutorial.cancel_objective_wait()
    intro = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.SFStageIntroWidget, False)
    check(bool(widgets), 'Stage03 intro widget exists')
    widgets[0].start_stage_intro_at(intro, 'Stage03', node)
    check(unreal.SFTutorialSubsystem.is_build_location_allowed(world, unreal.Vector(*xyz)),
          'Node %d accepts the central construction location' % node)
    check(not unreal.SFTutorialSubsystem.is_build_location_allowed(world, unreal.Vector(*old_xyz)),
          'Node %d rejects the former construction location' % node)
    ghosts = actors(world, unreal.SFTutorialGhostActor)
    check(any(g.get_actor_location().equals(unreal.Vector(*xyz)) for g in ghosts),
          'Node %d displays its ghost at the new location' % node)
    unreal.GameplayStatics.set_game_paused(world, False)

def tick(delta):
    try:
        if time.time() - state['start'] > 120:
            raise RuntimeError('PIE timed out: ' + state['phase'])
        world = ue.get_game_world()
        if not world:
            return
        gs = unreal.GameplayStatics.get_game_state(world)
        if not isinstance(gs, unreal.SFGameState):
            return
        if state['phase'] == 'starting':
            if gs.get_stage_state() == unreal.SFStageState.READY_TO_START:
                unreal.GameplayStatics.get_game_mode(world).start_stage()
            if not gs.is_stage_playing():
                return
            if not unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.SFStageIntroWidget, False):
                return
            check(str(gs.get_current_stage_row_name()) == 'Stage03', 'The saved defence map boots as Stage03')
            buildings = actors(world, unreal.SFBaseBuilding)
            check(any(b.get_actor_location().equals(unreal.Vector(*config['barracks'])) for b in buildings),
                  'Starting barracks spawns at the new centre')
            check(len(actors(world, unreal.Mineral)) == 10, 'All ten mineral deposits survive grid registration')
            state['initial_income'] = gs.get_mineral_income_rate(0)
            show_target(world, 5, config['academy'], (1200, 550, 0))
            refs['academy'] = spawn(world, 'AnimalRanged', config['academy'])
            state.update(phase='sanctuary', phase_start=time.time())
        elif state['phase'] == 'sanctuary' and time.time() - state['phase_start'] > 1:
            show_target(world, 8, config['sanctuary'], (1200, 50, 0))
            refs['sanctuary'] = spawn(world, 'AnimalSpecial', config['sanctuary'])
            tutorial = subsystem(world, unreal.SFTutorialSubsystem)
            tutorial.cancel_guide()
            tutorial.cancel_objective_wait()
            intro = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
            widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.SFStageIntroWidget, False)
            widgets[0].start_stage_intro_at(intro, 'Stage03', 12)
            unreal.GameplayStatics.set_game_paused(world, False)
            for label, xyz in config['minerals'].items():
                refs[label] = spawn(world, 'AnimalMineralMine', xyz)
            state.update(phase='verify', phase_start=time.time())
        elif state['phase'] == 'verify' and time.time() - state['phase_start'] > 5:
            synergy = subsystem(world, unreal.SFBuildingSynergySubsystem)
            bonuses = {name: synergy.get_production_bonus_percent(refs[name]) for name in ('academy', 'sanctuary')}
            state['synergy'] = bonuses
            check(bonuses['academy'] >= 9.9 and bonuses['sanctuary'] >= 24.9, 'Academy +10% and sanctuary +25% production synergy preserved')
            check(gs.get_mineral_income_rate(0) > state['initial_income'], 'Additional mineral deposits support working mines and increase income')
            state['income_with_added_mines'] = gs.get_mineral_income_rate(0)
            beast = unreal.SFUnitSpawnLibrary.spawn_unit_from_row(refs['sanctuary'], 'WarBeast')
            check(bool(beast), 'WarBeast finds a safe exit from the relocated sanctuary')
            navigation = subsystem(world, unreal.NavigationSystemV1)
            for lane in actors(world, unreal.LanePath):
                path = navigation.find_path_to_location_synchronously(
                    world, lane.get_actor_location(), unreal.Vector(-700, 500, 50), beast)
                check(bool(path) and path.is_valid() and not path.is_partial(), lane.get_name() + ' still reaches the base')
            finish()
    except Exception:
        finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
level.editor_request_begin_play()
