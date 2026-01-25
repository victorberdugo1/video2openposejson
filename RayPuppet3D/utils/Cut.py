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

# Sinónimos para cada parte del cuerpo (para búsqueda adaptativa)
PART_SYNONYMS = {
    "complete_head": ["head", "skull", "face", "cranium", "noggin", "helmet area"],
    "complete_neck": ["neck", "throat", "collar", "nape"],
    "complete_chest": ["chest", "torso upper", "breast", "thorax", "ribcage"],
    "complete_abdomen": ["abdomen", "belly", "stomach", "torso lower", "waist"],
    
    "complete_left_shoulder": ["left shoulder", "shoulder left"],
    "complete_left_upper_arm": ["left upper arm", "left bicep", "arm left upper"],
    "complete_left_forearm": ["left forearm", "left lower arm", "arm left lower"],
    "complete_left_hand": ["left hand", "hand left", "left palm"],
    
    "complete_right_shoulder": ["right shoulder", "shoulder right"],
    "complete_right_upper_arm": ["right upper arm", "right bicep", "arm right upper"],
    "complete_right_forearm": ["right forearm", "right lower arm", "arm right lower"],
    "complete_right_hand": ["right hand", "hand right", "right palm"],
    
    "complete_left_thigh": ["left thigh", "left upper leg", "leg left upper"],
    "complete_left_calf": ["left calf", "left lower leg", "leg left lower", "left shin"],
    "complete_left_foot": ["left foot", "foot left"],
    
    "complete_right_thigh": ["right thigh", "right upper leg", "leg right upper"],
    "complete_right_calf": ["right calf", "right lower leg", "leg right lower", "right shin"],
    "complete_right_foot": ["right foot", "foot right"]
}

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
    central_points = points[::2]  # Usar la mitad de los puntos
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

