"""
Detector de Keyframes

python keyframe_detector.py normalizer.json keyframes.json

# Más keyframes (20% en vez de 15%)
python keyframe_detector.py input.json keyframes.json --density 0.20

# Menos keyframes (10% en vez de 15%)
python keyframe_detector.py input.json keyframes.json --density 0.10

# Número exacto (ignora AUTO)
python keyframe_detector.py input.json keyframes.json --num 30

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

    def __init__(self, direction_threshold: float = 0.015):
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
        for fid in frame_ids:
            frame_data = animation_data[fid]['person_0']
            positions.append([[frame_data[j]['x'], frame_data[j]['y'], frame_data[j]['z']]
                               for j in joints])
        return np.array(positions), frame_ids, joints

    def _motion_scores(self, positions: np.ndarray, joints: List[str]) -> np.ndarray:
        """
        Per-frame importance score combining:
        - Direction change (angle between consecutive velocity vectors)
        - Acceleration magnitude (velocity magnitude delta)
        Both weighted equally after normalization per joint.
        """
        n = len(positions)
        scores = np.zeros(n)

        for ji, jname in enumerate(joints):
            if jname not in self.important_joints:
                continue

            vel = np.zeros((n, 3))
            vel[1:] = positions[1:, ji] - positions[:-1, ji]
            vmag = np.linalg.norm(vel, axis=1)

            # Acceleration
            acc = np.zeros(n)
            acc[1:] = np.abs(vmag[1:] - vmag[:-1])

            # Direction change angle
            angle = np.zeros(n)
            for i in range(2, n):
                n1, n2 = np.linalg.norm(vel[i - 1]), np.linalg.norm(vel[i])
                if n1 > 0.001 and n2 > 0.001:
                    cos_a = np.clip(np.dot(vel[i - 1], vel[i]) / (n1 * n2), -1.0, 1.0)
                    angle[i] = np.arccos(cos_a)

            scores += angle + acc * 2.0

        return scores

    def _detect_critical(self, scores: np.ndarray, fixed: Set[int]) -> Set[int]:
        """Local maxima above threshold, excluding fixed frames."""
        norm = scores / scores.max() if scores.max() > 0 else scores
        critical = set()
        w = 5
        for i in range(w, len(norm) - w):
            if i in fixed:
                continue
            if norm[i] > self.direction_threshold:
                if all(norm[i] >= norm[i + j] for j in range(-w, w + 1) if j != 0):
                    critical.add(i)
        return critical

    def _distribute_uniform(self, n_frames: int, taken: Set[int],
                            count: int, scores: np.ndarray) -> Set[int]:
        """
        Divide [0, n_frames-1] into `count` equal segments and pick the
        highest-score available frame inside each segment.
        Uses segment midpoint as fallback if all candidates are taken.
        """
        result = set()
        cuts = np.linspace(0, n_frames - 1, count + 2)

        for seg in range(count):
            lo = int(round(cuts[seg]))
            hi = int(round(cuts[seg + 1]))
            candidates = [i for i in range(lo + 1, hi) if i not in taken]
            if candidates:
                result.add(max(candidates, key=lambda i: scores[i]))
        return result

    def detect_keyframes(self, animation_data: Dict,
                         num_distributed: int = 15) -> List[KeyframeInfo]:
        positions, frame_ids, joints = self.extract_positions(animation_data)
        n = len(positions)

        fixed: Set[int] = {0, n - 1}
        scores = self._motion_scores(positions, joints)
        critical = self._detect_critical(scores, fixed)
        distributed = self._distribute_uniform(n, fixed | critical, num_distributed, scores)

        selected = fixed | critical | distributed

        # Normalize scores for reporting
        norm = scores / scores.max() if scores.max() > 0 else scores

        keyframes = []
        for idx in sorted(selected):
            if idx == 0:
                reason = 'First frame (fixed)'
            elif idx == n - 1:
                reason = 'Last frame (fixed)'
            elif idx in critical:
                reason = f'Critical — direction change (score {norm[idx]:.3f})'
            else:
                reason = f'Uniform distribution (score {norm[idx]:.3f})'

            keyframes.append(KeyframeInfo(
                frame_id=frame_ids[idx],
                frame_number=idx,
                score=norm[idx],
                reasons=[reason]
            ))

        return keyframes

    def save_keyframes(self, animation_data: Dict, keyframes: List[KeyframeInfo],
                       output_path: str):
        out = {kf.frame_id: animation_data[kf.frame_id] for kf in keyframes}
        with open(output_path, 'w') as f:
            json.dump(out, f, indent=2)

    def save_report(self, keyframes: List[KeyframeInfo], total_frames: int,
                    output_path: str):
        n_crit = sum(1 for kf in keyframes if 'Critical' in kf.reasons[0])
        n_dist = sum(1 for kf in keyframes if 'Uniform'  in kf.reasons[0])
        frames = [kf.frame_number for kf in keyframes]
        gaps = [frames[i + 1] - frames[i] for i in range(len(frames) - 1)]

        with open(output_path, 'w') as f:
            f.write("KEYFRAME DETECTION REPORT\n")
            f.write("=" * 60 + "\n\n")
            f.write(f"Total frames (input):  {total_frames}\n")
            f.write(f"Total keyframes:       {len(keyframes)}\n")
            f.write(f"Reduction:             {100*(1 - len(keyframes)/total_frames):.1f}%\n\n")
            f.write(f"Fixed (first/last):    2\n")
            f.write(f"Critical:              {n_crit}\n")
            f.write(f"Distributed:           {n_dist}\n\n")
            if gaps:
                f.write(f"Max gap:               {max(gaps)} frames\n")
                f.write(f"Avg gap:               {sum(gaps)/len(gaps):.1f} frames\n\n")
            f.write("=" * 60 + "\n\n")
            for i, kf in enumerate(keyframes):
                f.write(f"#{i+1:03d}  {kf.frame_id}  (frame {kf.frame_number:4d})  {kf.reasons[0]}\n")

        print(f"Report saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Hybrid Keyframe Detector',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('input')
    parser.add_argument('output')
    parser.add_argument('--num',       type=int,   default=None,
                        help='Number of distributed keyframes (overrides --density)')
    parser.add_argument('--density',   type=float, default=0.15,
                        help='Distributed keyframe density (default: 0.15)')
    parser.add_argument('--threshold', type=float, default=0.015,
                        help='Direction-change threshold (default: 0.015)')
    parser.add_argument('--report',    type=str,   default=None)
    parser.add_argument('--quiet',     action='store_true')
    args = parser.parse_args()

    detector = HybridKeyframeDetector(direction_threshold=args.threshold)

    try:
        animation_data = detector.load_animation(args.input)
        n_frames = len(animation_data)

        num_distributed = (max(5, args.num - 2) if args.num is not None
                           else max(10, int(n_frames * args.density)))

        keyframes = detector.detect_keyframes(animation_data, num_distributed)

        detector.save_keyframes(animation_data, keyframes, args.output)

        if args.report:
            detector.save_report(keyframes, n_frames, args.report)

        if not args.quiet:
            n_crit = sum(1 for kf in keyframes if 'Critical' in kf.reasons[0])
            frames = [kf.frame_number for kf in keyframes]
            gaps = [frames[i+1] - frames[i] for i in range(len(frames)-1)]
            print(f"{len(keyframes)} keyframes  "
                  f"({n_crit} critical + {len(keyframes)-n_crit-2} distributed + 2 fixed)  "
                  f"— {100*(1-len(keyframes)/n_frames):.1f}% reduction  "
                  f"— max gap {max(gaps)} frames")

    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()
