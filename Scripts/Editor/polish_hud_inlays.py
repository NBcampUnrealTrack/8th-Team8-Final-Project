"""Animate only existing cyan/gold artwork texels; preserve HUD textures and geometry."""
import json,shutil
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/EtherStonePolish';OUT.mkdir(exist_ok=True)
mel=unreal.MaterialEditingLibrary;report=[]
for path in unreal.EditorAssetLibrary.list_assets('/Game/Game/IN_UI/Materials/HUDRelief',True,False):
    material=unreal.load_asset(path)
    if not isinstance(material,unreal.Material) or '_Button' in material.get_name():continue
    custom=next((e for e in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if e.get_outer()==material),None)
    if not custom:continue
    code=custom.get_editor_property('code')
    if '// Facet polish v2' in code:report.append(path);continue
    assert '// Artwork inlay light' in code,path
    package=path.split('.')[0];src=ROOT/'Content'/Path(package.removeprefix('/Game/')+'.uasset');backup=OUT/'before'/src.relative_to(ROOT)
    if not backup.exists():backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,backup)
    inputs=list(custom.get_editor_property('inputs'))
    for name in ['SelectionPulse','HoverGlow']:
        inp=unreal.CustomInput();inp.set_editor_property('input_name',name);inputs.append(inp)
    custom.set_editor_property('inputs',inputs)
    for name in ['SelectionPulse','HoverGlow']:
        parameter=mel.create_material_expression(material,unreal.MaterialExpressionScalarParameter)
        parameter.set_editor_property('parameter_name',name);parameter.set_editor_property('default_value',0.)
        mel.connect_material_expressions(parameter,'',custom,name)
    code=code.replace('return max(rgb,0);','''// Facet polish v2: all reflections stay inside authored material masks.
float sweep=pow(saturate(1-abs(frac(UV.x*.64+UV.y*.23-Clock*.075)*2-1)),22);
float facet=pow(saturate(.5+.5*sin(Clock*1.8+UV.x*23+UV.y*17)),12);
float energy=.14*sweep+.075*facet+.48*saturate(SelectionPulse)+.16*saturate(HoverGlow);
rgb+=float3(.14,.52,.6)*inlay*energy;
float goldEdge=saturate(max(slope,0)*4)*warm;
rgb+=float3(.32,.21,.07)*goldEdge*sweep*(.23+.35*saturate(HoverGlow));
return max(rgb,0);''')
    custom.set_editor_property('code',code);mel.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    report.append(path)
assert len(report)>=3,report
(OUT/'hud_materials.json').write_text(json.dumps(report,indent=2))
