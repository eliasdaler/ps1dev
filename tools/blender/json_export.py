import bpy
import json
import sys
import mathutils

from operator import attrgetter

from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.types import Operator

from mathutils import Quaternion, Vector

bl_info = {
    "name": "PSXTools .json export",
    "description": "A plugin for exporting scenes to custom .json",
    "author": "Elias Daler",
    "version": (0, 1),
    "blender": (4, 1, 0),
    "category": "Import-Export",
}

def apply_modifiers(obj):
    ctx = bpy.context.copy()
    ctx['object'] = obj
    for _, m in enumerate(obj.modifiers):
        try:
            ctx['modifier'] = m
            with bpy.context.temp_override(**ctx):
                bpy.ops.object.modifier_apply(modifier=m.name)
        except RuntimeError:
            print(f"Error applying {m.name} to {obj.name}, removing it instead.")
            obj.modifiers.remove(m)

    for m in obj.modifiers:
        obj.modifiers.remove(m)


def get_texture_filename(mat):
    tex_node = mat.node_tree.nodes.get('Image Texture')
    if not tex_node:
        return None
    return tex_node.image.filepath.removeprefix("//")


def material_has_texture(mat):
    return get_texture_filename(mat) is not None


def collect_meshes(scene):
    meshes_set = set(o.data for o in scene.objects if o.type == 'MESH')
    mesh_list = list(meshes_set)
    mesh_list.sort(key=attrgetter("name"))
    return mesh_list

def collect_objects(scene):
    obj_set = set(o for o in scene.objects if o.type == 'MESH')
    obj_list = list(obj_set)
    obj_list.sort(key=attrgetter("name"))
    return obj_list

def collect_materials(scene):
    material_set = set()
    for object in scene.objects:
        for material_slot in object.material_slots:
            material_set.add(material_slot.material)
    material_list = list(material_set)
    material_list.sort(key=attrgetter("name"))
    return material_list

identity_quat = Quaternion()
identity_scale = Vector((1.0, 1.0, 1.0))

def get_object_data_json(obj, meshes_idx_map):
    object_data = {
        "name": obj.name,
        "mesh": meshes_idx_map[obj.data.name],
        "position": [obj.location.x, obj.location.y, obj.location.z],
    }

    quat = obj.matrix_local.to_quaternion()
    if quat != identity_quat:
        object_data["rotation"] = [quat.w, quat.x, quat.y, quat.z]

    if obj.scale != identity_scale:
        object_data["scale"] = [obj.scale.x, obj.scale.y, obj.scale.z]

    return object_data


def mesh_has_materials_with_textures(mesh):
    return any([material_has_texture(mat) for mat in mesh.materials])


def get_mesh_json(mesh, material_idx_map):
    uv_layer = mesh.uv_layers.active.data
    vertex_colors = None
    if mesh.color_attributes:
        vertex_colors = mesh.color_attributes[0].data

    vertices = [None] * len(mesh.vertices)

    faces = []
    has_materials_with_textures = mesh_has_materials_with_textures(mesh)
    for poly in mesh.polygons:
        vertex_indices = []
        uvs = []
        for loop_index in poly.loop_indices:
            vi = mesh.loops[loop_index].vertex_index
            vertex_indices.append(vi)

            vertex = mesh.vertices[vi]
            vertex_data = {
                "pos": [vertex.co.x, vertex.co.y, vertex.co.z]
            }

            if has_materials_with_textures:
                uv = uv_layer[loop_index].uv
                uvs.append([uv.x, uv.y])

            if vertex_colors:
                color = vertex_colors[vi].color_srgb
                vertex_data["color"] = [
                    int(color[0] * 255),
                    int(color[1] * 255),
                    int(color[2] * 255)
                ]

            vertices[vi] = vertex_data

        faces.append({"vertices": vertex_indices, "uvs": uvs})

    return {
        "name": mesh.name,
        "vertices": vertices,
        "faces": faces,
        "materials": [material_idx_map[mat.name] for mat in mesh.materials],
    }


def get_material_json(material):
    material_data = {
        "name": material.name,
    }

    texture = get_texture_filename(material)
    if texture:
        material_data["texture"] = texture

    return material_data


def find_root_bone(armature):
    root_bones = [b for b in armature.data.bones if b.parent == None]
    if len(root_bones) == 0:
        raise ValueError(f"{pose} doesn't have any bones")
    elif len(root_bones) > 1:
        raise ValueError(f"{pose} has multiple roots")
    return root_bones[0]

# Go from Blender's coordinate system into glTF's coordinate system:
#   X' =  X
#   Y' = -Z
#   Z' =  Y 
axis_basis_change = mathutils.Matrix(
            ((1.0,  0.0, 0.0, 0.0), 
             (0.0,  0.0, 1.0, 0.0), 
             (0.0, -1.0, 0.0, 0.0), 
             (0.0,  0.0, 0.0, 1.0))
            )

