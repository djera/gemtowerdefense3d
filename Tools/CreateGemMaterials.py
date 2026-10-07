"""Opaque polished gems: physical clear coat over the existing cel colors."""
import unreal, json, sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'Tools'))
from CelMaterial import finish_cel, update_cel_defaults, linear_color, palette
surface=json.loads((root/'assets/GemSurface.json').read_text())
gem_types=('Amethyst','Aquamarine','Diamond','Emerald','Opal','Ruby','Sapphire','Topaz')

def create_gem_materials():
    edit=unreal.MaterialEditingLibrary; library=unreal.EditorAssetLibrary
    folder='/Game/Prototype/Materials'; path=folder+'/M_GemPolished'
    if library.does_asset_exist(path): material=unreal.load_asset(path)
    else:
        material=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GemPolished',folder,unreal.Material,unreal.MaterialFactoryNew())
        def node(kind): return edit.create_material_expression(material,getattr(unreal,'MaterialExpression'+kind))
        def wire(a,b,pin): assert edit.connect_material_expressions(a,'',b,pin)
        def parameter(name):
            p=node('ScalarParameter'); p.set_editor_property('parameter_name',name); p.set_editor_property('default_value',surface[name]); return p
        tint=node('VectorParameter'); tint.set_editor_property('parameter_name','PastelColor'); tint.set_editor_property('default_value',unreal.LinearColor(.5,.5,.5,1))
        finish_cel(material,tint)
        cel=edit.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        fill=node('Multiply'); wire(cel,fill,'A'); wire(parameter('cel_fill'),fill,'B')
        assert edit.connect_material_property(fill,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        base=node('Multiply'); wire(tint,base,'A'); wire(parameter('diffuse_weight'),base,'B')
        assert edit.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
        # Clear-coat root pins are hidden from Python's MaterialProperty enum;
        # MakeMaterialAttributes exposes the same inputs by their pin names.
        attributes=node('MakeMaterialAttributes')
        pins={''.join(c.lower() for c in str(pin) if c.isalnum()):str(pin) for pin in edit.get_material_expression_input_names(attributes)}
        wire(base,attributes,pins['basecolor']); wire(fill,attributes,pins['emissivecolor'])
        for name in ('roughness','specular','clear_coat','clear_coat_roughness'):
            wire(parameter(name),attributes,pins[name.replace('_','')])
        assert edit.connect_material_property(attributes,'',unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
        material.set_editor_property('use_material_attributes',True)
        material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_CLEAR_COAT)
        material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
    update_cel_defaults(material)
    assert library.save_loaded_asset(material)
    for gem in gem_types:
        instance=unreal.load_asset(folder+'/M_Cel_'+gem)
        edit.set_material_instance_parent(instance,material)
        edit.set_material_instance_vector_parameter_value(instance,'PastelColor',linear_color(palette[gem]))
        for name,value in surface.items():
            if name!='environment_intensity': edit.set_material_instance_scalar_parameter_value(instance,name,value)
        edit.update_material_instance(instance)
        assert library.save_loaded_asset(instance)
    unreal.log('GEM_POLISHED_OPAQUE_MATERIALS_READY')

def ensure_gem_reflection_environment():
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    lights=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.SkyLight) and a.get_actor_label()=='GemReflectionSky']
    light=lights[0] if lights else actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,500))
    light.set_actor_label('GemReflectionSky')
    component=light.get_component_by_class(unreal.SkyLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    component.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
    component.set_editor_property('intensity',surface['environment_intensity'])
    component.set_editor_property('cast_shadows',False)
    unreal.log('GEM_REFLECTION_ENVIRONMENT_READY')

if __name__=='__main__':
    create_gem_materials()
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert level.load_level('/Game/Maps/Prototype')
    ensure_gem_reflection_environment()
    assert level.save_current_level()
