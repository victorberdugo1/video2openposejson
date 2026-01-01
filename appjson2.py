#!/usr/bin/env python3

import os
import json
import cv2
import numpy as np
from PIL import Image, ImageDraw
from moviepy.editor import VideoFileClip, ImageSequenceClip
import gradio as gr

# --- MediaPipe Tasks API 0.10.x ---
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision

# ----------------------------
# CONFIGURACIÓN
# ----------------------------
MODEL_URL = "https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_full/float16/latest/pose_landmarker_full.task"
MODEL_PATH = "pose_landmarker_full.task"

# Descargar modelo si no existe
if not os.path.exists(MODEL_PATH):
    print("[INFO] Descargando modelo pose_landmarker_full.task...")
    import urllib.request
    try:
        urllib.request.urlretrieve(MODEL_URL, MODEL_PATH)
        print("[INFO] Modelo descargado correctamente.")
    except Exception as e:
        print(f"[ERROR] No se pudo descargar el modelo: {e}")

# Mapeo de MediaPipe (33 puntos) a OpenPose (18 puntos)
MEDIAPIPE_TO_OPENPOSE = {
    0: (0, "Nose"),
    1: (11, "Neck"),          # Calculado
    2: (12, "RShoulder"),
    3: (14, "RElbow"),
    4: (16, "RWrist"),
    5: (11, "LShoulder"),
    6: (13, "LElbow"),
    7: (15, "LWrist"),
    8: (24, "RHip"),
    9: (26, "RKnee"),
    10: (28, "RAnkle"),
    11: (23, "LHip"),
    12: (25, "LKnee"),
    13: (27, "LAnkle"),
    14: (5, "REye"),
    15: (2, "LEye"),
    16: (8, "REar"),
    17: (7, "LEar")
}

# Conexiones OpenPose 18
OPENPOSE_CONNECTIONS_18 = [
    (1, 2), (1, 5), (2, 3), (3, 4), (5, 6), (6, 7),
    (1, 8), (1, 11), (8, 9), (9, 10), (11, 12), (12, 13),
    (0, 14), (0, 15), (14, 16), (15, 17), (2, 8), (5, 11)
]

# Lista de keypoints de MediaPipe para el JSON (igual que tu original)
MEDIAPIPE_KEYPOINTS = [
    (0, "Nose"), (2, "LEye"), (5, "REye"), (7, "LEar"), (8, "REar"),
    (11, "LShoulder"), (12, "RShoulder"), (13, "LElbow"), (14, "RElbow"),
    (15, "LWrist"), (16, "RWrist"), (23, "LHip"), (24, "RHip"),
    (25, "LKnee"), (26, "RKnee"), (27, "LAnkle"), (28, "RAnkle")
]

# ----------------------------
# INICIALIZAR DETECTOR DE MEDIAPIPE
# ----------------------------
def initialize_pose_detector():
    """Inicializa el detector de pose de MediaPipe Tasks"""
    if not os.path.exists(MODEL_PATH):
        print("[ERROR] No se encontró el modelo.")
        return None
    
    try:
        base_options = python.BaseOptions(model_asset_path=MODEL_PATH)
        options = vision.PoseLandmarkerOptions(
            base_options=base_options,
            running_mode=vision.RunningMode.IMAGE,
            num_poses=2,
            min_pose_detection_confidence=0.5,
            min_pose_presence_confidence=0.5,
            min_tracking_confidence=0.5
        )
        detector = vision.PoseLandmarker.create_from_options(options)
        print("[INFO] PoseLandmarker inicializado correctamente.")
        return detector
    except Exception as e:
        print(f"[ERROR] No se pudo inicializar PoseLandmarker: {e}")
        return None

pose_detector = initialize_pose_detector()

