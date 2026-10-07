import unreal, json
from pathlib import Path
from CelMaterial import finish_cel, linear_color, connected_nodes, update_cel_defaults, CEL_VERSION, style
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
outline_path=folder+'/M_CelOutline'
if not library.does_asset_exist(outline_path):
    mat=tools.create_asset('M_CelOutline',folder,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('two_sided',True)
    def n(cls): return edit.create_material_expression(mat,getattr(unreal,'MaterialExpression'+cls))
    color=n('Constant3Vector'); color.constant=linear_color(palette['Outline'])
    edit.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    sign=n('TwoSidedSign'); reverse=n('OneMinus')
    assert edit.connect_material_expressions(sign,'',reverse,'')
    edit.connect_material_property(reverse,'',unreal.MaterialProperty.MP_OPACITY_MASK)
    normal=n('VertexNormalWS'); expand=n('Multiply'); expand.set_editor_property('const_b',1.6)
    edit.connect_material_expressions(normal,'',expand,'A')
    edit.connect_material_property(expand,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    errors=edit.recompile_material(mat); assert not errors, list(errors)
    save(mat)
outline=unreal.load_asset(outline_path)
if library.get_metadata_tag(outline,'GemTDOutlineVersion')!='1':
    reverse=edit.get_material_property_input_node(outline,unreal.MaterialProperty.MP_OPACITY_MASK)
    sign=edit.create_material_expression(outline,unreal.MaterialExpressionTwoSidedSign)
    assert edit.connect_material_expressions(sign,'',reverse,'')
    errors=edit.recompile_material(outline); assert not errors, list(errors)
    library.set_metadata_tag(outline,'GemTDOutlineVersion','1')
    save(outline)
color=edit.get_material_property_input_node(outline,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
color.constant=linear_color(palette['Outline'])
expand=edit.get_material_property_input_node(outline,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
expand.set_editor_property('const_b',style['outline_width_cm'])
errors=edit.recompile_material(outline); assert not errors, list(errors)
save(outline)
unreal.log('GEM_PASTEL_CEL_MATERIALS_READY')
