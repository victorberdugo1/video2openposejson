# app_singleperson_with_previews_fix.py
import os
import cv2
import json
import numpy as np
from PIL import Image
import gradio as gr
from controlnet_aux import OpenposeDetector
import mediapipe as mp
from moviepy.editor import ImageSequenceClip

# -----------------------
# Inicializaciones
# -----------------------
openpose = OpenposeDetector.from_pretrained('lllyasviel/ControlNet')

mp_holistic = mp.solutions.holistic
mp_drawing = mp.solutions.drawing_utils

BODY_DS = mp_drawing.DrawingSpec(color=(255, 0, 0), thickness=2, circle_radius=2)
HAND_DS = mp_drawing.DrawingSpec(color=(0, 255, 0), thickness=2, circle_radius=2)
FACE_DS = mp_drawing.DrawingSpec(color=(0, 0, 255), thickness=1, circle_radius=1)

# -----------------------
# Helpers
# -----------------------
def get_path(obj):
    if obj is None:
        return None
    if isinstance(obj, str):
        return obj
    if isinstance(obj, dict) and "name" in obj:
        return obj["name"]
    if hasattr(obj, "name"):
        return obj.name
    return None

def read_image_unicode(path):
    data = np.fromfile(path, dtype=np.uint8)
    img = cv2.imdecode(data, cv2.IMREAD_COLOR)
    return img

def save_image_unicode(path, img_bgr):
    ext = os.path.splitext(path)[1]
    ok, buf = cv2.imencode(ext, img_bgr)
    if not ok:
        raise IOError("Error al codificar imagen")
    buf.tofile(path)

def draw_mp_landmarks_on_cv(img_cv, mp_results):
    if mp_results is None:
        return img_cv
    annotated = img_cv.copy()
    if mp_results.pose_landmarks:
        mp_drawing.draw_landmarks(
            image=annotated,
            landmark_list=mp_results.pose_landmarks,
            connections=mp_holistic.POSE_CONNECTIONS,
            landmark_drawing_spec=BODY_DS,
            connection_drawing_spec=BODY_DS
        )
    if mp_results.left_hand_landmarks:
        mp_drawing.draw_landmarks(
            image=annotated,
            landmark_list=mp_results.left_hand_landmarks,
            connections=mp_holistic.HAND_CONNECTIONS,
            landmark_drawing_spec=HAND_DS,
            connection_drawing_spec=HAND_DS
        )
    if mp_results.right_hand_landmarks:
        mp_drawing.draw_landmarks(
            image=annotated,
            landmark_list=mp_results.right_hand_landmarks,
            connections=mp_holistic.HAND_CONNECTIONS,
            landmark_drawing_spec=HAND_DS,
            connection_drawing_spec=HAND_DS
        )
    if mp_results.face_landmarks:
        mp_drawing.draw_landmarks(
            image=annotated,
            landmark_list=mp_results.face_landmarks,
            connections=mp_holistic.FACEMESH_TESSELATION,
            landmark_drawing_spec=FACE_DS,
            connection_drawing_spec=FACE_DS
        )
    return annotated

def mp_results_to_person_dict_full(mp_results, image_width, image_height):
    person = {
        "pose_keypoints_2d": [],
        "face_keypoints_2d": [],
        "hand_left_keypoints_2d": [],
        "hand_right_keypoints_2d": []
    }
    if mp_results is None:
        return person
    if mp_results.pose_landmarks:
        for lm in mp_results.pose_landmarks.landmark:
            x = float(lm.x * image_width)
            y = float(lm.y * image_height)
            conf = float(getattr(lm, "visibility", 1.0))
            person["pose_keypoints_2d"].extend([x, y, conf])
    if mp_results.face_landmarks:
        for lm in mp_results.face_landmarks.landmark:
            x = float(lm.x * image_width)
            y = float(lm.y * image_height)
            person["face_keypoints_2d"].extend([x, y, 1.0])
    if mp_results.left_hand_landmarks:
        for lm in mp_results.left_hand_landmarks.landmark:
            x = float(lm.x * image_width)
            y = float(lm.y * image_height)
            person["hand_left_keypoints_2d"].extend([x, y, 1.0])
    if mp_results.right_hand_landmarks:
        for lm in mp_results.right_hand_landmarks.landmark:
            x = float(lm.x * image_width)
            y = float(lm.y * image_height)
            person["hand_right_keypoints_2d"].extend([x, y, 1.0])
    return person

