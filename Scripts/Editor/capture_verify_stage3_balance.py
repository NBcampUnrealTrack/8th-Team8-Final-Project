"""Capture real Stage03 PIE buildings, then verify all five reduced spawn budgets.

Use standalone PIE via the command-line editor setting override. Constructed
academy/sanctuary and invulnerability exist only in this disposable session.
No assets or campaign clear records are saved by this script.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
out = root / 'output/qa/stage3-2026-10-21'
out.mkdir(parents=True, exist_ok=True)
intro = unreal.load_asset('/Game/Game/IN_UI/DT/DT_SFStageIntro')
layout = json.loads((root / 'Saved/Stage03Layout/configuration_verified.json').read_text(encoding='utf-8'))
ue = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor_actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_StageDefence')
camera = editor_actors.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(-5200, -200, 7000))
camera.set_actor_label('Stage03_Temporary_Capture')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(phase='starting', started=time.time(), waves={}, checks=[])
refs = {}
shots = [('base_overview.png', (-5200, -200, 7000), (-500, -200, 0), 55),
         ('central_buildings.png', (-2900, -200, 3200), (-100, 0, 0), 50)]

def actors(world, cls):
    return unreal.GameplayStatics.get_all_actors_of_class(world, cls)

def finish(error=None):
    state['passed'] = error is None
    if error:
        state['error'] = error
        unreal.log_error(error)
    (out / 'runtime_verified.json').write_text(json.dumps(state, indent=2), encoding='utf-8')
    unreal.log('STAGE03_CAPTURE_BALANCE_' + ('FAILED' if error else 'PASSED'))
    if refs.get('controller'):
        refs['controller'].stop_waves()
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    def quit_tick(delta):
        unreal.unregister_slate_post_tick_callback(quit_handle)
        unreal.SystemLibrary.quit_editor()
    global quit_handle
    quit_handle = unreal.register_slate_post_tick_callback(quit_tick)

def setup_shot(index):
    filename, xyz, target_xyz, fov = shots[index]
    cam = refs['camera']
    cam.set_actor_location(unreal.Vector(*xyz), False, True)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*xyz), unreal.Vector(*target_xyz)), False)
    refs['capture'].set_editor_property('fov_angle', fov)
    state.update(shot=index, phase='warmup', at=time.time())

def tick(delta):
    try:
        if time.time() - state['started'] > 240:
            raise RuntimeError('Timed out: ' + state['phase'])
        world = ue.get_game_world()
        if not world:
            return
        gs = unreal.GameplayStatics.get_game_state(world)
        if not isinstance(gs, unreal.SFGameState):
            return
        if state['phase'] == 'starting':
            if gs.get_stage_state() == unreal.SFStageState.READY_TO_START:
                unreal.GameplayStatics.get_game_mode(world).start_stage()
            widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.SFStageIntroWidget, False)
            if not gs.is_stage_playing() or not widgets:
                return
            assert str(gs.get_current_stage_row_name()) == 'Stage03'
            refs['controller'] = actors(world, unreal.SFDefenceWaveController)[0]
            controller = refs['controller']
            assert abs(controller.get_editor_property('spawn_count_multiplier') - 0.28 / 3) < 0.000001
            assert controller.get_editor_property('initial_preparation_duration') == 60
            assert controller.get_editor_property('preparation_duration') == 180
            assert len(actors(world, unreal.Mineral)) == 10
            for name, xyz in [('AnimalRanged', layout['academy']), ('AnimalSpecial', layout['sanctuary'])]:
                definition = unreal.load_asset('/Game/Core/Primary/Buildings/Definitions/Animal/DA_' + name)
                transform = unreal.Transform(location=unreal.Vector(*xyz), rotation=unreal.Rotator(yaw=90), scale=unreal.Vector(1, 1, 1))
                building = unreal.SFBuildingSpawnLibrary.spawn_building_from_definition(
                    world, definition, 0, transform, unreal.SFBuildingSpawnMode.VIEW_PORT_PLACED_MODE, 0)
                assert building, name
                refs[name] = building
            # Protect allied actors only during the accelerated spawn-count check.
            for actor in actors(world, unreal.Actor):
                combat = actor.get_component_by_class(unreal.CombatTargetComponent)
                if combat and combat.get_team_index() == 0:
                    combat.set_damage_multiplier(0)
            refs['camera'] = actors(world, unreal.SceneCapture2D)[0]
            capture = refs['camera'].get_component_by_class(unreal.SceneCaptureComponent2D)
            target = unreal.RenderingLibrary.create_render_target2d(world, 1920, 1280, unreal.TextureRenderTargetFormat.RTF_RGBA8)
            target.set_editor_property('target_gamma', 2.2)
            for prop, value in dict(texture_target=target, capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,
                                    capture_every_frame=False, capture_on_movement=False).items():
                capture.set_editor_property(prop, value)
            refs.update(capture=capture, target=target)
            unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
            unreal.GameplayStatics.set_game_paused(world, False)
            setup_shot(0)
        elif state['phase'] == 'warmup' and time.time() - state['at'] > (15 if state['shot'] == 0 else 4):
            refs['capture'].capture_scene()
            state.update(phase='export', at=time.time())
        elif state['phase'] == 'export' and time.time() - state['at'] > 2:
            filename = shots[state['shot']][0]
            unreal.RenderingLibrary.export_render_target(world, refs['target'], str(out), filename)
            assert (out / filename).exists()
            if state['shot'] == 0:
                setup_shot(1)
                return
            tutorial = next(o for o in unreal.ObjectIterator(unreal.SFTutorialSubsystem) if o.get_outer() == world)
            tutorial.cancel_guide()
            tutorial.cancel_objective_wait()
            widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.SFStageIntroWidget, False)
            widgets[0].start_stage_intro_at(intro, 'Stage03', 12)
            unreal.GameplayStatics.set_game_paused(world, False)
            unreal.GameplayStatics.set_global_time_dilation(world, 20)
            state.update(phase='waves', at=time.time())
        elif state['phase'] == 'waves':
            controller = refs['controller']
            wave = controller.get_wave_number()
            total = sum(s.get_editor_property('total_spawned') for s in controller.get_editor_property('spawners'))
            expected = [10, 15, 20, 25, 30]
            if wave > 0:
                prior = sum(expected[:wave - 1])
                assert total <= sum(expected[:wave]), ('Exceeded wave budget', wave, total)
                if wave not in state['waves']:
                    budget = total - prior + controller.get_remaining_spawn_count()
                    assert budget == expected[wave - 1], (wave, budget)
                    state['waves'][wave] = dict(planned=budget)
                if total == sum(expected[:wave]):
                    state['waves'][wave]['spawned'] = expected[wave - 1]
                if wave == 5 and total == 100:
                    assert all(v.get('planned') == v.get('spawned') for v in state['waves'].values())
                    state['checks'] = ['Ten mineral nodes', 'Central academy/sanctuary constructed',
                                       'Wave totals 10/15/20/25/30 spawned in PIE', 'Preparation 60/180 seconds retained']
                    finish()
    except Exception:
        finish(traceback.format_exc())

handle = unreal.register_slate_post_tick_callback(tick)
level.editor_request_begin_play()
