import bpy
import json
import sys
import mathutils
import itertools
from enum import Enum

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

identity_quat = Quaternion()
identity_scale = Vector((1.0, 1.0, 1.0))

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

def apply_modifiers(obj):
    ctx = bpy.context.copy()
    ctx['object'] = obj
    for _, m in enumerate(obj.modifiers):
        if m.type == 'ARRAY': # tiles
            continue
        if m.type == 'ARMATURE':
            continue
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


def collect_meshes(obj_list):
    meshes_set = set(o.data for o in obj_list)

    # Add meshes from prefabs even if they're not used in obj_list
    # E.g. for tiles/tile meshes
    prefab_collection = bpy.data.collections.get("Prefabs")
    if prefab_collection:
        for o in collect_objects(prefab_collection.objects):
            meshes_set.add(o.data)

    mesh_list = list(meshes_set)
    mesh_list.sort(key=attrgetter("name"))
    print([mesh.name for mesh in mesh_list])

    return mesh_list

def collect_objects(collection_objects):
    obj_set = set(o for o in collection_objects if o.type == 'MESH')
    obj_list = list(obj_set)
    obj_list.sort(key=attrgetter("name"))
    return obj_list

def assert_no_rotation(obj):
    quat = obj.matrix_local.to_quaternion()
    if quat != identity_quat:
        raise ValueError(f'"{obj.name}" has rotation')

def get_aabb(o):
    ps = [axis_basis_change @ o.matrix_world @ Vector(corner) for corner in o.bound_box]
    x_coords, y_coords, z_coords = zip(*ps)

    return {
        "min": [min(x_coords), min(y_coords), min(z_coords)],
        "max": [max(x_coords), max(y_coords), max(z_coords)],
    }

def get_collisions_json():
    collection = bpy.data.collections.get("Collision")
    if collection is None:
        return

    cs = []
    for o in collection.objects:
        assert_no_rotation(o)
        cs.append({"type":"box", "aabb": get_aabb(o)})
    return cs

interaction_prefix = "Interact."

def get_triggers_json():
    collection = bpy.data.collections.get("Triggers")
    if collection is None:
        return

    cs = []
    for o in collection.objects:
        assert_no_rotation(o)
        name = o.name

        interaction = False
        if name.startswith(interaction_prefix):
            interaction = True
            name = name[len(interaction_prefix):]

        obj = { 
            "name": name, 
            "interaction": interaction,
            "collider": {
                "type": "box", 
                "aabb": get_aabb(o),
            },
        }

        cs.append(obj)

    return cs

def collect_materials(scene):
    material_set = set()
    for object in scene.objects:
        for material_slot in object.material_slots:
            material_set.add(material_slot.material)
    material_list = list(material_set)
    material_list.sort(key=attrgetter("name"))
    return material_list

def get_object_data_json(obj, meshes_idx_map, has_armature, armature_scale):
    object_data = {
        "name": obj.name,
        "position": [obj.location.x, obj.location.z, -obj.location.y],
    }

    if not has_armature:
        object_data["mesh"] = meshes_idx_map[obj.data.name]

    quat = obj.matrix_local.to_quaternion()
    if quat != identity_quat:
        object_data["rotation"] = [quat.w, quat.x, quat.y, quat.z]

    if has_armature: 
        # if armature has scale, we change the object's by it so that scaling
        # armature automatically scales the model as well
        obj.scale = obj.scale * armature_scale

    if obj.scale != identity_scale:
        object_data["scale"] = [obj.scale.x, obj.scale.y, obj.scale.z]

    return object_data


def mesh_has_materials_with_textures(mesh):
    return any([material_has_texture(mat) for mat in mesh.materials])


