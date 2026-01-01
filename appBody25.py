import os
import cv2
import json
import numpy as np
from PIL import Image, ImageDraw
import gradio as gr
from moviepy.editor import ImageSequenceClip

# --- MediaPipe Tasks API 0.10.x ---
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision

# -----------------------
# Configuración de modelos
# -----------------------
MODEL_DIR = "models"
os.makedirs(MODEL_DIR, exist_ok=True)

POSE_MODEL_URL = "https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_full/float16/latest/pose_landmarker_full.task"
HAND_MODEL_URL = "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task"
FACE_MODEL_URL = "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/latest/face_landmarker.task"

POSE_MODEL_PATH = os.path.join(MODEL_DIR, "pose_landmarker_full.task")
HAND_MODEL_PATH = os.path.join(MODEL_DIR, "hand_landmarker.task")
FACE_MODEL_PATH = os.path.join(MODEL_DIR, "face_landmarker.task")

# Descargar modelos si no existen
def download_model(url, path):
    if not os.path.exists(path):
        print(f"[INFO] Descargando modelo desde {url}...")
        import urllib.request
        try:
            urllib.request.urlretrieve(url, path)
            print(f"[INFO] Modelo descargado en {path}")
        except Exception as e:
            print(f"[ERROR] No se pudo descargar {url}: {e}")

download_model(POSE_MODEL_URL, POSE_MODEL_PATH)
download_model(HAND_MODEL_URL, HAND_MODEL_PATH)
download_model(FACE_MODEL_URL, FACE_MODEL_PATH)

# -----------------------
# Definiciones de conexiones MANUALES
# -----------------------

# Conexiones para POSE (33 puntos) - estilo MediaPipe
POSE_CONNECTIONS = [
    # Torso y cabeza
    (0, 1), (0, 4), (1, 2), (2, 3), (3, 7), (4, 5), (5, 6), (6, 8),
    # Brazos
    (9, 10), (11, 12), (11, 13), (13, 15), (15, 17), (15, 19), (15, 21), (17, 19),
    (12, 14), (14, 16), (16, 18), (16, 20), (16, 22), (18, 20),
    # Piernas
    (23, 24), (23, 25), (25, 27), (27, 29), (27, 31), (29, 31),
    (24, 26), (26, 28), (28, 30), (30, 32), (28, 32),
    # Torso
    (11, 23), (12, 24), (23, 24)
]

# Conexiones para MANOS (21 puntos cada una) - estilo MediaPipe
HAND_CONNECTIONS = [
    # Palma
    (0, 1), (1, 2), (2, 3), (3, 4),  # Pulgar
    (0, 5), (5, 6), (6, 7), (7, 8),  # Índice
    (0, 9), (9, 10), (10, 11), (11, 12),  # Medio
    (0, 13), (13, 14), (14, 15), (15, 16),  # Anular
    (0, 17), (17, 18), (18, 19), (19, 20),  # Meñique
    # Conectores entre dedos
    (5, 9), (9, 13), (13, 17)
]

