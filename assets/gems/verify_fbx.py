import bpy, json
from pathlib import Path
root=Path(__file__).resolve().parent
results=[]
for path in sorted((root/'FBX').glob('*.fbx')):
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(path))
    objects=[o for o in bpy.context.selected_objects if o.type=='MESH']
    assert len(objects)==1, path.name
    ob=objects[0]
    assert len(ob.data.uv_layers)>0, path.name
    assert .12 < max(ob.dimensions) < .25, (path.name,tuple(ob.dimensions))
    assert ob.location.length < .0001, path.name
    results.append({'file':path.name,'dimensions_m':list(ob.dimensions),'uv_layers':len(ob.data.uv_layers)})
assert len(results)==40
(root/'verification.json').write_text(json.dumps(results,indent=2))
print('FBX_ROUNDTRIP_PASSED: 40 meshes, correct scale, UVs and origin')
