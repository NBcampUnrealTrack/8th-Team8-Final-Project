"""Report sound assets with no surviving asset or source-code references."""
import collections,json,re
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir())
OUT=ROOT/'Saved/SoundCleanup';OUT.mkdir(parents=True,exist_ok=True)
registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(['/Game'],force_rescan=True)
assets=registry.get_assets_by_path('/Game',recursive=True)
sounds={str(a.package_name):a for a in assets if str(a.asset_class_path.asset_name) in ('SoundWave','SoundCue','MetaSoundSource')}
options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True,include_searchable_names=True,include_soft_management_references=True,include_hard_management_references=True)
referencers={p:sorted(set(str(r) for r in registry.get_referencers(p,options))-{p}) for p in sounds}
source_roots=collections.defaultdict(list)
for folder in ['Source','Config']:
    for file in (ROOT/folder).rglob('*'):
        if file.suffix.lower() not in ('.h','.cpp','.ini','.json','.cs'):continue
        text=file.read_text(encoding='utf-8',errors='replace')
        for path in set(re.findall(r'/Game/[A-Za-z0-9_./\-]+',text)):
            package=path.split('.')[0].rstrip('/')
            for sound in sounds:
                if sound==package or sound.startswith(package+'/'):
                    source_roots[sound].append(str(file.relative_to(ROOT)))
live={p for p in sounds if any(r not in sounds for r in referencers[p]) or p in source_roots}
while True:
    expanded=live|{p for p in sounds if any(r in live for r in referencers[p])}
    if expanded==live:break
    live=expanded
unused=sorted(set(sounds)-live)
report={'total':len(sounds),'retained':len(live),'unused_count':len(unused),'unused':[{ 'package':p,'class':str(sounds[p].asset_class_path.asset_name),'referencers':referencers[p],'bytes':(ROOT/'Content'/(p.removeprefix('/Game/')+'.uasset')).stat().st_size} for p in unused],'retained_assets':[{'package':p,'referencers':referencers[p],'source_references':source_roots.get(p,[])} for p in sorted(live)]}
(OUT/'audit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log(f'SOUND AUDIT: {len(sounds)} total, {len(live)} retained, {len(unused)} unused')
