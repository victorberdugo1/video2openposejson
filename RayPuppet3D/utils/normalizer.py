# Básico (con valores óptimos por defecto)
# python normalizer.py -a animation.json -r body.json -o suavizada.json -v

# Ajustar suavidad (más suave)
#python normalizer.py -a animation.json -r body.json -o suavizada.json --process-noise 0.001 --measurement-noise 0.1 -v

# Solo normalizar, sin Kalman
#python normalizer.py -a animation.json -r body.json -o solo_norm.json --no-kalman -v

# Kalman estándar (no adaptativo)
#python normalizer.py -a animation.json -r body.json -o suavizada.json --no-adaptive -v

import json
import numpy as np
import sys
import os
from typing import Dict, List, Tuple

class KalmanFilter3D:
    """Filtro de Kalman para suavizar coordenadas 3D"""
    
    def __init__(self, process_noise=0.01, measurement_noise=0.1):
        """
        process_noise: ruido del proceso (menor = más suave pero más lag)
        measurement_noise: ruido de medición (menor = confía más en mediciones)
        """
        # Estado: [x, y, z, vx, vy, vz]
        self.state = np.zeros(6)
        
        # Matriz de covarianza del estado
        self.P = np.eye(6) * 1.0
        
        # Matriz de transición (modelo de velocidad constante)
        self.F = np.array([
            [1, 0, 0, 1, 0, 0],
            [0, 1, 0, 0, 1, 0],
            [0, 0, 1, 0, 0, 1],
            [0, 0, 0, 1, 0, 0],
            [0, 0, 0, 0, 1, 0],
            [0, 0, 0, 0, 0, 1]
        ])
        
        # Matriz de observación (solo vemos posición)
        self.H = np.array([
            [1, 0, 0, 0, 0, 0],
            [0, 1, 0, 0, 0, 0],
            [0, 0, 1, 0, 0, 0]
        ])
        
        # Ruido del proceso
        self.Q = np.eye(6) * process_noise
        
        # Ruido de medición
        self.R = np.eye(3) * measurement_noise
        
        self.initialized = False
    
    def update(self, measurement):
        """Actualiza el filtro con una nueva medición [x, y, z]"""
        z = np.array(measurement)
        
        if not self.initialized:
            # Primera medición: inicializar estado
            self.state[:3] = z
            self.initialized = True
            return z
        
        # Predicción
        self.state = self.F @ self.state
        self.P = self.F @ self.P @ self.F.T + self.Q
        
        # Innovación
        y = z - self.H @ self.state
        S = self.H @ self.P @ self.H.T + self.R
        
        # Ganancia de Kalman
        K = self.P @ self.H.T @ np.linalg.inv(S)
        
        # Actualización
        self.state = self.state + K @ y
        self.P = (np.eye(6) - K @ self.H) @ self.P
        
        return self.state[:3]  # Retornar solo posición

class AdaptiveKalmanFilter3D:
    """Filtro de Kalman adaptativo que ajusta el ruido según la velocidad"""
    
    def __init__(self, base_process_noise=0.005, base_measurement_noise=0.05):
        self.base_process_noise = base_process_noise
        self.base_measurement_noise = base_measurement_noise
        self.filter = None
        self.prev_pos = None
        self.velocities = []
        self.max_velocity_history = 10
        
    def update(self, measurement):
        """Actualiza con ajuste adaptativo del ruido"""
        if self.filter is None:
            # Inicializar filtro
            self.filter = KalmanFilter3D(
                process_noise=self.base_process_noise,
                measurement_noise=self.base_measurement_noise
            )
            self.prev_pos = np.array(measurement)
            return measurement
        
        # Calcular velocidad
        current_pos = np.array(measurement)
        velocity = np.linalg.norm(current_pos - self.prev_pos)
        self.velocities.append(velocity)
        
        if len(self.velocities) > self.max_velocity_history:
            self.velocities.pop(0)
        
        # Ajustar ruido según velocidad
        avg_velocity = np.mean(self.velocities)
        
        # Mayor velocidad = mayor ruido de proceso (más reactivo)
        # Menor velocidad = menor ruido (más suave)
        velocity_factor = 1.0 + (avg_velocity * 50)  # Ajustar factor según necesidad
        
        self.filter.Q = np.eye(6) * (self.base_process_noise * velocity_factor)
        
        # Aplicar filtro
        smoothed = self.filter.update(measurement)
        self.prev_pos = smoothed
        
        return smoothed

