"""Run with Blender: blender --background --python generate_gems.py"""
import bpy, math, random, json
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'FBX'
OUT.mkdir(exist_ok=True)
COLORS = {'Amethyst':'9747E6','Aquamarine':'48CEDD','Diamond':'EDF3F8','Emerald':'19AD59','Opal':'293344','Ruby':'DA294B','Sapphire':'294FCC','Topaz':'F4B52E'}
QUALITIES = ['Chipped','Flawed','Normal','Flawless','Perfect']
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = 1.0

def linear(v):
    return v / 12.92 if v <= .04045 else ((v + .055) / 1.055)**2.4

def material(kind, quality):
    mat = bpy.data.materials.new('M_' + kind + '_' + quality)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bs = nodes.get('Principled BSDF')
    rgb = tuple(linear(int(COLORS[kind][i:i+2],16)/255) for i in (0,2,4))
    bs.inputs['Base Color'].default_value = (*rgb,1)
    bs.inputs['Metallic'].default_value = .12
    bs.inputs['Roughness'].default_value = [.36,.27,.19,.12,.075][QUALITIES.index(quality)]
    bs.inputs['IOR'].default_value = 2.417 if kind == 'Diamond' else 1.55
    # Opaque stylized shading preserves hue and silhouette at gameplay distance.
    bs.inputs['Coat Weight'].default_value = .65
    mat.diffuse_color = (*rgb,1)
    if kind == 'Opal':
        noise = nodes.new('ShaderNodeTexNoise')
        noise.inputs['Scale'].default_value = 8
        ramp = nodes.new('ShaderNodeValToRGB')
        ramp.color_ramp.elements.remove(ramp.color_ramp.elements[1])
        for index, (pos, col) in enumerate([(0,'293344'),(.35,'294FCC'),(.48,'19AD59'),(.60,'9747E6'),(.72,'DA294B'),(1,'F4B52E')]):
            el = ramp.color_ramp.elements[0] if index == 0 else ramp.color_ramp.elements.new(pos)
            el.position = pos
            el.color = tuple(linear(int(col[i:i+2],16)/255) for i in (0,2,4)) + (1,)
        mat.node_tree.links.new(noise.outputs['Fac'], ramp.inputs[0])
        mat.node_tree.links.new(ramp.outputs[0], bs.inputs['Base Color'])
    return mat

def mesh(quality):
    qi = QUALITIES.index(quality)
    n = [5,6,12,8,16][qi]
    # Entirely different silhouettes encode grade, independently of color.
    rings = [
        [(0.012,.16),(.045,.115),(.075,.045),(.052,.025)],
        [(.032,.17),(.063,.135),(.065,.045),(.045,.018)],
        [(.035,.15),(.070,.13),(.095,.10),(.100,.075),(.090,.040),(.060,.014)],
        [(.067,.125),(.086,.105),(.100,.075),(.100,.055),(.060,.020)],
        # High crown, thin girdle, and long arrow-like pavilion to a single tip.
        [(.050,.230),(.080,.207),(.100,.180),(.100,.174),(.046,.080)]
    ][qi]
    rng = random.Random(42 + qi)
    verts=[]
    for ri,(radius,z) in enumerate(rings):
        for j in range(n):
            a=2*math.pi*j/n
            r=radius
            zz=z
            if qi == 0:
                r *= [1.25,.35,1.10,.48,1.20][j]
                zz += rng.uniform(-.016,.016)
                x=r*math.cos(a)*.62 + .025*(zz/.16)
                y=r*math.sin(a)*1.2
            elif qi == 1:
                r *= [1,.82,1.05,1,.9,1.0][j]
                x=r*math.cos(a)*.72
                y=r*math.sin(a)*1.25
                zz += rng.uniform(-.006,.006)
            elif qi == 3:
                # Square outline with clipped corners and concentric step facets.
                outline=[(1,.65),(.65,1),(-.65,1),(-1,.65),(-1,-.65),(-.65,-1),(.65,-1),(1,-.65)]
                x,y=(v*r for v in outline[j])
            else:
                x,y=r*math.cos(a),r*math.sin(a)
                if qi == 2: y *= .60
            verts.append((x,y,zz))
    verts.append((0,0,0))
    tip=len(verts)-1
    faces=[tuple(range(n))]
    for ri in range(len(rings)-1):
        for j in range(n):
            a=ri*n+j; b=ri*n+(j+1)%n; c=(ri+1)*n+(j+1)%n; d=(ri+1)*n+j
            # Alternating triangular facets produce crisp brilliant-style reflections.
            if qi in (1,2,3) or ri == 2: faces.append((a,d,c,b))
            elif (j+ri)%2: faces.extend([(a,d,b),(b,d,c)])
            else: faces.extend([(a,d,c),(a,c,b)])
    last=(len(rings)-1)*n
    for j in range(n): faces.append((last+j,tip,last+(j+1)%n))
    data=bpy.data.meshes.new('SM_Gem_'+quality)
    data.from_pydata(verts,[],faces)
    data.update()
    obj=bpy.data.objects.new('SM_Gem_'+quality,data)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active=obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.uv.smart_project(island_margin=.03)
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.select_set(False)
    return data

