# Básico (con valores óptimos por defecto)
# python normalizer.py -a animation.json -r body.json -o suavizada.json -v

# Ajustar suavidad (más suave)
#python normalizer.py -a animation.json -r body.json -o suavizada.json --process-noise 0.001 --measurement-noise 0.1 -v

# Solo normalizar, sin Kalman
#python normalizer.py -a animation.json -r body.json -o solo_norm.json --no-kalman -v

# Kalman estándar (no adaptativo)
#python normalizer.py -a animation.json -r body.json -o suavizada.json --no-adaptive -v

#!/usr/bin/env python3
"""
Normalizer con escala uniforme - Adapta animación a body de referencia
manteniendo proporciones del movimiento
"""

import json
import numpy as np
import sys
import os

class KalmanFilter3D:
    """Filtro de Kalman para suavizar coordenadas 3D"""
    
    def __init__(self, process_noise=0.005, measurement_noise=0.05):
        self.state = np.zeros(6)
        self.P = np.eye(6) * 1.0
        self.F = np.array([
            [1, 0, 0, 1, 0, 0],
            [0, 1, 0, 0, 1, 0],
            [0, 0, 1, 0, 0, 1],
            [0, 0, 0, 1, 0, 0],
            [0, 0, 0, 0, 1, 0],
            [0, 0, 0, 0, 0, 1]
        ])
        self.H = np.array([
            [1, 0, 0, 0, 0, 0],
            [0, 1, 0, 0, 0, 0],
            [0, 0, 1, 0, 0, 0]
        ])
        self.Q = np.eye(6) * process_noise
        self.R = np.eye(3) * measurement_noise
        self.initialized = False
    
    def update(self, measurement):
        z = np.array(measurement)
        
        if not self.initialized:
            self.state[:3] = z
            self.initialized = True
            return z
        
        self.state = self.F @ self.state
        self.P = self.F @ self.P @ self.F.T + self.Q
        
        y = z - self.H @ self.state
        S = self.H @ self.P @ self.H.T + self.R
        K = self.P @ self.H.T @ np.linalg.inv(S)
        
        self.state = self.state + K @ y
        self.P = (np.eye(6) - K @ self.H) @ self.P
        
        return self.state[:3]

class AdaptiveKalmanFilter3D:
    """Filtro de Kalman adaptativo"""
    
    def __init__(self, base_process_noise=0.005, base_measurement_noise=0.05):
        self.base_process_noise = base_process_noise
        self.base_measurement_noise = base_measurement_noise
        self.filter = None
        self.prev_pos = None
        self.velocities = []
        self.max_velocity_history = 10
        
    def update(self, measurement):
        if self.filter is None:
            self.filter = KalmanFilter3D(
                process_noise=self.base_process_noise,
                measurement_noise=self.base_measurement_noise
            )
            self.prev_pos = np.array(measurement)
            return measurement
        
        current_pos = np.array(measurement)
        velocity = np.linalg.norm(current_pos - self.prev_pos)
        self.velocities.append(velocity)
        
        if len(self.velocities) > self.max_velocity_history:
            self.velocities.pop(0)
        
        avg_velocity = np.mean(self.velocities)
        velocity_factor = 1.0 + (avg_velocity * 50)
        
        self.filter.Q = np.eye(6) * (self.base_process_noise * velocity_factor)
        
        smoothed = self.filter.update(measurement)
        self.prev_pos = smoothed
        
        return smoothed

def smooth_animation_with_kalman(animation_data, process_noise=0.005, measurement_noise=0.05, 
                                 adaptive=True, verbose=False):
    """Aplica filtro de Kalman a toda la animación"""
    if verbose:
        print("🔄 Aplicando suavizado Kalman...")
        print(f"   Modo: {'Adaptativo' if adaptive else 'Estándar'}")
    
    smoothed_animation = {}
    persons_data = {}
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
    
    for person_id, joints_data in persons_data.items():
        if verbose:
            print(f"   Procesando {person_id}...")
        
        for joint_name, joint_trajectory in joints_data.items():
            if adaptive:
                kalman = AdaptiveKalmanFilter3D(process_noise, measurement_noise)
            else:
                kalman = KalmanFilter3D(process_noise, measurement_noise)
            
            for i, point in enumerate(joint_trajectory):
                measurement = [point['x'], point['y'], point['z']]
                smoothed_coords = kalman.update(measurement)
                
                joint_trajectory[i]['x_smooth'] = float(smoothed_coords[0])
                joint_trajectory[i]['y_smooth'] = float(smoothed_coords[1])
                joint_trajectory[i]['z_smooth'] = float(smoothed_coords[2])
    
    for frame_name in frame_names:
        smoothed_animation[frame_name] = {}
        
        for person_id, joints_data in persons_data.items():
            smoothed_animation[frame_name][person_id] = {}
            
            for joint_name, joint_trajectory in joints_data.items():
                frame_idx = next(i for i, p in enumerate(joint_trajectory) 
                               if p['frame'] == frame_name)
                
                point = joint_trajectory[frame_idx]
                
                smoothed_joint = {
                    'x': point['x_smooth'],
                    'y': point['y_smooth'],
                    'z': point['z_smooth']
                }
                smoothed_joint.update(point['other_data'])
                
                smoothed_animation[frame_name][person_id][joint_name] = smoothed_joint
    
    if verbose:
        print("✅ Suavizado completado")
    
    return smoothed_animation