# -----------------------
# Procesar imagen (single-person)
# -----------------------
def process_image_single(image_path):
    img_cv = read_image_unicode(image_path)
    if img_cv is None:
        raise ValueError("No se pudo leer la imagen. Revisa la ruta o formato.")
    H, W = img_cv.shape[:2]

    with mp_holistic.Holistic(
        static_image_mode=True,
        model_complexity=2,
        refine_face_landmarks=True,
        min_detection_confidence=0.5
    ) as holistic:
        results = holistic.process(cv2.cvtColor(img_cv, cv2.COLOR_BGR2RGB))

    try:
        pil = Image.fromarray(cv2.cvtColor(img_cv, cv2.COLOR_BGR2RGB))
        op_pil = openpose(pil, hand=True, face=True)
        op_cv = cv2.cvtColor(np.array(op_pil), cv2.COLOR_RGB2BGR)
    except Exception:
        op_cv = img_cv.copy()

    vis_cv = draw_mp_landmarks_on_cv(op_cv, results)

    out_img_name = "singleperson_openpose_result.png"
    save_image_unicode(out_img_name, vis_cv)

    person = mp_results_to_person_dict_full(results, W, H)
    json_obj = {"people": [person]}

    json_path = "singleperson_openpose_result.json"
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(json_obj, f, indent=2, ensure_ascii=False)

    return out_img_name, json_path

