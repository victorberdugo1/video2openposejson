#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import cv2
import json
import numpy as np
import gradio as gr
import torch
from segment_anything import sam_model_registry, SamPredictor, SamAutomaticMaskGenerator

MODEL_DIR = "models"
os.makedirs(MODEL_DIR, exist_ok=True)

SAM_MODEL_URL = "https://dl.fbaipublicfiles.com/segment_anything/sam_vit_h_4b8939.pth"
SAM_CHECKPOINT = os.path.join(MODEL_DIR, "sam_vit_h_4b8939.pth")
SAM_MODEL_TYPE = "vit_h"

def download_model(url, path):
    if not os.path.exists(path):
        print(f"\n{'='*60}")
        print(f"DESCARGANDO MODELO SAM (2.4 GB)")
        print(f"Archivo: {os.path.basename(path)}")
        print(f"Esto puede tardar 5-15 minutos dependiendo de tu conexión")
        print(f"{'='*60}\n")
        
        import urllib.request
        
        def progress_hook(count, block_size, total_size):
            percent = int(count * block_size * 100 / total_size)
            mb_downloaded = count * block_size / (1024 * 1024)
            mb_total = total_size / (1024 * 1024)
            bar_length = 40
            filled = int(bar_length * percent / 100)
            bar = '█' * filled + '-' * (bar_length - filled)
            print(f"\r[{bar}] {percent}% ({mb_downloaded:.1f}/{mb_total:.1f} MB)", end='')
        
        try:
            urllib.request.urlretrieve(url, path, progress_hook)
            print(f"\n\n✓ Modelo descargado correctamente: {path}\n")
        except Exception as e:
            print(f"\n\n✗ Error descargando modelo: {e}")
            print(f"Descarga manual desde: {url}")
            print(f"Guárdalo en: {path}\n")
            raise
    else:
        print(f"✓ Modelo SAM encontrado: {os.path.basename(path)}")

print("\n" + "="*60)
print("INICIALIZANDO SISTEMA")
print("="*60)

if not os.path.exists(SAM_CHECKPOINT):
    print("\nPRIMERA VEZ: Descargando modelo SAM...")
    download_model(SAM_MODEL_URL, SAM_CHECKPOINT)
else:
    print(f"\n✓ Modelo SAM disponible")

print("="*60 + "\n")

BODY_PARTS_GRID = {
    "head": {"row": 0, "col": 1, "spans": 1},
    "neck": {"row": 1, "col": 1, "spans": 1},
    
    "left_shoulder": {"row": 2, "col": 0, "spans": 1},
    "chest": {"row": 2, "col": 1, "spans": 1},
    "right_shoulder": {"row": 2, "col": 2, "spans": 1},
    
    "left_upper_arm": {"row": 3, "col": 0, "spans": 1},
    "abdomen": {"row": 3, "col": 1, "spans": 1},
    "right_upper_arm": {"row": 3, "col": 2, "spans": 1},
    
    "left_forearm": {"row": 4, "col": 0, "spans": 1},
    "pelvis": {"row": 4, "col": 1, "spans": 1},
    "right_forearm": {"row": 4, "col": 2, "spans": 1},
    
    "left_hand": {"row": 5, "col": 0, "spans": 1},
    "right_hand": {"row": 5, "col": 2, "spans": 1},
    
    "left_thigh": {"row": 6, "col": 0, "spans": 1},
    "right_thigh": {"row": 6, "col": 2, "spans": 1},
    
    "left_calf": {"row": 7, "col": 0, "spans": 1},
    "right_calf": {"row": 7, "col": 2, "spans": 1},
    
    "left_foot": {"row": 8, "col": 0, "spans": 1},
    "right_foot": {"row": 8, "col": 2, "spans": 1}
}

def init_sam():
    try:
        if not os.path.exists(SAM_CHECKPOINT):
            print(f"Modelo SAM no encontrado: {SAM_CHECKPOINT}")
            return None, None
        
        device = "cuda" if torch.cuda.is_available() else "cpu"
        print(f"Cargando SAM en {device}...")
        
        sam = sam_model_registry[SAM_MODEL_TYPE](checkpoint=SAM_CHECKPOINT)
        sam.to(device=device)
        
        predictor = SamPredictor(sam)
        mask_generator = SamAutomaticMaskGenerator(
            sam,
            points_per_side=32,
            pred_iou_thresh=0.9,
            stability_score_thresh=0.95,
            crop_n_layers=1,
            crop_n_points_downscale_factor=2,
            min_mask_region_area=100
        )
        
        print("SAM inicializado correctamente")
        return predictor, mask_generator
    except Exception as e:
        print(f"Error al inicializar SAM: {e}")
        return None, None