def get_mesh_json(mesh, material_idx_map):
    uv_layer = mesh.uv_layers[0].data
    vertex_colors = None
    if mesh.color_attributes:
        if mesh.color_attributes[0].domain == "CORNER":
            raise ValueError(f"Wrong color attribute for mesh {mesh.name}: should be 'Vertex', not 'Face Corner'")
        vertex_colors = mesh.color_attributes[0].data

    vertices = [None] * len(mesh.vertices)
    faces = []
    has_materials_with_textures = mesh_has_materials_with_textures(mesh)

    bias_attr = mesh.attributes.get("Bias")

    for face_idx, poly in enumerate(mesh.polygons):
        face_vert_indices = []
        face_uvs = []
        for loop_index in poly.loop_indices:
            vi = mesh.loops[loop_index].vertex_index
            face_vert_indices.append(vi)

            vertex = mesh.vertices[vi]
            vertex_data = {
                "pos": [vertex.co.x, vertex.co.z, -vertex.co.y]
            }

            if has_materials_with_textures:
                uv = uv_layer[loop_index].uv
                face_uvs.append([uv.x, uv.y])

            if vertex_colors:
                color = vertex_colors[vi].color_srgb
                vertex_data["color"] = [
                    int(color[0] * 255),
                    int(color[1] * 255),
                    int(color[2] * 255)
                ]

            vertices[vi] = vertex_data

        face_json = {
            "vertices": face_vert_indices,
        }

        if has_materials_with_textures:
            face_json["uvs"] = face_uvs
            material_idx = material_idx_map[mesh.materials[poly.material_index].name]
            face_json["material"] = material_idx

        if bias_attr:
            bias = bias_attr.data[face_idx].value
            if bias != 0:
                face_json["bias"] = bias

        faces.append(face_json)

    return {
        "name": mesh.name,
        "faces": faces,
        "vertices": vertices,
    }

def get_mesh_json_armature(obj, meshes_list, material_idx_map, joints, joint_name_to_id):
    num_joints = len(joint_name_to_id)
    meshes_data = []

    for mesh in meshes_list:
        bias_attr = mesh.attributes.get("Bias")

        uv_layer = mesh.uv_layers[0].data
        vertex_colors = None
        if mesh.color_attributes:
            vertex_colors = mesh.color_attributes[0].data

        # This is similar to what we do in get_mesh_json,
        # but we separate the mesh into submeshes which are
        # influenced by a particular joint
        for idx, joint in enumerate(joints):
            gid = obj.vertex_groups[joint.name].index

            vertices = [] 
            faces = []

            has_materials_with_textures = mesh_has_materials_with_textures(mesh)

            for face_idx, poly in enumerate(mesh.polygons):
                face_vert_indices = []
                face_uvs = []

                skip_face = False
                for loop_index in poly.loop_indices:
                    vi = mesh.loops[loop_index].vertex_index
                    vertex = mesh.vertices[vi]

                    if skip_face or (gid not in (g.group for g in vertex.groups)):
                        skip_face = True
                        break

                    new_index = len(vertices)
                    face_vert_indices.append(new_index)

                    vertex_data = {
                        "pos": [vertex.co.x, vertex.co.z, -vertex.co.y]
                    }

                    if has_materials_with_textures:
                        uv = uv_layer[loop_index].uv
                        face_uvs.append([uv.x, uv.y])

                    if vertex_colors:
                        color = vertex_colors[vi].color_srgb
                        vertex_data["color"] = [
                            int(color[0] * 255),
                            int(color[1] * 255),
                            int(color[2] * 255)
                        ]

                    vertices.append(vertex_data)

                if not skip_face:
                    face_json = {
                        "vertices": face_vert_indices,
                        "uvs": face_uvs,
                    }
                    if has_materials_with_textures:
                        material_idx = material_idx_map[mesh.materials[poly.material_index].name]
                        face_json["material"] = material_idx
                    if bias_attr:
                        bias = bias_attr.data[face_idx].value
                        if bias != 0:
                            face_json["bias"] = bias

                    faces.append(face_json)

            if faces and vertices:
                meshes_data.append({
                    "name": mesh.name,
                    "joint_id": idx,
                    "faces": faces,
                    "vertices": vertices,
                })

    return meshes_data


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
    root_bone = root_bones[0]
    if root_bone.name != 'Root': # HACK for IK skeletons with "Root" bone
        return root_bone
    # Root bone has children (IK, etc. - find first deform bone)
    for bone in root_bone.children:
        if not skip_bone(bone):
            return bone

