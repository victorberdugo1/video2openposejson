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
facial_z_offset = 0.0  # Offset en Z para puntos faciales (profundidad)
shoulder_x_offset = 0.04  # Offset en X para separar los hombros (ajusta este valor)
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

# --- Create rotation matrix ---
rot_x = Matrix.Rotation(math.radians(90), 4, 'X')
rot_y = Matrix.Rotation(math.radians(180), 4, 'Y')
rotation_matrix = rot_y @ rot_x

scene = bpy.context.scene
frame_start = scene.frame_start
frame_end = scene.frame_end

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

def generate_facial_points(head_pos, neck_pos):
    """
    Genera los puntos faciales ANTES del escalado
    basándose en la posición de la cabeza y el cuello en el espacio original
    """
    # Calcular dirección de la cara (de cuello a cabeza)
    face_up = head_pos - neck_pos
    if face_up.length < 0.001:
        face_up = Vector((0, 1, 0))
    face_up.normalize()
    
    # Vector derecha perpendicular (en el plano XZ)
    face_right = Vector((-face_up.z, 0, face_up.x))
    if face_right.length < 0.001:
        face_right = Vector((1, 0, 0))
    face_right.normalize()
    
    # Vector adelante (perpendicular a up y right)
    face_forward = face_right.cross(face_up)
    face_forward.normalize()
    
    # Calcular tamaño de cabeza basado en distancia cuello-cabeza
    head_neck_dist = (head_pos - neck_pos).length
    head_size = head_neck_dist * 0.5  # 50% de la distancia cuello-cabeza
    
    facial_points = {}
    
    # Nariz - adelante y ligeramente abajo desde la cabeza
    nose_offset = face_forward * head_size * 1.2
    nose_offset += face_up * (-head_size * 0.2)  # Un poco abajo
    facial_points["Nose"] = head_pos + nose_offset
    
    # Ojos - adelante, arriba y a los lados
    eye_forward = head_size * 0.8
    eye_up = head_size * 0.3
    eye_separation = head_size * 0.4
    
    eye_base = head_pos + face_forward * eye_forward + face_up * eye_up
    facial_points["REye"] = eye_base + face_right * eye_separation
    facial_points["LEye"] = eye_base - face_right * eye_separation
    
    # Orejas - atrás, al nivel de los ojos, más separadas
    ear_back = -head_size * 0.3
    ear_up = head_size * 0.2
    ear_separation = head_size * 0.8
    
    ear_base = head_pos + face_forward * ear_back + face_up * ear_up
    facial_points["REar"] = ear_base + face_right * ear_separation
    facial_points["LEar"] = ear_base - face_right * ear_separation
    
    return facial_points

def transform_and_scale_point(pos, bone_name=None):
    """Transforma y escala un punto al espacio [0,1]"""
    # Center around origin
    centered_pos = pos - center
    
    # Scale each axis independently
    scaled_pos = Vector((
        centered_pos.x * scale_x,
        centered_pos.y * scale_y,
        centered_pos.z * scale_z
    ))
    
    # Move to target center
    final_pos = scaled_pos + target_center
    
    # Aplicar offsets según el tipo de punto
    if bone_name in ["Nose", "LEye", "REye", "LEar", "REar"]:
        # Offsets para puntos faciales
        final_pos.y += facial_y_offset
        final_pos.z += facial_z_offset
    elif bone_name == "LShoulder":
        # LShoulder se mueve hacia la derecha (aumentar X)
        final_pos.x += shoulder_x_offset
    elif bone_name == "RShoulder":
        # RShoulder se mueve hacia la izquierda (disminuir X)
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
    
    # Primero, procesar los huesos normales Y guardar posiciones en espacio original
    head_pos_world = None
    neck_pos_world = None
    
    for mixa, openpose in bone_map.items():
        if mixa not in armature.pose.bones:
            continue
        
        bone = armature.pose.bones[mixa]
        world_pos = armature.matrix_world @ bone.head
        
        # Apply rotation
        rotated_pos = rotation_matrix @ world_pos
        
        # Guardar posiciones en espacio rotado pero SIN escalar
        if openpose == "Head":
            head_pos_world = rotated_pos
        elif openpose == "Neck":
            neck_pos_world = rotated_pos
        
        # Transform and scale para exportar (pasando el nombre del hueso)
        final_dict = transform_and_scale_point(rotated_pos, bone_name=openpose)
        frame_data[openpose] = final_dict
    
    # Generar puntos faciales ANTES del escalado
    if head_pos_world and neck_pos_world:
        facial_points = generate_facial_points(head_pos_world, neck_pos_world)
        
        # Ahora SÍ aplicar la transformación y escalado a los puntos faciales CON OFFSET
        for name, pos in facial_points.items():
            final_dict = transform_and_scale_point(pos, bone_name=name)
            frame_data[name] = final_dict
    
    output[f"frame_{f:04d}"] = {"person_0": frame_data}

# --- Save JSON ---
with open(output_path, "w") as f:
    json.dump(output, f, indent=2)

print(f"✓ OpenPose JSON exported to {output_path}")
print(f"✓ Processed {frame_end - frame_start + 1} frames")
print(f"✓ Generated facial points from Head bone orientation")
print(f"✓ Point cloud scaled and centered in [0, 1] space")
print(f"✓ Facial points offset - Y: {facial_y_offset}, Z: {facial_z_offset}")
print(f"✓ Shoulders separated by X offset: {shoulder_x_offset}")