sam_predictor, sam_mask_generator = init_sam()

def segment_character(img):
    """Segmenta el personaje completo automáticamente"""
    if sam_mask_generator is None:
        raise ValueError("SAM no disponible")
    
    print("Segmentando personaje...")
    masks = sam_mask_generator.generate(img)
    
    if not masks:
        raise ValueError("No se detectó personaje")
    
    masks = sorted(masks, key=lambda x: x['area'], reverse=True)
    main_mask = masks[0]['segmentation'].astype(np.uint8) * 255
    
    kernel = np.ones((5, 5), np.uint8)
    main_mask = cv2.morphologyEx(main_mask, cv2.MORPH_CLOSE, kernel, iterations=2)
    main_mask = cv2.GaussianBlur(main_mask, (21, 21), 0)
    main_mask = (main_mask > 127).astype(np.uint8) * 255
    
    print(f"Personaje segmentado (área: {masks[0]['area']})")
    return main_mask

def create_grid_regions(mask, rows=9, cols=3):
    """Divide la máscara en regiones grid basadas en el bounding box"""
    H, W = mask.shape
    
    y_coords, x_coords = np.where(mask > 0)
    if len(y_coords) == 0:
        return {}
    
    y_min, y_max = y_coords.min(), y_coords.max()
    x_min, x_max = x_coords.min(), x_coords.max()
    
    height = y_max - y_min
    width = x_max - x_min
    
    row_height = height / rows
    col_width = width / cols
    
    regions = {}
    
    for part_name, config in BODY_PARTS_GRID.items():
        row = config["row"]
        col = config["col"]
        
        r_start = int(y_min + row * row_height)
        r_end = int(y_min + (row + 1) * row_height)
        c_start = int(x_min + col * col_width)
        c_end = int(x_min + (col + 1) * col_width)
        
        r_start = max(0, r_start)
        r_end = min(H, r_end)
        c_start = max(0, c_start)
        c_end = min(W, c_end)
        
        region_mask = np.zeros((H, W), dtype=np.uint8)
        region_mask[r_start:r_end, c_start:c_end] = mask[r_start:r_end, c_start:c_end]
        
        if region_mask.sum() > 0:
            regions[part_name] = region_mask
    
    return regions

def refine_part_with_sam(img, region_mask, body_mask, skip_small=True):
    """Refina una región usando SAM"""
    if sam_predictor is None:
        return region_mask
    
    # Saltar regiones muy pequeñas para acelerar
    if skip_small and region_mask.sum() < 1000:
        return region_mask
    
    sam_predictor.set_image(img)
    
    y_coords, x_coords = np.where(region_mask > 0)
    if len(y_coords) == 0:
        return region_mask
    
    center_y = int(np.mean(y_coords))
    center_x = int(np.mean(x_coords))
    
    y_min, y_max = y_coords.min(), y_coords.max()
    x_min, x_max = x_coords.min(), x_coords.max()
    
    input_point = np.array([[center_x, center_y]])
    input_label = np.array([1])
    input_box = np.array([x_min, y_min, x_max, y_max])
    
    try:
        masks, scores, _ = sam_predictor.predict(
            point_coords=input_point,
            point_labels=input_label,
            box=input_box[None, :],
            multimask_output=True
        )
        
        best_mask = masks[np.argmax(scores)].astype(np.uint8) * 255
        refined = cv2.bitwise_and(body_mask, best_mask)
        refined = cv2.GaussianBlur(refined, (11, 11), 0)
        
        return refined
        
    except Exception as e:
        print(f"Error refinando parte: {e}")
        return region_mask