# -----------------------
# Inicializar detectores
# -----------------------
def initialize_detectors():
    """Inicializa los detectores de pose, manos y cara"""
    detectors = {}
    
    try:
        # Pose detector
        if os.path.exists(POSE_MODEL_PATH):
            base_options = python.BaseOptions(model_asset_path=POSE_MODEL_PATH)
            options = vision.PoseLandmarkerOptions(
                base_options=base_options,
                running_mode=vision.RunningMode.IMAGE,
                num_poses=1,
                min_pose_detection_confidence=0.5,
                min_pose_presence_confidence=0.5,
                min_tracking_confidence=0.5
            )
            detectors['pose'] = vision.PoseLandmarker.create_from_options(options)
            print("[INFO] PoseLandmarker inicializado")
        else:
            print("[ERROR] No se encontró el modelo de pose")
            detectors['pose'] = None
    except Exception as e:
        print(f"[ERROR] No se pudo inicializar PoseLandmarker: {e}")
        detectors['pose'] = None
    
    try:
        # Hand detector
        if os.path.exists(HAND_MODEL_PATH):
            base_options = python.BaseOptions(model_asset_path=HAND_MODEL_PATH)
            options = vision.HandLandmarkerOptions(
                base_options=base_options,
                running_mode=vision.RunningMode.IMAGE,
                num_hands=2,
                min_hand_detection_confidence=0.5,
                min_hand_presence_confidence=0.5,
                min_tracking_confidence=0.5
            )
            detectors['hand'] = vision.HandLandmarker.create_from_options(options)
            print("[INFO] HandLandmarker inicializado")
        else:
            print("[ERROR] No se encontró el modelo de manos")
            detectors['hand'] = None
    except Exception as e:
        print(f"[ERROR] No se pudo inicializar HandLandmarker: {e}")
        detectors['hand'] = None
    
    try:
        # Face detector
        if os.path.exists(FACE_MODEL_PATH):
            base_options = python.BaseOptions(model_asset_path=FACE_MODEL_PATH)
            options = vision.FaceLandmarkerOptions(
                base_options=base_options,
                running_mode=vision.RunningMode.IMAGE,
                num_faces=1,
                min_face_detection_confidence=0.5,
                min_face_presence_confidence=0.5,
                min_tracking_confidence=0.5
            )
            detectors['face'] = vision.FaceLandmarker.create_from_options(options)
            print("[INFO] FaceLandmarker inicializado")
        else:
            print("[ERROR] No se encontró el modelo de cara")
            detectors['face'] = None
    except Exception as e:
        print(f"[ERROR] No se pudo inicializar FaceLandmarker: {e}")
        detectors['face'] = None
    
    return detectors

detectors = initialize_detectors()

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
    try:
        data = np.fromfile(path, dtype=np.uint8)
        img = cv2.imdecode(data, cv2.IMREAD_COLOR)
        return img
    except Exception as e:
        print(f"[ERROR] No se pudo leer la imagen {path}: {e}")
        return None

def save_image_unicode(path, img_bgr):
    try:
        ext = os.path.splitext(path)[1]
        ok, buf = cv2.imencode(ext, img_bgr)
        if not ok:
            raise IOError("Error al codificar imagen")
        buf.tofile(path)
    except Exception as e:
        print(f"[ERROR] No se pudo guardar la imagen {path}: {e}")