def try_segment_with_synonyms(img, body_mask, landmark_points, W, H, margin, part_name, part_type, 
                               head_offset=30, foot_offset=30):
    """Intenta segmentar una parte usando sinónimos hasta encontrarla"""
    
    if sam_predictor is None:
        # Sin SAM, usar método tradicional
        return create_part_mask_fallback(body_mask, landmark_points, W, H, margin, 
                                        part_type, head_offset, foot_offset)
    
    synonyms = PART_SYNONYMS.get(part_name, [part_name])
    print(f"[🔍] Intentando detectar '{part_name}' con {len(synonyms)} sinónimos...")
    
    points = np.array(landmark_points, dtype=np.int32)
    
    # Márgenes generosos
    type_margins = {
        "head": margin * 4.5 + head_offset,
        "hand": margin * 3.5,
        "foot": margin * 3.0 + foot_offset,
        "arm": margin * 2.5,
        "leg": margin * 2.5,
        "neck": margin * 3.0,
        "torso": margin * 2.5
    }
    m = type_margins.get(part_type, margin * 2.5)
    
    # ROI expandida
    x_min = max(0, int(points[:, 0].min() - m))
    x_max = min(W, int(points[:, 0].max() + m))
    y_min = max(0, int(points[:, 1].min() - m))
    y_max = min(H, int(points[:, 1].max() + m))
    
    if part_type == "head":
        y_min = max(0, y_min - head_offset)
    elif part_type == "foot":
        y_max = min(H, y_max + foot_offset)
    
    input_box = np.array([x_min, y_min, x_max, y_max])
    
    best_mask = None
    best_score = 0
    best_synonym = None
    
    # Estrategia 1: Puntos + Box (método híbrido más robusto)
    try:
        input_points = points
        input_labels = np.ones(len(input_points))
        
        masks, scores, _ = sam_predictor.predict(
            point_coords=input_points,
            point_labels=input_labels,
            box=input_box[None, :],
            multimask_output=True
        )
        
        # Obtener la mejor máscara
        idx = np.argmax(scores)
        if scores[idx] > best_score:
            best_score = scores[idx]
            best_mask = masks[idx]
            best_synonym = "points+box"
            print(f"  ├─ Método points+box: score={scores[idx]:.3f}")
    
    except Exception as e:
        print(f"  ├─ Error en points+box: {e}")
    
    # Estrategia 2: Solo Box (más general)
    try:
        masks, scores, _ = sam_predictor.predict(
            box=input_box[None, :],
            multimask_output=True
        )
        
        idx = np.argmax(scores)
        if scores[idx] > best_score:
            best_score = scores[idx]
            best_mask = masks[idx]
            best_synonym = "box_only"
            print(f"  ├─ Método box_only: score={scores[idx]:.3f}")
    
    except Exception as e:
        print(f"  ├─ Error en box_only: {e}")
    
    # Estrategia 3: Puntos expandidos (más puntos de guía)
    if len(landmark_points) >= 2:
        try:
            # Crear puntos intermedios entre landmarks
            expanded_points = []
            for i in range(len(landmark_points)):
                expanded_points.append(landmark_points[i])
                if i < len(landmark_points) - 1:
                    mid = ((landmark_points[i][0] + landmark_points[i+1][0]) // 2,
                           (landmark_points[i][1] + landmark_points[i+1][1]) // 2)
                    expanded_points.append(mid)
            
            expanded_points = np.array(expanded_points, dtype=np.int32)
            input_labels = np.ones(len(expanded_points))
            
            masks, scores, _ = sam_predictor.predict(
                point_coords=expanded_points,
                point_labels=input_labels,
                box=input_box[None, :],
                multimask_output=True
            )
            
            idx = np.argmax(scores)
            if scores[idx] > best_score:
                best_score = scores[idx]
                best_mask = masks[idx]
                best_synonym = "expanded_points"
                print(f"  ├─ Método expanded_points: score={scores[idx]:.3f}")
        
        except Exception as e:
            print(f"  ├─ Error en expanded_points: {e}")
    
    # Si encontramos algo, procesarlo
    if best_mask is not None and best_score > 0.5:
        mask_result = (best_mask.astype(np.uint8) * 255)
        
        # Aplicar máscara del cuerpo
        result = cv2.bitwise_and(body_mask, mask_result)
        
        # Si el resultado es vacío, usar solo la máscara SAM
        if result.max() == 0:
            result = mask_result
        
        # Refinamiento
        kernel = np.ones((5, 5), np.uint8)
        result = cv2.morphologyEx(result, cv2.MORPH_CLOSE, kernel, iterations=2)
        result = cv2.GaussianBlur(result, (11, 11), 0)
        
        print(f"  └─ ✓ Detectado con '{best_synonym}' (score: {best_score:.3f})")
        return result
    
    # Si SAM no funcionó, usar fallback
    print(f"  └─ SAM no detectó la parte (mejor score: {best_score:.3f}), usando fallback")
    return create_part_mask_fallback(body_mask, landmark_points, W, H, margin, 
                                    part_type, head_offset, foot_offset)

def create_part_mask_fallback(body_mask, landmark_points, W, H, margin, part_type, 
                              head_offset=30, foot_offset=30):
    """Método tradicional de creación de máscara de parte"""
    
    points = np.array(landmark_points, dtype=np.int32)
    
    type_margins = {
        "head": margin * 3.0 + head_offset,
        "hand": margin * 2.5,
        "foot": margin * 2.0 + foot_offset,
        "arm": margin * 1.5,
        "leg": margin * 1.5,
        "neck": margin * 2.0,
        "torso": margin * 1.8
    }
    m = type_margins.get(part_type, margin * 1.5)
    
    roi_mask = np.zeros((H, W), dtype=np.uint8)
    
    if len(landmark_points) == 2 and part_type in ["arm", "leg"]:
        cv2.line(roi_mask, tuple(landmark_points[0]), tuple(landmark_points[1]), 
                255, thickness=int(m * 2))
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (int(m), int(m)))
        roi_mask = cv2.dilate(roi_mask, kernel, iterations=2)
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
    
    kernel = np.ones((5, 5), np.uint8)
    result = cv2.morphologyEx(result, cv2.MORPH_CLOSE, kernel, iterations=2)
    result = cv2.GaussianBlur(result, (11, 11), 0)
    
    return result

