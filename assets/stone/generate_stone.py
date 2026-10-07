"""Deterministic, faceted maze boulder; run with Blender --background --python."""
import bpy, random, math
from pathlib import Path
root=Path(__file__).resolve().parent
random.seed(8721)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'
scene.unit_settings.scale_length=1
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1)
stone=bpy.context.object
stone.name='SM_MazeStone'
for v in stone.data.vertices:
    p=v.co
    variation=random.uniform(.88,1.10)
    p.x*=.46*variation
    p.y*=.43*variation
    p.z=max(0,.33+p.z*.34*variation)
stone.data.update()
mat=bpy.data.materials.new('M_Stone')
mat.diffuse_color=(.20,.23,.27,1)
mat.roughness=.94
stone.data.materials.append(mat)
for face in stone.data.polygons: face.use_smooth=False
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.remove_doubles(threshold=.0001)
bpy.ops.mesh.normals_make_consistent(inside=False)
bpy.ops.uv.smart_project(island_margin=.025)
bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.wm.save_as_mainfile(filepath=str(root/'MazeStone.blend'))
bpy.ops.export_scene.fbx(filepath=str(root/'SM_MazeStone.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',bake_anim=False)
print('MAZE_STONE_EXPORTED',len(stone.data.vertices),len(stone.data.polygons))