# -----------------------
# Dibujar landmarks sobre FONDO NEGRO con COLORES y TAMAÑOS AUMENTADOS
# -----------------------
def draw_landmarks_on_black_bg(image, pose_results, hand_results, face_results):
    """Dibuja landmarks sobre fondo negro con colores separados y mayor tamaño"""
    if image is None:
        return None
    
    H, W = image.shape[:2]
    
    # Crear fondo negro
    black_bg = np.zeros((H, W, 3), dtype=np.uint8)
    
    # Convertir a PIL para dibujar
    pil_image = Image.fromarray(black_bg)
    draw = ImageDraw.Draw(pil_image)
    
    # COLORES:
    # CUERPO: AZUL brillante (0, 0, 255) en BGR -> (255, 0, 0) en RGB
    # MANOS: VERDE brillante (0, 255, 0) en BGR -> (0, 255, 0) en RGB  
    # CARA: ROJO brillante (255, 0, 0) en BGR -> (0, 0, 255) en RGB
    
    # Dibujar POSE (cuerpo completo) - COLOR AZUL BRILLANTE
    if pose_results and pose_results.pose_landmarks:
        pose_landmarks = pose_results.pose_landmarks[0]
        pose_points = []
        
        # Guardar todos los puntos de pose
        for landmark in pose_landmarks:
            x = int(landmark.x * W)
            y = int(landmark.y * H)
            pose_points.append((x, y))
        
        # Dibujar CONEXIONES de pose (líneas AZULES GRUESAS sobre fondo negro)
        for start_idx, end_idx in POSE_CONNECTIONS:
            if start_idx < len(pose_points) and end_idx < len(pose_points):
                start_x, start_y = pose_points[start_idx]
                end_x, end_y = pose_points[end_idx]
                # Línea azul más gruesa para mejor visibilidad
                draw.line([(start_x, start_y), (end_x, end_y)], fill=(255, 0, 0), width=5)
        
        # Dibujar PUNTOS de pose (círculos AZULES GRANDES con borde blanco)
        for x, y in pose_points:
            # Borde blanco grande para mejor visibilidad
            draw.ellipse((x-6, y-6, x+6, y+6), fill=(255, 255, 255))  # Borde blanco
            # Interior azul grande
            draw.ellipse((x-5, y-5, x+5, y+5), fill=(255, 0, 0))  # Interior azul
    
    # Dibujar MANOS - COLOR VERDE BRILLANTE
    if hand_results and hand_results.hand_landmarks:
        for hand_landmarks in hand_results.hand_landmarks:
            hand_points = []
            
            # Guardar todos los puntos de la mano
            for landmark in hand_landmarks:
                x = int(landmark.x * W)
                y = int(landmark.y * H)
                hand_points.append((x, y))
            
            # Dibujar CONEXIONES de mano (líneas VERDES más gruesas)
            for start_idx, end_idx in HAND_CONNECTIONS:
                if start_idx < len(hand_points) and end_idx < len(hand_points):
                    start_x, start_y = hand_points[start_idx]
                    end_x, end_y = hand_points[end_idx]
                    draw.line([(start_x, start_y), (end_x, end_y)], fill=(0, 255, 0), width=4)
            
            # Dibujar PUNTOS de mano (círculos VERDES con borde blanco)
            for x, y in hand_points:
                draw.ellipse((x-4, y-4, x+4, y+4), fill=(255, 255, 255))  # Borde blanco
                draw.ellipse((x-3, y-3, x+3, y+3), fill=(0, 255, 0))  # Interior verde
    
    # Dibujar CARA - Puntos ROJOS más grandes (todos los 468 puntos)
    if face_results and face_results.face_landmarks:
        face_landmarks = face_results.face_landmarks[0]
        
        # Usar diferentes tamaños para puntos de cara según importancia
        # Contorno facial: puntos más grandes
        # Puntos internos: puntos medianos
        # Puntos finos: puntos pequeños
        
        # Primero, dibujar puntos clave del contorno (más grandes)
        contour_indices = [10, 33, 67, 103, 133, 152, 172, 199, 234, 263, 291, 323, 356, 389]
        for idx in contour_indices:
            if idx < len(face_landmarks):
                landmark = face_landmarks[idx]
                x = int(landmark.x * W)
                y = int(landmark.y * H)
                # Puntos rojos grandes para contorno
                draw.ellipse((x-2, y-2, x+2, y+2), fill=(0, 0, 255))  # Punto rojo grande
        
        # Luego, dibujar el resto de puntos (más pequeños)
        for i, landmark in enumerate(face_landmarks):
            # Saltar los puntos del contorno que ya dibujamos
            if i in contour_indices:
                continue
            x = int(landmark.x * W)
            y = int(landmark.y * H)
            # Puntos rojos pequeños para detalles internos
            draw.ellipse((x-1, y-1, x+1, y+1), fill=(0, 0, 255))  # Punto rojo pequeño
    
    # Convertir de vuelta a OpenCV
    result = cv2.cvtColor(np.array(pil_image), cv2.COLOR_RGB2BGR)
    return result

