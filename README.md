PROJECT video2openposejson + RayPuppet3D
===========


<p align="center">
<div style="display: flex; justify-content: center; gap: 20px;">
  <video src="https://github.com/user-attachments/assets/20879ed8-2fce-4060-957a-5d7e2de1ff65"
         width="480"
         autoplay
         loop
         muted
         playsinline></video>
  <video src="https://github.com/user-attachments/assets/c151cf15-85b6-429e-bd41-7a18e9b8ba23"
         width="480"
         autoplay
         loop
         muted
         playsinline></video>
</div>
</p>

This project uses Python to process videos and extract keypoints.

REQUIREMENTS
------------
- Python 3.11 recommended (mediapipe does not support Python 3.12 yet)
- pip


CREATE VIRTUAL ENVIRONMENT
--------------------------

### Windows (PowerShell or CMD):
  1. Create the environment:
```
python -m venv env
```
  2. Activate the environment:

PowerShell:
```
.\env\Scripts\Activate.ps1
```
  CMD:
```
.\env\Scripts\activate.bat
```

### Linux / macOS:
  1. Create the environment:
```
python3 -m venv env
```
  2. Activate the environment:
```
source env/bin/activate
```

INSTALL REQUIREMENTS
--------------------
With the virtual environment activated:
```
python -m pip install --upgrade pip
```
```
pip install -r requirements.txt
```
REQUIREMENTS IN requirements.txt:
```
urllib3>=2.0.0
requests>=2.31.0
matplotlib>=3.8.0
mediapipe==0.10.31
opencv-python==4.8.1.78
moviepy==2.2.1
imageio==2.36.1
imageio-ffmpeg==0.5.1
gradio==5.29.1
numpy==1.26.4
Pillow==10.4.0
```

> **Note:** MediaPipe models (pose, hand, face) are downloaded automatically into the
> `models/` folder on first run. No manual download needed.

RUN THE PROJECT
---------------
Once the environment is activated and packages installed:
```
python appjson2.py
```
This will open a local interface accessible at:
http://127.0.0.1:7860/

- Upload a **video** or **GIF** recorded with two cameras: one side view and one front view
- Press **Procesar** to generate:
  - OpenPose-processed video
  - Individual frames
  - `all_keypoints.json` file with the two detected people separately

This will also create `3d_combined_data.json`, containing the merged 3D skeleton.

To visualize it, run the keypoints visualizer:
```
python view.py
```

For single-person detection (body + hands + face), run:
```
python appBody25.py
```
This will open a local interface at http://127.0.0.1:7860/ and generate:
- OpenPose-style visualization on black background (body: blue, hands: green, face: red)
- JSON with 33 pose points + 42 hand points + 468 face points per frame

DEACTIVATE THE ENVIRONMENT
--------------------------
```
deactivate
```