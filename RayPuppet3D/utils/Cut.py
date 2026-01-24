#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import cv2
import json
import numpy as np
import gradio as gr
import mediapipe as mp
from mediapipe.tasks import python
from mediapipe.tasks.python import vision
import torch
from segment_anything import sam_model_registry, SamPredictor
from psd_tools import PSDImage
from psd_tools.api.layers import PixelLayer
from PIL import Image

MODEL_DIR = "models"
os.makedirs(MODEL_DIR, exist_ok=True)

POSE_MODEL_URL = "https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_full/float16/latest/pose_landmarker_full.task"
POSE_MODEL_PATH = os.path.join(MODEL_DIR, "pose_landmarker_full.task")

# SAM model configuration - ViT-H (el más preciso)
SAM_MODEL_URL = "https://dl.fbaipublicfiles.com/segment_anything/sam_vit_h_4b8939.pth"
SAM_CHECKPOINT = os.path.join(MODEL_DIR, "sam_vit_h_4b8939.pth")
SAM_MODEL_TYPE = "vit_h"

def download_model(url, path, show_progress=True):
    """Descarga modelos con barra de progreso"""
    if not os.path.exists(path):
        print(f"[⬇] Descargando: {os.path.basename(path)}")
        print(f"[🔗] URL: {url}")
        
        import urllib.request
        
        if show_progress:
            def progress_hook(count, block_size, total_size):
                percent = int(count * block_size * 100 / total_size)
                mb_downloaded = count * block_size / (1024 * 1024)
                mb_total = total_size / (1024 * 1024)
                print(f"\r[📥] Progreso: {percent}% ({mb_downloaded:.1f}/{mb_total:.1f} MB)", end='')
            
            urllib.request.urlretrieve(url, path, progress_hook)
            print()  # Nueva línea después de la barra
        else:
            urllib.request.urlretrieve(url, path)
        
        print(f"[✓] Modelo descargado: {path}")
    else:
        print(f"[✓] Modelo ya existe: {os.path.basename(path)}")

# Descargar modelos necesarios
print("[🚀] Inicializando modelos...")
download_model(POSE_MODEL_URL, POSE_MODEL_PATH, show_progress=False)
download_model(SAM_MODEL_URL, SAM_CHECKPOINT, show_progress=True)

BODY_PARTS = {
    "complete_head": {"landmarks": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10], "type": "head"},
    "complete_neck": {"landmarks": [0, 9, 10, 11, 12], "type": "neck"},
    "complete_chest": {"landmarks": [11, 12, 23, 24], "type": "torso"},
    "complete_abdomen": {"landmarks": [23, 24, 25, 26], "type": "torso"},
    
    "complete_left_shoulder": {"landmarks": [11, 13], "type": "arm"},
    "complete_left_upper_arm": {"landmarks": [11, 13], "type": "arm"},
    "complete_left_forearm": {"landmarks": [13, 15], "type": "arm"},
    "complete_left_hand": {"landmarks": [15, 17, 19, 21], "type": "hand"},
    
    "complete_right_shoulder": {"landmarks": [12, 14], "type": "arm"},
    "complete_right_upper_arm": {"landmarks": [12, 14], "type": "arm"},
    "complete_right_forearm": {"landmarks": [14, 16], "type": "arm"},
    "complete_right_hand": {"landmarks": [16, 18, 20, 22], "type": "hand"},
    
    "complete_left_thigh": {"landmarks": [23, 25], "type": "leg"},
    "complete_left_calf": {"landmarks": [25, 27], "type": "leg"},
    "complete_left_foot": {"landmarks": [27, 29, 31], "type": "foot"},
    
    "complete_right_thigh": {"landmarks": [24, 26], "type": "leg"},
    "complete_right_calf": {"landmarks": [26, 28], "type": "leg"},
    "complete_right_foot": {"landmarks": [28, 30, 32], "type": "foot"}
}

def init_pose_detector():
    try:
        base_opts = python.BaseOptions(model_asset_path=POSE_MODEL_PATH)
        opts = vision.PoseLandmarkerOptions(
            base_options=base_opts,
            running_mode=vision.RunningMode.IMAGE,
            num_poses=1,
            min_pose_detection_confidence=0.5
        )
        detector = vision.PoseLandmarker.create_from_options(opts)
        print("[✓] Detector de pose inicializado")
        return detector
    except Exception as e:
        print(f"[✗] Error al inicializar detector: {e}")
        return None

