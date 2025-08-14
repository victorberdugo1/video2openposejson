PROJECT video2openpose json
===========

This project uses Python to process videos and extract keypoints.

REQUIREMENTS
------------
- Python 3.10 or higher
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
  diffusers==0.14.0
  opencv-python
  ffmpeg-python
  moviepy
  controlnet_aux
  mediapipe

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
  - all_keypoints.json file with the two detected people separately

This will create 3d_combined_data.json, containing the merged 3D skeleton.

To visualize it, run the keypoints visualizer:
```
python view.py
```
DEACTIVATE THE ENVIRONMENT
--------------------------
```
deactivate
```
NOTES
-----
- Make sure Python is up-to-date.
- On Linux/macOS you might need to install ffmpeg via your system:
    sudo apt install ffmpeg