# ----------------------------
# FUNCIÓN PARA DIBUJAR OPENPOSE DE 18 PUNTOS
# ----------------------------
def draw_openpose_18_keypoints(image, landmarks_list):
    if not landmarks_list:
        return image
    draw_image = image.copy()
    draw = ImageDraw.Draw(draw_image)
    width, height = image.size
    
    for person_id, landmarks in enumerate(landmarks_list):
        color = (255, 0, 0) if person_id == 0 else (0, 0, 255)
        openpose_points = {}
        for op_idx, (mp_idx, name) in MEDIAPIPE_TO_OPENPOSE.items():
            if mp_idx < len(landmarks):
                landmark = landmarks[mp_idx]
                x = int(landmark.x * width)
                y = int(landmark.y * height)
                openpose_points[op_idx] = (x, y)
        if 2 in openpose_points and 5 in openpose_points:
            rx, ry = openpose_points[2]
            lx, ly = openpose_points[5]
            openpose_points[1] = ((rx + lx)//2, (ry + ly)//2)
        for start_idx, end_idx in OPENPOSE_CONNECTIONS_18:
            if start_idx in openpose_points and end_idx in openpose_points:
                start_x, start_y = openpose_points[start_idx]
                end_x, end_y = openpose_points[end_idx]
                draw.line([(start_x, start_y), (end_x, end_y)], fill=color, width=3)
        for idx, (x, y) in openpose_points.items():
            radius = 3 if idx in [0, 14, 15, 16, 17] else 4
            draw.ellipse((x-radius-1, y-radius-1, x+radius+1, y+radius+1), fill=(255,255,255))
            draw.ellipse((x-radius, y-radius, x+radius, y+radius), fill=color)
    return draw_image

# ----------------------------
# EXTRACCIÓN DE FRAMES
# ----------------------------
def get_frames(video_in):
    frames = []
    clip = VideoFileClip(video_in)
    target_fps = min(clip.fps, 30)
    clip_resized = clip.resize(height=512)
    clip_resized.write_videofile("video_resized.mp4", fps=target_fps, verbose=False, logger=None)
    cap = cv2.VideoCapture("video_resized.mp4")
    fps = cap.get(cv2.CAP_PROP_FPS)
    idx = 0
    while True:
        ret, frame = cap.read()
        if not ret: break
        path = f'frame_{idx:04d}.jpg'
        cv2.imwrite(path, frame)
        frames.append(path)
        idx += 1
    cap.release()
    cv2.destroyAllWindows()
    return frames, fps

# ----------------------------
# PROCESAMIENTO DE FRAME (ASIGNACIÓN LEFT/RIGHT)
# ----------------------------
def process_frame(frame_path):
    original_image = Image.open(frame_path)
    width, height = original_image.size
    img_np = np.array(original_image)
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=img_np)
    detection = pose_detector.detect(mp_image)
    landmarks = detection.pose_landmarks
    if landmarks:
        op_img = draw_openpose_18_keypoints(original_image, landmarks)
    else:
        op_img = Image.fromarray(np.zeros((height, width, 3), dtype=np.uint8))
    op_img = op_img.resize((width, height))
    out_img = f"openpose_{os.path.basename(frame_path)}"
    op_img.save(out_img)
    frame_kps = {}
    if landmarks:
        mean_xs = [float(np.mean([p.x for p in lm])) for lm in landmarks]
        assigned = {}
        for idx, mx in enumerate(mean_xs):
            if mx < 0.5 and "side" not in assigned: assigned["side"] = idx
            elif mx >= 0.5 and "front" not in assigned: assigned["front"] = idx
        # fallback por orden
        if "side" not in assigned: assigned["side"] = int(np.argmin(mean_xs))
        if "front" not in assigned: assigned["front"] = int(np.argmax(mean_xs))
        # Construir person_0 = lateral, person_1 = frontal
        for key, pid in [("side","person_0"),("front","person_1")]:
            lm = landmarks[assigned[key]]
            person = {}
            for idx, name in MEDIAPIPE_KEYPOINTS:
                if idx < len(lm):
                    v = lm[idx]
                    person[name] = {'x': float(v.x),'y': float(v.y),'z': float(v.z),'visibility': float(getattr(v,"visibility",1.0))}
                else:
                    person[name] = {'x': None,'y': None,'z': None,'visibility': 0.0}
            if 11 < len(lm) and 12 < len(lm):
                l,r = lm[11],lm[12]
                person["Neck"] = {'x': float((l.x+r.x)/2),'y': float((l.y+r.y)/2),'z': float((l.z+r.z)/2),
                                  'visibility': float((getattr(l,"visibility",1.0)+getattr(r,"visibility",1.0))/2)}
            frame_kps[pid] = person
    for pid, kps in frame_kps.items():
        json_path = f"openpose_{os.path.splitext(os.path.basename(frame_path))[0]}_{pid}.json"
        with open(json_path,'w') as f:
            json.dump(kps,f,indent=2)
    return out_img, frame_kps

