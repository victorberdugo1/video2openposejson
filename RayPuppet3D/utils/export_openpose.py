import bpy
import sys
import json
from mathutils import Vector, Matrix

# --- Arguments ---
argv = sys.argv
argv = argv[argv.index("--") + 1:]  # after '--'
fbx_path = argv[0]
output_path = argv[1]

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
    "mixamorig:Head": "Head",
    "mixamorig:LeftShoulder": "LShoulder",
    "mixamorig:RightShoulder": "RShoulder",
    "mixamorig:LeftArm": "LArm",
    "mixamorig:RightArm": "RArm",
    "mixamorig:LeftForeArm": "LElbow",
    "mixamorig:RightForeArm": "RElbow",
    "mixamorig:LeftHand": "LWrist",
    "mixamorig:RightHand": "RWrist",
    "mixamorig:Hips": "MidHip",
    "mixamorig:LeftUpLeg": "LHip",
    "mixamorig:RightUpLeg": "RHip",
    "mixamorig:LeftLeg": "LKnee",
    "mixamorig:RightLeg": "RKnee",
    "mixamorig:LeftFoot": "LAnkle",
    "mixamorig:RightFoot": "RAnkle",
}

# --- Coordinate conversion matrix ---
# Rotate 90° around X to stand upright, then 180° around Y to face camera
rot_x = Matrix.Rotation(1.5707963, 4, 'X')  # 90 degrees to stand up
rot_y = Matrix.Rotation(3.14159265, 4, 'Y')  # 180 degrees to face forward
scale_factor = 10.0  # Adjust this value to change size (10x larger)
scale_matrix = Matrix.Scale(scale_factor, 4)
conversion_matrix = scale_matrix @ rot_y @ rot_x

scene = bpy.context.scene
frame_start = scene.frame_start
frame_end = scene.frame_end

output = {}

for f in range(frame_start, frame_end + 1):
    scene.frame_set(f)
    bpy.context.view_layer.update()
    
    frame_data = {}
    
    for mixa, openpose in bone_map.items():
        if mixa not in armature.pose.bones:
            continue
        
        bone = armature.pose.bones[mixa]
        
        # Get world position
        world_pos = armature.matrix_world @ bone.head
        
        # Apply coordinate conversion
        converted_pos = conversion_matrix @ world_pos
        
        # Store as OpenPose format (X, Y, Z)
        frame_data[openpose] = {
            "x": round(converted_pos.x, 6),
            "y": round(converted_pos.y, 6),
            "z": round(converted_pos.z, 6)
        }
    
    # --- Face landmarks (approximate from head bone) ---
    if "mixamorig:Head" in armature.pose.bones:
        head = armature.pose.bones["mixamorig:Head"]
        head_world = armature.matrix_world @ head.head
        head_pos = conversion_matrix @ head_world
        
        # Nose (center of face)
        nose_offset = Vector((0, 0.05, 0))  # Slightly forward
        nose_pos = head_pos + nose_offset
        frame_data["Nose"] = {
            "x": round(nose_pos.x, 6),
            "y": round(nose_pos.y, 6),
            "z": round(nose_pos.z, 6)
        }
        
        # Eyes
        eye_offset_y = 0.02
        eye_offset_x = 0.04
        
        leye_pos = head_pos + Vector((-eye_offset_x, eye_offset_y, 0))
        frame_data["LEye"] = {
            "x": round(leye_pos.x, 6),
            "y": round(leye_pos.y, 6),
            "z": round(leye_pos.z, 6)
        }
        
        reye_pos = head_pos + Vector((eye_offset_x, eye_offset_y, 0))
        frame_data["REye"] = {
            "x": round(reye_pos.x, 6),
            "y": round(reye_pos.y, 6),
            "z": round(reye_pos.z, 6)
        }
        
        # Ears
        ear_offset_x = 0.07
        ear_offset_z = -0.02
        
        lear_pos = head_pos + Vector((-ear_offset_x, 0, ear_offset_z))
        frame_data["LEar"] = {
            "x": round(lear_pos.x, 6),
            "y": round(lear_pos.y, 6),
            "z": round(lear_pos.z, 6)
        }
        
        rear_pos = head_pos + Vector((ear_offset_x, 0, ear_offset_z))
        frame_data["REar"] = {
            "x": round(rear_pos.x, 6),
            "y": round(rear_pos.y, 6),
            "z": round(rear_pos.z, 6)
        }
    
    output[f"frame_{f:04d}"] = {"person_0": frame_data}

# --- Save JSON ---
with open(output_path, "w") as f:
    json.dump(output, f, indent=2)

print(f"✓ OpenPose JSON exported to {output_path}")
print(f"✓ Processed {frame_end - frame_start + 1} frames")