class Joint():
    def __init__(self, name, transform, translation, rotation, scale, children):
        self.name = name
        self.transform = transform
        self.translation = translation
        self.rotation = rotation
        self.scale = scale
        self.children = children

def has_scale(scale):
    return not (scale[0] == 1.0 and scale[1] == 1.0 and scale[2] == 1.0)

def get_joint_data(bone, joint_name_to_id, armature_scale):
    transform_global = (axis_basis_change @ bone.matrix_local) if bone.parent == None else \
                      (bone.parent.matrix_local.inverted_safe() @ bone.matrix_local)
    translation, rotation, scale = transform_global.decompose()
    if has_scale(armature_scale):
        translation.x *= armature_scale.x
        translation.y *= armature_scale.y
        translation.z *= armature_scale.z
        transform_global = mathutils.Matrix.LocRotScale(translation, rotation, scale)
    joint_data = Joint(bone.name, transform_global,
                       translation, rotation, scale,
                       [ joint_name_to_id[c.name] for c in bone.children if not skip_bone(c) ])
    return joint_data

class JointFCurves():
    def __init__(self):
        self.translation = [None] * 3
        self.rotation = [None] * 4

def skip_bone(bone):
    return (bone.name == "Root") or \
           ("FK" in bone.name) or ("IK" in bone.name) or \
           ("Pole" in bone.name) or ("Control" in bone.name) or \
           ("Pivot" in bone.name)


def get_joint_data_json(joint, joint_name_to_id):
    joint_data = {
            "name": joint.name,
            "translation": [joint.translation.x, joint.translation.y, joint.translation.z],
            "rotation":    [joint.rotation.w, joint.rotation.x, joint.rotation.y, joint.rotation.z],
            "scale":       [joint.scale.x, joint.scale.y, joint.scale.z],
    }
    if joint.children:
        joint_data["children"] = joint.children
    return joint_data

def find_unique_times(tracks):
    return sorted(set(itertools.chain.from_iterable(
            [
                [k.co[0] for k in keys.keyframe_points] 
                 for keys in tracks
            ]
        )))

class TrackType(Enum):
    ROTATION = 0
    TRANSLATION = 1
    SCALE = 2

class Track():
    def __init__(self, track_type : TrackType, joint_id, keys):
        self.track_type = track_type
        self.joint_id = joint_id
        self.keys = keys
    
    def toJSON(self):
        return {
            "track_type": self.track_type.value,
            "joint_id": self.joint_id,
            "keys": self.keys
        }

def vectors_close(a, b):
    if a is None or b is None:
        return False
    eps = 0.0001
    for idx in range(len(a)):
        if abs(a[idx] - b[idx]) > eps:
            return False
    return True

def is_const_track(track):
    first_key_value = track[0][1]
    for key in track:
        if not vectors_close(key[1], first_key_value):
            return False
    return True

# Removes keys with the similar values
# Note, we need to repeat keys twice for interpolation to work correctly, e.g.
# [t1, v1], [t2, v1], [t3, v2] and NOT [t1, v1], [t3, v2]
def remove_repeated_values(track):
    result = []
    value = None
    num_repeats = 0
    for idx, key in enumerate(track):
        if not vectors_close(key[1], value):
            if num_repeats > 1:
                result.append(track[idx - 1])
            result.append(key)
            value = key[1]
            num_repeats = 0
        else:
            num_repeats += 1
    return result