def process_image(image_path, use_sam_refine=True, resize_for_speed=False, max_size=1024):
    img = cv2.imread(image_path)
    if img is None:
        raise ValueError("No se pudo cargar la imagen")
    
    H, W = img.shape[:2]
    original_size = (W, H)
    
    # Redimensionar para acelerar procesamiento
    if resize_for_speed and max(H, W) > max_size:
        scale = max_size / max(H, W)
        new_W, new_H = int(W * scale), int(H * scale)
        img = cv2.resize(img, (new_W, new_H), interpolation=cv2.INTER_AREA)
        H, W = img.shape[:2]
        print(f"Imagen redimensionada: {original_size[0]}x{original_size[1]} → {W}x{H}")
    
    print(f"Resolución de procesamiento: {W}x{H}")
    
    rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    
    body_mask = segment_character(rgb)
    
    print("Dividiendo en regiones...")
    regions = create_grid_regions(body_mask)
    
    output_dir = "body_layers"
    os.makedirs(output_dir, exist_ok=True)
    
    parts_info = {}
    
    for part_name, region_mask in regions.items():
        print(f"Procesando: {part_name}")
        
        if use_sam_refine:
            part_mask = refine_part_with_sam(rgb, region_mask, body_mask, skip_small=True)
        else:
            part_mask = region_mask
        
        if part_mask.max() == 0:
            print(f"{part_name} vacío")
            continue
        
        rgba = cv2.cvtColor(img, cv2.COLOR_BGR2BGRA)
        rgba[:, :, 3] = part_mask
        
        out_path = os.path.join(output_dir, f"{part_name}.png")
        cv2.imwrite(out_path, rgba)
        
        parts_info[part_name] = {
            "file": out_path,
            "size": [W, H]
        }
    
    json_path = os.path.join(output_dir, "layers_info.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump({
            "original_size": [W, H],
            "sam_refine": use_sam_refine,
            "parts": parts_info,
            "total": len(parts_info)
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
    
    print(f"Listo! {len(parts_info)} partes extraídas")
    
    return output_dir, json_path, len(parts_info)

def gradio_process(image, use_sam_refine, fast_mode):
    if image is None:
        return None, None, "Sube una imagen primero"
    
    try:
        import time
        start_time = time.time()
        
        output_dir, json_file, num_parts = process_image(
            image, use_sam_refine, resize_for_speed=fast_mode
        )
        
        elapsed = time.time() - start_time
        
        status = f"""Éxito!

Tiempo: {elapsed:.1f} segundos
Carpeta: {output_dir}/
Partes: {num_parts}
Refinamiento SAM: {'Activado' if use_sam_refine else 'Desactivado'}
Modo rápido: {'Activado' if fast_mode else 'Desactivado'}

Archivos generados:
- *.png → Partes separadas con transparencia
- _body_full.png → Cuerpo completo
- _reference.jpg → Referencia visual
- layers_info.json → Metadatos
"""
        
        ref_path = os.path.join(output_dir, "_reference.jpg")
        return ref_path, json_file, status
        
    except Exception as e:
        import traceback
        return None, None, f"Error: {str(e)}\n\n{traceback.format_exc()}"

with gr.Blocks(title="Segmentador Universal de Personajes", theme=gr.themes.Soft()) as app:
    gr.Markdown("""
    # Segmentador Universal de Personajes
    
    Funciona con humanos, criaturas, robots, aliens, etc.
    Usa SAM para segmentación automática sin necesidad de pose detection.
    
    **Tiempos estimados (1080x1080):**
    - Con GPU + Refinamiento: ~25-35 segundos
    - Con CPU + Refinamiento: ~5-8 minutos
    - Modo rápido (GPU): ~10-15 segundos
    - Modo rápido (CPU): ~2-3 minutos
    """)
    
    with gr.Row():
        with gr.Column():
            img_input = gr.Image(type="filepath", label="Imagen")
            refine_input = gr.Checkbox(
                value=True, 
                label="Refinamiento SAM por parte",
                info="Más preciso pero más lento"
            )
            fast_input = gr.Checkbox(
                value=True,
                label="Modo rápido (redimensiona a 1024px)",
                info="RECOMENDADO: Reduce de 30 seg a 12 seg (GPU) o de 6 min a 2 min (CPU)"
            )
            btn = gr.Button("Extraer Capas", variant="primary", size="lg")
        
        with gr.Column():
            ref_output = gr.Image(label="Referencia")
            json_output = gr.File(label="JSON")
            status_output = gr.Markdown()
    
    btn.click(
        gradio_process, 
        [img_input, refine_input, fast_input], 
        [ref_output, json_output, status_output]
    )

if __name__ == "__main__":
    app.launch()