def init_sam():
    """Inicializa SAM (Segment Anything Model)"""
    try:
        if not os.path.exists(SAM_CHECKPOINT):
            print(f"[⚠] Modelo SAM no encontrado en {SAM_CHECKPOINT}")
            print(f"[ℹ] Descarga el modelo desde: https://github.com/facebookresearch/segment-anything#model-checkpoints")
            print(f"[ℹ] Colócalo en: {SAM_CHECKPOINT}")
            return None
        
        device = "cuda" if torch.cuda.is_available() else "cpu"
        print(f"[🔧] Cargando SAM en {device}...")
        
        sam = sam_model_registry[SAM_MODEL_TYPE](checkpoint=SAM_CHECKPOINT)
        sam.to(device=device)
        predictor = SamPredictor(sam)
        
        print("[✓] SAM inicializado correctamente")
        return predictor
    except Exception as e:
        print(f"[✗] Error al inicializar SAM: {e}")
        return None

pose_detector = init_pose_detector()
sam_predictor = init_sam()

def segment_body_with_sam(img, pose_landmarks, W, H, head_offset=30, foot_offset=30):
    """Segmenta el cuerpo completo usando SAM - adaptativo a cualquier tipo de personaje"""
    print("[🔬] Segmentando cuerpo con SAM...")
    
    if sam_predictor is None:
        print("[⚠] SAM no disponible, usando método alternativo")
        return segment_body_fallback(img, pose_landmarks, W, H, head_offset, foot_offset)
    
    # Preparar imagen para SAM
    sam_predictor.set_image(img)
    
    # Obtener TODOS los puntos clave del cuerpo
    points = np.array([[lm.x * W, lm.y * H] for lm in pose_landmarks])
    
    # Bounding box generosa pero no excesiva
    margin = 100
    x_min = max(0, int(points[:, 0].min() - margin))
    x_max = min(W, int(points[:, 0].max() + margin))
    y_min = max(0, int(points[:, 1].min() - head_offset - margin))
    y_max = min(H, int(points[:, 1].max() + foot_offset + margin))
    
    # Usar puntos centrales como guía positiva + bounding box
    # Esto da contexto pero permite flexibilidad
    central_points = points[::2]  # Usar la mitad de los puntos para no saturar
    input_labels = np.ones(len(central_points))
    input_box = np.array([x_min, y_min, x_max, y_max])
    
    try:
        # Predecir con SAM usando puntos + box
        masks, scores, logits = sam_predictor.predict(
            point_coords=central_points,
            point_labels=input_labels,
            box=input_box[None, :],
            multimask_output=True
        )
        
        # Seleccionar la mejor máscara (por score)
        best_idx = np.argmax(scores)
        best_mask = masks[best_idx]
        mask_binary = best_mask.astype(np.uint8) * 255
        
        # Verificar que no esté vacía
        if mask_binary.max() == 0:
            print("[⚠] Máscara vacía, usando fallback")
            return segment_body_fallback(img, pose_landmarks, W, H, head_offset, foot_offset)
        
        # Refinamiento suave
        kernel_close = np.ones((7, 7), np.uint8)
        mask_binary = cv2.morphologyEx(mask_binary, cv2.MORPH_CLOSE, kernel_close, iterations=2)
        
        # Suavizado de bordes
        mask_binary = cv2.GaussianBlur(mask_binary.astype(float), (25, 25), 0)
        mask_final = (mask_binary).astype(np.uint8)
        
        print(f"[✓] Segmentación SAM completa (score: {scores[best_idx]:.3f})")
        return mask_final
        
    except Exception as e:
        print(f"[⚠] Error en SAM: {e}, usando fallback")
        return segment_body_fallback(img, pose_landmarks, W, H, head_offset, foot_offset)

def segment_body_fallback(img, pose_landmarks, W, H, head_offset=30, foot_offset=30):
    """Método de segmentación alternativo sin SAM"""
    print("[🔧] Usando método ConvexHull...")
    
    points = np.array([[lm.x * W, lm.y * H] for lm in pose_landmarks])
    
    # Ajustar puntos para incluir offsets
    y_coords = points[:, 1].copy()
    y_coords[y_coords == y_coords.min()] -= head_offset
    y_coords[y_coords == y_coords.max()] += foot_offset
    points[:, 1] = y_coords
    
    mask_binary = np.zeros(img.shape[:2], dtype=np.uint8)
    hull = cv2.convexHull(points.astype(np.int32))
    cv2.fillPoly(mask_binary, [hull], 1)
    
    # Refinar
    kernel = np.ones((5, 5), np.uint8)
    mask_binary = cv2.morphologyEx(mask_binary, cv2.MORPH_CLOSE, kernel, iterations=3)
    mask_binary = cv2.GaussianBlur(mask_binary.astype(float), (21, 21), 0)
    
    return (mask_binary * 255).astype(np.uint8)

