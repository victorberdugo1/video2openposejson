# Set-Alias blender "C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"
# blender --background --python export_openpose.py -- Animation.fbx output_openpose.json

import bpy
import sys
import json
from mathutils import Vector, Matrix
import math

# --------------------------------------------------
# ARGUMENTOS
# --------------------------------------------------
argv = sys.argv
argv = argv[argv.index("--") + 1:]
fbx_path = argv[0]
output_path = argv[1]

# --------------------------------------------------
# AJUSTES
# --------------------------------------------------
scale_x_factor = 0.18
scale_y_factor = 0.7
scale_z_factor = 0.15

# Offsets finales (aplicados en espacio OpenPose)
facial_y_offset = 0.0
facial_z_offset = 0.01

shoulder_x_offset = 0.01
shoulder_z_offset = 0.0

# --------------------------------------------------
# LIMPIAR ESCENA
# --------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)

# --------------------------------------------------
# IMPORTAR FBX
# --------------------------------------------------
bpy.ops.import_scene.fbx(
    filepath=fbx_path,
    axis_forward='-Z',
    axis_up='Y',
    global_scale=1.0
)

# --------------------------------------------------
# ARMATURE
# --------------------------------------------------
armature = None
for obj in bpy.context.scene.objects:
    if obj.type == 'ARMATURE':
        armature = obj
        break

if not armature:
    raise Exception("No armature found")

scene = bpy.context.scene

# --------------------------------------------------
# MAPEO MIXAMO → OPENPOSE (SIN HEAD - solo Neck y faciales)
# --------------------------------------------------
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
}

# --------------------------------------------------
# MATRIZ DE CONVERSIÓN
# --------------------------------------------------
rot = Matrix.Rotation(math.radians(180), 4, 'Y') @ Matrix.Rotation(math.radians(90), 4, 'X')
rot3 = rot.to_3x3()

# --------------------------------------------------
# RANGO FRAMES
# --------------------------------------------------
if armature.animation_data and armature.animation_data.action:
    action = armature.animation_data.action
    frame_start = int(action.frame_range[0])
    frame_end = int(action.frame_range[1])
else:
    frame_start = scene.frame_start
    frame_end = scene.frame_end

# --------------------------------------------------
# FUNCIONES AUXILIARES
# --------------------------------------------------
def bone_point_world(bone, factor):
    """Punto a lo largo del hueso en espacio MUNDO."""
    return (armature.matrix_world @ bone.matrix) @ Vector((0, bone.length * factor, 0))


def factor_for(name):
    return {
        "Neck": 0.0,
        "LShoulder": 0.6,
        "RShoulder": 0.6,
        "LElbow": 0.5,
        "RElbow": 0.5,
        "LWrist": 0.9,
        "RWrist": 0.9,
        "LHip": 0.3,
        "RHip": 0.3,
        "LKnee": 0.5,
        "RKnee": 0.5,
        "LAnkle": 0.8,
        "RAnkle": 0.8,
    }[name]


def transform_world_to_openpose(p_world):
    """Transforma de mundo a OpenPose normalizado."""
    v = rot3 @ (p_world - center)
    return Vector((0.5 + v.x * scale_x, 0.5 + v.y * scale_y, 0.5 + v.z * scale_z))


def facial_points_from_head_bone(head_bone):
    """
    Genera puntos faciales basados en el hueso Head,
    con proporciones realistas según el JSON de referencia.
    """
    # Posición del Head en mundo
    head_matrix = armature.matrix_world @ head_bone.matrix
    head_pos = head_matrix @ Vector((0, head_bone.length * 0.7, 0))  # Punto medio-alto del head
    head_rot3 = head_matrix.to_3x3()
    
    # Vectores de orientación
    forward = head_rot3 @ Vector((0, 0, 1))
    up = head_rot3 @ Vector((0, 1, 0))
    right = head_rot3 @ Vector((1, 0, 0))
    
    # Escala base (ajustada según referencia)
    scale = head_bone.length * 0.8
    
    # Posiciones faciales en MUNDO (basadas en proporciones del JSON referencia)
    face_points_world = {
        # Nariz: adelante del centro, ligeramente abajo
        "Nose": head_pos + forward * (1.0 * scale) - up * (0.2 * scale),
        
        # Ojos: adelante, arriba, separados horizontalmente
        "REye": head_pos + forward * (0.8 * scale) + up * (0.1 * scale) + right * (0.3 * scale),
        "LEye": head_pos + forward * (0.8 * scale) + up * (0.1 * scale) - right * (0.3 * scale),
        
        # Orejas: a los lados, ligeramente atrás y al nivel de los ojos
        "REar": head_pos - forward * (0.5 * scale) - up * (0.05 * scale) + right * (0.35 * scale),
        "LEar": head_pos - forward * (0.5 * scale) - up * (0.05 * scale) - right * (0.35 * scale),
    }
    
    return face_points_world