meshes={q:mesh(q) for q in QUALITIES}
for obj in list(bpy.context.scene.objects): bpy.data.objects.remove(obj,do_unlink=True)
objects=[]
for row,kind in enumerate(COLORS):
    for col,q in enumerate(QUALITIES):
        obj=bpy.data.objects.new('SM_'+kind+'_'+q,meshes[q].copy())
        bpy.context.collection.objects.link(obj)
        obj.data.materials.append(material(kind,q))
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active=obj
        if kind == 'Opal':
            texdir=ROOT/'Textures'; texdir.mkdir(exist_ok=True)
            mat=obj.data.materials[0]
            img=bpy.data.images.new('T_Opal_'+q,width=256,height=256)
            node=mat.node_tree.nodes.new('ShaderNodeTexImage'); node.image=img
            mat.node_tree.nodes.active=node
            bpy.context.scene.render.engine='CYCLES'
            bpy.context.scene.cycles.samples=8
            bpy.ops.object.bake(type='DIFFUSE',pass_filter={'COLOR'},margin=8)
            img.filepath_raw=str(texdir/(img.name+'.png')); img.file_format='PNG'; img.save()
            mat.node_tree.links.new(node.outputs['Color'],mat.node_tree.nodes.get('Principled BSDF').inputs['Base Color'])
        bpy.ops.export_scene.fbx(filepath=str(OUT/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,mesh_smooth_type='FACE',add_leaf_bones=False,bake_anim=False)
        obj.location=(col*.29,row*.29,.07)
        obj.rotation_euler.x=math.radians(-65 if q == 'Perfect' else 40)
        objects.append(obj)

# Validate manifold topology and export coverage.
import bmesh
report=[]
for obj in objects:
    bm=bmesh.new(); bm.from_mesh(obj.data)
    assert all(e.is_manifold for e in bm.edges), obj.name
    assert len(obj.data.uv_layers)>0
    report.append({'name':obj.name,'vertices':len(obj.data.vertices),'triangles':sum(len(p.vertices)-2 for p in obj.data.polygons)})
    bm.free()
assert len(list(OUT.glob('*.fbx'))) == 40
(ROOT/'manifest.json').write_text(json.dumps({'colors_srgb':COLORS,'qualities':QUALITIES,'size_cm':{'maximum_width':20,'maximum_height':23},'grade_shapes':dict(zip(QUALITIES,['Jagged narrow shard','Elongated rough hexagonal crystal','Wide oval faceted stone','Square clipped-corner step cut','Brilliant crown with long pointed pavilion'])),'assets':report},indent=2))

# Orthographic contact sheet with readable labels.
def text_obj(body, x,y,size):
    curve=bpy.data.curves.new('Label','FONT'); curve.body=body; curve.size=size; curve.align_x='CENTER'
    ob=bpy.data.objects.new(body,curve); bpy.context.collection.objects.link(ob); ob.location=(x,y,.002)
    mat=bpy.data.materials.get('Labels')
    if not mat:
        mat=bpy.data.materials.new('Labels'); mat.diffuse_color=(.8,.85,.95,1)
    curve.materials.append(mat)
for col,q in enumerate(QUALITIES): text_obj(q,col*.29,2.28,.040)
for row,kind in enumerate(COLORS): text_obj(kind,-.30,row*.29-.02,.035)
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.015))
floor=bpy.context.object; floor.name='Preview_Backdrop'
mat=bpy.data.materials.new('Backdrop'); mat.diffuse_color=(.018,.024,.040,1); floor.data.materials.append(mat)
bpy.ops.object.camera_add(location=(.48,1.06,5))
cam=bpy.context.object; cam.rotation_euler=(0,0,0); cam.rotation_euler=(Vector((.48,1.06,0))-cam.location).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO'; cam.data.ortho_scale=2.65
scene=bpy.context.scene; scene.camera=cam
for location,power,size in [((-.8,1.5,2),250,1.5),((2,0,1.5),180,1),((.5,3,1),150,.8)]:
    bpy.ops.object.light_add(type='AREA',location=location)
    light=bpy.context.object; light.data.energy=power; light.data.shape='DISK'; light.data.size=size
    light.rotation_euler=(Vector((.5,1,0))-light.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='CYCLES'; scene.cycles.samples=32
scene.world.color=(.22,.22,.22)
scene.view_settings.view_transform='Standard'
scene.view_settings.exposure=-2
scene.render.resolution_x=1500; scene.render.resolution_y=1800; scene.render.resolution_percentage=100
scene.render.filepath=str(ROOT/'gem_preview.png')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'GemLibrary.blend'))
bpy.ops.render.render(write_still=True)
print('GEM_PACK_VALIDATED: 40 manifold UV-mapped meshes exported.')
