#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
//#include "bones.h"
//#include "gui.h"

// -----------------------------------------------------------
// Prototipos de funciones (sin implementar real)
// -----------------------------------------------------------
// Bone* boneLoadStructure(const char* filename);
// void meshLoadData(const char* filename, t_mesh* mesh, Bone* root);
// void LoadTextures(t_mesh* mesh);
// void animationLoadKeyframes(const char* filename, Bone* root);
// void meshDraw(t_mesh* mesh, Bone* root, int frameNum);
// void DrawBones(Bone* root, bool drawBones);
// void InitializeGUI(void);
// void UpdateGUI(void);
// bool UpdateBoneProperties(Bone* bone, int frameNum);
// void DrawGUI(t_mesh* mesh);
// void mouseAnimate(Bone* bone, int frameNum);
// void DrawOnTop(Bone* bone, t_mesh* mesh, int frameNum);
// void boneFreeTree(Bone* root);

// src/main.c
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "bonetile.h"   


static Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow,
                                int physCols, int physRows,
                                bool mirrored, bool *outMirrored)
{
    if (logicalCol < 0) logicalCol = 0;
    if (logicalCol >= ATLAS_COLS) logicalCol = ATLAS_COLS - 1;
    if (logicalRow < 0) logicalRow = 0;
    if (logicalRow >= ATLAS_ROWS) logicalRow = ATLAS_ROWS - 1;

    float physCellW = (float)tex.width  / (float)physCols;
    float physCellH = (float)tex.height / (float)physRows;

    int blockW = physCols / ATLAS_COLS;
    int blockH = physRows / ATLAS_ROWS;

    int physCol = logicalCol * blockW;

    int physRow = logicalRow * blockH;

    float srcX = physCol * physCellW;
    float srcY = physRow * physCellH;
    float srcW = physCellW * blockW;
    float srcH = physCellH * blockH;

    if (outMirrored) *outMirrored = mirrored;
    return (Rectangle){ srcX, srcY, srcW, srcH };
}

