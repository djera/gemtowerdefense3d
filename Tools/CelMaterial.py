import unreal
edit=unreal.MaterialEditingLibrary

def linear_color(hexcode):
    def linear(v): return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
    return unreal.LinearColor(*(linear(int(hexcode[i:i+2],16)/255) for i in (0,2,4)),1)

def finish_cel(mat,color):
    """Three flat lighting bands, independent of the scene's light exposure."""
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    edit.set_base_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    def node(kind): return edit.create_material_expression(mat,getattr(unreal,'MaterialExpression'+kind))
    def value(v):
        n=node('Constant'); n.r=v; return n
    def wire(a,out,b,pin): assert edit.connect_material_expressions(a,out,b,pin)
    normal=node('PixelNormalWS')
    light=node('Constant3Vector'); light.constant=unreal.LinearColor(.3,-.4,.8660254,1)
    dot=node('DotProduct'); wire(normal,'',dot,'A'); wire(light,'',dot,'B')
    low=node('If'); wire(dot,'',low,'A'); wire(value(.15),'',low,'B')
    wire(value(.87),'',low,'A > B'); wire(value(.70),'',low,'A < B'); wire(value(.87),'',low,'A == B')
    high=node('If'); wire(dot,'',high,'A'); wire(value(.65),'',high,'B')
    wire(value(1),'',high,'A > B'); wire(low,'',high,'A < B'); wire(value(1),'',high,'A == B')
    output=node('Multiply'); wire(color,'',output,'A'); wire(high,'',output,'B')
    edit.connect_material_property(output,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    errors=edit.recompile_material(mat)
    assert not errors, list(errors)