def get_action_json(joints, joint_name_to_id, action, armature_scale, unused_joints):
    num_joints = len(joints)
    curves = { }
    for idx, fcurve in enumerate(action.fcurves):
        if not fcurve.group.name in joint_name_to_id:
            continue
        joint_id = joint_name_to_id[fcurve.group.name]
        if joint_id == None:
            continue

        joint_curves = None
        if joint_id not in curves:
            joint_curves = JointFCurves()
        else:
            joint_curves = curves[joint_id]

        if fcurve.data_path.endswith('location'):
            joint_curves.translation[fcurve.array_index] = fcurve
        elif fcurve.data_path.endswith('rotation_quaternion'):
            joint_curves.rotation[fcurve.array_index] = fcurve
        # skip scale keys for now
        curves[joint_id] = joint_curves

    final_tracks = []
    for joint_id, cv in curves.items():
        if joint_id in unused_joints:
            continue
        joint = joints[joint_id]
        translation_track_times = find_unique_times(cv.translation)
        translation_track = []
        for t in translation_track_times:
            t_local = Vector((0.0, 0.0, 0.0))
            for i in range(3):
                t_local[i] = cv.translation[i].evaluate(t) * armature_scale[i]
            t_global = joint.transform @ t_local
            translation_track.append([t, [t_global.x, t_global.y, t_global.z]])

        if is_const_track(translation_track):
            translation_track = [translation_track[0]]

        rotation_track_times = find_unique_times(cv.translation)
        rotation_track = []
        for t in rotation_track_times:
            q_local = Quaternion()
            for i in range(4):
                q_local[i] = cv.rotation[i].evaluate(t)
            q_global = joint.rotation @ q_local
            rotation_track.append([t, [q_global.w, q_global.x, q_global.y, q_global.z]])

        if is_const_track(rotation_track):
            rotation_track = [rotation_track[0]]
        else:
            rotation_track = remove_repeated_values(rotation_track)

        final_tracks.append(Track(TrackType.TRANSLATION, joint_id, translation_track))
        final_tracks.append(Track(TrackType.ROTATION, joint_id, rotation_track))

    # TODO: use_frame_range + frame_start/frame_end?
    if (action.frame_range[0] != 0):
        raise ValueError('Animation does not start from 0 frame')
    action_length = action.frame_range[1]

    looped = True
    if action.use_frame_range: # most animations are looped by default
        looped = action.use_cyclic

    return {
        "name": action.name,
        "looped": looped,
        "length": action_length,
        "tracks": [t.toJSON() for t in final_tracks]
    }

def any_object_selected():
    return not any(obj.select_get() for obj in bpy.context.view_layer.objects)