int main(void) {
    const int screenW = 2056;
    const int screenH = 1504;
    InitWindow(screenW, screenH, "Bonetiles");
    SetTargetFPS(60);

    Camera camera = {0};
    camera.position = (Vector3){0.0f, 0.6f, 2.5f};
    camera.target   = (Vector3){0.0f, 0.6f, 0.0f};
    camera.up       = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy     = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int camMode = 1;
    float orbitYaw = 0.0f, orbitPitch = -0.2f, orbitRadius = 2.5f;

    // Hardcode: posiciones Neck y Nose normalizadas (JSON)
    Vector3 neckPos = {
        0.753111869096756f * 2.0f - 1.0f,
        1.0f - 0.26760193705558777f,
        0.210185244679451f * 2.0f - 1.0f
    };
    Vector3 nosePos = {
        0.7485565543174744f * 2.0f - 1.0f,
        1.0f - 0.18256297707557678f,
        0.25478237867355347f * 2.0f - 1.0f
    };

    float bonetileSize = 0.35f;

    // Texturas (placeholders si no existen)
    Texture2D texA = {0}, texB = {0};
    Image imgA = LoadImage("texA.png");
    if (imgA.data == NULL) {
        imgA = GenImageColor(1024, 1024, CLITERAL(Color){60,120,220,255});
        ImageDrawText(&imgA, "A", 8, 8, 128, BLACK);
    }
    texA = LoadTextureFromImage(imgA); UnloadImage(imgA);
    SetTextureFilter(texA, TEXTURE_FILTER_POINT);
    SetTextureWrap(texA, TEXTURE_WRAP_CLAMP);

    Image imgB = LoadImage("texB.png");
    if (imgB.data == NULL) {
        imgB = GenImageColor(1024, 1024, CLITERAL(Color){120,200,80,255});
        ImageDrawText(&imgB, "B", 8, 8, 128, BLACK);
    }
    texB = LoadTextureFromImage(imgB); UnloadImage(imgB);
    SetTextureFilter(texB, TEXTURE_FILTER_POINT);
    SetTextureWrap(texB, TEXTURE_WRAP_CLAMP);

    const int physColsA = 8, physRowsA = 8;
    const int physColsB = 8, physRowsB = 8;


    const int indices[3][8] = {
		{  0,  4,  5,  6,  7,  6,  5,  4 },
        {  2, 12, 13, 14, 15, 14, 13, 12 },
        {  1,  8,  9, 10, 11, 10,  9,  8 }
    };
    const int topdownIndex = 3;

    Rectangle srcNeck = {0}, srcNose = {0};
    float rotNeck = 0.0f, rotNose = 0.0f;
    bool finalMirrNeck = false, finalMirrNose = false;
    const float sectorAngles[8] = {0,45,90,135,180,225,270,315};

    const float TOPDOWN_ANGLE = 70.0f;
    const float HIGH_THRESHOLD = 22.5f;
    const float MAIN_THRESHOLD = -22.5f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_ONE)) camMode = 1;
        if (IsKeyPressed(KEY_TWO)) camMode = 2;

        // Camera control (orbit around neck in mode 1)
        if (camMode == 1) {
            Vector2 md = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                orbitYaw   += md.x * 0.01f;
                orbitPitch += -md.y * 0.01f;
                orbitPitch = Clamp(orbitPitch, -1.4f, 1.4f);
            }
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                orbitRadius -= wheel * 0.5f;
                orbitRadius = Clamp(orbitRadius, 0.5f, 10.0f);
            }
            Vector3 target = neckPos;
            float x = orbitRadius * cosf(orbitPitch) * sinf(orbitYaw);
            float y = orbitRadius * sinf(orbitPitch);
            float z = orbitRadius * cosf(orbitPitch) * cosf(orbitYaw);
            camera.position = (Vector3){ target.x + x, target.y + y, target.z + z };
            camera.target = target;
        } else {
            Vector2 md = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                orbitYaw += md.x * 0.002f;
                orbitPitch += -md.y * 0.002f;
                orbitPitch = Clamp(orbitPitch, -1.49f, 1.49f);
            }
            Vector3 forwardDir = { sinf(orbitYaw)*cosf(orbitPitch), sinf(orbitPitch), cosf(orbitYaw)*cosf(orbitPitch) };
            forwardDir = Vector3Normalize(forwardDir);
            Vector3 rightDir = Vector3Normalize(Vector3CrossProduct((Vector3){0,1,0}, forwardDir));
            float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 6.0f : 2.5f;
            if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(forwardDir, speed*dt));
            if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(forwardDir, speed*dt));
            if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(rightDir, speed*dt));
            if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(rightDir, speed*dt));
            camera.target = Vector3Add(camera.position, forwardDir);
        }

        // --- NECK: calcular yaw/pitch ---
        Vector3 camDirN = Vector3Subtract(camera.position, neckPos);
        float yawN = atan2f(camDirN.x, camDirN.z);
        if (yawN < 0.0f) yawN += 2.0f*PI;
        float yawDegN = yawN * RAD2DEG;

        float horizN = sqrtf(camDirN.x*camDirN.x + camDirN.z*camDirN.z);
        float pitchN = atan2f(camDirN.y, horizN);
        float pitchDegN = pitchN * RAD2DEG;

        int chosenRowN = -1;
        bool useTopdownN = false;
        if (pitchDegN >= TOPDOWN_ANGLE) useTopdownN = true;
        else if (pitchDegN >= HIGH_THRESHOLD) chosenRowN = 2;
        else if (pitchDegN >= MAIN_THRESHOLD) chosenRowN = 0;
        else if (pitchDegN >= -TOPDOWN_ANGLE) chosenRowN = 1;
        else useTopdownN = true;

        int sectorN = 0;
        float minDiffN = 360.0f;
        for (int i = 0; i < 8; i++) {
            float diff = fabsf(yawDegN - sectorAngles[i]);
            if (diff > 180.0f) diff = 360.0f - diff;
            if (diff < minDiffN) { minDiffN = diff; sectorN = i; }
        }

        int chosenIndexN;
        if (useTopdownN) {
            chosenIndexN = topdownIndex;
            rotNeck = sectorAngles[sectorN];
            finalMirrNeck = false;
        } else {
            chosenIndexN = indices[chosenRowN][sectorN];
            rotNeck = 0.0f;
            finalMirrNeck = !(sectorN >= 5 && sectorN <= 7);
        }

        int logicalColNeck = chosenIndexN % ATLAS_COLS;
        int logicalRowNeck = chosenIndexN / ATLAS_COLS;
        srcNeck = SrcFromLogical(texA, logicalColNeck, logicalRowNeck, physColsA, physRowsA, finalMirrNeck, &finalMirrNeck);

Vector3 camDirO = Vector3Subtract(camera.position, nosePos);
float yawO = atan2f(camDirO.x, camDirO.z);
if (yawO < 0.0f) yawO += 2.0f*PI;
float yawDegO = yawO * RAD2DEG;

float horizO = sqrtf(camDirO.x*camDirO.x + camDirO.z*camDirO.z);
float pitchO = atan2f(camDirO.y, horizO);
float pitchDegO = pitchO * RAD2DEG;

int chosenRowO = -1;
bool useTopdownO = false;
bool isTopViewO = false;

if (pitchDegO >= TOPDOWN_ANGLE) {
    useTopdownO = true;
    isTopViewO = true;
} else if (pitchDegO >= HIGH_THRESHOLD) {
    chosenRowO = 2;
} else if (pitchDegO >= MAIN_THRESHOLD) {
    chosenRowO = 0;
} else if (pitchDegO >= -TOPDOWN_ANGLE) {
    chosenRowO = 1;
} else {
    useTopdownO = true;
    isTopViewO = false;
}

