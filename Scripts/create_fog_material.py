"""Regenerate the checked-in fog material with UE 5.5's PythonScriptPlugin commandlet.

UnrealEditor-Cmd.exe StrikeForge.uproject -run=pythonscript
    -script=Scripts/create_fog_material.py -EnablePlugins=PythonScriptPlugin -unattended
"""
import unreal

PATH = "/Game/Core/FogOfWar/M_FogOfWar"
lib = unreal.MaterialEditingLibrary
material = unreal.load_asset(PATH)
if material is None:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_FogOfWar", "/Game/Core/FogOfWar", unreal.Material, unreal.MaterialFactoryNew())
else:
    lib.delete_all_material_expressions(material)

material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)

def node(cls, x, y, **properties):
    expression = lib.create_material_expression(material, cls, x, y)
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression

scene = node(unreal.MaterialExpressionSceneTexture, -800, -200,
             scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
depth = node(unreal.MaterialExpressionSceneTexture, -800, -400,
             scene_texture_id=unreal.SceneTextureId.PPI_SCENE_DEPTH)
mask = node(unreal.MaterialExpressionTextureObjectParameter, -800, 0,
            parameter_name="VisionMask", texture=unreal.load_asset("/Engine/EngineResources/Black"))
minimum = node(unreal.MaterialExpressionVectorParameter, -800, 200,
               parameter_name="MapMin", default_value=unreal.LinearColor(-5000, -5000, 0, 0))
size = node(unreal.MaterialExpressionVectorParameter, -800, 400,
            parameter_name="MapSize", default_value=unreal.LinearColor(10000, 10000, 1, 1))
explored = node(unreal.MaterialExpressionScalarParameter, -800, 600,
                parameter_name="ExploredBrightness", default_value=0.38)
unexplored = node(unreal.MaterialExpressionScalarParameter, -800, 800,
                  parameter_name="UnexploredBrightness", default_value=0.15)
inputs = ["Scene", "Depth", "VisionMask", "MapMin", "MapSize", "Explored", "Unexplored"]
custom_inputs = []
for name in inputs:
    item = unreal.CustomInput()
    item.set_editor_property("input_name", name)
    custom_inputs.append(item)
custom = node(unreal.MaterialExpressionCustom, -200, 0,
              output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
              description="Team vision and persistent exploration",
              inputs=custom_inputs,
              code="""
// After-tonemap SV_Position is in output pixels, but scene depth and the inverse
// view matrix use the primary render resolution. Reconstruct both from the same
// viewport UV instead of WorldPosition's resolution-fraction approximation.
// This also handles PIE viewport offsets inside a larger editor render buffer.
float2 depthUV = ViewportUVToBufferUV(GetViewportUV(Parameters));
float deviceZ = max(ConvertToDeviceZ(Depth.r), 1e-18);
float4 depthPosition = float4(depthUV * View.BufferSizeAndInvSize.xy, deviceZ, 1.0);
float3 translatedPosition = SvPositionToTranslatedWorld(depthPosition);
float3 worldPosition = DFFastSubtractDemote(translatedPosition, PrimaryView.PreViewTranslation);
float2 uv = (worldPosition.xy - MapMin.xy) / max(MapSize.xy, float2(1, 1));
float inside = all(uv >= 0) && all(uv <= 1);
float2 fog = Texture2DSample(VisionMask, VisionMaskSampler, saturate(uv)).rg * inside;
float brightness = lerp(lerp(Unexplored, Explored, fog.g), 1.0, fog.r);
return Scene.rgb * brightness;
""")
for expression, name in zip([scene, depth, mask, minimum, size, explored, unexplored], inputs):
    lib.connect_material_expressions(expression, "", custom, name)
lib.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError("Could not save fog material; close editors holding the asset and retry.")
unreal.log("FOG_MATERIAL_SAVED " + PATH)