def create_part_mask_with_sam(img, body_mask, landmark_points, W, H, margin, part_type, 
                               head_offset=30, foot_offset=30):
    """Crea máscara de parte corporal - adaptativo a cualquier tipo de personaje"""
    
    if len(landmark_points) < 1:
        return np.zeros((H, W), dtype=np.uint8)
    
    # Si SAM no está disponible, usar método tradicional
    if sam_predictor is None:
        return create_part_mask_fallback(body_mask, landmark_points, W, H, margin, 
                                        part_type, head_offset, foot_offset)
    
    points = np.array(landmark_points, dtype=np.int32)
    
    # Márgenes generosos pero balanceados
    type_margins = {
        "head": margin * 4.0 + head_offset,
        "hand": margin * 3.0,
        "foot": margin * 2.5 + foot_offset,
        "arm": margin * 2.0,
        "leg": margin * 2.0,
        "neck": margin * 2.5,
        "torso": margin * 2.2
    }
    m = type_margins.get(part_type, margin * 2.0)
    
    # ROI expandida
    x_min = max(0, int(points[:, 0].min() - m))
    x_max = min(W, int(points[:, 0].max() + m))
    y_min = max(0, int(points[:, 1].min() - m))
    y_max = min(H, int(points[:, 1].max() + m))
    
    if part_type == "head":
        y_min = max(0, y_min - head_offset)
    elif part_type == "foot":
        y_max = min(H, y_max + foot_offset)
    
    # Usar puntos + bounding box (híbrido para mejor resultado)
    input_points = points
    input_labels = np.ones(len(input_points))
    input_box = np.array([x_min, y_min, x_max, y_max])
    
    try:
        # SAM con puntos + box
        masks, scores, _ = sam_predictor.predict(
            point_coords=input_points,
            point_labels=input_labels,
            box=input_box[None, :],
            multimask_output=True
        )
        
        # Seleccionar mejor máscara por score
        best_idx = np.argmax(scores)
        best_mask = (masks[best_idx].astype(np.uint8) * 255)
        
        # Verificar que no esté vacía
        if best_mask.max() == 0:
            print(f"  └─ Máscara vacía, usando fallback")
            return create_part_mask_fallback(body_mask, landmark_points, W, H, margin, 
                                            part_type, head_offset, foot_offset)
        
        # Aplicar máscara del cuerpo
        result = cv2.bitwise_and(body_mask, best_mask)
        
        # Si el resultado es vacío, intentar con solo la máscara SAM
        if result.max() == 0:
            result = best_mask
        
        # Refinamiento suave
        kernel = np.ones((5, 5), np.uint8)
        result = cv2.morphologyEx(result, cv2.MORPH_CLOSE, kernel, iterations=1)
        
        # Suavizado
        result = cv2.GaussianBlur(result, (11, 11), 0)
        
        print(f"  └─ SAM score: {scores[best_idx]:.3f}")
        
        return result
        
    except Exception as e:
        print(f"  └─ Error SAM: {e}")
        return create_part_mask_fallback(body_mask, landmark_points, W, H, margin, 
                                        part_type, head_offset, foot_offset)

