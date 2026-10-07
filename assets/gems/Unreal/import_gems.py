"""Execute in Unreal Editor with the Python Editor Script Plugin enabled."""
import unreal, json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / 'manifest.json').read_text())
tools = unreal.AssetToolsHelpers.get_asset_tools()
edit = unreal.MaterialEditingLibrary

def import_file(path, destination, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = destination
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options: task.options = options
    tools.import_asset_tasks([task])
    if not task.imported_object_paths: raise RuntimeError('Import failed: ' + str(path))
    return unreal.load_asset(task.imported_object_paths[0])

for asset in manifest['assets']:
    name = asset['name']
    _, kind, quality = name.split('_')
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.generate_lightmap_u_vs = True
    mesh = import_file(ROOT/'FBX'/(name+'.fbx'), '/Game/Gems/Meshes', options)
    matname = 'M_' + kind + '_' + quality
    matpath = '/Game/Gems/Materials/' + matname
    mat = unreal.load_asset(matpath)
    if mat is None:
        mat = tools.create_asset(matname, '/Game/Gems/Materials', unreal.Material, unreal.MaterialFactoryNew())
    edit.delete_all_material_expressions(mat)
    if kind == 'Opal':
        texture = import_file(ROOT/'Textures'/('T_Opal_'+quality+'.png'), '/Game/Gems/Textures')
        color = edit.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -400, 0)
        color.texture = texture
        output = 'RGB'
    else:
        color = edit.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
        h = manifest['colors_srgb'][kind]
        def linear(v): return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
        rgb = [linear(int(h[i:i+2],16)/255) for i in (0,2,4)]
        color.constant = unreal.LinearColor(*rgb, 1)
        output = ''
    edit.connect_material_property(color, output, unreal.MaterialProperty.MP_BASE_COLOR)
    for prop, value, y in [(unreal.MaterialProperty.MP_ROUGHNESS,[.36,.27,.19,.12,.075][manifest['qualities'].index(quality)],180),(unreal.MaterialProperty.MP_METALLIC,.12,260),(unreal.MaterialProperty.MP_SPECULAR,.8,340)]:
        expr = edit.create_material_expression(mat, unreal.MaterialExpressionConstant, -200,y)
        expr.r = value
        edit.connect_material_property(expr,'',prop)
    edit.recompile_material(mat)
    mesh.set_material(0,mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
unreal.log('Imported 40 gem variants into /Game/Gems.')
