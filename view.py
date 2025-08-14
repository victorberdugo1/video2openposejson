import json
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Button

# Cargar datos
with open("3d_combined_data.json") as f:
    data = json.load(f)

# Extraer frames
frames = [data[k] for k in sorted(data) if k.startswith("frame_")]

# Calcular límites globales usando ejes corregidos y X invertido
all_x, all_y, all_z = [], [], []
for frame in frames:
    for person in frame.values():
        for p in person.values():
            all_x.append(-p["x"])  # invertir para visualización coherente
            all_y.append(p["z"])   # profundidad
            all_z.append(p["y"])   # altura
xlim = (min(all_x), max(all_x))
ylim = (min(all_y), max(all_y))
zlim = (min(all_z), max(all_z))

# Conexiones esqueléticas
connections = [
    ("Neck", "RShoulder"), ("Neck", "LShoulder"),
    ("RShoulder", "RElbow"), ("RElbow", "RWrist"),
    ("LShoulder", "LElbow"), ("LElbow", "LWrist"),
    ("Neck", "RHip"), ("Neck", "LHip"),
    ("RHip", "RKnee"), ("RKnee", "RAnkle"),
    ("LHip", "LKnee"), ("LKnee", "LAnkle"),
    ("Neck", "Nose"), ("Nose", "REye"), ("Nose", "LEye"),
    ("REye", "REar"), ("LEye", "LEar")
]

# Crear figura
fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(111, projection="3d")
plt.subplots_adjust(bottom=0.2)

# Ajustar límites para proporciones reales
max_range = max(xlim[1]-xlim[0], ylim[1]-ylim[0], zlim[1]-zlim[0]) / 2.0
mid_x = (xlim[0] + xlim[1]) / 2
mid_y = (ylim[0] + ylim[1]) / 2
mid_z = (zlim[0] + zlim[1]) / 2

def set_equal_aspect(ax):
    ax.set_xlim(mid_x - max_range, mid_x + max_range)
    ax.set_ylim(mid_y - max_range, mid_y + max_range)
    ax.set_zlim(mid_z - max_range, mid_z + max_range)
    ax.set_box_aspect([1, 1, 1])

def init():
    ax.clear()
    set_equal_aspect(ax)
    ax.set_xlabel("X axis")
    ax.set_ylabel("Y axis")
    ax.set_zlabel("Z axis")
    ax.view_init(elev=15, azim=70)  # vista frontal
    return []

def update(frame_num):
    ax.clear()
    set_equal_aspect(ax)
    ax.set_xlabel("X")
    ax.set_ylabel("Profundidad (Z JSON)")
    ax.set_zlabel("Altura (Y JSON)")

    frame = frames[frame_num]
    for person_id, keypoints in frame.items():
        offset = 0.05
        # Nariz (para etiqueta del ID)
        p4 = keypoints["Nose"]
        ax.text(-p4["x"], p4["z"], p4["y"] - offset, person_id,
                fontsize=10, color='blue', weight='bold', ha='center')

        # Dibujar puntos y etiquetas de keypoints
        for key, p in keypoints.items():
            ax.scatter(-p["x"], p["z"], p["y"], s=30)
            ax.text(-p["x"], p["z"], p["y"], f"{key}", size=7)

        # Dibujar líneas del esqueleto
        for start, end in connections:
            if start in keypoints and end in keypoints:
                p1, p2 = keypoints[start], keypoints[end]
                ax.plot([-p1["x"], -p2["x"]],
                        [p1["z"], p2["z"]],
                        [p1["y"], p2["y"]], 'r-')

    ax.set_title(f"Frame: {frame_num+1}/{len(frames)}")
    ax.invert_zaxis()
    return []

ani = FuncAnimation(fig, update, frames=len(frames), init_func=init,
                    interval=100, blit=False)

# Clase para Play/Pause
class PlayPause:
    def __init__(self, anim):
        self.anim = anim
        self.paused = False
    def __call__(self, event):
        if self.paused:
            self.anim.event_source.start()
            play_button.label.set_text("Pausar")
        else:
            self.anim.event_source.stop()
            play_button.label.set_text("Play")
        self.paused = not self.paused

# Botones
ax_play = plt.axes([0.3, 0.05, 0.1, 0.075])
ax_reset = plt.axes([0.5, 0.05, 0.1, 0.075])
play_button = Button(ax_play, "Pausar")
reset_button = Button(ax_reset, "Reiniciar")
play_pause = PlayPause(ani)
play_button.on_clicked(play_pause)

def reset(event):
    ani.event_source.stop()
    ani.frame_seq = ani.new_frame_seq()
    update(0)
    play_button.label.set_text("Pausar")
    play_pause.paused = False

reset_button.on_clicked(reset)

plt.show()
