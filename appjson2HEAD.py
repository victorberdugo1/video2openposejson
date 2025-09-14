import gradio as gr
from controlnet_aux import OpenposeDetector
import os
import cv2
import numpy as np
from PIL import Image
from moviepy.editor import VideoFileClip, ImageSequenceClip
import json
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision

model_path = 'pose_landmarker_full.task'
VisionRunningMode = mp.tasks.vision.RunningMode

# Nota: revisa estos índices según tu versión de MediaPipe si algo no cuadra.
MEDIAPIPE_KEYPOINTS = [
    (0, "Nose"), (2, "LEye"), (5, "REye"), (7, "LEar"), (8, "REar"),
    (11, "LShoulder"), (12, "RShoulder"), (13, "LElbow"), (14, "RElbow"),
    (15, "LWrist"), (16, "RWrist"), (23, "LHip"), (24, "RHip"),
    (25, "LKnee"), (26, "RKnee"), (27, "LAnkle"), (28, "RAnkle")
]

openpose = OpenposeDetector.from_pretrained('lllyasviel/ControlNet')

def initialize_pose_detector():
    base_options = python.BaseOptions(model_asset_path=model_path)
    options = vision.PoseLandmarkerOptions(
        base_options=base_options,
        running_mode=VisionRunningMode.IMAGE,
        num_poses=2,
        min_pose_detection_confidence=0.5
    )
    return vision.PoseLandmarker.create_from_options(options)

pose_detector = initialize_pose_detector()

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
        if not ret:
            break
        path = f'frame_{idx:04d}.jpg'
        cv2.imwrite(path, frame)
        frames.append(path)
        idx += 1
    cap.release()
    cv2.destroyAllWindows()
    return frames, fps

def process_frame(frame_path):
    original_image = Image.open(frame_path).convert("RGB")
    width, height = original_image.size

    image = mp.Image.create_from_file(frame_path)
    detection = pose_detector.detect(image)
    landmarks = detection.pose_landmarks  # lista de LandmarkLists o None

    if landmarks:
        op_img = openpose(original_image)
    else:
        op_img = Image.fromarray(np.zeros((height, width, 3), dtype=np.uint8))

    op_img = op_img.resize((width, height))
    out_img = f"openpose_{frame_path}"
    op_img.save(out_img)

    frame_kps = {}
    if landmarks:
        # landmarks es iterable con hasta num_poses elementos (cada uno es una lista de lm)
        centers = []
        for lm in landmarks:
            # calcular centro X en espacio normalizado para orden
            xs = [lm[idx].x for idx, _ in MEDIAPIPE_KEYPOINTS]
            centers.append(np.mean(xs))
        order = np.argsort(centers)

        for new_id, orig_idx in enumerate(order):
            lm = landmarks[orig_idx]
            person = {}
            for idx, name in MEDIAPIPE_KEYPOINTS:
                v = lm[idx]
                # Convertir a píxeles para x,y; mantener z tal cual (MediaPipe z es relativo)
                x_px = float(v.x * width)
                y_px = float(v.y * height)
                z_rel = float(v.z)  # puedes escalar según necesidad
                person[name] = {
                    'x': x_px,
                    'y': y_px,
                    'z': z_rel,
                    'visibility': float(v.visibility) if hasattr(v, 'visibility') else 1.0
                }

            # Neck aproximado en píxeles
            l, r = lm[11], lm[12]
            person["Neck"] = {
                'x': float(((l.x + r.x)/2) * width),
                'y': float(((l.y + r.y)/2) * height),
                'z': float(((l.z + r.z)/2)),
                'visibility': float((l.visibility + r.visibility)/2) if hasattr(l, 'visibility') else 1.0
            }

            frame_kps[f"person_{new_id}"] = person

    # Guardar JSON por persona por frame (opcional)
    for pid, kps in frame_kps.items():
        json_path = f"openpose_{os.path.splitext(frame_path)[0]}_{pid}.json"
        with open(json_path, 'w') as f:
            json.dump(kps, f, indent=2)

    return out_img, frame_kps

def create_video(imgs, fps, prefix):
    sizes = [Image.open(img).size for img in imgs]
    if len(set(sizes)) != 1:
        raise ValueError(f"Inconsistent image sizes detected: {set(sizes)}")

    clip = ImageSequenceClip(imgs, fps=fps)
    out = f"{prefix}_result.mp4"
    clip.write_videofile(out, fps=fps, verbose=False, logger=None)
    return out

def convert_gif_to_video(gif_file):
    clip = VideoFileClip(gif_file.name)
    clip.write_videofile("gif_video.mp4", verbose=False, logger=None)
    return "gif_video.mp4"

def infer(video_in):
    # Limpiar archivos anteriores
    [os.remove(f) for f in os.listdir() if f.startswith(('frame_', 'openpose_')) and os.path.isfile(f)]
    
    # Procesar video
    frames, fps = get_frames(video_in)
    imgs = []
    all_kps = {}
    
    for fp in frames:
        img, kps = process_frame(fp)
        imgs.append(img)
        all_kps[os.path.splitext(fp)[0]] = kps
    
    # Guardar todos los keypoints
    with open("all_keypoints.json", 'w') as f:
        json.dump(all_kps, f, indent=2)
    
    # Crear video
    vid = create_video(imgs, fps, "openpose")
    [os.remove(f) for f in frames]
    
    # --- Generar automáticamente el JSON combinado 3D (CORREGIDO) ---
    combined_3d = {}
    min_visibility = 0.2  # filtrar puntos poco visibles
    for frame_key in all_kps:
        frame_data = all_kps[frame_key]
        if "person_0" in frame_data and "person_1" in frame_data:
            combined_person = {}
            side_person = frame_data["person_0"]   # lateral (según cómo detectes)
            front_person = frame_data["person_1"]  # frontal
            for joint_name in front_person:
                if joint_name in side_person:
                    # Usa Z desde la vista lateral (no X). Además, chequea visibility.
                    vis_front = front_person[joint_name].get('visibility', 1.0)
                    vis_side  = side_person[joint_name].get('visibility', 1.0)
                    if vis_front < min_visibility or vis_side < min_visibility:
                        # marcar como no fiable (puedes omitir o poner null)
                        continue
                    combined_person[joint_name] = {
                        # Mantén X,Y de la vista frontal (en píxeles) y Z de la vista lateral (relativo)
                        "x": front_person[joint_name]["x"],
                        "y": front_person[joint_name]["y"],
                        "z": side_person[joint_name]["z"]
                    }
            if combined_person:
                combined_3d[frame_key] = {"person_0": combined_person}
    with open('3d_combined_data.json', 'w') as f:
        json.dump(combined_3d, f, indent=2)
    
    return vid, imgs + ["all_keypoints.json", "3d_combined_data.json"]

def clean_files():
    for f in os.listdir():
        if os.path.isfile(f) and (f.startswith('frame_') or f.startswith('openpose_') or f == "video_resized.mp4"):
            os.remove(f)

title = """
<div style="text-align:center; max-width:500px; margin:0 auto;">
  <h1>Video 2 OpenPose (3D json)</h1>
</div>
"""

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