def create_psd_from_layers(output_dir, parts_info, W, H, body_mask, img):
    """Crea un archivo PSD con todas las capas"""
    try:
        print("[📦] Creando archivo PSD...")
        
        # Crear PSD vacío
        psd = PSDImage.new("RGBA", (W, H))
        
        # PRIMERO: Agregar _body_full como capa de fondo
        # Crear la imagen body_full directamente desde la máscara
        body_rgba = cv2.cvtColor(img, cv2.COLOR_BGR2BGRA)
        body_rgba[:, :, 3] = body_mask
        
        # Convertir a PIL
        body_rgba_rgb = cv2.cvtColor(body_rgba, cv2.COLOR_BGRA2RGBA)
        body_pil = Image.fromarray(body_rgba_rgb)
        
        # Crear capa base
        body_layer = PixelLayer.frompil(body_pil, psd, name="_body_full (BASE)")
        psd.append(body_layer)
        print("[✓] Capa base '_body_full' agregada al fondo")
        
        # DESPUÉS: Agregar cada parte como capa encima
        for part_name in reversed(list(parts_info.keys())):
            part_path = parts_info[part_name]["file"]
            
            # Cargar imagen PNG
            img_part = Image.open(part_path)
            
            # Crear capa y agregarla
            layer = PixelLayer.frompil(img_part, psd, name=part_name)
            psd.append(layer)
            print(f"  ├─ Capa '{part_name}' agregada")
        
        # Guardar PSD
        psd_path = os.path.join(output_dir, "body_layers.psd")
        psd.save(psd_path)
        
        print(f"[✓] PSD creado: {psd_path}")
        return psd_path
        
    except Exception as e:
        print(f"[⚠] No se pudo crear PSD: {e}")
        print("[ℹ] Instala psd-tools: pip install psd-tools")
        return None

def segment_full_body_sam_only(img, W, H):
    """Segmenta el cuerpo completo usando SOLO SAM - para personajes no humanos"""
    print("[🔬] Segmentando personaje completo con SAM (sin pose detection)...")
    
    if sam_predictor is None:
        raise ValueError("SAM no disponible. Se requiere para personajes no humanos.")
    
    sam_predictor.set_image(img)
    
    # Usar punto central como guía
    center_x, center_y = W // 2, H // 2
    
    # Definir región de interés (ROI) - asumimos que el personaje ocupa la parte central
    margin_w = int(W * 0.1)
    margin_h = int(H * 0.1)
    
    input_box = np.array([margin_w, margin_h, W - margin_w, H - margin_h])
    
    try:
        # Predecir con SAM usando solo el box central
        masks, scores, _ = sam_predictor.predict(
            box=input_box[None, :],
            multimask_output=True
        )
        
        # Seleccionar la máscara más grande (probablemente el personaje principal)
        areas = [mask.sum() for mask in masks]
        best_idx = np.argmax(areas)
        best_mask = masks[best_idx]
        mask_binary = best_mask.astype(np.uint8) * 255
        
        print(f"[✓] Personaje segmentado (score: {scores[best_idx]:.3f}, área: {areas[best_idx]})")
        return mask_binary, None  # Retorna mask y None para landmarks
        
    except Exception as e:
        raise ValueError(f"Error al segmentar personaje con SAM: {e}")

