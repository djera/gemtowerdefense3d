"""Run via Unreal's File > Execute Python Script with Play mode stopped.
Refreshes material settings in the current editor session without reloading
the level. Materials are saved; use Save All to persist any level lighting edits.
"""
import unreal, sys, runpy, importlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'Tools'))
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), 'Stop Play mode before refreshing art.'
# Long-lived editor sessions otherwise retain old JSON values and helper code.
for name in ('CelMaterial','CreateOutlineMaterial','CreateGemMaterials'):
    if name in sys.modules: importlib.reload(sys.modules[name])
runpy.run_path(str(root/'Tools/CreateCelMaterials.py'),run_name='__main__')
runpy.run_path(str(root/'Tools/CreateTerrainMaterial.py'),run_name='__main__')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
if any(a.get_class().get_name()=='GemBoard' for a in actors):
    from CreateGemMaterials import ensure_gem_reflection_environment
    ensure_gem_reflection_environment()
unreal.log('GEM_ART_REFRESHED_IN_EDITOR: materials saved; use Save All to keep level lighting edits.')