def create_part_mask_fallback(body_mask, landmark_points, W, H, margin, part_type, 
                              head_offset=30, foot_offset=30):
    """Método tradicional de creación de máscara de parte"""
    
    points = np.array(landmark_points, dtype=np.int32)
    
    type_margins = {
        "head": margin * 2.5 + head_offset,
        "hand": margin * 2,
        "foot": margin * 1.5 + foot_offset,
        "arm": margin * 1.2,
        "leg": margin * 1.2,
        "neck": margin * 1.5,
        "torso": margin * 1.3
    }
    m = type_margins.get(part_type, margin)
    
    roi_mask = np.zeros((H, W), dtype=np.uint8)
    
    if len(landmark_points) == 2 and part_type in ["arm", "leg"]:
        cv2.line(roi_mask, tuple(landmark_points[0]), tuple(landmark_points[1]), 
                255, thickness=int(m * 2))
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (int(m), int(m)))
        roi_mask = cv2.dilate(roi_mask, kernel, iterations=1)
    else:
        x_min = max(0, int(points[:, 0].min() - m))
        x_max = min(W, int(points[:, 0].max() + m))
        y_min = max(0, int(points[:, 1].min() - m))
        y_max = min(H, int(points[:, 1].max() + m))
        
        if part_type == "head":
            y_min = max(0, y_min - head_offset)
        elif part_type == "foot":
            y_max = min(H, y_max + foot_offset)
            
        roi_mask[y_min:y_max, x_min:x_max] = 255
    
    result = cv2.bitwise_and(body_mask, roi_mask)
    
    kernel = np.ones((3, 3), np.uint8)
    result = cv2.morphologyEx(result, cv2.MORPH_CLOSE, kernel)
    result = cv2.GaussianBlur(result, (11, 11), 0)
    
    return result

def create_psd_from_layers(output_dir, parts_info, W, H):
    """Crea un archivo PSD con todas las capas"""
    try:
        from psd_tools import PSDImage
        from psd_tools.api.layers import PixelLayer
        
        print("[📦] Creando archivo PSD...")
        
        # Crear PSD vacío
        psd = PSDImage.new("RGBA", (W, H))
        
        # PRIMERO: Agregar _body_full como capa de fondo
        body_full_path = os.path.join(output_dir, "_body_full.png")
        if os.path.exists(body_full_path):
            body_img = Image.open(body_full_path)
            body_layer = PixelLayer.frompil(body_img, psd, name="_body_full (BASE)")
            psd.append(body_layer)
            print("[✓] Capa base '_body_full' agregada al fondo")
        
        # DESPUÉS: Agregar cada parte como capa encima
        for part_name in reversed(list(parts_info.keys())):
            part_path = parts_info[part_name]["file"]
            
            # Cargar imagen PNG
            img = Image.open(part_path)
            
            # Crear capa y agregarla
            layer = PixelLayer.frompil(img, psd, name=part_name)
            psd.append(layer)
        
        # Guardar PSD
        psd_path = os.path.join(output_dir, "body_layers.psd")
        psd.save(psd_path)
        
        print(f"[✓] PSD creado: {psd_path}")
        return psd_path
        
    except Exception as e:
        print(f"[⚠] No se pudo crear PSD: {e}")
        print("[ℹ] Instala psd-tools: pip install psd-tools")
        return None