# ----------------------------
# CREACIÓN DE VIDEO
# ----------------------------
def create_video(imgs, fps, prefix):
    sizes = [Image.open(img).size for img in imgs]
    if len(set(sizes)) != 1:
        raise ValueError(f"Inconsistent image sizes: {set(sizes)}")
    clip = ImageSequenceClip(imgs, fps=fps)
    out = f"{prefix}_result.mp4"
    clip.write_videofile(out, fps=fps, verbose=False, logger=None)
    return out

# ----------------------------
# GIF A VIDEO
# ----------------------------
def convert_gif_to_video(gif_file):
    clip = VideoFileClip(gif_file.name)
    clip.write_videofile("gif_video.mp4", verbose=False, logger=None)
    return "gif_video.mp4"

# ----------------------------
# INFERENCIA PRINCIPAL
# ----------------------------
def infer(video_in):
    [os.remove(f) for f in os.listdir() if f.startswith(('frame_','openpose_')) and os.path.isfile(f)]
    frames,fps = get_frames(video_in)
    imgs=[]
    all_kps={}
    for fp in frames:
        img,kps = process_frame(fp)
        imgs.append(img)
        all_kps[os.path.splitext(fp)[0]] = kps
    with open("all_keypoints.json",'w') as f: json.dump(all_kps,f,indent=2)
    vid = create_video(imgs,fps,"openpose")
    [os.remove(f) for f in frames]
    # JSON combinado 3D
    combined_3d={}
    for frame_key in all_kps:
        frame_data = all_kps[frame_key]
        if "person_0" in frame_data and "person_1" in frame_data:
            side_person = frame_data["person_0"]
            front_person = frame_data["person_1"]
            torso_center_x = (side_person["LShoulder"]["x"] + side_person["RShoulder"]["x"])/2 if "LShoulder" in side_person else 0.5
            combined_person={}
            for joint_name in front_person:
                if joint_name in side_person:
                    combined_person[joint_name] = {
                        "x": front_person[joint_name]["x"],
                        "y": front_person[joint_name]["y"],
                        "z": side_person[joint_name]["x"] - torso_center_x
                    }
            combined_3d[frame_key] = {"person_0": combined_person}
    with open('3d_combined_data.json','w') as f: json.dump(combined_3d,f,indent=2)
    return vid, imgs + ["all_keypoints.json","3d_combined_data.json"]

# ----------------------------
# LIMPIEZA DE ARCHIVOS
# ----------------------------
def clean_files():
    for f in os.listdir():
        if os.path.isfile(f) and (f.startswith('frame_') or f.startswith('openpose_') or f=="video_resized.mp4"):
            os.remove(f)

# ----------------------------
# INTERFAZ GRADIO
# ----------------------------
title = """<div style="text-align:center; max-width:500px; margin:0 auto;">
<h1>Video 2 OpenPose (3D json)</h1></div>"""

with gr.Blocks() as demo:
    gr.HTML(title)
    with gr.Row():
        with gr.Column():
            video_in = gr.Video(sources=["upload"])
            gif_in = gr.File(label="Import GIF", file_types=['.gif'])
            gif_in.change(fn=convert_gif_to_video, inputs=gif_in, outputs=video_in)
            btn = gr.Button("Process")
            clean_btn = gr.Button("Clean")
        with gr.Column():
            video_out = gr.Video(label="OpenPose Result")
            files = gr.Files(label="Generated Files")
    btn.click(fn=infer, inputs=[video_in], outputs=[video_out, files])
    clean_btn.click(fn=clean_files, inputs=[], outputs=[])

demo.launch()