int sectorO = 0;
float minDiffO = 360.0f;
for (int i = 0; i < 8; i++) {
    float diff = fabsf(yawDegO - sectorAngles[i]);
    if (diff > 180.0f) diff = 360.0f - diff;
    if (diff < minDiffO) { minDiffO = diff; sectorO = i; }
}

int chosenIndexO;
if (useTopdownO) {
    chosenIndexO = topdownIndex;
    
    if (isTopViewO) {
        rotNose = sectorAngles[sectorO] + 180.0f;
        finalMirrNose = false;
    } else {
        rotNose = 360.0f - sectorAngles[sectorO];
        finalMirrNose = true;
    }
} else {
    chosenIndexO = indices[chosenRowO][sectorO];
    rotNose = 0.0f;
    finalMirrNose = !(sectorO >= 5 && sectorO <= 7);
}

int logicalColNose = chosenIndexO % ATLAS_COLS;
        int logicalRowNose = chosenIndexO / ATLAS_COLS;
        srcNose = SrcFromLogical(texB, logicalColNose, logicalRowNose, physColsB, physRowsB, finalMirrNose, &finalMirrNose);

        // Tamaño en mundo (basado en celda lógica de referencia texA)
        float physCellW = (float)texA.width / (float)physColsA;
        float physCellH = (float)texA.height / (float)physRowsA;
        float logicalCellW = physCellW * (physColsA / ATLAS_COLS);
        float logicalCellH = physCellH * (physRowsA / ATLAS_ROWS);
        float aspect = logicalCellW / logicalCellH;
        Vector2 worldSize = (Vector2){ bonetileSize * aspect, bonetileSize };

        // Dibujado
        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(camera);
                DrawGrid(24, 0.5f);
                DrawSphereWires(neckPos, 0.035f, 8,8, RED);
                DrawSphereWires(nosePos, 0.035f, 8,8, BLUE);

                float dNeck = Vector3Distance(camera.position, neckPos);
                float dNose = Vector3Distance(camera.position, nosePos);

                rlDisableDepthTest();
                BeginBlendMode(BLEND_ALPHA);
                if (dNeck > dNose) {
                    DrawBonetileCustom(texA, camera, srcNeck, neckPos, worldSize, rotNeck, finalMirrNeck);
                    DrawBonetileCustom(texB, camera, srcNose, nosePos, worldSize, rotNose, finalMirrNose);
                } else {
                    DrawBonetileCustom(texB, camera, srcNose, nosePos, worldSize, rotNose, finalMirrNose);
                    DrawBonetileCustom(texA, camera, srcNeck, neckPos, worldSize, rotNeck, finalMirrNeck);
                }
                EndBlendMode();
                rlEnableDepthMask();
            EndMode3D();

            // Overlay 4x4 con etiquetas y resaltado
            const int gridCols = ATLAS_COLS, gridRows = ATLAS_ROWS;
            const int cellW = 220, cellH = 28, margin = 6;
            const int startX = screenW - (cellW + 20);
            const int startY = 20;
            DrawText("Atlas logical 4x4", startX, startY - 20, 20, DARKGRAY);

            for (int r = 0; r < gridRows; r++) {
                for (int c = 0; c < gridCols; c++) {
                    int idx = r * gridCols + c;
                    int xcell = startX + c * ((cellW / gridCols) + margin);
                    int ycell = startY + r * (cellH + margin);
                    Rectangle rcell = { (float)xcell, (float)ycell, (float)(cellW / gridCols - 6), (float)cellH };
                    Color bg = LIGHTGRAY; Color txt = BLACK;
                    if (idx == chosenIndexN) { bg = Fade(RED, 0.85f); txt = WHITE; }
                    else if (idx == chosenIndexO) { bg = Fade(BLUE, 0.85f); txt = WHITE; }
                    DrawRectangleRec(rcell, bg);
                    DrawRectangleLines((int)rcell.x, (int)rcell.y, (int)rcell.width, (int)rcell.height, GRAY);

                }
            }

            DrawText(TextFormat("Neck: idx %02d (col %d row %d) yaw %.1f pitch %.1f",
                                chosenIndexN, logicalColNeck, logicalRowNeck, yawDegN, pitchDegN),
                     20, screenH - 60, 16, RED);
            DrawText(TextFormat("Nose: idx %02d (col %d row %d) yaw %.1f pitch %.1f",
                                chosenIndexO, logicalColNose, logicalRowNose, yawDegO, pitchDegO),
                     20, screenH - 36, 16, BLUE);

        EndDrawing();
    }

    UnloadTexture(texA);
    UnloadTexture(texB);
    CloseWindow();
    return 0;
}

