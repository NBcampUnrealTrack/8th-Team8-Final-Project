"""One-time access and ravine polish, scoped to the new ambush map."""
import unreal, sys, json, random, math
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(unreal.Paths.project_dir());sys.path.insert(0,str(ROOT/'Scripts/Editor'))
import ambush_layout as L
out=ROOT/'Saved/Ambush';assert not (out/'refined.json').exists()
w=unreal.EditorLoadingAndSavingUtils.load_map(L.MAP);assert w
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=list(sub.get_all_level_actors());changes=[]
for a in actors:
 name=a.get_actor_label()
 if name.startswith('Ambush_Ether_'):
  p=a.get_actor_location();u,v=L.uv(p.x,p.y)
  a.set_actor_location(unreal.Vector(*L.world(u-.022,v,offset=p.z-L.height(u,v))),False,True);changes.append(name)
 elif name.startswith('Ambush_RavineCliffs_'):
  p=a.get_actor_location();s=a.get_actor_scale3d();b=a.static_mesh_component.static_mesh.get_bounds()
  bottom=p.z+(b.origin.z-b.box_extent.z)*s.z
  s.z*=.62;a.set_actor_scale3d(s);p.z=bottom-(b.origin.z-b.box_extent.z)*s.z-85
  a.set_actor_location(p,False,True);changes.append(name)
m=unreal.load_asset(L.ART+'/M_AmbushGround')
for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
 if n.get_outer()!=m:continue
 code=n.get_editor_property('code')
 old='color=lerp(color,D*float3(.30,.29,.25),cliff);'
 if old in code:
  new='float grain=n.v(float2((P.x+P.y)/270,P.z/160));float3 rock=lerp(float3(.075,.071,.062),float3(.21,.20,.17),grain);color=lerp(color,rock,cliff);'
  n.set_editor_property('code',code.replace(old,new));changes.append('cliff_material')
unreal.MaterialEditingLibrary.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
# Loose talus beneath the severed bridge, outside all traversable surfaces.
rng=random.Random(61);catalog=json.loads((ROOT/'Scripts/Editor/stage_hero_scenery_assets.json').read_text(encoding='utf-8-sig'))
IGNORE=getattr(unreal.CollisionResponseType,next(n for n in dir(unreal.CollisionResponseType) if n.endswith('IGNORE')))
for i in range(38):
 u,v=rng.uniform(.39,.61),rng.uniform(.845,.895)
 if L.ravine_q(u,v)>.74:continue
 name=rng.choice(['SM_Rocks_01','SM_Rocks_07','SM_Rocks_09']);mesh=unreal.load_asset('/Game/Game/Stage/StageHeroArt/Collision/SM_HeroSolid_'+name)
 if not mesh:mesh=unreal.load_asset(catalog[name]['path'])
 b=mesh.get_bounds();sx=rng.uniform(180,410)/max(b.box_extent.x*2,b.box_extent.y*2);sz=rng.uniform(110,270)/(b.box_extent.z*2)
 yaw=rng.uniform(0,360);c,s=math.cos(math.radians(yaw)),math.sin(math.radians(yaw));x,y=L.xy(u,v)
 p=unreal.Vector(x-(c*b.origin.x-s*b.origin.y)*sx,y-(s*b.origin.x+c*b.origin.y)*sx,L.height(u,v)-(b.origin.z-b.box_extent.z)*sz)
 a=sub.spawn_actor_from_class(unreal.StaticMeshActor,p,unreal.Rotator(pitch=0,yaw=yaw,roll=0));a.set_actor_label('Ambush_RavineTalus_%02d'%i);a.set_folder_path('Ambush/RavineCliffs');a.tags=[unreal.Name('SF_Ambush')]
 a.set_actor_scale3d(unreal.Vector(sx,sx,sz));comp=a.static_mesh_component;comp.set_static_mesh(mesh);comp.set_collision_profile_name('BlockAll');comp.set_collision_response_to_channel(unreal.CollisionChannel.ECC_LANDSCAPE,IGNORE)
 for j,path in enumerate(catalog.get(name,{}).get('materials',[])):
  if path:comp.set_material(j,unreal.load_asset(path))
unreal.EditorLoadingAndSavingUtils.save_map(w,L.MAP)
(out/'refined.json').write_text(json.dumps(changes,indent=2),encoding='utf-8')
