#python normalizer.py -a animation.json -r body.json -o 3d_combined_data.json

import json
import numpy as np
import sys
import os

def normalize_to_exact_body_size(animation_file, reference_file, output_file, verbose=False):
    """
    Normaliza una animación para que tenga EXACTAMENTE el mismo tamaño y posición que el body de referencia
    """
    
    if verbose:
        print("🔄 Iniciando normalización a tamaño exacto del body...")
    
    # Cargar datos
    with open(animation_file, 'r') as f:
        animation_data = json.load(f)
    
    with open(reference_file, 'r') as f:
        reference_data = json.load(f)
    
    # Obtener el body de referencia
    ref_frame_name = list(reference_data.keys())[0]
    reference_body = reference_data[ref_frame_name]["person_0"]
    
    # Obtener el primer frame de la animación
    first_anim_frame = list(animation_data.keys())[0]
    first_body = animation_data[first_anim_frame]["person_0"]
    
    if verbose:
        print(f"📋 Body de referencia: {ref_frame_name}")
        print(f"📋 Primer frame animación: {first_anim_frame}")
    
    # Calcular la transformación exacta
    transform = calculate_exact_transformation(first_body, reference_body, verbose)
    
    # Aplicar transformación a todos los frames
    normalized_animation = {}
    frame_count = 0
    
    for frame_name, frame_data in animation_data.items():
        normalized_frame = {}
        
        for person_id, person_data in frame_data.items():
            normalized_person = apply_exact_transformation(person_data, transform)
            normalized_frame[person_id] = normalized_person
        
        normalized_animation[frame_name] = normalized_frame
        frame_count += 1
        
        if verbose and frame_count % 50 == 0:
            print(f"🔄 Procesados {frame_count} frames...")
    
    # Guardar resultado
    with open(output_file, 'w') as f:
        json.dump(normalized_animation, f, indent=2)
    
    if verbose:
        print(f"✅ Normalización completada: {frame_count} frames")
        print(f"💾 Guardado en: {output_file}")
        
        # Verificación final
        verify_transformation(normalized_animation, reference_body, verbose)
    
    return normalized_animation

def calculate_exact_transformation(source_body, target_body, verbose=False):
    """
    Calcula la transformación exacta para que source_body tenga el mismo tamaño y posición que target_body
    """
    
    # Puntos clave para la transformación
    key_joints = ['LShoulder', 'RShoulder', 'LHip', 'RHip', 'Nose', 'LAnkle', 'RAnkle']
    
    # Extraer puntos de origen y destino
    source_points = {}
    target_points = {}
    
    for joint in key_joints:
        if joint in source_body and joint in target_body:
            source_points[joint] = [source_body[joint]['x'], source_body[joint]['y'], source_body[joint]['z']]
            target_points[joint] = [target_body[joint]['x'], target_body[joint]['y'], target_body[joint]['z']]
    
    # Calcular centros
    source_center = calculate_center(source_points)
    target_center = calculate_center(target_points)
    
    # Calcular escalas por eje
    source_ranges = calculate_ranges(source_points, source_center)
    target_ranges = calculate_ranges(target_points, target_center)
    
    # Factores de escala
    scale_x = target_ranges['x'] / source_ranges['x'] if source_ranges['x'] > 0 else 1.0
    scale_y = target_ranges['y'] / source_ranges['y'] if source_ranges['y'] > 0 else 1.0
    scale_z = target_ranges['z'] / source_ranges['z'] if source_ranges['z'] > 0 else 1.0
    
    transform = {
        'source_center': source_center,
        'target_center': target_center,
        'scale_x': scale_x,
        'scale_y': scale_y,
        'scale_z': scale_z
    }
    
    if verbose:
        print("🔧 Transformación calculada:")
        print(f"   Centro origen: [{source_center[0]:.4f}, {source_center[1]:.4f}, {source_center[2]:.4f}]")
        print(f"   Centro destino: [{target_center[0]:.4f}, {target_center[1]:.4f}, {target_center[2]:.4f}]")
        print(f"   Escalas: X={scale_x:.4f}, Y={scale_y:.4f}, Z={scale_z:.4f}")
        print(f"   Rangos origen: X={source_ranges['x']:.4f}, Y={source_ranges['y']:.4f}, Z={source_ranges['z']:.4f}")
        print(f"   Rangos destino: X={target_ranges['x']:.4f}, Y={target_ranges['y']:.4f}, Z={target_ranges['z']:.4f}")
        print()
    
    return transform

def calculate_center(points_dict):
    """Calcula el centro geométrico de un conjunto de puntos"""
    if not points_dict:
        return [0.5, 0.5, 0.0]
    
    x_coords = [point[0] for point in points_dict.values()]
    y_coords = [point[1] for point in points_dict.values()]
    z_coords = [point[2] for point in points_dict.values()]
    
    return [
        sum(x_coords) / len(x_coords),
        sum(y_coords) / len(y_coords), 
        sum(z_coords) / len(z_coords)
    ]