def process_image(image_path, margin=20, head_offset=30, foot_offset=30, use_sam=True):
    img = cv2.imread(image_path)
    if img is None:
        raise ValueError("No se pudo cargar la imagen")
    
    H, W = img.shape[:2]
    print(f"[🖼] Resolución: {W}x{H}")
    print(f"[⚙] Offset cabeza: {head_offset}px, Offset pies: {foot_offset}px")
    print(f"[🤖] Usar SAM: {use_sam and sam_predictor is not None}")
    
    rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    
    if not pose_detector:
        raise ValueError("Detector de pose no disponible")
    
    results = pose_detector.detect(mp_img)
    
    if not results or not results.pose_landmarks:
        raise ValueError("❌ No se detectó persona en la imagen")
    
    landmarks = results.pose_landmarks[0]
    
    # Segmentar cuerpo con SAM o método alternativo
    if use_sam and sam_predictor is not None:
        body_mask = segment_body_with_sam(rgb, landmarks, W, H, head_offset, foot_offset)
    else:
        body_mask = segment_body_fallback(rgb, landmarks, W, H, head_offset, foot_offset)
    
    output_dir = "body_layers"
    os.makedirs(output_dir, exist_ok=True)
    
    parts_info = {}
    
    for part_name, config in BODY_PARTS.items():
        print(f"[🔧] Procesando: {part_name}")
        
        pts = []
        for idx in config["landmarks"]:
            if idx < len(landmarks):
                lm = landmarks[idx]
                pts.append([int(lm.x * W), int(lm.y * H)])
        
        if not pts:
            continue
        
        # Crear máscara de parte con SAM o método tradicional
        if use_sam and sam_predictor is not None:
            part_mask = create_part_mask_with_sam(rgb, body_mask, pts, W, H, margin, 
                                                 config["type"], head_offset, foot_offset)
        else:
            part_mask = create_part_mask_fallback(body_mask, pts, W, H, margin, 
                                                 config["type"], head_offset, foot_offset)
        
        if part_mask.max() == 0:
            print(f"[⚠] {part_name} vacío")
            continue
        
        rgba = cv2.cvtColor(img, cv2.COLOR_BGR2BGRA)
        rgba[:, :, 3] = part_mask
        
        out_path = os.path.join(output_dir, f"{part_name}.png")
        cv2.imwrite(out_path, rgba)
        
        parts_info[part_name] = {
            "file": out_path,
            "size": [W, H],
            "type": config["type"]
        }
    
    # Crear archivo PSD
    psd_path = create_psd_from_layers(output_dir, parts_info, W, H)
    
    # Guardar JSON de información
    json_path = os.path.join(output_dir, "layers_info.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump({
            "original_size": [W, H],
            "head_offset": head_offset,
            "foot_offset": foot_offset,
            "sam_used": use_sam and sam_predictor is not None,
            "parts": parts_info,
            "total": len(parts_info),
            "psd_file": psd_path
        }, f, indent=2, ensure_ascii=False)
    
    ref_img = img.copy()
    for i, name in enumerate(parts_info.keys()):
        cv2.putText(ref_img, name, (10, 30 + i * 20),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
    
    ref_path = os.path.join(output_dir, "_reference.jpg")
    cv2.imwrite(ref_path, ref_img)
    
    body_rgba = cv2.cvtColor(img, cv2.COLOR_BGR2BGRA)
    body_rgba[:, :, 3] = body_mask
    cv2.imwrite(os.path.join(output_dir, "_body_full.png"), body_rgba)
    
    print(f"[✅] ¡Listo! {len(parts_info)} partes extraídas")
    
    return output_dir, psd_path, len(parts_info)

def gradio_process(image, margin, head_offset, foot_offset, use_sam):
    if image is None:
        return None, None, "❌ Sube una imagen primero"
    
    try:
        output_dir, psd_file, num_parts = process_image(
            image, int(margin), int(head_offset), int(foot_offset), use_sam
        )
        
        sam_status = "✓ SAM activado" if (use_sam and sam_predictor is not None) else "○ Método tradicional"
        
        status = f"""✅ **¡ÉXITO!**

📁 Carpeta: `{output_dir}/`
🎯 Partes: **{num_parts}**
🤖 Segmentación: {sam_status}
⬆ Offset cabeza: **{head_offset}px**
⬇ Offset pies: **{foot_offset}px**

📦 **Archivos generados:**
- `body_layers.psd` → Archivo Photoshop con todas las capas
- `*.png` → Cada parte separada
- `_body_full.png` → Cuerpo completo
- `_reference.jpg` → Referencia visual
"""
        
        ref_path = os.path.join(output_dir, "_reference.jpg")
        return ref_path, psd_file, status
        
    except Exception as e:
        import traceback
        return None, None, f"❌ Error: {str(e)}\n\n{traceback.format_exc()}"

with gr.Blocks(title="Extractor Capas Corporales", theme=gr.themes.Soft()) as app:
    gr.Markdown("""
    # 🎭 Extractor de Capas Corporales
    
    Sube una imagen y obtén un archivo PSD con todas las partes del cuerpo separadas en capas.
    """)
    
    with gr.Row():
        with gr.Column():
            img_input = gr.Image(type="filepath", label="📸 Imagen")
            use_sam_input = gr.Checkbox(
                value=True, 
                label="🤖 Usar SAM",
                info="Segmentación de alta precisión (requiere modelo)"
            )
            margin_input = gr.Slider(10, 50, value=20, step=5, 
                                    label="📏 Margen (píxeles)")
            head_input = gr.Slider(0, 200, value=30, step=10,
                                  label="⬆ Offset cabeza (píxeles)")
            foot_input = gr.Slider(0, 200, value=30, step=10,
                                  label="⬇ Offset pies (píxeles)")
            btn = gr.Button("🚀 Extraer Capas", variant="primary", size="lg")
        
        with gr.Column():
            ref_output = gr.Image(label="📋 Referencia")
            psd_output = gr.File(label="📄 Archivo PSD")
            status_output = gr.Markdown()
    
    btn.click(
        gradio_process, 
        [img_input, margin_input, head_input, foot_input, use_sam_input], 
        [ref_output, psd_output, status_output]
    )

if __name__ == "__main__":
    app.launch()