"""Create/update a closed silhouette shell for flat-shaded meshes."""
import unreal, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CelMaterial import linear_color, palette, style

def create_outline_material():
    edit=unreal.MaterialEditingLibrary
    library=unreal.EditorAssetLibrary
    folder='/Game/Prototype/Materials'
    path=folder+'/M_CelOutline'
    material=unreal.load_asset(path) if library.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CelOutline',folder,unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property('two_sided',True)
    def node(kind): return edit.create_material_expression(material,getattr(unreal,'MaterialExpression'+kind))
    def wire(a,b,pin): assert edit.connect_material_expressions(a,'',b,pin)
    color=edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not color:
        color=node('Constant3Vector')
        assert edit.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    color.constant=linear_color(palette['Outline'])
    expand=edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    if not expand:
        expand=node('Multiply')
        assert edit.connect_material_property(expand,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    if library.get_metadata_tag(material,'GemTDOutlineVersion')!='2':
        # Flat-shaded vertices share positions but have different face normals.
        # A position-based displacement keeps their copies coincident, preventing
        # the cracks introduced by extruding each face along VertexNormalWS.
        position=node('WorldPosition')
        position.set_editor_property('world_position_shader_offset',unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
        center=node('ObjectPositionWS')
        radial=node('Subtract'); wire(position,radial,'A'); wire(center,radial,'B')
        direction=node('Normalize'); wire(radial,direction,'')
        wire(direction,expand,'A')
        sign=node('TwoSidedSign'); backfaces=node('OneMinus'); wire(sign,backfaces,'')
        assert edit.connect_material_property(backfaces,'',unreal.MaterialProperty.MP_OPACITY_MASK)
        library.set_metadata_tag(material,'GemTDOutlineVersion','2')
    expand.set_editor_property('const_b',style['outline_width_cm'])
    errors=edit.recompile_material(material); assert not errors,list(errors)
    assert library.save_loaded_asset(material),'Could not save the outline material'
    unreal.log('GEM_CLOSED_OUTLINE_MATERIAL_READY')
    return material

if __name__=='__main__':
    create_outline_material()
