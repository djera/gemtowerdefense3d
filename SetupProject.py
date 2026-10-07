"""Run once through UnrealEditor-Cmd with -run=pythonscript -script=..."""
import unreal, runpy, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'Tools'))
tools=unreal.AssetToolsHelpers.get_asset_tools()
edit=unreal.MaterialEditingLibrary
def save(asset):
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset), 'Could not save '+asset.get_path_name()
colors={'Snow':(.72,.82,.88),'Grass':(.11,.28,.13),'Road':(.24,.27,.29),'Intersection':(.17,.19,.21),'Checkpoint':(.8,.42,.025),'Stone':(.16,.19,.22),'Enemy':(.7,.12,.07),'Path':(.06,.5,.55),'Valid':(.08,.85,.51),'Invalid':(.95,.08,.12)}
for name,rgb in colors.items():
    path='/Game/Prototype/Materials/M_'+name
    mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if mat is not None:
        edit.set_base_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        edit.recompile_material(mat)
        save(mat)
        continue
    mat=tools.create_asset('M_'+name,'/Game/Prototype/Materials',unreal.Material,unreal.MaterialFactoryNew())
    edit.set_base_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    color=edit.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-300,0)
    color.constant=unreal.LinearColor(*rgb,1)
    edit.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    # Low emissive fill keeps an empty template readable without baked lighting.
    fill=edit.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-300,180)
    fill.constant=unreal.LinearColor(*(c*.3 for c in rgb),1)
    edit.connect_material_property(fill,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough=edit.create_material_expression(mat,unreal.MaterialExpressionConstant,-300,300); rough.r=.8
    edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(mat); save(mat)
if not unreal.EditorAssetLibrary.does_asset_exist('/Game/Gems/Meshes/SM_Topaz_Perfect'):
    runpy.run_path(str(ROOT/'assets/gems/Unreal/import_gems.py'),run_name='__main__')
if (ROOT/'assets/stone/SM_MazeStone.fbx').exists():
    task=unreal.AssetImportTask()
    task.filename=str(ROOT/'assets/stone/SM_MazeStone.fbx')
    task.destination_path='/Game/Prototype/Meshes'
    task.destination_name='SM_MazeStone'
    task.automated=True; task.save=True; task.replace_existing=True
    options=unreal.FbxImportUI()
    options.import_mesh=True; options.import_as_skeletal=False
    options.import_materials=False; options.import_textures=False
    options.automated_import_should_detect_type=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task.options=options
    tools.import_asset_tasks([task])
    assert task.imported_object_paths, 'Stone mesh import failed'
stone=unreal.load_asset('/Game/Prototype/Meshes/SM_MazeStone')
stone.set_material(0,unreal.load_asset('/Game/Prototype/Materials/M_Stone'))
save(stone)
runpy.run_path(str(ROOT/'Tools/CreateCelMaterials.py'),run_name='__main__')
runpy.run_path(str(ROOT/'Tools/CreateTerrainMaterial.py'),run_name='__main__')
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist('/Game/Maps/Prototype'):
    level.load_level('/Game/Maps/Prototype')
else:
    level.new_level('/Game/Maps/Prototype')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
board_class=unreal.load_class(None,'/Script/GemTowerDefense.GemBoard')
boards=[a for a in actors.get_all_level_actors() if a.get_class()==board_class]
board=boards[0] if boards else actors.spawn_actor_from_class(board_class,unreal.Vector())
board.set_actor_label('GemBoard_20x20')
board.load_default_layout()
from CreateGemMaterials import ensure_gem_reflection_environment
ensure_gem_reflection_environment()
if not any(isinstance(a,unreal.PlayerStart) for a in actors.get_all_level_actors()):
    actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(0,0,200))
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-2260,-1710,2090),unreal.Rotator(-35.264,45,0))
assert level.save_current_level(), 'Could not save the populated starter level'
unreal.log('GEM_TEMPLATE_SETUP_COMPLETE')
