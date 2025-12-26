# Set-Alias blender "C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"
# blender --background --python export_openpose.py -- .\Talking.fbx output_openpose.json

import bpy
import sys
import json
from mathutils import Vector, Matrix
import math

# --- Arguments ---
argv = sys.argv
argv = argv[argv.index("--") + 1:]  # after '--'
fbx_path = argv[0]
output_path = argv[1]

# ===== AJUSTA ESTOS VALORES PARA CONTROLAR EL ESCALADO =====
scale_x_factor = 0.25  # Escala en X (ancho)
scale_y_factor = 0.7  # Escala en Y (altura)
scale_z_factor = 0.1  # Escala en Z (profundidad)
facial_y_offset = -0.02  # Offset en Y para puntos faciales (altura)
facial_z_offset = -0.01  # Offset en Z para puntos faciales (profundidad)
shoulder_x_offset = 0.04  # Offset en X para separar los hombros
# ============================================================

# --- Clean scene ---
bpy.ops.wm.read_factory_settings(use_empty=True)

# --- Import FBX ---
bpy.ops.import_scene.fbx(
    filepath=fbx_path,
    axis_forward='-Z',
    axis_up='Y',
    global_scale=1.0
)

# --- Get armature ---
armature = None
for obj in bpy.context.scene.objects:
    if obj.type == 'ARMATURE':
        armature = obj
        break

if armature is None:
    raise Exception("No armature found in FBX")

# --- Debug: Print all bone names ---
print("\n=== Available bones in armature ===")
for bone in armature.pose.bones:
    print(f"  - {bone.name}")
print("===================================\n")

# --- Bone mapping (Mixamo -> OpenPose) ---
bone_map = {
    "mixamorig:Neck": "Neck",
    "mixamorig:LeftShoulder": "RShoulder",
    "mixamorig:RightShoulder": "LShoulder",
    "mixamorig:LeftForeArm": "RElbow",
    "mixamorig:RightForeArm": "LElbow",
    "mixamorig:LeftHand": "RWrist",
    "mixamorig:RightHand": "LWrist",
    "mixamorig:LeftUpLeg": "RHip",
    "mixamorig:RightUpLeg": "LHip",
    "mixamorig:LeftLeg": "RKnee",
    "mixamorig:RightLeg": "LKnee",
    "mixamorig:LeftFoot": "RAnkle",
    "mixamorig:RightFoot": "LAnkle",
    "mixamorig:Head": "Head",
}

# Verificar qué huesos existen
found_bones = []
missing_bones = []
for mixa_bone in bone_map.keys():
    if mixa_bone in armature.pose.bones:
        found_bones.append(mixa_bone)
    else:
        missing_bones.append(mixa_bone)

print(f"Found {len(found_bones)} bones from mapping")
if missing_bones:
    print(f"WARNING: Missing bones: {missing_bones}")
    
if len(found_bones) == 0:
    raise Exception("No bones from bone_map found in armature. Check bone names above.")

# --- Create rotation matrix ---
rot_x = Matrix.Rotation(math.radians(90), 4, 'X')
rot_y = Matrix.Rotation(math.radians(180), 4, 'Y')
rotation_matrix = rot_y @ rot_x

scene = bpy.context.scene

# Detectar el rango real de la animación desde el armature
if armature.animation_data and armature.animation_data.action:
    action = armature.animation_data.action
    frame_start = int(action.frame_range[0])
    frame_end = int(action.frame_range[1])
    print(f"Animation detected: frames {frame_start} to {frame_end}")
else:
    # Si no hay animación en el armature, usar el rango de la escena
    frame_start = scene.frame_start
    frame_end = scene.frame_end
    print(f"No animation data found, using scene range: {frame_start} to {frame_end}")

# --- Calculate point cloud bounds (all frames) ---
print("Calculating point cloud bounds...")
all_points = []

for f in range(frame_start, frame_end + 1):
    scene.frame_set(f)
    bpy.context.view_layer.update()
    
    for mixa in bone_map.keys():
        if mixa in armature.pose.bones:
            bone = armature.pose.bones[mixa]
            world_pos = armature.matrix_world @ bone.head
            rotated_pos = rotation_matrix @ world_pos
            all_points.append(rotated_pos)

if len(all_points) == 0:
    raise Exception("No valid bone positions found. Check bone mapping.")

# Calculate center of point cloud
center = Vector((0, 0, 0))
for p in all_points:
    center += p
center /= len(all_points)

# Calculate min/max for each axis independently
min_x = min(p.x for p in all_points)
max_x = max(p.x for p in all_points)
min_y = min(p.y for p in all_points)
max_y = max(p.y for p in all_points)
min_z = min(p.z for p in all_points)
max_z = max(p.z for p in all_points)

# Calculate range for each axis
range_x = max_x - min_x
range_y = max_y - min_y
range_z = max_z - min_z

# Calculate scale factor for each axis to fit in 0-1 with margin
margin = 0.05  # 5% margin on each side
target_range = 1.0 - (2 * margin)

scale_x = (target_range / range_x if range_x > 0 else 1.0) * scale_x_factor
scale_y = (target_range / range_y if range_y > 0 else 1.0) * scale_y_factor
scale_z = (target_range / range_z if range_z > 0 else 1.0) * scale_z_factor