def get_bone_data_json(bone, bone_name_to_id):
    transform_local = (axis_basis_change @ bone.matrix_local) if bone.parent == None else \
                      (bone.parent.matrix_local.inverted_safe() @ bone.matrix_local)
    translation, rotation, scale = transform_local.decompose()
    bone_data = {
            "name": bone.name,
            "translation": [translation.x, translation.y, translation.z],
            "rotation":    [rotation.w, rotation.x, rotation.y, rotation.z],
            "scale":       [scale.x, scale.y, scale.z],
    }
    if bone.children:
        bone_data["children"] = [ bone_name_to_id[c.name] for c in bone.children ]
    return bone_data


def write_psxtools_json(context, filepath):
    f = open(filepath, 'w', encoding='utf-8')

    scene = context.scene

    # collect meshes
    meshes_list = collect_meshes(scene)
    meshes_idx_map = {mesh.name: idx for idx, mesh in enumerate(meshes_list)}

    # collect materials
    material_list = collect_materials(scene)
    material_idx_map = {mat.name: idx for idx, mat in enumerate(material_list)}

    # apply modifiers
    for o in scene.objects:
        apply_modifiers(o)

    obj_list = collect_objects(scene)

    data = {
        "objects":   [get_object_data_json(obj, meshes_idx_map)
                      for obj in obj_list],
        "materials": [get_material_json(mat)
                      for mat in material_list],
        "meshes":    [get_mesh_json(mesh, material_idx_map)
                      for mesh in meshes_list],
    }

    armature = scene.objects['Armature.Test']
    if armature:
        bone_name_to_id = {}
        root_bone = find_root_bone(armature)
        bones = armature.data.bones
        for idx, bone in enumerate(bones):
            if bone.name in bone_name_to_id:
                raise ValueError('Repeated bone name: {bone.name}')
            bone_name_to_id[bone.name] = idx
        if bone_name_to_id[root_bone.name] != 0:
            raise ValueError('Root bone did not get id = 0')

        armature_data = {
            "joints": [get_bone_data_json(b, bone_name_to_id) for b in bones],
        }

        inverse_bind_matrices = [None] * len(bones)
        for idx, bone in enumerate(bones):
            ib = (axis_basis_change @ bone.matrix_local).inverted_safe()
            inverse_bind_matrices[idx] = [
                    [ib[0][0], ib[0][1], ib[0][2], ib[0][3]],
                    [ib[1][0], ib[1][1], ib[1][2], ib[1][3]],
                    [ib[2][0], ib[2][1], ib[2][2], ib[2][3]],
                    [ib[3][0], ib[3][1], ib[3][2], ib[3][3]],
                ]
        armature_data["inverseBindMatrices"] = inverse_bind_matrices

        # bone influences
        # TODO: check if this child exists
        if armature.children:
            obj = armature.children[0]
            bone_influences = [None] * len(bones)
            for idx, bone in enumerate(bones):
                gid = obj.vertex_groups[bone.name].index
                bone_influences[idx] = [v.index for v in obj.data.vertices \
                                        if gid in [g.group for g in v.groups]]
            armature_data["bone_influences"] = bone_influences
        else:
            bone_influences = [[0]] * len(bones)
            armature_data["bone_influences"] = bone_influences

        data["armature"] = armature_data

    json.dump(data, f, indent=4)
    f.close()
    return {'FINISHED'}


class ExportPSXToolsJSON(Operator, ExportHelper):
    """Save a psxtools .json"""
    bl_idname = "psxtools_json.save"
    bl_label = "Export psxtools .json"

    # ExportHelper mix-in class uses this.
    filename_ext = ".json"

    filter_glob = StringProperty(
        default="*.json",
        options={'HIDDEN'},
        maxlen=255,
    )

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.
    use_setting = BoolProperty(
        name="Example Boolean",
        description="Example Tooltip",
        default=True,
    )

    type = EnumProperty(
        name="Example Enum",
        description="Choose between two items",
        items=(
            ('OPT_A', "First Option", "Description one"),
            ('OPT_B', "Second Option", "Description two"),
        ),
        default='OPT_A',
    )

    def execute(self, context):
        return write_psxtools_json(context, self.filepath)


def menu_func_export(self, context):
    self.layout.operator(ExportPSXToolsJSON.bl_idname, text="psxtools .json")


classes = {
    ExportPSXToolsJSON
}


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)

    # TODO: unregistering ExportPSXToolsJSON class fails with
    # missing bl_rna attribute, but I'm too lazy to fix that
    try:
        for cls in classes:
            bpy.utils.unregister_class(cls)
    except RuntimeError:
        pass


if __name__ == "__main__":
    cli_mode = False
    argv = sys.argv[sys.argv.index("--") + 1:]  # get all args after "--"
    if len(argv) > 0:
        cli_mode = True
        export_filename = argv[0]
        print(f"CLI mode, export to: {export_filename}")
        write_psxtools_json(bpy.context, export_filename)
        sys.exit(0)

    if not cli_mode:
        register()

    # show export menu
    bpy.ops.psxtools_json.save('INVOKE_DEFAULT')
