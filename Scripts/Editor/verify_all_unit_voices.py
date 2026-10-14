"""Fresh-load every cue and exercise both responses for all 18 playable characters."""
from pathlib import Path
import unreal
VOICE_WORK=Path(unreal.Paths.project_dir())/'Saved/BeastFinalVoices'
source=Path(unreal.Paths.project_dir())/'Scripts/Editor/verify_unit_voices_runtime.py'
exec(compile(source.read_text(encoding='utf-8'),str(source),'exec'))