def calculate_jitter_metrics(animation_data, verbose=False):
    """Calcula métricas de jittering"""
    if not verbose:
        return
    
    frame_names = sorted(animation_data.keys())
    if len(frame_names) < 3:
        return
    
    total_acceleration = 0
    joint_count = 0
    person_id = "person_0"
    
    for joint_name in animation_data[frame_names[0]][person_id].keys():
        positions = []
        
        for frame_name in frame_names:
            joint = animation_data[frame_name][person_id][joint_name]
            pos = np.array([joint['x'], joint['y'], joint['z']])
            positions.append(pos)
        
        velocities = np.diff(positions, axis=0)
        accelerations = np.diff(velocities, axis=0)
        
        avg_accel = np.mean([np.linalg.norm(a) for a in accelerations])
        total_acceleration += avg_accel
        joint_count += 1
    
    avg_jitter = total_acceleration / joint_count if joint_count > 0 else 0
    print(f"📊 Jitter promedio: {avg_jitter:.6f}")

def normalize_to_body_with_uniform_scale(animation_file, reference_file, output_file, 
                                         apply_kalman=True, process_noise=0.005, 
                                         measurement_noise=0.05, adaptive=True, verbose=False):
    """
    Normaliza animación al body de referencia usando ESCALA UNIFORME
    Esto mantiene las proporciones del movimiento original
    """
    
    if verbose:
        print("🎯 Normalizando con escala uniforme...")
        print()
    
    # Cargar datos
    with open(animation_file, 'r') as f:
        animation_data = json.load(f)
    
    with open(reference_file, 'r') as f:
        reference_data = json.load(f)
    
    # Métricas antes
    if verbose:
        print("📈 ANTES del procesamiento:")
        calculate_jitter_metrics(animation_data, verbose)
        print()
    
    # Obtener referencia
    ref_frame_name = list(reference_data.keys())[0]
    reference_body = reference_data[ref_frame_name]["person_0"]
    
    first_anim_frame = list(animation_data.keys())[0]
    first_body = animation_data[first_anim_frame]["person_0"]
    
    # Calcular transformación con escala UNIFORME
    transform = calculate_uniform_transformation(first_body, reference_body, verbose)
    
    # Aplicar transformación
    if verbose:
        print("🔧 Aplicando transformación con escala uniforme...")
    
    normalized_animation = {}
    for frame_name, frame_data in animation_data.items():
        normalized_frame = {}
        for person_id, person_data in frame_data.items():
            normalized_person = apply_uniform_transformation(person_data, transform)
            normalized_frame[person_id] = normalized_person
        normalized_animation[frame_name] = normalized_frame
    
    # Aplicar Kalman
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
    
    # Métricas después
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
        verify_proportions(normalized_animation, first_body, verbose)

def calculate_uniform_transformation(source_body, target_body, verbose=False):
    """
    Calcula transformación con ESCALA UNIFORME basada en altura
    """
    key_joints = ['LShoulder', 'RShoulder', 'LHip', 'RHip', 'Nose', 'LAnkle', 'RAnkle', 'Head', 'Neck']
    
    source_points = {}
    target_points = {}
    
    for joint in key_joints:
        if joint in source_body and joint in target_body:
            source_points[joint] = [source_body[joint]['x'], source_body[joint]['y'], 
                                   source_body[joint]['z']]
            target_points[joint] = [target_body[joint]['x'], target_body[joint]['y'], 
                                   target_body[joint]['z']]
    
    # Calcular centros
    source_center = calculate_center(source_points)
    target_center = calculate_center(target_points)
    
    # Calcular altura (rango en Y)
    source_y_coords = [p[1] for p in source_points.values()]
    target_y_coords = [p[1] for p in target_points.values()]
    
    source_height = max(source_y_coords) - min(source_y_coords)
    target_height = max(target_y_coords) - min(target_y_coords)
    
    # ESCALA UNIFORME basada en altura
    uniform_scale = target_height / source_height if source_height > 0 else 1.0
    
    transform = {
        'source_center': source_center,
        'target_center': target_center,
        'uniform_scale': uniform_scale
    }
    
    if verbose:
        print("🎯 Transformación calculada:")
        print(f"   Centro origen: [{source_center[0]:.4f}, {source_center[1]:.4f}, {source_center[2]:.4f}]")
        print(f"   Centro destino: [{target_center[0]:.4f}, {target_center[1]:.4f}, {target_center[2]:.4f}]")
        print(f"   Altura origen: {source_height:.4f}")
        print(f"   Altura destino: {target_height:.4f}")
        print(f"   Escala UNIFORME: {uniform_scale:.4f}")
        print()
    
    return transform