# -----------------------
# Convertir resultados a JSON (COMPLETO)
# -----------------------
def results_to_json_full(pose_results, hand_results, face_results, image_width, image_height):
    """Convierte resultados de MediaPipe Tasks a formato JSON OpenPose completo"""
    person = {
        "pose_keypoints_2d": [],
        "face_keypoints_2d": [],
        "hand_left_keypoints_2d": [],
        "hand_right_keypoints_2d": []
    }
    
    # POSE - Todos los 33 puntos
    if pose_results and pose_results.pose_landmarks:
        for landmark in pose_results.pose_landmarks[0]:
            x = float(landmark.x * image_width)
            y = float(landmark.y * image_height)
            conf = float(getattr(landmark, "visibility", 1.0))
            person["pose_keypoints_2d"].extend([x, y, conf])
    else:
        # Si no hay pose, llenar con ceros para 33 puntos
        for _ in range(33):
            person["pose_keypoints_2d"].extend([0.0, 0.0, 0.0])
    
    # MANOS - Determinar izquierda y derecha
    left_hand_points = []
    right_hand_points = []
    
    if hand_results and hand_results.hand_landmarks:
        for i, hand_landmarks in enumerate(hand_results.hand_landmarks):
            hand_points = []
            
            # Obtener todos los puntos de la mano
            for landmark in hand_landmarks:
                x = float(landmark.x * image_width)
                y = float(landmark.y * image_height)
                hand_points.extend([x, y, 1.0])  # Confianza 1.0 para manos
            
            # Determinar si es mano izquierda o derecha
            if i < len(hand_results.handedness):
                handedness = hand_results.handedness[i][0].category_name
                if handedness == "Left" and not left_hand_points:
                    left_hand_points = hand_points
                elif handedness == "Right" and not right_hand_points:
                    right_hand_points = hand_points
            else:
                # Si no hay información de handedness, asignar por orden
                if not left_hand_points:
                    left_hand_points = hand_points
                elif not right_hand_points:
                    right_hand_points = hand_points
    
    # Asignar puntos de manos al JSON
    person["hand_left_keypoints_2d"] = left_hand_points if left_hand_points else [0.0] * (21 * 3)
    person["hand_right_keypoints_2d"] = right_hand_points if right_hand_points else [0.0] * (21 * 3)
    
    # CARA - TODOS los 468 puntos
    if face_results and face_results.face_landmarks:
        # Extraer TODOS los 468 puntos de la cara
        for landmark in face_results.face_landmarks[0]:
            x = float(landmark.x * image_width)
            y = float(landmark.y * image_height)
            person["face_keypoints_2d"].extend([x, y, 1.0])
    else:
        # Si no hay cara, llenar con ceros para 468 puntos
        for _ in range(468):
            person["face_keypoints_2d"].extend([0.0, 0.0, 0.0])
    
    return person

# -----------------------
# Procesar imagen (single-person)
# -----------------------
def process_image_single(image_path):
    """Procesa una imagen individual"""
    img_cv = read_image_unicode(image_path)
    if img_cv is None:
        raise ValueError("No se pudo leer la imagen. Revisa la ruta o formato.")
    
    H, W = img_cv.shape[:2]
    
    # Convertir a formato MediaPipe
    rgb_image = cv2.cvtColor(img_cv, cv2.COLOR_BGR2RGB)
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_image)
    
    # Ejecutar detecciones
    pose_results = None
    hand_results = None
    face_results = None
    
    if detectors['pose']:
        pose_results = detectors['pose'].detect(mp_image)
    
    if detectors['hand']:
        hand_results = detectors['hand'].detect(mp_image)
    
    if detectors['face']:
        face_results = detectors['face'].detect(mp_image)
    
    # Dibujar landmarks sobre FONDO NEGRO con colores y tamaño aumentado
    vis_cv = draw_landmarks_on_black_bg(img_cv, pose_results, hand_results, face_results)
    if vis_cv is None:
        # Si hay error, crear fondo negro simple
        vis_cv = np.zeros((H, W, 3), dtype=np.uint8)
    
    # Guardar imagen resultante
    out_img_name = "singleperson_openpose_result.png"
    save_image_unicode(out_img_name, vis_cv)
    
    # Crear JSON COMPLETO
    person = results_to_json_full(pose_results, hand_results, face_results, W, H)
    json_obj = {"people": [person]}
    
    json_path = "singleperson_openpose_result.json"
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(json_obj, f, indent=2, ensure_ascii=False)
    
    return out_img_name, json_path