# --------------------------------------------------
# CÁLCULO BASE (center, escalas)
# --------------------------------------------------
scene.frame_set(frame_start)
bpy.context.view_layer.update()

base_points = []
for mixa, opname in bone_map.items():
    b = armature.pose.bones.get(mixa)
    if b:
        p_world = bone_point_world(b, factor_for(opname))
        base_points.append(rot3 @ p_world)

if not base_points:
    raise Exception("No base points found for scaling/centering")

center = sum(base_points, Vector()) / len(base_points)

xs = [p.x for p in base_points]
ys = [p.y for p in base_points]
zs = [p.z for p in base_points]

range_x = max(xs) - min(xs) if max(xs) != min(xs) else 1.0
range_y = max(ys) - min(ys) if max(ys) != min(ys) else 1.0
range_z = max(zs) - min(zs) if max(zs) != min(zs) else 1.0

target_range = 0.9
scale_x = (target_range / range_x) * scale_x_factor
scale_y = (target_range / range_y) * scale_y_factor
scale_z = (target_range / range_z) * scale_z_factor

# --------------------------------------------------
# EXPORTAR FRAMES
# --------------------------------------------------
output = {}

for f in range(frame_start, frame_end + 1):
    scene.frame_set(f)
    bpy.context.view_layer.update()

    person = {}

    # 1) Joints principales del cuerpo (sin Head)
    for mixa, opname in bone_map.items():
        b = armature.pose.bones.get(mixa)
        if not b:
            continue

        p_world = bone_point_world(b, factor_for(opname))
        p_open = transform_world_to_openpose(p_world)

        person[opname] = {"x": float(p_open.x), "y": float(p_open.y), "z": float(p_open.z)}

    # 2) Puntos faciales desde el hueso Head
    head_bone = armature.pose.bones.get("mixamorig:Head")
    if head_bone:
        face_world = facial_points_from_head_bone(head_bone)
        
        for name, p_world in face_world.items():
            p_open = transform_world_to_openpose(p_world)
            person[name] = {"x": float(p_open.x), "y": float(p_open.y), "z": float(p_open.z)}

    # 3) Detectar orientación del cuerpo
    neck_bone = armature.pose.bones.get("mixamorig:Neck")
    if neck_bone:
        neck_forward_world = (armature.matrix_world @ neck_bone.matrix).to_3x3() @ Vector((0, 0, 1))
        neck_forward_open = rot3 @ neck_forward_world
        facing_back = neck_forward_open.z < 0
    else:
        facing_back = False

    # 4) Aplicar offsets finales
    shoulder_sign = -1 if facing_back else 1
    
    if "LShoulder" in person:
        person["LShoulder"]["x"] += shoulder_x_offset * shoulder_sign
        person["LShoulder"]["z"] += shoulder_z_offset
    if "RShoulder" in person:
        person["RShoulder"]["x"] -= shoulder_x_offset * shoulder_sign
        person["RShoulder"]["z"] += shoulder_z_offset

    facial_z_sign = -1 if facing_back else 1
    for name in ["Nose", "LEye", "REye", "LEar", "REar"]:
        if name in person:
            person[name]["y"] += facial_y_offset
            person[name]["z"] += facial_z_offset * facial_z_sign

    output[f"frame_{f:04d}"] = {"person_0": person}

# --------------------------------------------------
# GUARDAR JSON
# --------------------------------------------------
with open(output_path, "w") as f:
    json.dump(output, f, indent=2)

print("✓ Export complete")
print(f"✓ Frames: {frame_end - frame_start + 1}")
print(f"✓ Body joints: {len(bone_map)}")
print("✓ Facial points: 5 (Nose, LEye, REye, LEar, REar)")