def calculate_center(points_dict):
    """Calcula centro geométrico"""
    if not points_dict:
        return [0.5, 0.5, 0.5]
    
    x_coords = [point[0] for point in points_dict.values()]
    y_coords = [point[1] for point in points_dict.values()]
    z_coords = [point[2] for point in points_dict.values()]
    
    return [
        sum(x_coords) / len(x_coords),
        sum(y_coords) / len(y_coords), 
        sum(z_coords) / len(z_coords)
    ]

def apply_uniform_transformation(body_data, transform):
    """Aplica transformación con escala uniforme"""
    transformed_body = {}
    
    for joint_name, joint_data in body_data.items():
        transformed_joint = {}
        
        if 'x' in joint_data and 'y' in joint_data and 'z' in joint_data:
            # Centrar
            centered_x = joint_data['x'] - transform['source_center'][0]
            centered_y = joint_data['y'] - transform['source_center'][1]
            centered_z = joint_data['z'] - transform['source_center'][2]
            
            # Escalar UNIFORMEMENTE
            scaled_x = centered_x * transform['uniform_scale']
            scaled_y = centered_y * transform['uniform_scale']
            scaled_z = centered_z * transform['uniform_scale']
            
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

def verify_proportions(normalized_animation, original_body, verbose=False):
    """Verifica que las proporciones se mantuvieron"""
    if not verbose:
        return
    
    first_frame = list(normalized_animation.keys())[0]
    normalized_body = normalized_animation[first_frame]["person_0"]
    
    print("🔍 VERIFICACIÓN DE PROPORCIONES:")
    
    # Verificar ratio hombros/caderas
    if all(k in original_body for k in ['LShoulder', 'RShoulder', 'LHip', 'RHip']):
        orig_shoulder_width = abs(original_body['LShoulder']['x'] - original_body['RShoulder']['x'])
        orig_hip_width = abs(original_body['LHip']['x'] - original_body['RHip']['x'])
        
        norm_shoulder_width = abs(normalized_body['LShoulder']['x'] - normalized_body['RShoulder']['x'])
        norm_hip_width = abs(normalized_body['LHip']['x'] - normalized_body['RHip']['x'])
        
        orig_ratio = orig_shoulder_width / orig_hip_width if orig_hip_width > 0 else 0
        norm_ratio = norm_shoulder_width / norm_hip_width if norm_hip_width > 0 else 0
        
        print(f"   Ratio hombros/caderas:")
        print(f"     Original:    {orig_ratio:.4f}")
        print(f"     Normalizado: {norm_ratio:.4f}")
        print(f"     Diferencia:  {abs(orig_ratio - norm_ratio):.6f}")
        
        if abs(orig_ratio - norm_ratio) < 0.01:
            print(f"     ✅ Proporciones mantenidas")
        else:
            print(f"     ⚠️  Proporciones cambiaron")

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Normaliza animación con escala uniforme y Kalman'
    )
    
    parser.add_argument('-a', '--animation', required=True, 
                       help='Archivo de animación JSON')
    parser.add_argument('-r', '--reference', required=True, 
                       help='Archivo body de referencia JSON')
    parser.add_argument('-o', '--output', default='normalized.json', 
                       help='Archivo de salida')
    parser.add_argument('-v', '--verbose', action='store_true', 
                       help='Mostrar información detallada')
    parser.add_argument('--no-kalman', action='store_true', 
                       help='Desactivar suavizado Kalman')
    parser.add_argument('--process-noise', type=float, default=0.005,
                       help='Ruido del proceso (default: 0.005)')
    parser.add_argument('--measurement-noise', type=float, default=0.05,
                       help='Ruido de medición (default: 0.05)')
    parser.add_argument('--no-adaptive', action='store_true',
                       help='Usar Kalman estándar')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.animation):
        print(f"❌ Error: '{args.animation}' no existe")
        sys.exit(1)
        
    if not os.path.exists(args.reference):
        print(f"❌ Error: '{args.reference}' no existe")
        sys.exit(1)
    
    try:
        normalize_to_body_with_uniform_scale(
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