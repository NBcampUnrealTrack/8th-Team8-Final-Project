"""Import the approved Beast recordings without reimporting Etherian/Brawler audio."""
from pathlib import Path
import unreal
VOICE_WORK=Path(unreal.Paths.project_dir())/'Saved/BeastFinalVoices'
VOICE_FACTION='Beast'
source=Path(unreal.Paths.project_dir())/'Scripts/Editor/install_unit_voices.py'
exec(compile(source.read_text(encoding='utf-8'),str(source),'exec'))
