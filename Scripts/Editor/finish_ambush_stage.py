"""Readable gorge lighting and survivors from the preceding assault."""
import unreal,sys
from pathlib import Path
sys.dont_write_bytecode=True;sys.path.insert(0,str(Path(__file__).parent))
import ambush_layout as L
w=unreal.EditorLoadingAndSavingUtils.load_map(L.MAP);assert w
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
# Project-wide supported-agent settings override per-Recast dimensions on load.
# Exclude the full canopy footprint near paths on this map, so the commander's
# 230cm capsule cannot be routed under foliage by the 144cm shared nav agent.
for label,a in actors.items():
 if not label.startswith('Ambush_RidgeForest_'):continue
 p=a.get_actor_location();u,v=L.uv(p.x,p.y)
 if min(L.road_distance(u,v),L.exit_distance(u,v))>1500 and L.center_distance(u,v)>4100:continue
 blocker_label=label.replace('RidgeForest','CanopyNoWalk')
 if blocker_label in actors:continue
 center,extent=a.get_actor_bounds(True)
 center.z=L.height(u,v)+300
 b=sub.spawn_actor_from_class(unreal.NavModifierVolume,center)
 b.set_actor_label(blocker_label);b.set_folder_path('Ambush/Navigation/Canopies');b.tags=[unreal.Name('SF_Ambush')]
 b.set_editor_property('area_class',unreal.NavArea_Null)
 _,base=b.get_actor_bounds(False)
 b.set_actor_scale3d(unreal.Vector((extent.x+35)/base.x,(extent.y+35)/base.y,700/base.z))
sky=actors['Ambush_Sky'].light_component
sky.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.set_editor_property('real_time_capture',False)
sky.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
sky.set_editor_property('intensity',.85)
for front,(u,v) in enumerate([(.44,.535),(.50,.495),(.56,.535)]):
 for j,(unit,du,dv) in enumerate([('Fighter',-.006,0),('Fighter',.006,0),('Shooter',0,.018)]):
  label='Ambush_Survivor_%d_%d'%(front,j)
  if label in actors:continue
  cls=unreal.EditorAssetLibrary.load_blueprint_class('/Game/Core/Units/Etherial_Human/BP_Human_'+unit)
  a=sub.spawn_actor_from_class(cls,unreal.Vector(*L.world(u+du,v+dv,offset=100)))
  a.set_actor_label(label);a.set_folder_path('Ambush/Gameplay/Survivors');a.tags=[unreal.Name('SF_Ambush')]
  a.set_editor_property('unit_row_name',unit)
  a.get_component_by_class(unreal.CombatTargetComponent).set_editor_property('team_index',0)
unreal.EditorLoadingAndSavingUtils.save_map(w,L.MAP)
