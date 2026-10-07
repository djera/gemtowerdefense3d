"""Build the world-aligned S-tile material used by the board's runtime mask."""
import unreal, struct, zlib, json
from CelMaterial import finish_cel, linear_color
from pathlib import Path
root=Path(__file__).resolve().parents[1]
tools=unreal.AssetToolsHelpers.get_asset_tools()
edit=unreal.MaterialEditingLibrary
path='/Game/Prototype/Materials/M_TerrainBlendPastel'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    source=root/'assets/terrain/T_TerrainBlendDefault.png'
    source.parent.mkdir(parents=True,exist_ok=True)
    def chunk(kind,data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    source.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2,2,8,6,0,0,0))+
        chunk(b'IDAT',zlib.compress((b'\x00'+b'\xff\xff\x00\xff'*2)*2))+chunk(b'IEND',b''))
    task=unreal.AssetImportTask()
    task.filename=str(source); task.destination_path='/Game/Prototype/Textures'
    task.automated=True; task.save=True; task.replace_existing=True
    tools.import_asset_tasks([task])
    texture=unreal.load_asset('/Game/Prototype/Textures/T_TerrainBlendDefault')
    texture.set_editor_property('srgb',False)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    mat=tools.create_asset('M_TerrainBlendPastel','/Game/Prototype/Materials',unreal.Material,unreal.MaterialFactoryNew())
    edit.set_base_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    def node(kind,x=0,y=0): return edit.create_material_expression(mat,getattr(unreal,'MaterialExpression'+kind),x,y)
    def wire(a,out,b,pin): assert edit.connect_material_expressions(a,out,b,pin), (a.get_name(),b.get_name(),pin,list(edit.get_material_expression_input_names(b)))
    def value(number):
        n=node('Constant'); n.r=number; return n
    world=node('WorldPosition',-1000,0)
    xy=node('ComponentMask',-800,0)
    for channel,enabled in [('r',True),('g',True),('b',False),('a',False)]: xy.set_editor_property(channel,enabled)
    wire(world,'',xy,'')
    offset=node('Add',-600,0); offset.set_editor_property('const_b',1000); wire(xy,'',offset,'A')
    uv=node('Divide',-400,0); uv.set_editor_property('const_b',2000); wire(offset,'',uv,'A')
    sample=node('TextureSampleParameter2D',-200,0)
    sample.set_editor_property('parameter_name','TerrainMask'); sample.set_editor_property('texture',texture)
    sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    wire(uv,'',sample,'UVs')
    safe=node('Max',0,150); safe.set_editor_property('const_b',.001); wire(sample,'G',safe,'A')
    ratio=node('Divide',180,0); wire(sample,'R',ratio,'A'); wire(safe,'',ratio,'B')
    blend=node('SmoothStep',360,0)
    wire(value(.25),'',blend,'Min'); wire(value(.75),'',blend,'Max'); wire(ratio,'',blend,'Value')
    palette=json.loads((root/'assets/ArtPalette.json').read_text())
    grass=node('Constant3Vector',360,200); grass.constant=linear_color(palette['Grass'])
    snow=node('Constant3Vector',360,350); snow.constant=linear_color(palette['Snow'])
    color=node('LinearInterpolate',580,0)
    wire(grass,'',color,'A'); wire(snow,'',color,'B'); wire(blend,'',color,'Alpha')
    finish_cel(mat,color)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('GEM_TERRAIN_BLEND_MATERIAL_READY')