print(f"Point cloud center: [{center.x:.3f}, {center.y:.3f}, {center.z:.3f}]")
print(f"Ranges - X: {range_x:.3f}, Y: {range_y:.3f}, Z: {range_z:.3f}")
print(f"User scale factors - X: {scale_x_factor:.2f}, Y: {scale_y_factor:.2f}, Z: {scale_z_factor:.2f}")
print(f"Final scale factors - X: {scale_x:.6f}, Y: {scale_y:.6f}, Z: {scale_z:.6f}")
print(f"Facial offsets - Y: {facial_y_offset:.3f}, Z: {facial_z_offset:.3f}")
print(f"Shoulder X offset: {shoulder_x_offset:.3f}")

# Target center (middle of 0-1 space)
target_center = Vector((0.5, 0.5, 0.5))

def get_bone_rotation_matrix(bone, armature):
    """Obtiene la matriz de rotación del hueso en espacio mundial"""
    world_matrix = armature.matrix_world @ bone.matrix
    return world_matrix.to_3x3()

def generate_facial_points(head_bone, neck_pos, armature):
    """
    Genera los puntos faciales usando la rotación real del hueso Head
    """
    head_pos = armature.matrix_world @ head_bone.head
    
    # Obtener la matriz de rotación del hueso Head
    head_rotation = get_bone_rotation_matrix(head_bone, armature)
    
    # Aplicar la rotación de conversión a la posición y vectores
    head_pos_rotated = rotation_matrix @ head_pos
    neck_pos_rotated = rotation_matrix @ neck_pos
    
    # Los vectores de orientación del hueso en su sistema local
    local_up = Vector((0, 1, 0))
    local_forward = Vector((0, 0, 1))
    local_right = Vector((1, 0, 0))
    
    # Transformar estos vectores usando la rotación del hueso
    world_up = head_rotation @ local_up
    world_forward = head_rotation @ local_forward
    world_right = head_rotation @ local_right
    
    # Aplicar la rotación de conversión a los vectores de dirección
    face_up = rotation_matrix.to_3x3() @ world_up
    face_forward = rotation_matrix.to_3x3() @ world_forward
    face_right = rotation_matrix.to_3x3() @ world_right
    
    # Normalizar
    face_up.normalize()
    face_forward.normalize()
    face_right.normalize()
    
    # Calcular tamaño de cabeza basado en distancia cuello-cabeza
    head_neck_dist = (head_pos_rotated - neck_pos_rotated).length
    head_size = head_neck_dist * 0.5
    
    facial_points = {}
    
    # Nariz
    nose_offset = face_forward * head_size * 1.2
    nose_offset += face_up * (-head_size * 0.2)
    facial_points["Nose"] = head_pos_rotated + nose_offset
    
    # Ojos
    eye_forward = head_size * 0.8
    eye_up = head_size * 0.3
    eye_separation = head_size * 0.4
    
    eye_base = head_pos_rotated + face_forward * eye_forward + face_up * eye_up
    facial_points["LEye"] = eye_base - face_right * eye_separation
    facial_points["REye"] = eye_base + face_right * eye_separation
    
    # Orejas
    ear_back = -head_size * 0.3
    ear_up = head_size * 0.2
    ear_separation = head_size * 0.8
    
    ear_base = head_pos_rotated + face_forward * ear_back + face_up * ear_up
    facial_points["LEar"] = ear_base - face_right * ear_separation
    facial_points["REar"] = ear_base + face_right * ear_separation
    
    return facial_points

def transform_and_scale_point(pos, bone_name=None):
    """Transforma y escala un punto al espacio [0,1]"""
    centered_pos = pos - center
    
    scaled_pos = Vector((
        centered_pos.x * scale_x,
        centered_pos.y * scale_y,
        centered_pos.z * scale_z
    ))
    
    final_pos = scaled_pos + target_center
    
    # Aplicar offsets
    if bone_name in ["Nose", "LEye", "REye", "LEar", "REar"]:
        final_pos.y += facial_y_offset
        final_pos.z += facial_z_offset
    elif bone_name == "LShoulder":
        final_pos.x += shoulder_x_offset
    elif bone_name == "RShoulder":
        final_pos.x -= shoulder_x_offset
    
    return {
        "x": round(final_pos.x, 6),
        "y": round(final_pos.y, 6),
        "z": round(final_pos.z, 6)
    }

# --- Export frames ---
output = {}

for f in range(frame_start, frame_end + 1):
    scene.frame_set(f)
    bpy.context.view_layer.update()
    
    frame_data = {}
    head_bone = None
    neck_pos_world = None
    
    for mixa, openpose in bone_map.items():
        if mixa not in armature.pose.bones:
            continue
        
        bone = armature.pose.bones[mixa]
        world_pos = armature.matrix_world @ bone.head
        rotated_pos = rotation_matrix @ world_pos
        
        if openpose == "Head":
            head_bone = bone
        elif openpose == "Neck":
            neck_pos_world = world_pos
        
        final_dict = transform_and_scale_point(rotated_pos, bone_name=openpose)
        frame_data[openpose] = final_dict
    
    # Generar puntos faciales
    if head_bone and neck_pos_world:
        facial_points = generate_facial_points(head_bone, neck_pos_world, armature)
        for name, pos in facial_points.items():
            final_dict = transform_and_scale_point(pos, bone_name=name)
            frame_data[name] = final_dict
    
    output[f"frame_{f:04d}"] = {"person_0": frame_data}

# --- Save JSON ---
with open(output_path, "w") as f:
    json.dump(output, f, indent=2)

print(f"✓ OpenPose JSON exported to {output_path}")
print(f"✓ Processed {frame_end - frame_start + 1} frames")
print(f"✓ Generated facial points using Head bone rotation matrix")
print(f"✓ Point cloud scaled and centered in [0, 1] space")
