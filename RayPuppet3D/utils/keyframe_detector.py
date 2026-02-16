"""
Detector de Keyframes

python keyframe_detector.py input.json output.json

# Más keyframes (20% en vez de 15%)
python keyframe_detector.py input.json output.json --density 0.20

# Menos keyframes (10% en vez de 15%)
python keyframe_detector.py input.json output.json --density 0.10

# Número exacto (ignora AUTO)
python keyframe_detector.py input.json output.json --num 30

"""

import json
import numpy as np
import argparse
from typing import Dict, List, Tuple, Set
from dataclasses import dataclass


@dataclass
class KeyframeInfo:
    frame_id: str
    frame_number: int
    score: float
    reasons: List[str]


class HybridKeyframeDetector:
    """
    Detector híbrido que garantiza:
    1. Cobertura temporal completa
    2. Captura de momentos críticos (impactos, cambios de dirección)
    """
    
    def __init__(self, 
                 direction_threshold: float = 0.015):
        """
        Args:
            direction_threshold: Umbral para detectar cambios de dirección significativos
        """
        self.direction_threshold = direction_threshold
        
        self.important_joints = [
            'RWrist', 'LWrist', 'RElbow', 'LElbow',
            'RShoulder', 'LShoulder', 'RHip', 'LHip',
            'RKnee', 'LKnee', 'RAnkle', 'LAnkle'
        ]
    
    def load_animation(self, filepath: str) -> Dict:
        with open(filepath, 'r') as f:
            return json.load(f)
    
    def extract_positions(self, animation_data: Dict) -> Tuple[np.ndarray, List[str], List[str]]:
        frame_ids = sorted(animation_data.keys())
        joints = list(animation_data[frame_ids[0]]['person_0'].keys())
        
        positions = []
        for frame_id in frame_ids:
            frame_data = animation_data[frame_id]['person_0']
            frame_positions = []
            for joint in joints:
                pos = frame_data[joint]
                frame_positions.append([pos['x'], pos['y'], pos['z']])
            positions.append(frame_positions)
        
        return np.array(positions), frame_ids, joints
    
    def detect_direction_changes(self, positions: np.ndarray, joints: List[str]) -> np.ndarray:
        """
        Detecta CAMBIOS DE DIRECCIÓN usando análisis vectorial
        
        Esto identifica:
        - Impactos (velocidad alta → baja)
        - Cambios bruscos de trayectoria
        - Inicios/paros de movimiento
        """
        n_frames = len(positions)
        direction_change_score = np.zeros(n_frames)
        
        for joint_idx, joint_name in enumerate(joints):
            if joint_name not in self.important_joints:
                continue
            
            # Calcular vectores de velocidad
            velocities = np.zeros((n_frames, 3))
            velocities[1:] = positions[1:, joint_idx] - positions[:-1, joint_idx]
            
            # Calcular magnitud de velocidad
            velocity_magnitude = np.linalg.norm(velocities, axis=1)
            
            # Detectar cambios bruscos de velocidad (aceleración)
            acceleration = np.zeros(n_frames)
            acceleration[1:] = np.abs(velocity_magnitude[1:] - velocity_magnitude[:-1])
            
            # Detectar cambios de dirección (ángulo entre vectores de velocidad)
            for i in range(2, n_frames):
                v1 = velocities[i-1]
                v2 = velocities[i]
                
                # Si ambos vectores tienen magnitud significativa
                if np.linalg.norm(v1) > 0.001 and np.linalg.norm(v2) > 0.001:
                    # Calcular ángulo entre vectores
                    cos_angle = np.dot(v1, v2) / (np.linalg.norm(v1) * np.linalg.norm(v2))
                    cos_angle = np.clip(cos_angle, -1.0, 1.0)
                    angle = np.arccos(cos_angle)
                    
                    # Cambio de dirección: ángulo grande = cambio brusco
                    direction_change_score[i] += angle
            
            # Agregar aceleración al score (cambios bruscos de velocidad)
            direction_change_score += acceleration * 2.0
        
        return direction_change_score
    
    def detect_critical_frames(self, positions: np.ndarray, joints: List[str], 
                               threshold_multiplier: float = 1.0) -> Set[int]:
        """
        Detecta TODOS los frames críticos basados en cambios de dirección
        SIN LÍMITE - todos los que superen el umbral se incluyen
        
        Args:
            threshold_multiplier: Multiplicador del umbral (1.0 = normal, 0.5 = más sensible)
        
        Returns:
            Set de índices de frames críticos
        """
        direction_scores = self.detect_direction_changes(positions, joints)
        
        # Normalizar scores
        if np.max(direction_scores) > 0:
            direction_scores = direction_scores / np.max(direction_scores)
        
        # Ajustar umbral
        adjusted_threshold = self.direction_threshold * threshold_multiplier
        
        # Encontrar TODOS los picos locales que superen el umbral
        critical_frames = set()
        
        # Buscar máximos locales en ventana de 5 frames
        window = 5
        for i in range(window, len(direction_scores) - window):
            # Si supera el umbral y es máximo local
            if direction_scores[i] > adjusted_threshold:
                is_local_max = True
                for j in range(-window, window + 1):
                    if j != 0 and direction_scores[i + j] > direction_scores[i]:
                        is_local_max = False
                        break
                
                if is_local_max:
                    critical_frames.add(i)
        
        return critical_frames
    
    def detect_keyframes_hybrid(self, animation_data: Dict, 
                                num_distributed: int = 15) -> List[KeyframeInfo]:
        """
        Detección HÍBRIDA de keyframes
        
        Args:
            num_distributed: Número de keyframes DISTRIBUIDOS (además de los críticos)
        
        Algoritmo:
        1. Detectar TODOS los frames críticos (sin límite)
        2. Distribuir num_distributed keyframes en el resto
        3. Total = críticos + distribuidos + 2 (inicio/final)
        """
        positions, frame_ids, joints = self.extract_positions(animation_data)
        n_frames = len(positions)
        
        # Detectar TODOS los frames críticos (sin límite de cantidad)
        critical_frames = self.detect_critical_frames(positions, joints)
        
        num_critical = len(critical_frames)
        
        print(f"\n🎯 Estrategia híbrida:")
        print(f"   - {num_critical} keyframes críticos detectados (TODOS los cambios de dirección)")
        print(f"   - {num_distributed} keyframes distribuidos uniformemente")
        print(f"   - 2 keyframes fijos (inicio y final)")
        print(f"   = {num_critical + num_distributed + 2} keyframes TOTALES")
        
        print(f"\n📍 Frames críticos detectados: {sorted(critical_frames)}")
        
        # Calcular scores de importancia para distribución
        direction_scores = self.detect_direction_changes(positions, joints)
        
        # Crear set de frames ya seleccionados
        selected_frames = set([0, n_frames - 1])  # Primero y último
        selected_frames.update(critical_frames)
        
        # Distribución uniforme en frames NO críticos
        available_frames = [i for i in range(1, n_frames - 1) if i not in selected_frames]
        
        if num_distributed > 0 and len(available_frames) > 0:
            # Dividir en segmentos
            segment_size = len(available_frames) / num_distributed
            
            for i in range(num_distributed):
                start_idx = int(i * segment_size)
                end_idx = int((i + 1) * segment_size)
                
                segment = available_frames[start_idx:end_idx] if end_idx <= len(available_frames) else available_frames[start_idx:]
                
                if segment:
                    # Tomar el de mayor importancia en este segmento
                    best_frame = max(segment, key=lambda idx: direction_scores[idx])
                    selected_frames.add(best_frame)
        
        # Crear lista de keyframes
        keyframes = []
        
        for frame_idx in sorted(selected_frames):
            reasons = []
            
            if frame_idx == 0:
                reasons.append('Primer frame')
            elif frame_idx == n_frames - 1:
                reasons.append('Último frame')
            elif frame_idx in critical_frames:
                reasons.append('⚡ CRÍTICO: Cambio de dirección')
                reasons.append(f'Score: {direction_scores[frame_idx]:.3f}')
            else:
                reasons.append('Distribución uniforme')
                reasons.append(f'Score: {direction_scores[frame_idx]:.3f}')
            
            keyframes.append(KeyframeInfo(
                frame_id=frame_ids[frame_idx],
                frame_number=frame_idx,
                score=direction_scores[frame_idx],
                reasons=reasons
            ))
        
        return keyframes
    
    def save_keyframes(self, animation_data: Dict, keyframes: List[KeyframeInfo], 
                      output_path: str):
        """Guarda keyframes en formato original"""
        keyframe_data = {}
        
        for kf in keyframes:
            keyframe_data[kf.frame_id] = animation_data[kf.frame_id]
        
        with open(output_path, 'w') as f:
            json.dump(keyframe_data, f, indent=2)
        
        print(f"✓ Keyframes guardados: {output_path}")
    
    def save_report(self, keyframes: List[KeyframeInfo], output_path: str):
        """Guarda reporte detallado"""
        with open(output_path, 'w') as f:
            f.write("=" * 80 + "\n")
            f.write("REPORTE DE KEYFRAMES - DETECCIÓN HÍBRIDA\n")
            f.write("=" * 80 + "\n\n")
            
            f.write(f"Total de keyframes: {len(keyframes)}\n")
            f.write(f"Estrategia: Híbrida (Distribución + Cambios de Dirección)\n\n")
            
            # Contar tipos
            critical_count = len([kf for kf in keyframes if '⚡ CRÍTICO' in ' '.join(kf.reasons)])
            distributed_count = len([kf for kf in keyframes if 'Distribución' in ' '.join(kf.reasons)])
            
            f.write(f"Desglose:\n")
            f.write(f"  - Frames críticos (cambios de dirección): {critical_count}\n")
            f.write(f"  - Frames distribuidos: {distributed_count}\n")
            f.write(f"  - Frames fijos (inicio/final): 2\n\n")
            
            f.write("=" * 80 + "\n\n")
            
            for i, kf in enumerate(keyframes):
                f.write(f"Keyframe #{i+1}:\n")
                f.write(f"  Frame ID: {kf.frame_id}\n")
                f.write(f"  Frame Number: {kf.frame_number}\n")
                f.write(f"  Razones:\n")
                for reason in kf.reasons:
                    f.write(f"    - {reason}\n")
                f.write("\n")
        
        print(f"✓ Reporte guardado: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Detector HÍBRIDO de Keyframes (Distribución + Cambios de Dirección)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Ejemplos de uso:
  # Uso básico - CALCULA AUTOMÁTICAMENTE (15% de frames)
  python3 keyframe_detector_hybrid.py input.json output.json
  
  # Más keyframes automáticos (20% de frames)
  python3 keyframe_detector_hybrid.py input.json output.json --density 0.20
  
  # Menos keyframes automáticos (10% de frames)
  python3 keyframe_detector_hybrid.py input.json output.json --density 0.10
  
  # Especificar número exacto (ignora --density)
  python3 keyframe_detector_hybrid.py input.json output.json --num 25
  
  # Con reporte detallado
  python3 keyframe_detector_hybrid.py input.json output.json --report info.txt
        """
    )
    
    parser.add_argument('input', help='Archivo JSON de entrada')
    parser.add_argument('output', help='Archivo JSON de salida')
    
    parser.add_argument('--num', type=int, default=None,
                       help='Número de keyframes DISTRIBUIDOS (default: auto según --density). Los críticos se agregan además.')
    
    parser.add_argument('--density', type=float, default=0.15,
                       help='Densidad de keyframes DISTRIBUIDOS cuando es AUTO (0.1=10%%, 0.15=15%%, 0.2=20%%, default: 0.15)')
    
    parser.add_argument('--threshold', type=float, default=0.015,
                       help='Umbral para detectar cambios de dirección (default: 0.015)')
    
    parser.add_argument('--report', type=str, default=None,
                       help='Archivo para reporte detallado')
    
    parser.add_argument('--quiet', action='store_true',
                       help='Modo silencioso')
    
    args = parser.parse_args()
    
    if not args.quiet:
        print("🎬 Detector HÍBRIDO de Keyframes")
        print("=" * 60)
        print(f"\n📂 Entrada:  {args.input}")
        print(f"📁 Salida:   {args.output}")
        if args.num is not None:
            print(f"\n⚙️  Configuración:")
            print(f"   Keyframes distribuidos: {args.num} (manual)")
            print(f"   Keyframes críticos: TODOS (auto)")
            print(f"   Umbral cambios: {args.threshold}")
        else:
            print(f"\n⚙️  Configuración:")
            print(f"   Keyframes distribuidos: AUTO (densidad {args.density*100:.0f}%)")
            print(f"   Keyframes críticos: TODOS (auto)")
            print(f"   Umbral cambios: {args.threshold}")
    
    detector = HybridKeyframeDetector(
        direction_threshold=args.threshold
    )
    
    try:
        if not args.quiet:
            print("\n🔄 Cargando animación...")
        animation_data = detector.load_animation(args.input)
        
        n_frames = len(animation_data)
        
        # Calcular automáticamente el número de keyframes DISTRIBUIDOS
        if args.num is None:
            # Usar el porcentaje especificado en --density para DISTRIBUIDOS solamente
            # Los críticos se agregarán además de esto
            num_distributed = max(10, int(n_frames * args.density))
            
            if not args.quiet:
                print(f"   ✓ {n_frames} frames cargados")
                print(f"   ℹ️  Keyframes distribuidos calculados: {num_distributed} ({args.density*100:.0f}% de {n_frames})")
                print(f"   ℹ️  + Keyframes críticos (se detectarán todos los cambios de dirección)")
        else:
            # Si se especifica --num, se usa para distribuidos
            num_distributed = max(5, args.num - 2)  # -2 para inicio/final
            if not args.quiet:
                print(f"   ✓ {n_frames} frames cargados")
                print(f"   ℹ️  Keyframes distribuidos especificados: {num_distributed}")
                print(f"   ℹ️  + Keyframes críticos (se detectarán todos los cambios de dirección)")
        
        if not args.quiet:
            print("\n🔍 Detectando keyframes...")
        keyframes = detector.detect_keyframes_hybrid(
            animation_data,
            num_distributed=num_distributed
        )
        
        if not args.quiet:
            print(f"\n   ✓ {len(keyframes)} keyframes detectados")
            print(f"   ✓ Reducción: {100 * (1 - len(keyframes)/len(animation_data)):.1f}%")
        
        if not args.quiet:
            print("\n💾 Guardando resultados...")
        
        detector.save_keyframes(animation_data, keyframes, args.output)
        
        if args.report:
            detector.save_report(keyframes, args.report)
        
        if not args.quiet:
            # Mostrar distribución
            frames = [kf.frame_number for kf in keyframes]
            n_frames = len(animation_data)
            
            print("\n📊 Distribución de keyframes:")
            print("-" * 60)
            
            q1 = n_frames // 4
            q2 = n_frames // 2
            q3 = 3 * n_frames // 4
            
            count_q1 = len([f for f in frames if f < q1])
            count_q2 = len([f for f in frames if q1 <= f < q2])
            count_q3 = len([f for f in frames if q2 <= f < q3])
            count_q4 = len([f for f in frames if f >= q3])
            
            print(f"  Frames 0-{q1-1}:      {count_q1} keyframes")
            print(f"  Frames {q1}-{q2-1}:    {count_q2} keyframes")
            print(f"  Frames {q2}-{q3-1}:    {count_q3} keyframes")
            print(f"  Frames {q3}-{n_frames-1}:  {count_q4} keyframes")
            
            print("\n📋 Lista de keyframes:")
            for kf in keyframes:
                marker = "⚡" if "⚡ CRÍTICO" in ' '.join(kf.reasons) else "  "
                print(f"  {marker} {kf.frame_id:12s} | {kf.reasons[0]}")
            
            print("\n✅ ¡Completado!\n")
        else:
            print(f"✓ {len(keyframes)} keyframes guardados")
    
    except Exception as e:
        print(f"❌ Error: {str(e)}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()
