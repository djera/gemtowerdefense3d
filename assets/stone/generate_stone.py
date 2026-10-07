"""Broad, grounded, angular boulder; run with Blender --background --python.
Coordinates are meters. The eight-vertex flat bottom lies at Z=0.
"""
import bpy
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parent
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'
scene.unit_settings.scale_length=1
# Irregular chamfered slab: its base is its widest section, not a sphere tip.
base=[(-.47,-.26),(-.33,-.43),(.19,-.45),(.43,-.29),(.46,.16),(.25,.41),(-.23,.40),(-.46,.18)]
shoulder_heights=[.22,.29,.25,.31,.34,.27,.32,.24]
top_heights=[.43,.47,.50,.46,.49,.54,.51,.45]
vertices=[(x,y,0) for x,y in base]
vertices += [(x*.94+.005,y*.93-.015,z) for (x,y),z in zip(base,shoulder_heights)]
vertices += [(x*.57+.045,y*.59-.025,z) for (x,y),z in zip(base,top_heights)]
faces=[tuple(reversed(range(8)))]
for ring in (0,1):
    for i in range(8):
        j=(i+1)%8
        faces.append((ring*8+i,ring*8+j,(ring+1)*8+j,(ring+1)*8+i))
faces += [(16,16+i,16+i+1) for i in range(1,7)]
mesh=bpy.data.meshes.new('Boulder_FacetedBody')
mesh.from_pydata(vertices,[],faces); mesh.update()
stone=bpy.data.objects.new('SM_MazeStone',mesh)
bpy.context.collection.objects.link(stone)
bpy.context.view_layer.objects.active=stone; stone.select_set(True)
body=bpy.data.materials.new('M_Stone'); body.diffuse_color=(.086,.102,.133,1); body.roughness=1
ink=bpy.data.materials.new('M_StoneInk'); ink.diffuse_color=(.004,.005,.009,1); ink.roughness=1
mesh.materials.append(body); mesh.materials.append(ink)
# Sparse broken creases are slightly raised ribbons on existing facets.
ink_vertices=[]; ink_faces=[]
def stroke(points,normal,width=.007):
    for a,b in zip(points,points[1:]):
        a,b=Vector(a)+normal*.001,Vector(b)+normal*.001
        side=(b-a).cross(normal).normalized()*width
        start=len(ink_vertices)
        ink_vertices.extend([a-side,b-side,b+side,a+side])
        ink_faces.append(tuple(start+i for i in range(4)))
for ids in [(16,18,19),(16,20,21),(8,9,17),(11,12,20),(14,15,23)]:
    a,b,c=(Vector(vertices[i]) for i in ids)
    normal=(b-a).cross(c-a).normalized()
    if normal.dot((a+b+c)/3-Vector((0,0,.2)))<0: normal=-normal
    stroke([a*.18+b*.72+c*.10,a*.43+b*.38+c*.19,a*.40+b*.14+c*.46],normal)
line_mesh=bpy.data.meshes.new('Boulder_InkFractures')
line_mesh.from_pydata(ink_vertices,[],ink_faces); line_mesh.materials.append(ink)
lines=bpy.data.objects.new('Boulder_InkFractures',line_mesh); bpy.context.collection.objects.link(lines)
lines.select_set(True); bpy.ops.object.join()
stone=bpy.context.object; stone.name='SM_MazeStone'
for face in stone.data.polygons: face.use_smooth=False
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.normals_make_consistent(inside=False)
bpy.ops.uv.smart_project(island_margin=.025)
bpy.ops.object.mode_set(mode='OBJECT')
assert abs(min(v.co.z for v in stone.data.vertices))<.00001
assert sum(abs(v.co.z)<.00001 for v in stone.data.vertices)>=8
bpy.ops.wm.save_as_mainfile(filepath=str(root/'MazeStone.blend'))
bpy.ops.export_scene.fbx(filepath=str(root/'SM_MazeStone.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',bake_anim=False)
print('GROUNDED_BOULDER_EXPORTED',len(stone.data.vertices),len(stone.data.polygons),tuple(stone.dimensions))
