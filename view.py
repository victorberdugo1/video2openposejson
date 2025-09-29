

import json
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.widgets import Button
import tkinter as tk
from tkinter import filedialog, messagebox
import os
import sys

class SkeletonViewer:
    def __init__(self):
        self.data = None
        self.frames = []
        self.ani = None
        self.play_pause = None
        self.fig = None
        self.ax = None
        self.play_button = None
        self.reset_button = None
        
        # Conexiones esqueléticas
        self.connections = [
            ("Neck", "RShoulder"), ("Neck", "LShoulder"),
            ("RShoulder", "RElbow"), ("RElbow", "RWrist"),
            ("LShoulder", "LElbow"), ("LElbow", "LWrist"),
            ("Neck", "RHip"), ("Neck", "LHip"),
            ("RHip", "RKnee"), ("RKnee", "RAnkle"),
            ("LHip", "LKnee"), ("LKnee", "LAnkle"),
            ("Neck", "Nose"), ("Nose", "REye"), ("Nose", "LEye"),
            ("REye", "REar"), ("LEye", "LEar")
        ]
        
        # Intentar cargar archivo por defecto
        if not self.load_default_file():
            self.show_file_selector()
        else:
            self.setup_visualization()
    
    def load_default_file(self):
        """Intenta cargar el archivo por defecto"""
        try:
            with open("3d_combined_data.json") as f:
                self.data = json.load(f)
            return True
        except FileNotFoundError:
            return False
        except Exception as e:
            messagebox.showerror("Error", f"Error al cargar archivo por defecto: {e}")
            return False
    
    def show_file_selector(self):
        """Muestra ventana simple para seleccionar archivo"""
        root = tk.Tk()
        root.title("Seleccionar archivo JSON")
        root.geometry("500x250")
        root.configure(bg='lightgray')
        
        # Centrar la ventana
        root.eval('tk::PlaceWindow . center')
        
        # Etiqueta informativa
        info_label = tk.Label(
            root, 
            text="No se encontró 3d_combined_data.json\nSelecciona un archivo JSON:",
            font=("Arial", 12),
            bg='lightgray',
            fg='darkblue'
        )
        info_label.pack(pady=20)
        
        # Botón para abrir archivo
        def select_file():
            file_path = filedialog.askopenfilename(
                title="Seleccionar archivo JSON",
                filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
            )
            if file_path and self.load_json_file(file_path):
                root.destroy()
                self.setup_visualization()
        
        select_button = tk.Button(
            root, 
            text="Seleccionar archivo JSON", 
            command=select_file,
            font=("Arial", 12),
            bg='lightblue',
            width=20
        )
        select_button.pack(pady=10)
        
        # Botón para salir
        def exit_app():
            root.destroy()
            sys.exit()
        
        exit_button = tk.Button(
            root, 
            text="Salir", 
            command=exit_app,
            font=("Arial", 12),
            bg='lightcoral',
            width=10
        )
        exit_button.pack(pady=5)
        
        root.mainloop()
    
    def load_json_file(self, file_path):
        """Carga un archivo JSON"""
        try:
            with open(file_path, 'r') as f:
                self.data = json.load(f)
            
            # Extraer frames
            self.frames = [self.data[k] for k in sorted(self.data) if k.startswith("frame_")]
            
            if not self.frames:
                messagebox.showerror("Error", "No se encontraron frames en el archivo JSON")
                return False
            
            return True
        except Exception as e:
            messagebox.showerror("Error", f"Error al cargar archivo: {e}")
            return False
    
    def setup_visualization(self):
        """Configura la visualización 3D"""
        # Extraer frames si no se ha hecho ya
        if not self.frames:
            self.frames = [self.data[k] for k in sorted(self.data) if k.startswith("frame_")]
        
        # Calcular límites globales usando ejes corregidos y X invertido
        all_x, all_y, all_z = [], [], []
        for frame in self.frames:
            for person in frame.values():
                for p in person.values():
                    all_x.append(-p["x"])  # invertir para visualización coherente
                    all_y.append(p["z"])   # profundidad
                    all_z.append(p["y"])   # altura
        
        xlim = (min(all_x), max(all_x))
        ylim = (min(all_y), max(all_y))
        zlim = (min(all_z), max(all_z))
        
        # Crear figura
        self.fig = plt.figure(figsize=(10, 8))
        self.ax = self.fig.add_subplot(111, projection="3d")
        plt.subplots_adjust(bottom=0.2)
        
        # Ajustar límites para proporciones reales
        max_range = max(xlim[1]-xlim[0], ylim[1]-ylim[0], zlim[1]-zlim[0]) / 2.0
        mid_x = (xlim[0] + xlim[1]) / 2
        mid_y = (ylim[0] + ylim[1]) / 2
        mid_z = (zlim[0] + zlim[1]) / 2
        
        self.max_range = max_range
        self.mid_x, self.mid_y, self.mid_z = mid_x, mid_y, mid_z
        
        # Configurar animación
        self.ani = FuncAnimation(
            self.fig, 
            self.update, 
            frames=len(self.frames), 
            init_func=self.init,
            interval=100, 
            blit=False
        )
        
        # Configurar botones
        self.setup_buttons()
        
        plt.show()
    
    def set_equal_aspect(self):
        """Establece proporciones iguales en los ejes"""
        self.ax.set_xlim(self.mid_x - self.max_range, self.mid_x + self.max_range)
        self.ax.set_ylim(self.mid_y - self.max_range, self.mid_y + self.max_range)
        self.ax.set_zlim(self.mid_z - self.max_range, self.mid_z + self.max_range)
        self.ax.set_box_aspect([1, 1, 1])
    
    def init(self):
        """Inicializa la visualización"""
        self.ax.clear()
        self.set_equal_aspect()
        self.ax.set_xlabel("X axis")
        self.ax.set_ylabel("Y axis")
        self.ax.set_zlabel("Z axis")
        self.ax.view_init(elev=15, azim=70)  # vista frontal
        return []
    
    def update(self, frame_num):
        """Actualiza la visualización para cada frame"""
        self.ax.clear()
        self.set_equal_aspect()
        self.ax.set_xlabel("X")
        self.ax.set_ylabel("Profundidad (Z JSON)")
        self.ax.set_zlabel("Altura (Y JSON)")
        
        frame = self.frames[frame_num]
        for person_id, keypoints in frame.items():
            offset = 0.05
            # Nariz (para etiqueta del ID)
            p4 = keypoints["Nose"]
            self.ax.text(-p4["x"], p4["z"], p4["y"] - offset, person_id,
                        fontsize=10, color='blue', weight='bold', ha='center')
            
            # Dibujar puntos y etiquetas de keypoints
            for key, p in keypoints.items():
                self.ax.scatter(-p["x"], p["z"], p["y"], s=30)
                self.ax.text(-p["x"], p["z"], p["y"], f"{key}", size=7)
            
            # Dibujar líneas del esqueleto
            for start, end in self.connections:
                if start in keypoints and end in keypoints:
                    p1, p2 = keypoints[start], keypoints[end]
                    self.ax.plot([-p1["x"], -p2["x"]],
                               [p1["z"], p2["z"]],
                               [p1["y"], p2["y"]], 'r-')
        
        self.ax.set_title(f"Frame: {frame_num+1}/{len(self.frames)}")
        self.ax.invert_zaxis()
        return []
    
    def setup_buttons(self):
        """Configura los botones de control"""
        # Clase para Play/Pause
        class PlayPause:
            def __init__(self, viewer):
                self.viewer = viewer
                self.paused = False
            
            def __call__(self, event):
                if self.paused:
                    self.viewer.ani.event_source.start()
                    self.viewer.play_button.label.set_text("Pausar")
                else:
                    self.viewer.ani.event_source.stop()
                    self.viewer.play_button.label.set_text("Play")
                self.paused = not self.paused
        
        # Botones
        ax_play = plt.axes([-0.5, 0.05, 0.5, 0.075])
        ax_reset = plt.axes([-0.5, 0.05, 0.1, 0.075])
        self.play_button = Button(ax_play, "Pausar")
        self.reset_button = Button(ax_reset, "Reiniciar")
        
        self.play_pause = PlayPause(self)
        self.play_button.on_clicked(self.play_pause)
        
        def reset(event):
            self.ani.event_source.stop()
            self.ani.frame_seq = self.ani.new_frame_seq()
            self.update(0)
            self.play_button.label.set_text("Pausar")
            self.play_pause.paused = False
        
        self.reset_button.on_clicked(reset)

# Ejecutar aplicación
if __name__ == "__main__":
    viewer = SkeletonViewer()