# -----------------------
# Procesar vídeo (single-person por frame)
# -----------------------
def process_video_single(video_path, resize_height=None, target_fps=None):
    """Procesa un vídeo frame por frame"""
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
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        if resize_height and resize_height > 0:
            h, w = frame.shape[:2]
            scale = float(resize_height) / float(h)
            frame = cv2.resize(frame, (int(w * scale), int(resize_height)))
        
        H, W = frame.shape[:2]
        
        # Convertir a formato MediaPipe
        rgb_image = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_image)
        
        # Ejecutar detecciones
        pose_results = None
        hand_results = None
        face_results = None
        
        if detectors['pose']:
            pose_results = detectors['pose'].detect(mp_image)
        
        if detectors['hand']:
            hand_results = detectors['hand'].detect(mp_image)
        
        if detectors['face']:
            face_results = detectors['face'].detect(mp_image)
        
        # Dibujar landmarks sobre FONDO NEGRO con colores y tamaño aumentado
        vis_cv = draw_landmarks_on_black_bg(frame, pose_results, hand_results, face_results)
        if vis_cv is None:
            # Si hay error, crear fondo negro simple
            vis_cv = np.zeros((H, W, 3), dtype=np.uint8)
        
        # Guardar frame
        fname = os.path.join(tmp_dir, f"frame_{idx:05d}.png")
        save_image_unicode(fname, vis_cv)
        frames_paths.append(fname)
        
        # Extraer datos para JSON
        person = results_to_json_full(pose_results, hand_results, face_results, W, H)
        frames_data.append({"frame_index": idx, "people": [person]})
        
        idx += 1
    
    cap.release()
    
    if len(frames_paths) == 0:
        raise ValueError("No se procesaron frames del vídeo.")
    
    # Crear video
    out_fps = target_fps if target_fps and target_fps > 0 else input_fps
    clip = ImageSequenceClip(frames_paths, fps=out_fps)
    out_video = "singleperson_openpose_result.mp4"
    clip.write_videofile(out_video, fps=out_fps, verbose=False, logger=None)
    
    # Guardar JSON
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
  <h1>Single-person → MediaPipe Tasks (FONDO NEGRO con COLORES)</h1>
  <p>CUERPO: Azul | MANOS: Verde | CARA: Rojo - Puntos y líneas más grandes</p>
  <p>JSON completo con: 33 puntos pose + 42 puntos manos + 468 puntos cara</p>
</div>
"""

def infer(mode, image_in, video_in, resize_height, target_fps):
    # Limpiar archivos previos
    for f in os.listdir('.'):
        if f.startswith(("singleperson_openpose_result",)) and os.path.isfile(f):
            try:
                os.remove(f)
            except:
                pass
    
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
        return out_img, None, [out_img, json_path]
    else:
        if not video_path:
            return None, None, []
        out_vid, json_path, first_frame = process_video_single(
            video_path,
            resize_height=(int(resize_height) if resize_height else None),
            target_fps=(int(target_fps) if target_fps else None)
        )
        files = [out_vid, json_path]
        if first_frame:
            files.append(first_frame)
        return None, out_vid, files

def toggle_inputs(m):
    if m == "Imagen":
        return gr.update(visible=True), gr.update(visible=False), gr.update(visible=True), gr.update(visible=False)
    else:
        return gr.update(visible=False), gr.update(visible=True), gr.update(visible=False), gr.update(visible=True)

with gr.Blocks() as demo:
    gr.HTML(title)
    with gr.Row():
        with gr.Column():
            mode = gr.Radio(choices=["Imagen", "Vídeo"], value="Imagen", label="Modo")
            image_in = gr.Image(type="filepath", label="Sube una imagen (jpg/png)")
            video_in = gr.File(label="Sube un vídeo (mp4)", visible=False)
            resize_height = gr.Number(value=512, label="(Vídeo) altura de frame para procesar (0=original)")
            target_fps = gr.Number(value=0, label="(Vídeo) fps de salida (0=usar fps original)")
            btn = gr.Button("Procesar")
        with gr.Column():
            image_out = gr.Image(type="filepath", label="Resultado (imagen)", visible=True)
            video_out = gr.Video(label="Resultado (vídeo)", visible=False)
            files = gr.Files(label="Archivos generados")
    
    mode.change(fn=toggle_inputs, inputs=[mode], outputs=[image_in, video_in, image_out, video_out])
    btn.click(fn=infer, inputs=[mode, image_in, video_in, resize_height, target_fps], outputs=[image_out, video_out, files])

if __name__ == "__main__":
    demo.launch()