def calculate_ranges(points_dict, center):
    """Calcula los rangos (extensión máxima) desde el centro en cada eje"""
    if not points_dict:
        return {'x': 0.1, 'y': 0.1, 'z': 0.1}
    
    max_x_dist = max(abs(point[0] - center[0]) for point in points_dict.values())
    max_y_dist = max(abs(point[1] - center[1]) for point in points_dict.values())
    max_z_dist = max(abs(point[2] - center[2]) for point in points_dict.values())
    
    return {
        'x': max_x_dist * 2,  # diámetro completo
        'y': max_y_dist * 2,
        'z': max_z_dist * 2
    }

def apply_exact_transformation(body_data, transform):
    """
    Aplica la transformación exacta a un cuerpo
    """
    transformed_body = {}
    
    for joint_name, joint_data in body_data.items():
        transformed_joint = {}
        
        # Transformar coordenadas: centrar, escalar, reposicionar
        if 'x' in joint_data and 'y' in joint_data and 'z' in joint_data:
            # Centrar en origen
            centered_x = joint_data['x'] - transform['source_center'][0]
            centered_y = joint_data['y'] - transform['source_center'][1]
            centered_z = joint_data['z'] - transform['source_center'][2]
            
            # Escalar
            scaled_x = centered_x * transform['scale_x']
            scaled_y = centered_y * transform['scale_y']
            scaled_z = centered_z * transform['scale_z']
            
            # Reposicionar al centro de destino
            transformed_joint['x'] = scaled_x + transform['target_center'][0]
            transformed_joint['y'] = scaled_y + transform['target_center'][1]
            transformed_joint['z'] = scaled_z + transform['target_center'][2]
        
        # Preservar otros campos
        for key, value in joint_data.items():
            if key not in ['x', 'y', 'z']:
                transformed_joint[key] = value
        
        transformed_body[joint_name] = transformed_joint
    
    return transformed_body

def verify_transformation(normalized_animation, reference_body, verbose=False):
    """
    Verifica que la transformación fue exitosa comparando el primer frame transformado con la referencia
    """
    if not verbose:
        return
    
    first_frame_name = list(normalized_animation.keys())[0]
    first_transformed = normalized_animation[first_frame_name]["person_0"]
    
    # Comparar hombros
    if 'LShoulder' in first_transformed and 'RShoulder' in first_transformed:
        transformed_shoulder_width = abs(first_transformed['LShoulder']['x'] - first_transformed['RShoulder']['x'])
        transformed_center_x = (first_transformed['LShoulder']['x'] + first_transformed['RShoulder']['x']) / 2
        
        if 'LShoulder' in reference_body and 'RShoulder' in reference_body:
            ref_shoulder_width = abs(reference_body['LShoulder']['x'] - reference_body['RShoulder']['x'])
            ref_center_x = (reference_body['LShoulder']['x'] + reference_body['RShoulder']['x']) / 2
            
            print("🔍 VERIFICACIÓN FINAL:")
            print(f"   Ancho hombros referencia: {ref_shoulder_width:.6f}")
            print(f"   Ancho hombros transformado: {transformed_shoulder_width:.6f}")
            print(f"   Diferencia: {abs(ref_shoulder_width - transformed_shoulder_width):.6f}")
            print(f"   Centro X referencia: {ref_center_x:.6f}")
            print(f"   Centro X transformado: {transformed_center_x:.6f}")
            print(f"   Diferencia centro: {abs(ref_center_x - transformed_center_x):.6f}")
            
            if abs(ref_shoulder_width - transformed_shoulder_width) < 0.001:
                print("✅ Ancho de hombros: CORRECTO")
            else:
                print("⚠️ Ancho de hombros: DIFIERE")
                
            if abs(ref_center_x - transformed_center_x) < 0.001:
                print("✅ Posición X: CORRECTA")  
            else:
                print("⚠️ Posición X: DIFIERE")

# Script principal
if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Normaliza animación para que tenga exactamente el tamaño del body de referencia'
    )
    
    parser.add_argument('-a', '--animation', required=True, help='Archivo de animación JSON')
    parser.add_argument('-r', '--reference', required=True, help='Archivo body de referencia JSON') 
    parser.add_argument('-o', '--output', default='exact_size_normalized.json', help='Archivo de salida')
    parser.add_argument('-v', '--verbose', action='store_true', help='Mostrar información detallada')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.animation):
        print(f"❌ Error: '{args.animation}' no existe")
        sys.exit(1)
        
    if not os.path.exists(args.reference):
        print(f"❌ Error: '{args.reference}' no existe")
        sys.exit(1)
    
    try:
        normalize_to_exact_body_size(args.animation, args.reference, args.output, args.verbose)
        print(f"🎉 Normalización exitosa: {args.output}")
    except Exception as e:
        print(f"❌ Error: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)