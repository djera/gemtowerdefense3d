import unreal, json
from pathlib import Path
from CelMaterial import finish_cel, linear_color, connected_nodes, update_cel_defaults, CEL_VERSION, style
from CreateOutlineMaterial import create_outline_material
from CreateGemMaterials import create_gem_materials
root=Path(__file__).resolve().parents[1]
palette=json.loads((root/'assets/ArtPalette.json').read_text())
tools=unreal.AssetToolsHelpers.get_asset_tools()
edit=unreal.MaterialEditingLibrary
library=unreal.EditorAssetLibrary
def save(asset):
    assert library.save_loaded_asset(asset), 'Could not save '+asset.get_path_name()
folder='/Game/Prototype/Materials'
master_path=folder+'/M_PastelCelMaster'
if library.does_asset_exist(master_path): master=unreal.load_asset(master_path)
else:
    master=tools.create_asset('M_PastelCelMaster',folder,unreal.Material,unreal.MaterialFactoryNew())
    tint=edit.create_material_expression(master,unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property('parameter_name','PastelColor'); tint.set_editor_property('default_value',unreal.LinearColor(.5,.5,.5,1))
    finish_cel(master,tint)
    save(master)
if library.get_metadata_tag(master,'GemTDCelVersion')!=CEL_VERSION:
    tint=next(n for n in connected_nodes(master) if isinstance(n,unreal.MaterialExpressionVectorParameter) and str(n.get_editor_property('parameter_name'))=='PastelColor')
    finish_cel(master,tint)
update_cel_defaults(master); save(master)
materials={}
for name,color in palette.items():
    path=folder+'/M_Cel_'+name
    mat=unreal.load_asset(path) if library.does_asset_exist(path) else tools.create_asset('M_Cel_'+name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    edit.set_material_instance_parent(mat,master)
    edit.set_material_instance_vector_parameter_value(mat,'PastelColor',linear_color(color))
    edit.set_material_instance_scalar_parameter_value(mat,'GridInk',1 if name in ('Road','Intersection') else 0)
    edit.update_material_instance(mat); save(mat); materials[name]=mat
for name in ['Amethyst','Aquamarine','Diamond','Emerald','Opal','Ruby','Sapphire','Topaz']:
    for grade in ['Chipped','Flawed','Normal','Flawless','Perfect']:
        mesh=unreal.load_asset('/Game/Gems/Meshes/SM_'+name+'_'+grade)
        mesh.set_material(0,materials[name]); save(mesh)
stone=unreal.load_asset('/Game/Prototype/Meshes/SM_MazeStone')
stone.set_material(0,materials['Stone']); save(stone)
if stone.get_num_sections(0)>1:
    stone.set_material(1,materials['Outline']); save(stone)
create_outline_material()
create_gem_materials()
unreal.log('GEM_PASTEL_CEL_MATERIALS_READY')
