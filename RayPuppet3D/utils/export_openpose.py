# Set-Alias blender "C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"
# blender --background --python export_openpose.py -- Talking.fbx output_openpose.json

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
# AJUSTES (mantén/ajusta según necesites)
# --------------------------------------------------
scale_x_factor = 0.18
scale_y_factor = 0.7
scale_z_factor = 0.08

# Offsets faciales (se aplican relativos a la orientación de la cabeza, en Ejes OpenPose)
facial_y_offset = -0.02  # desplazamiento "arriba/abajo" relativo a la cabeza
facial_z_offset = 0.02   # desplazamiento "adelante/atrás" relativo a la cabeza

# Offsets hombros en OpenPose (puro X / Z)
shoulder_x_offset = 0.01   # ancho (X)
shoulder_z_offset = 0.0    # profundidad (Z) opcional

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
# MAPEO MIXAMO → OPENPOSE (NO CAMBIADO)
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
    "mixamorig:Head": "Head",
}

# --------------------------------------------------
# MATRIZ DE CONVERSIÓN (Mixamo -> OpenPose) 
# rot = Matrix.Rotation(math.radians(90), 4, 'Y') @ Matrix.Rotation(math.radians(90), 4, 'X')
# --------------------------------------------------
rot = Matrix.Rotation(math.radians(180), 4, 'Y') @ Matrix.Rotation(math.radians(90), 4, 'X')
rot3 = rot.to_3x3()

# --------------------------------------------------
# RANGO FRAMES (robusto)
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
    """
    Punto a lo largo del hueso en espacio MUNDO.
    """
    return (armature.matrix_world @ bone.matrix) @ Vector((0, bone.length * factor, 0))


def bone_right_world(bone):
    """
    Eje X del hueso en mundo (normalizado).
    """
    m = armature.matrix_world @ bone.matrix
    return (m.to_3x3() @ Vector((1, 0, 0))).normalized()


def factor_for(name):
    return {
        "Head": 0.3,
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
    """
    Centrado, rotación de ejes y escalado -> Vector en espacio OpenPose normalizado.
    """
    v = rot3 @ (p_world - center)
    return Vector((0.5 + v.x * scale_x, 0.5 + v.y * scale_y, 0.5 + v.z * scale_z))


def facial_points_world(head_pos_world, head_rot3_world):
    """
    Genera puntos faciales en ESPACIO MUNDO (sin offsets).
    """
    fw = head_rot3_world @ Vector((0, 0, 1))
    up = head_rot3_world @ Vector((0, 1, 0))
    rt = head_rot3_world @ Vector((1, 0, 0))
    s = 0.1

    return {
        "Nose": head_pos_world + fw * s,
        "LEye": head_pos_world + fw * 0.8 * s + up * 0.4 * s - rt * 0.6 * s,
        "REye": head_pos_world + fw * 0.8 * s + up * 0.4 * s + rt * 0.6 * s,
        "LEar": head_pos_world - fw * 0.2 * s - rt * 1.2 * s,
        "REar": head_pos_world - fw * 0.2 * s + rt * 1.2 * s,
    }

# --------------------------------------------------
# CALCULO BASE (center, escalas)
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

target_center = Vector((0.5, 0.5, 0.5))

# --------------------------------------------------
# EXPORTAR FRAMES (principal)
# --------------------------------------------------
output = {}

for f in range(frame_start, frame_end + 1):
    scene.frame_set(f)
    bpy.context.view_layer.update()

    person = {}
    head_world_pos = None
    head_rot3_world = None

    # 1) Joints principales
    for mixa, opname in bone_map.items():
        b = armature.pose.bones.get(mixa)
        if not b:
            continue

        p_world = bone_point_world(b, factor_for(opname))
        p_open = transform_world_to_openpose(p_world)

        # ---------- HOMBROS: offset horizontal en X puro de OpenPose (sin mezclar Z) ----------
        if opname in ("LShoulder", "RShoulder"):
            # proyección de right vector a X OpenPose (filtrado Z)
            right_open = rot3 @ bone_right_world(b)
            right_proj = Vector((right_open.x, 0.0, 0.0))
            if right_proj.length < 1e-6:
                right_proj = Vector((1.0, 0.0, 0.0))
            else:
                right_proj = right_proj.normalized()

            if opname == "LShoulder":
                p_open += right_proj * shoulder_x_offset
            else:
                p_open -= right_proj * shoulder_x_offset

            # profundidad Z opcional
            p_open.z += shoulder_z_offset

        person[opname] = {"x": float(p_open.x), "y": float(p_open.y), "z": float(p_open.z)}

        if opname == "Head":
            head_world_pos = p_world
            head_rot3_world = (armature.matrix_world @ b.matrix).to_3x3()

    # 2) Puntos faciales: generados en mundo -> transform -> offsets relativos a la cabeza en OpenPose
    if head_world_pos is not None and head_rot3_world is not None:
        # vectores de cabeza en espacio OpenPose (deciden dirección de los offsets)
        forward_open = rot3 @ (head_rot3_world @ Vector((0, 0, 1)))
        up_open = rot3 @ (head_rot3_world @ Vector((0, 1, 0)))

        # normalizar (fallback si algo raro)
        if forward_open.length < 1e-6:
            forward_open = Vector((0.0, 0.0, 1.0))
        else:
            forward_open = forward_open.normalized()

        if up_open.length < 1e-6:
            up_open = Vector((0.0, 1.0, 0.0))
        else:
            up_open = up_open.normalized()

        face_world = facial_points_world(head_world_pos, head_rot3_world)
        for name, p_w in face_world.items():
            p_open = transform_world_to_openpose(p_w)

            # Aplicar offsets relat. a la orientación de la cabeza (en espacio OpenPose)
            p_open += up_open * facial_y_offset
            p_open += forward_open * facial_z_offset

            person[name] = {"x": float(p_open.x), "y": float(p_open.y), "z": float(p_open.z)}

    output[f"frame_{f:04d}"] = {"person_0": person}

# --------------------------------------------------
# GUARDAR JSON
# --------------------------------------------------
with open(output_path, "w") as f:
    json.dump(output, f, indent=2)

print("✓ Export complete")
print(f"✓ Frames: {frame_end - frame_start + 1}")
print("✓ Shoulder X/Z offsets aplicados en OpenPose")
print("✓ Offsets faciales aplicados en ejes locales de la cabeza (convertidos a OpenPose)")

