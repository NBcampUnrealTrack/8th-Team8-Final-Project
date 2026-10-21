"""Exercise first entry and cleared replays with a disposable campaign save slot."""
import json
import time
import traceback
from pathlib import Path
import unreal

slot = 'SFTutorialReplayQA'
if '-SFCampaignSaveSlot=' + slot not in unreal.SystemLibrary.get_command_line():
    raise RuntimeError('A disposable -SFCampaignSaveSlot=SFTutorialReplayQA is required')
for name in [slot, slot + '_Backup']:
    unreal.GameplayStatics.delete_game_in_slot(name, 0)
out = Path(unreal.Paths.project_saved_dir()) / 'TutorialReplay'
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Core/Levels/Lobby')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(start=time.time(), phase='lobby', checks=[], index=0, snapshots=[])
stages = ['Stage%02d' % i for i in range(1, 8)] + ['Stage07']

def check(ok, message):
    if not ok:
        raise AssertionError(message)
    state['checks'].append(message)

def actors(world, cls):
    return unreal.GameplayStatics.get_all_actors_of_class(world, cls)

def widgets(world, cls):
    return unreal.WidgetLibrary.get_all_widgets_of_class(world, cls, True)

def travel(gi):
    stage = stages[state['index']]
    check(gi.select_stage(stage), stage + ' selected')
    if stage == 'Stage07':
        check(gi.select_race(unreal.SFRace.HUMAN if state['index'] == 7 else unreal.SFRace.BEAST),
              'Final stage faction selected after stage selection')
    check(gi.move_to_stage(), stage + ' travel requested')
    state.update(phase='loading', since=time.time())

def finish(error=None):
    state['passed'] = error is None
    state['error'] = error
    (out / 'runtime_report.json').write_text(json.dumps(state, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    def quit_tick(dt):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        for name in [slot, slot + '_Backup']:
            unreal.GameplayStatics.delete_game_in_slot(name, 0)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle = unreal.register_slate_post_tick_callback(quit_tick)

def tick(dt):
    try:
        if time.time() - state['start'] > 360:
            raise RuntimeError('Timed out: ' + state['phase'])
        world = editor.get_game_world()
        if not world:
            return
        gi = unreal.GameplayStatics.get_game_instance(world)
        if state['phase'] == 'lobby':
            if not widgets(world, unreal.LobbyWidget):
                return
            gi.select_race(unreal.SFRace.BEAST)
            check(gi.select_stage('Stage01'), 'First stage selected')
            gi.move_to_stage()
            state.update(phase='first', since=time.time())
            return
        gs = unreal.GameplayStatics.get_game_state(world)
        if not isinstance(gs, unreal.SFGameState):
            return
        if gs.get_stage_state() == unreal.SFStageState.INITIALIZATION_FAILED:
            raise RuntimeError('Stage initialization failed: ' + str(gs.get_current_stage_row_name()))
        if gs.get_stage_state() == unreal.SFStageState.READY_TO_START:
            loading = widgets(world, unreal.LoadingWidget)
            for widget in loading:
                widget.finish_loading_screen()
        if state['phase'] == 'first':
            if not gs.is_stage_playing() or not widgets(world, unreal.SFStageIntroWidget):
                return
            check(not gs.is_cleared_stage_replay(), 'First entry retains story and tutorial')
            for stage in stages[:7]:
                check(gi.mark_stage_cleared(stage), stage + ' disposable clear recorded')
            check(not gs.is_cleared_stage_replay(), 'Recording first clear preserves current-run story policy')
            unreal.GameplayStatics.set_game_paused(world, False)
            travel(gi)
        elif state['phase'] == 'loading':
            if time.time() - state['since'] < 1 or not gs.is_stage_playing():
                return
            stage = stages[state['index']]
            if str(gs.get_current_stage_row_name()) != stage:
                return
            check(gs.is_cleared_stage_replay(), stage + ' classified as a replay')
            check(not widgets(world, unreal.SFStageIntroWidget), stage + ' skips opening story/tutorial')
            check(not unreal.GameplayStatics.is_game_paused(world), stage + ' starts unpaused')
            buildings = [b for b in actors(world, unreal.SFBaseBuilding) if unreal.SFUnitSpawnLibrary.resolve_team_index(b) == 0]
            by_id = {str(b.get_building_definition().get_editor_property('building_id')): b for b in buildings}
            check(all(b.get_editor_property('current_building_state') == unreal.BuildingState.COMPLETED for b in buildings),
                  stage + ' buildings are fully constructed')
            units = [u for u in actors(world, unreal.UnitBase) if unreal.SFUnitSpawnLibrary.resolve_team_index(u) == 0]
            unit_names = [str(u.get_unit_row_name()) for u in units]
            state['snapshots'].append(dict(stage=stage, buildings={k: b.get_current_tier() for k,b in by_id.items()}, units=unit_names))
            if state['index'] == 0:
                for name in ['AnimalMineralMine', 'AnimalHouse', 'AnimalBarrack']:
                    check(name in by_id, stage + ' starts with ' + name)
                check('Brawler' in unit_names, 'Stage01 produced Brawler exists')
            elif state['index'] == 1:
                check('AnimalEtherMine' in by_id, 'Stage02 ether mine exists')
                check(by_id['AnimalNexus'].get_current_tier() == 1, 'Stage02 nexus upgraded')
                check(by_id['AnimalBarrack'].get_current_tier() == 1, 'Stage02 barracks upgraded')
                check('Berserker' in unit_names, 'Stage02 produced Berserker exists')
                spawners = actors(world, unreal.UnitSpawner)
                check(bool(spawners) and all(s.is_spawner_activated() for s in spawners), 'Stage02 enemy gates open')
            elif state['index'] == 2:
                for name in ['AnimalRanged', 'AnimalSpecial']:
                    check(name in by_id, stage + ' starts with ' + name)
                check(by_id['AnimalNexus'].get_current_tier() == 2, 'Stage03 nexus starts at tier 3')
                check('WarBeast' in unit_names, 'Stage03 produced WarBeast exists')
            check(unreal.GameplayStatics.get_game_mode(world).process_victory(), stage + ' victory accepted')
            state.update(phase='victory', since=time.time())
        elif state['phase'] == 'victory' and time.time() - state['since'] > 0.5:
            check(not widgets(world, unreal.SFStageIntroWidget), stages[state['index']] + ' skips victory epilogue')
            state['index'] += 1
            if state['index'] == len(stages):
                finish()
            else:
                unreal.GameplayStatics.set_game_paused(world, False)
                travel(gi)
    except Exception:
        finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