def smooth_animation_with_kalman(animation_data, process_noise=0.005, measurement_noise=0.05, 
                                 adaptive=True, verbose=False):
    """
    Aplica filtro de Kalman a toda la animación
    """
    if verbose:
        print("🔄 Iniciando suavizado con Kalman...")
        print(f"   Modo: {'Adaptativo' if adaptive else 'Estándar'}")
        print(f"   Process noise: {process_noise}")
        print(f"   Measurement noise: {measurement_noise}")
    
    smoothed_animation = {}
    
    # Organizar datos por persona y articulación
    persons_data = {}
    
    # Extraer todos los frames y organizarlos
    frame_names = sorted(animation_data.keys())
    
    for frame_name in frame_names:
        frame_data = animation_data[frame_name]
        
        for person_id, person_data in frame_data.items():
            if person_id not in persons_data:
                persons_data[person_id] = {}
            
            for joint_name, joint_data in person_data.items():
                if joint_name not in persons_data[person_id]:
                    persons_data[person_id][joint_name] = []
                
                persons_data[person_id][joint_name].append({
                    'frame': frame_name,
                    'x': joint_data.get('x', 0),
                    'y': joint_data.get('y', 0),
                    'z': joint_data.get('z', 0),
                    'other_data': {k: v for k, v in joint_data.items() 
                                  if k not in ['x', 'y', 'z']}
                })
    
    # Aplicar Kalman a cada articulación de cada persona
    for person_id, joints_data in persons_data.items():
        if verbose:
            print(f"   Procesando {person_id}...")
        
        for joint_name, joint_trajectory in joints_data.items():
            # Crear filtro para esta articulación
            if adaptive:
                kalman = AdaptiveKalmanFilter3D(process_noise, measurement_noise)
            else:
                kalman = KalmanFilter3D(process_noise, measurement_noise)
            
            # Suavizar trayectoria
            for i, point in enumerate(joint_trajectory):
                measurement = [point['x'], point['y'], point['z']]
                smoothed_coords = kalman.update(measurement)
                
                # Actualizar coordenadas suavizadas
                joint_trajectory[i]['x_smooth'] = float(smoothed_coords[0])
                joint_trajectory[i]['y_smooth'] = float(smoothed_coords[1])
                joint_trajectory[i]['z_smooth'] = float(smoothed_coords[2])
    
    # Reconstruir estructura de animación con datos suavizados
    for frame_name in frame_names:
        smoothed_animation[frame_name] = {}
        
        for person_id, joints_data in persons_data.items():
            smoothed_animation[frame_name][person_id] = {}
            
            for joint_name, joint_trajectory in joints_data.items():
                # Encontrar el índice del frame actual
                frame_idx = next(i for i, p in enumerate(joint_trajectory) 
                               if p['frame'] == frame_name)
                
                point = joint_trajectory[frame_idx]
                
                # Crear datos de articulación suavizada
                smoothed_joint = {
                    'x': point['x_smooth'],
                    'y': point['y_smooth'],
                    'z': point['z_smooth']
                }
                
                # Añadir otros datos
                smoothed_joint.update(point['other_data'])
                
                smoothed_animation[frame_name][person_id][joint_name] = smoothed_joint
    
    if verbose:
        print("✅ Suavizado completado")
    
    return smoothed_animation

def calculate_jitter_metrics(animation_data, verbose=False):
    """Calcula métricas de jittering (aceleración promedio)"""
    if not verbose:
        return
    
    frame_names = sorted(animation_data.keys())
    if len(frame_names) < 3:
        return
    
    total_acceleration = 0
    joint_count = 0
    
    # Analizar primera persona
    person_id = "person_0"
    
    for joint_name in animation_data[frame_names[0]][person_id].keys():
        positions = []
        
        for frame_name in frame_names:
            joint = animation_data[frame_name][person_id][joint_name]
            pos = np.array([joint['x'], joint['y'], joint['z']])
            positions.append(pos)
        
        # Calcular aceleraciones
        velocities = np.diff(positions, axis=0)
        accelerations = np.diff(velocities, axis=0)
        
        avg_accel = np.mean([np.linalg.norm(a) for a in accelerations])
        total_acceleration += avg_accel
        joint_count += 1
    
    avg_jitter = total_acceleration / joint_count if joint_count > 0 else 0
    print(f"📊 Jitter promedio (aceleración): {avg_jitter:.6f}")