# -----------------------
# Procesar vídeo (single-person por frame)
# -----------------------
def process_video_single(video_path, resize_height=None, target_fps=None):
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        raise ValueError("No se pudo abrir el vídeo.")
    input_fps = cap.get(cv2.CAP_PROP_FPS) or 15
    frames_paths = []
    frames_data = []
    tmp_dir = "tmp_frames_single"
    if os.path.isdir(tmp_dir):
        for f in os.listdir(tmp_dir):
            try:
                os.remove(os.path.join(tmp_dir, f))
            except Exception:
                pass
    os.makedirs(tmp_dir, exist_ok=True)
    idx = 0

    with mp_holistic.Holistic(
        static_image_mode=False,
        model_complexity=2,
        refine_face_landmarks=True,
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5
    ) as holistic:
        while True:
            ret, frame = cap.read()
            if not ret:
                break
            if resize_height:
                h, w = frame.shape[:2]
                scale = float(resize_height) / float(h)
                frame = cv2.resize(frame, (int(w*scale), int(resize_height)))

            H, W = frame.shape[:2]

            results = holistic.process(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))

            try:
                pil = Image.fromarray(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
                op_pil = openpose(pil, hand=True, face=True)
                op_cv = cv2.cvtColor(np.array(op_pil), cv2.COLOR_RGB2BGR)
            except Exception:
                op_cv = frame.copy()

            vis_cv = draw_mp_landmarks_on_cv(op_cv, results)

            fname = os.path.join(tmp_dir, f"frame_{idx:05d}.png")
            save_image_unicode(fname, vis_cv)
            frames_paths.append(fname)

            person = mp_results_to_person_dict_full(results, W, H)
            frames_data.append({"frame_index": idx, "people": [person]})

            idx += 1

    cap.release()

    if len(frames_paths) == 0:
        raise ValueError("No se procesaron frames del vídeo.")

    out_fps = target_fps or input_fps or 15
    clip = ImageSequenceClip(frames_paths, fps=out_fps)
    out_video = "singleperson_openpose_result.mp4"
    clip.write_videofile(out_video, fps=out_fps, verbose=False, logger=None)

    json_path = "singleperson_openpose_result.json"
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump({"frames": frames_data}, f, indent=2, ensure_ascii=False)

    first_frame = frames_paths[0] if len(frames_paths) > 0 else None

    return out_video, json_path, first_frame

# -----------------------
# Interfaz Gradio
# -----------------------
title = """
<div style="text-align:center; max-width:700px; margin:0 auto;">
  <h1>Single-person → OpenPose style + MediaPipe (máximo keypoints)</h1>
  <p>Detecta UNA persona por imagen/frame y extrae todos los keypoints (pose 33, face 468, manos 21 cada una).</p>
  <p>Esta versión muestra el resultado correcto por modo: imagen o vídeo.</p>
</div>
"""

def infer(mode, image_in, video_in, resize_height, target_fps):
    # limpiar archivos previos (solo los que genera el script)
    for f in os.listdir('.'):
        if f.startswith(("singleperson_openpose_result",)) and os.path.isfile(f):
            try: os.remove(f)
            except: pass
    tmp_dir = "tmp_frames_single"
    if os.path.isdir(tmp_dir):
        try:
            for item in os.listdir(tmp_dir):
                os.remove(os.path.join(tmp_dir, item))
        except Exception:
            pass

    image_path = get_path(image_in)
    video_path = get_path(video_in)

    if mode == "Imagen":
        if not image_path:
            return None, None, []
        out_img, json_path = process_image_single(image_path)
        # Mostrar SOLO la imagen en preview; no mostramos vídeo
        return out_img, None, [out_img, json_path]
    else:
        if not video_path:
            return None, None, []
        out_vid, json_path, first_frame = process_video_single(
            video_path,
            resize_height=(int(resize_height) if resize_height else None),
            target_fps=(int(target_fps) if target_fps else None)
        )
        # Para evitar la "imagen rara" en preview, NO devolvemos la miniatura como preview de imagen.
        # Devolvemos SOLO el vídeo en el componente video_out.
        files = [out_vid, json_path]
        # si quieres revisar la miniatura, la incluimos en Archivos generados (no como preview)
        if first_frame:
            files.append(first_frame)
        return None, out_vid, files

with gr.Blocks() as demo:
    gr.HTML(title)
    with gr.Row():
        with gr.Column():
            mode = gr.Radio(choices=["Imagen", "Vídeo"], value="Imagen", label="Modo")
            image_in = gr.Image(type="filepath", label="Sube una imagen (jpg/png)")
            video_in = gr.File(label="Sube un vídeo (mp4)", visible=False)
            resize_height = gr.Number(value=512, label="(Vídeo) altura de frame para procesar (dejar 0 para original)")
            target_fps = gr.Number(value=0, label="(Vídeo) fps de salida (0=usar fps original)")
            btn = gr.Button("Procesar")
        with gr.Column():
            image_out = gr.Image(type="filepath", label="Resultado (imagen)", visible=True)
            video_out = gr.Video(label="Resultado (vídeo)", visible=False)
            files = gr.Files(label="Archivos generados")

    def toggle_inputs(m):
        if m == "Imagen":
            # mostrar input imagen y preview de imagen; ocultar input/vídeo
            return gr.update(visible=True), gr.update(visible=False), gr.update(visible=True), gr.update(visible=False)
        else:
            # mostrar input vídeo y preview de vídeo; ocultar imagen preview
            return gr.update(visible=False), gr.update(visible=True), gr.update(visible=False), gr.update(visible=True)

    mode.change(fn=toggle_inputs, inputs=[mode], outputs=[image_in, video_in, image_out, video_out])
    btn.click(fn=infer, inputs=[mode, image_in, video_in, resize_height, target_fps], outputs=[image_out, video_out, files])

if __name__ == "__main__":
    demo.launch()
