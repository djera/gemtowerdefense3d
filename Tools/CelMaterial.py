import unreal, json
from pathlib import Path
edit=unreal.MaterialEditingLibrary
root=Path(__file__).resolve().parents[1]
style=json.loads((root/'assets/CelStyle.json').read_text())
palette=json.loads((root/'assets/ArtPalette.json').read_text())
CEL_VERSION='2'

def linear_color(hexcode):
    def linear(v): return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
    return unreal.LinearColor(*(linear(int(hexcode[i:i+2],16)/255) for i in (0,2,4)),1)

def finish_cel(mat,color):
    """High-contrast flat bands and optional ink borders on road cells."""
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    edit.set_base_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    def node(kind): return edit.create_material_expression(mat,getattr(unreal,'MaterialExpression'+kind))
    def value(v):
        n=node('Constant'); n.r=v; return n
    def parameter(name,v):
        n=node('ScalarParameter'); n.set_editor_property('parameter_name',name); n.set_editor_property('default_value',v); return n
    def wire(a,out,b,pin): assert edit.connect_material_expressions(a,out,b,pin)
    normal=node('PixelNormalWS')
    light=node('Constant3Vector'); light.constant=unreal.LinearColor(.3,-.4,.8660254,1)
    dot=node('DotProduct'); wire(normal,'',dot,'A'); wire(light,'',dot,'B')
    shadow=parameter('shadow_band',style['shadow_band']); mid=parameter('midtone_band',style['midtone_band'])
    highlight=parameter('highlight_band',style['highlight_band'])
    low=node('If'); wire(dot,'',low,'A'); wire(parameter('shadow_threshold',style['shadow_threshold']),'',low,'B')
    wire(mid,'',low,'A > B'); wire(shadow,'',low,'A < B'); wire(mid,'',low,'A == B')
    high=node('If'); wire(dot,'',high,'A'); wire(parameter('highlight_threshold',style['highlight_threshold']),'',high,'B')
    wire(highlight,'',high,'A > B'); wire(low,'',high,'A < B'); wire(highlight,'',high,'A == B')
    lit=node('Multiply'); wire(color,'',lit,'A'); wire(high,'',lit,'B')
    # World-aligned cell borders apply only to Road/Intersection instances.
    world=node('WorldPosition'); offset=node('Add'); offset.set_editor_property('const_b',1000); wire(world,'',offset,'A')
    cells=node('Divide'); cells.set_editor_property('const_b',100); wire(offset,'',cells,'A')
    fraction=node('Frac'); wire(cells,'',fraction,'')
    opposite=node('OneMinus'); wire(fraction,'',opposite,'')
    edge=node('Min'); wire(fraction,'',edge,'A'); wire(opposite,'',edge,'B')
    channels=[]
    for channel in ('r','g'):
        mask=node('ComponentMask')
        for c in ('r','g','b','a'): mask.set_editor_property(c,c==channel)
        wire(edge,'',mask,''); channels.append(mask)
    distance=node('Min'); wire(channels[0],'',distance,'A'); wire(channels[1],'',distance,'B')
    border=node('If'); wire(distance,'',border,'A'); wire(parameter('road_border_fraction',style['road_border_fraction']),'',border,'B')
    wire(value(0),'',border,'A > B'); wire(value(1),'',border,'A < B'); wire(value(1),'',border,'A == B')
    strength=node('Multiply'); wire(border,'',strength,'A'); wire(parameter('GridInk',0),'',strength,'B')
    ink=node('VectorParameter'); ink.set_editor_property('parameter_name','InkColor'); ink.set_editor_property('default_value',linear_color(palette['Outline']))
    output=node('LinearInterpolate'); wire(lit,'',output,'A'); wire(ink,'',output,'B'); wire(strength,'',output,'Alpha')
    edit.connect_material_property(output,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    errors=edit.recompile_material(mat)
    assert not errors, list(errors)
    unreal.EditorAssetLibrary.set_metadata_tag(mat,'GemTDCelVersion',CEL_VERSION)

def connected_nodes(mat):
    start=edit.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    queue=[start] if start else []; seen=set()
    for node in queue:
        if not node or node.get_path_name() in seen: continue
        seen.add(node.get_path_name()); yield node
        queue.extend(edit.get_inputs_for_material_expression(mat,node))

def update_cel_defaults(mat):
    for node in connected_nodes(mat):
        if isinstance(node,unreal.MaterialExpressionScalarParameter) and str(node.get_editor_property('parameter_name')) in style:
            node.set_editor_property('default_value',style[str(node.get_editor_property('parameter_name'))])
        if isinstance(node,unreal.MaterialExpressionVectorParameter) and str(node.get_editor_property('parameter_name'))=='InkColor':
            node.set_editor_property('default_value',linear_color(palette['Outline']))
    errors=edit.recompile_material(mat); assert not errors, list(errors)