def estimate_body_parts_from_mask(body_mask, W, H):
    """Estima regiones de partes corporales basándose solo en la máscara del cuerpo"""
    print("[🔍] Estimando partes corporales desde la máscara...")
    
    # Encontrar contorno principal
    contours, _ = cv2.findContours(body_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if not contours:
        return {}
    
    main_contour = max(contours, key=cv2.contourArea)
    x, y, w, h = cv2.boundingRect(main_contour)
    
    # Dividir verticalmente en regiones aproximadas
    parts_regions = {
        "complete_head": {
            "box": [x, y, x + w, y + int(h * 0.15)],
            "type": "head"
        },
        "complete_neck": {
            "box": [x + int(w * 0.35), y + int(h * 0.15), x + int(w * 0.65), y + int(h * 0.22)],
            "type": "neck"
        },
        "complete_chest": {
            "box": [x, y + int(h * 0.22), x + w, y + int(h * 0.45)],
            "type": "torso"
        },
        "complete_abdomen": {
            "box": [x + int(w * 0.25), y + int(h * 0.45), x + int(w * 0.75), y + int(h * 0.60)],
            "type": "torso"
        },
        "complete_left_arm": {
            "box": [x, y + int(h * 0.22), x + int(w * 0.25), y + int(h * 0.60)],
            "type": "arm"
        },
        "complete_right_arm": {
            "box": [x + int(w * 0.75), y + int(h * 0.22), x + w, y + int(h * 0.60)],
            "type": "arm"
        },
        "complete_left_leg": {
            "box": [x, y + int(h * 0.60), x + int(w * 0.45), y + h],
            "type": "leg"
        },
        "complete_right_leg": {
            "box": [x + int(w * 0.55), y + int(h * 0.60), x + w, y + h],
            "type": "leg"
        }
    }
    
    return parts_regions

def segment_part_sam_boxed(img, body_mask, box, W, H, part_type):
    """Segmenta una parte usando SAM con bounding box"""
    
    if sam_predictor is None:
        return None
    
    x1, y1, x2, y2 = box
    input_box = np.array([max(0, x1), max(0, y1), min(W, x2), min(H, y2)])
    
    try:
        masks, scores, _ = sam_predictor.predict(
            box=input_box[None, :],
            multimask_output=True
        )
        
        best_idx = np.argmax(scores)
        best_mask = (masks[best_idx].astype(np.uint8) * 255)
        
        # Aplicar máscara del cuerpo
        result = cv2.bitwise_and(body_mask, best_mask)
        
        if result.max() == 0:
            result = best_mask
        
        # Refinamiento
        kernel = np.ones((5, 5), np.uint8)
        result = cv2.morphologyEx(result, cv2.MORPH_CLOSE, kernel, iterations=2)
        result = cv2.GaussianBlur(result, (11, 11), 0)
        
        print(f"  └─ SAM score: {scores[best_idx]:.3f}")
        return result
        
    except Exception as e:
        print(f"  └─ Error SAM: {e}")
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
    
    # Variable para indicar si usamos pose detection o no
    use_pose_detection = False
    landmarks = None
    
    if not results or not results.pose_landmarks:
        print("⚠ No se detectó pose humana - usando detección SAM pura para personaje no humano")
        if not use_sam or sam_predictor is None:
            raise ValueError("❌ SAM es requerido para personajes no humanos. Activa la opción SAM.")
        # Modo SAM puro para personajes no humanos
        body_mask, landmarks = segment_full_body_sam_only(rgb, W, H)
        use_pose_detection = False
    else:
        # Modo normal con pose detection
        print("✓ Pose humana detectada - usando landmarks")
        landmarks = results.pose_landmarks[0]
        use_pose_detection = True
        # Segmentar cuerpo con SAM o método alternativo
        if use_sam and sam_predictor is not None:
            body_mask = segment_body_with_sam(rgb, landmarks, W, H, head_offset, foot_offset)
        else:
            body_mask = segment_body_fallback(rgb, landmarks, W, H, head_offset, foot_offset)
    
    output_dir = "body_layers"
    os.makedirs(output_dir, exist_ok=True)
    
    parts_info = {}
    failed_parts = []
    
    # Determinar qué partes procesar según el modo
    if use_pose_detection:
        # Modo normal con landmarks
        parts_to_process = BODY_PARTS
        
        for part_name, config in parts_to_process.items():
            print(f"[🔧] Procesando: {part_name}")
            
            pts = []
            for idx in config["landmarks"]:
                if idx < len(landmarks):
                    lm = landmarks[idx]
                    pts.append([int(lm.x * W), int(lm.y * H)])
            
            if not pts:
                print(f"  └─ ⚠ No hay landmarks disponibles")
                failed_parts.append(part_name)
                continue
            
            # Intentar segmentar con sinónimos y múltiples estrategias
            if use_sam and sam_predictor is not None:
                part_mask = try_segment_with_synonyms(rgb, body_mask, pts, W, H, margin, 
                                                     part_name, config["type"], head_offset, foot_offset)
            else:
                part_mask = create_part_mask_fallback(body_mask, pts, W, H, margin, 
                                                     config["type"], head_offset, foot_offset)
            
            if part_mask.max() == 0:
                print(f"  └─ ⚠ Máscara vacía después de todos los intentos")
                failed_parts.append(part_name)
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
            print(f"  └─ ✓ Guardado: {out_path}")
    else:
        # Modo SAM puro - estimar regiones
        print("[📍] Modo personaje no humano - estimando regiones corporales...")
        estimated_regions = estimate_body_parts_from_mask(body_mask, W, H)
        
        for part_name, region_info in estimated_regions.items():
            print(f"[🔧] Procesando: {part_name}")
            
            part_mask = segment_part_sam_boxed(rgb, body_mask, region_info["box"], 
                                              W, H, region_info["type"])
            
            if part_mask is None or part_mask.max() == 0:
                print(f"  └─ ⚠ No se pudo segmentar")
                failed_parts.append(part_name)
                continue
            
            rgba = cv2.cvtColor(img, cv2.COLOR_BGR2BGRA)
            rgba[:, :, 3] = part_mask
            
            out_path = os.path.join(output_dir, f"{part_name}.png")
            cv2.imwrite(out_path, rgba)
            
            parts_info[part_name] = {
                "file": out_path,
                "size": [W, H],
                "type": region_info["type"]
            }
            print(f"  └─ ✓ Guardado: {out_path}")
    
    # Crear archivo PSD (pasando body_mask e img)
    psd_path = create_psd_from_layers(output_dir, parts_info, W, H, body_mask, img)
    
    # Guardar JSON de información
    json_path = os.path.join(output_dir, "layers_info.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump({
            "original_size": [W, H],
            "head_offset": head_offset,
            "foot_offset": foot_offset,
            "sam_used": use_sam and sam_predictor is not None,
            "pose_detection_used": use_pose_detection,
            "character_type": "human" if use_pose_detection else "non-human",
            "parts": parts_info,
            "total_detected": len(parts_info),
            "total_failed": len(failed_parts),
            "failed_parts": failed_parts,
            "psd_file": psd_path
        }, f, indent=2, ensure_ascii=False)
    
    # Crear imagen de referencia
    ref_img = img.copy()
    for i, name in enumerate(parts_info.keys()):
        cv2.putText(ref_img, name, (10, 30 + i * 20),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
    
    # Agregar partes fallidas en rojo
    for i, name in enumerate(failed_parts):
        cv2.putText(ref_img, f"FALLO: {name}", (10, 30 + (len(parts_info) + i) * 20),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
    
    ref_path = os.path.join(output_dir, "_reference.jpg")
    cv2.imwrite(ref_path, ref_img)
    
    # Guardar body_full
    body_rgba = cv2.cvtColor(img, cv2.COLOR_BGR2BGRA)
    body_rgba[:, :, 3] = body_mask
    cv2.imwrite(os.path.join(output_dir, "_body_full.png"), body_rgba)
    
    print(f"\n[✅] ¡Listo!")
    print(f"    🎭 Tipo: {'Humano (con pose)' if use_pose_detection else 'No-humano (SAM puro)'}")
    print(f"    ✓ Partes detectadas: {len(parts_info)}")
    print(f"    ✗ Partes fallidas: {len(failed_parts)}")
    if failed_parts:
        print(f"    Fallidas: {', '.join(failed_parts)}")
    
    return output_dir, psd_path, len(parts_info), failed_parts

def gradio_process(image, margin, head_offset, foot_offset, use_sam):
    if image is None:
        return None, None, "❌ Sube una imagen primero"
    
    try:
        output_dir, psd_file, num_parts, failed = process_image(
            image, int(margin), int(head_offset), int(foot_offset), use_sam
        )
        
        sam_status = "✓ SAM activado" if (use_sam and sam_predictor is not None) else "○ Método tradicional"
        
        failed_text = ""
        if failed:
            failed_text = f"\n⚠ **Partes no detectadas:** {', '.join(failed)}"
        
        status = f"""✅ **¡ÉXITO!**
📁 Carpeta: {output_dir}/
🎯 Partes detectadas: {num_parts}
🤖 Segmentación: {sam_status}
⬆ Offset cabeza: {head_offset}px
⬇ Offset pies: {foot_offset}px
{failed_text}
📦 Archivos generados:

body_layers.psd → Archivo Photoshop con todas las capas
*.png → Cada parte separada
_body_full.png → Cuerpo completo
_reference.jpg → Referencia visual
layers_info.json → Información de las capas
"""
        ref_path = os.path.join(output_dir, "_reference.jpg")
        return ref_path, psd_file, status
    except Exception as e:
        import traceback
        return None, None, f"❌ Error: {str(e)}\n\n{traceback.format_exc()}"

with gr.Blocks(title="Extractor Capas Corporales", theme=gr.themes.Soft()) as app:
    gr.Markdown("# 🎭 Extractor de Capas Corporales")
    
    with gr.Row():
        with gr.Column():
            img_input = gr.Image(type="filepath", label="📸 Imagen")
            use_sam_input = gr.Checkbox(
                value=True, 
                label="🤖 Usar SAM (Recomendado)",
                info="Segmentación de alta precisión con IA"
            )
            margin_input = gr.Slider(10, 50, value=20, step=5, 
                                    label="📏 Margen (píxeles)",
                                    info="Espacio extra alrededor de cada parte")
            head_input = gr.Slider(0, 200, value=30, step=10,
                                  label="⬆ Offset cabeza (píxeles)",
                                  info="Extensión superior para capturar toda la cabeza")
            foot_input = gr.Slider(0, 200, value=30, step=10,
                                  label="⬇ Offset pies (píxeles)",
                                  info="Extensión inferior para capturar todos los pies")
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