def write_psxtools_json(context, filepath):
    f = open(filepath, 'w', encoding='utf-8')

    scene = context.scene

    # apply modifiers
    for o in scene.objects:
        apply_modifiers(o)

    # TODO: somehow allow to place objects in other collections?
    default_collection = bpy.data.collections.get("Collection")
    obj_list = collect_objects(default_collection.objects)

    # collect meshes
    meshes_list = collect_meshes(obj_list)
    meshes_idx_map = {mesh.name: idx for idx, mesh in enumerate(meshes_list)}

    # collect materials
    material_list = collect_materials(scene)
    material_idx_map = {mat.name: idx for idx, mat in enumerate(material_list)}

    if bpy.context.object:
        # in Edit/Pose/etc. modes some things don't work properly
        bpy.ops.object.mode_set(mode='OBJECT') 

    has_armature = 'Armature' in scene.objects
    armature_scale = None
    if has_armature:
        armature = scene.objects['Armature']
        armature_scale = armature.scale

    data = {
        "objects":   [get_object_data_json(obj, meshes_idx_map, has_armature, armature_scale)
                      for obj in obj_list],
        "materials": [get_material_json(mat)
                      for mat in material_list],
    }

    collisions = get_collisions_json()
    if collisions:
        data["collision"] = collisions

    triggers = get_triggers_json()
    if triggers:
        data["triggers"] = triggers

    # HACK: if no armature - each object gets a corresponding mesh
    # Otherwise, we assume that all meshes are submeshes of the only object in the file
    if not has_armature:
        data["meshes"] = [get_mesh_json(mesh, material_idx_map) for mesh in meshes_list]

    if has_armature:
        armature = scene.objects['Armature']
        armature_scale = armature.scale
        joint_name_to_id = {}
        root_bone = find_root_bone(armature)
        bones = armature.data.bones

        jointIdx = 0
        for idx, bone in enumerate(bones):
            if skip_bone(bone):
                continue
            if bone.name in joint_name_to_id:
                raise ValueError('Repeated bone name: {bone.name}')
            joint_name_to_id[bone.name] = jointIdx
            jointIdx += 1
        if joint_name_to_id[root_bone.name] != 0:
            raise ValueError('Root bone did not get id = 0')

        joints = [None] * len(joint_name_to_id)
        for idx, bone in enumerate(bones):
            if skip_bone(bone):
                continue
            joints[joint_name_to_id[bone.name]] = get_joint_data(bone, joint_name_to_id, armature_scale)

        # meshes
        data["meshes"] = get_mesh_json_armature(armature.children[0], meshes_list, material_idx_map, joints, joint_name_to_id)

        # Find unused joints - for now only drop leaf joints if no mesh if affected by it
        used_joints = set()
        unused_joints = set()
        for mesh in data["meshes"]:
            used_joints.add(int(mesh["joint_id"]))
        for j in joints:
            if len(j.children) == 0:
                joint_id = joint_name_to_id[j.name]
                if not joint_id in used_joints:
                    unused_joints.add(joint_id)

        # write armature
        num_joints = len(joint_name_to_id)
        inverse_bind_matrices = [None] * num_joints
        for bone in bones:
            if skip_bone(bone):
                continue
            local = bone.matrix_local.copy()
            if has_scale(armature_scale):
                translation, rotation, scale = local.decompose()
                translation.x *= armature_scale.x
                translation.y *= armature_scale.y
                translation.z *= armature_scale.z
                local = mathutils.Matrix.LocRotScale(translation, rotation, scale)
            bind_matrix = axis_basis_change @ local
            ib = bind_matrix.inverted_safe()
            inverse_bind_matrices[joint_name_to_id[bone.name]] = [
                    [ib[0][0], ib[0][1], ib[0][2], ib[0][3]],
                    [ib[1][0], ib[1][1], ib[1][2], ib[1][3]],
                    [ib[2][0], ib[2][1], ib[2][2], ib[2][3]],
                    [ib[3][0], ib[3][1], ib[3][2], ib[3][3]],
                ]

        armature_data = {
            "joints": [get_joint_data_json(j, joint_name_to_id) for j in joints]
        }

        armature_data["inverseBindMatrices"] = inverse_bind_matrices
        data["armature"] = armature_data

        # Go into pose mode, otherwise animation bake won't work
        bpy.context.view_layer.objects.active = armature
        bpy.ops.object.mode_set(mode='POSE')

        data["animations"] = []
        for action in bpy.data.actions:
            start_frame = int(action.frame_range[0])
            end_frame = int(action.frame_range[1])
            if action.use_frame_range:
                start_frame = int(action.frame_start)
                end_frame = int(action.frame_end)

            armature.animation_data.action = action
            bpy.ops.nla.bake(
                    frame_start=start_frame, 
                    frame_end=end_frame, 
                    step=1,
                    only_selected=False, 
                    visual_keying=True, 
                    clear_constraints=False, 
                    clear_parents=False, 
                    use_current_action=True, 
                    clean_curves=False, 
                    bake_types={'POSE'},
                    channel_types={'LOCATION', 'ROTATION'},
            )
            action_json = get_action_json(joints, joint_name_to_id, 
                                          action, armature_scale, unused_joints)
            data["animations"].append(action_json)

    json.dump(data, f, indent=2)
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
