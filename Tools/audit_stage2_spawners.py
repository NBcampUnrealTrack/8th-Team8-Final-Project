import unreal
assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/Game/Stage/PT_Stage_2')
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor, unreal.UnitSpawner):
        unreal.log('SPAWNER_AUDIT {} class={} team={} start={} tag={} delay={} unit={}'.format(
            actor.get_actor_label(), actor.get_class().get_path_name(), actor.get_editor_property('team_index'),
            actor.get_editor_property('start_active'), actor.get_editor_property('activation_objective_tag'),
            actor.get_editor_property('initial_delay'), actor.get_editor_property('unit_row_name')))
unreal.log('SPAWNER_AUDIT_OK')