def normalize_to_exact_body_size(animation_file, reference_file, output_file, 
                                 apply_kalman=True, process_noise=0.005, 
                                 measurement_noise=0.05, adaptive=True, verbose=False):
    """
    Normaliza animación y aplica filtro de Kalman para suavizado
    """
    
    if verbose:
        print("🎯 Iniciando normalización con suavizado Kalman...")
        print()
    
    # Cargar datos
    with open(animation_file, 'r') as f:
        animation_data = json.load(f)
    
    with open(reference_file, 'r') as f:
        reference_data = json.load(f)
    
    # Métricas antes del procesamiento
    if verbose:
        print("📈 ANTES del procesamiento:")
        calculate_jitter_metrics(animation_data, verbose)
        print()
    
    # Obtener referencia
    ref_frame_name = list(reference_data.keys())[0]
    reference_body = reference_data[ref_frame_name]["person_0"]
    
    first_anim_frame = list(animation_data.keys())[0]
    first_body = animation_data[first_anim_frame]["person_0"]
    
    # Calcular transformación
    transform = calculate_exact_transformation(first_body, reference_body, verbose)
    
    # Aplicar transformación
    if verbose:
        print("🔧 Aplicando transformación espacial...")
    
    normalized_animation = {}
    for frame_name, frame_data in animation_data.items():
        normalized_frame = {}
        for person_id, person_data in frame_data.items():
            normalized_person = apply_exact_transformation(person_data, transform)
            normalized_frame[person_id] = normalized_person
        normalized_animation[frame_name] = normalized_frame
    
    # Aplicar Kalman si está habilitado
    if apply_kalman:
        if verbose:
            print()
        normalized_animation = smooth_animation_with_kalman(
            normalized_animation, 
            process_noise=process_noise,
            measurement_noise=measurement_noise,
            adaptive=adaptive,
            verbose=verbose
        )
    
    # Métricas después del procesamiento
    if verbose:
        print()
        print("📉 DESPUÉS del procesamiento:")
        calculate_jitter_metrics(normalized_animation, verbose)
        print()
    
    # Guardar
    with open(output_file, 'w') as f:
        json.dump(normalized_animation, f, indent=2)
    
    if verbose:
        print(f"💾 Guardado en: {output_file}")
        verify_transformation(normalized_animation, reference_body, verbose)
    
    return normalized_animation

def calculate_exact_transformation(source_body, target_body, verbose=False):
    """Calcula transformación exacta"""
    key_joints = ['LShoulder', 'RShoulder', 'LHip', 'RHip', 'Nose', 'LAnkle', 'RAnkle']
    
    source_points = {}
    target_points = {}
    
    for joint in key_joints:
        if joint in source_body and joint in target_body:
            source_points[joint] = [source_body[joint]['x'], source_body[joint]['y'], 
                                   source_body[joint]['z']]
            target_points[joint] = [target_body[joint]['x'], target_body[joint]['y'], 
                                   target_body[joint]['z']]
    
    source_center = calculate_center(source_points)
    target_center = calculate_center(target_points)
    
    source_ranges = calculate_ranges(source_points, source_center)
    target_ranges = calculate_ranges(target_points, target_center)
    
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
        print("🎯 Transformación calculada:")
        print(f"   Centro origen: [{source_center[0]:.4f}, {source_center[1]:.4f}, {source_center[2]:.4f}]")
        print(f"   Centro destino: [{target_center[0]:.4f}, {target_center[1]:.4f}, {target_center[2]:.4f}]")
        print(f"   Escalas: X={scale_x:.4f}, Y={scale_y:.4f}, Z={scale_z:.4f}")
        print()
    
    return transform

def calculate_center(points_dict):
    """Calcula centro geométrico"""
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
    """Calcula rangos desde el centro"""
    if not points_dict:
        return {'x': 0.1, 'y': 0.1, 'z': 0.1}
    
    max_x_dist = max(abs(point[0] - center[0]) for point in points_dict.values())
    max_y_dist = max(abs(point[1] - center[1]) for point in points_dict.values())
    max_z_dist = max(abs(point[2] - center[2]) for point in points_dict.values())
    
    return {
        'x': max_x_dist * 2,
        'y': max_y_dist * 2,
        'z': max_z_dist * 2
    }

def apply_exact_transformation(body_data, transform):
    """Aplica transformación a un cuerpo"""
    transformed_body = {}
    
    for joint_name, joint_data in body_data.items():
        transformed_joint = {}
        
        if 'x' in joint_data and 'y' in joint_data and 'z' in joint_data:
            # Centrar
            centered_x = joint_data['x'] - transform['source_center'][0]
            centered_y = joint_data['y'] - transform['source_center'][1]
            centered_z = joint_data['z'] - transform['source_center'][2]
            
            # Escalar
            scaled_x = centered_x * transform['scale_x']
            scaled_y = centered_y * transform['scale_y']
            scaled_z = centered_z * transform['scale_z']
            
            # Reposicionar
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
    """Verifica la transformación"""
    if not verbose:
        return
    
    first_frame_name = list(normalized_animation.keys())[0]
    first_transformed = normalized_animation[first_frame_name]["person_0"]
    
    if 'LShoulder' in first_transformed and 'RShoulder' in first_transformed:
        transformed_shoulder_width = abs(first_transformed['LShoulder']['x'] - 
                                        first_transformed['RShoulder']['x'])
        
        if 'LShoulder' in reference_body and 'RShoulder' in reference_body:
            ref_shoulder_width = abs(reference_body['LShoulder']['x'] - 
                                    reference_body['RShoulder']['x'])
            
            print("🔍 VERIFICACIÓN:")
            print(f"   Ancho hombros referencia: {ref_shoulder_width:.6f}")
            print(f"   Ancho hombros transformado: {transformed_shoulder_width:.6f}")
            print(f"   Diferencia: {abs(ref_shoulder_width - transformed_shoulder_width):.6f}")
            
            if abs(ref_shoulder_width - transformed_shoulder_width) < 0.001:
                print("   ✅ Ancho de hombros: CORRECTO")
            else:
                print("   ⚠️ Ancho de hombros: DIFIERE")

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Normaliza y suaviza animación con filtro de Kalman'
    )
    
    parser.add_argument('-a', '--animation', required=True, 
                       help='Archivo de animación JSON')
    parser.add_argument('-r', '--reference', required=True, 
                       help='Archivo body de referencia JSON')
    parser.add_argument('-o', '--output', default='smoothed_normalized.json', 
                       help='Archivo de salida')
    parser.add_argument('-v', '--verbose', action='store_true', 
                       help='Mostrar información detallada')
    parser.add_argument('--no-kalman', action='store_true', 
                       help='Desactivar suavizado Kalman')
    parser.add_argument('--process-noise', type=float, default=0.005,
                       help='Ruido del proceso (default: 0.005, menor = más suave)')
    parser.add_argument('--measurement-noise', type=float, default=0.05,
                       help='Ruido de medición (default: 0.05)')
    parser.add_argument('--no-adaptive', action='store_true',
                       help='Usar Kalman estándar en vez de adaptativo')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.animation):
        print(f"❌ Error: '{args.animation}' no existe")
        sys.exit(1)
        
    if not os.path.exists(args.reference):
        print(f"❌ Error: '{args.reference}' no existe")
        sys.exit(1)
    
    try:
        normalize_to_exact_body_size(
            args.animation,
            args.reference,
            args.output,
            apply_kalman=not args.no_kalman,
            process_noise=args.process_noise,
            measurement_noise=args.measurement_noise,
            adaptive=not args.no_adaptive,
            verbose=args.verbose
        )
        print(f"✅ Proceso completado: {args.output}")
    except Exception as e:
        print(f"❌ Error: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)