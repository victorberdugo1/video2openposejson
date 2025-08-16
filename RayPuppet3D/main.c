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

/*
#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <float.h>

#define ATLAS_COLS 8
#define ATLAS_ROWS 5

Texture2D texture;
Vector3 billboardPos = {0.0f, 0.0f, 0.0f};
float billboardSize = 2.0f;

void DrawBillboardFull(Camera camera, Texture2D tex, Rectangle src, Vector3 pos, float size)
{
    Vector3 camDir = Vector3Subtract(camera.position, pos);
    float distance = Vector3Length(camDir);

    if (distance < 0.001f) {
        DrawBillboardPro(camera, tex, src, pos, camera.up, (Vector2){size, size}, (Vector2){0.5f, 0.5f}, 0.0f, WHITE);
        return;
    }

    camDir = Vector3Scale(camDir, 1.0f / distance);
    Vector3 worldUp = {0.0f, 1.0f, 0.0f};
    Vector3 right, up;

    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    right = Vector3CrossProduct(forward, camera.up);
    right = Vector3Normalize(right);

    if (fabsf(Vector3DotProduct(camDir, worldUp)) > 0.999f) {
        up = Vector3CrossProduct(camDir, right);
    } else {
        right = Vector3CrossProduct(worldUp, camDir);
        right = Vector3Normalize(right);
        up = Vector3CrossProduct(camDir, right);
    }

    DrawBillboardPro(camera, tex, src, pos, up, (Vector2){size, size}, (Vector2){0.5f, 0.5f}, 0.0f, WHITE);
}

int main(void)
{
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "Billboard - Orbital y Free Camera");
    SetTargetFPS(60);

    // -------------------------
    // Cámara base
    // -------------------------
    Camera camera = {
        .position = {4.0f, 2.0f, 4.0f},
        .target = {0.0f, 1.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 60.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    // -------------------------
    // Variables de órbita (modo 1)
    // -------------------------
    float orbitYaw, orbitPitch, orbitRadius;
    const float yawSensitivity = 0.01f;
    const float pitchSensitivity = 0.01f;
    const float zoomSensitivity = 0.5f;
    const float pitchMax = 1.4f;  // ~80 grados
    const float pitchMin = -1.4f; // ~-80 grados
    const float radiusMin = 1.0f;
    const float radiusMax = 30.0f;

    //const float PI = 3.14159265358979323846f;

    // Inicializar órbita desde posición inicial de la cámara
    {
        Vector3 dir = Vector3Subtract(camera.position, camera.target);
        orbitRadius = Vector3Length(dir);
        if (orbitRadius <= 0.0001f) orbitRadius = 6.0f;
        orbitYaw = atan2f(dir.x, dir.z);                 // yaw alrededor del eje Y
        orbitPitch = asinf(dir.y / orbitRadius);         // pitch
    }

    // -------------------------
    // Variables de cámara libre (modo 2)
    // -------------------------
    Vector3 freePos = camera.position;
    float freeYaw, freePitch;
    float freeSpeed = 5.0f;
    const float freeYawSens = 0.005f;
    const float freePitchSens = 0.005f;
    const float freeSprintMult = 2.5f;

    // Inicializar free yaw/pitch basados en la orientación actual
    {
        Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        freeYaw = atan2f(forward.x, forward.z);
        freePitch = asinf(forward.y);
    }

    // -------------------------
    // Modo activo: 1 = orbital, 2 = free
    // -------------------------
    int camMode = 1;

    // Cargar textura (si no existe, generamos una checker con números)
    Image img = LoadImage("tex.png");
    if (img.data == NULL) {
        img = GenImageChecked(1024, 640, 128, 80, RED, GREEN);
        // Añadir marcadores para cada frame
        for (int row = 0; row < ATLAS_ROWS; row++) {
            for (int col = 0; col < ATLAS_COLS; col++) {
                char textBuf[16];
                sprintf(textBuf, "%d", row * ATLAS_COLS + col);
                ImageDrawText(&img, textBuf,
                              col * (img.width / ATLAS_COLS) + 20,
                              row * (img.height / ATLAS_ROWS) + 20,
                              40, BLACK);
            }
        }
    }
    texture = LoadTextureFromImage(img);
    UnloadImage(img);

    // Main loop
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // -------------------
        // Cambio de modo por tecla (1 o 2)
        // -------------------
        if (IsKeyPressed(KEY_ONE)) {
            // switch to orbital: calcular orbit vars desde la cámara actual
            camMode = 1;
            Vector3 dir = Vector3Subtract(camera.position, billboardPos);
            orbitRadius = Vector3Length(dir);
            if (orbitRadius <= 0.0001f) orbitRadius = 6.0f;
            orbitYaw = atan2f(dir.x, dir.z);
            orbitPitch = asinf(dir.y / orbitRadius);
        }
        if (IsKeyPressed(KEY_TWO)) {
            // switch to free: tomar posición y orientación actual
            camMode = 2;
            freePos = camera.position;
            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            freeYaw = atan2f(forward.x, forward.z);
            freePitch = asinf(forward.y);
        }

        // -------------------
        // Controls: mover billboard con teclado (siempre activos)
        // -------------------
        if (IsKeyDown(KEY_RIGHT)) billboardPos.x += 0.1f;
        if (IsKeyDown(KEY_LEFT))  billboardPos.x -= 0.1f;
        if (IsKeyDown(KEY_UP))    billboardPos.z -= 0.1f;
        if (IsKeyDown(KEY_DOWN))  billboardPos.z += 0.1f;
        if (IsKeyDown(KEY_SPACE) && !IsKeyDown(KEY_LEFT_CONTROL)) billboardPos.y += 0.1f;
        if (IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_LEFT_CONTROL)) billboardPos.y -= 0.1f;

        // -------------------
        // Cámara orbital (modo 1)
        // -------------------
        if (camMode == 1) {
            // Orbita manual con ratón
            Vector2 md = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                orbitYaw   += md.x * yawSensitivity;
                orbitPitch += -md.y * pitchSensitivity; // invertir Y si prefieres
                if (orbitPitch > pitchMax) orbitPitch = pitchMax;
                if (orbitPitch < pitchMin) orbitPitch = pitchMin;
            }

            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                orbitRadius -= wheel * zoomSensitivity;
                if (orbitRadius < radiusMin) orbitRadius = radiusMin;
                if (orbitRadius > radiusMax) orbitRadius = radiusMax;
            }

            // Actualizar cámara (esféricas -> cartesianas) orbitando alrededor de billboardPos
            Vector3 target = billboardPos; // puedes usar target.y += offset si quieres orbitar por encima
            float x = orbitRadius * cosf(orbitPitch) * sinf(orbitYaw);
            float y = orbitRadius * sinf(orbitPitch);
            float z = orbitRadius * cosf(orbitPitch) * cosf(orbitYaw);

            camera.position = (Vector3){ target.x + x, target.y + y, target.z + z };
            camera.target = target;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
        }
        // -------------------
        // Cámara libre (modo 2)
        // -------------------
        else if (camMode == 2) {
            // Mirar con botón derecho (o podrías permitir siempre)
            Vector2 md = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                freeYaw   += md.x * freeYawSens;
                freePitch += -md.y * freePitchSens;
                // limitar pitch para evitar flip
                const float freePitchLimit = 1.49f;
                if (freePitch > freePitchLimit) freePitch = freePitchLimit;
                if (freePitch < -freePitchLimit) freePitch = -freePitchLimit;
            }

            // Movimiento WASD
            Vector3 forwardDir = { sinf(freeYaw) * cosf(freePitch), sinf(freePitch), cosf(freeYaw) * cosf(freePitch) };
            forwardDir = Vector3Normalize(forwardDir);
            Vector3 rightDir = Vector3Normalize(Vector3CrossProduct((Vector3){0.0f,1.0f,0.0f}, forwardDir)); // right = cross(up, forward)
            // Ajuste velocidad y sprint
            float speed = freeSpeed;
            if (IsKeyDown(KEY_LEFT_SHIFT)) speed *= freeSprintMult;
            if (IsKeyDown(KEY_W)) freePos = Vector3Add(freePos, Vector3Scale(forwardDir, speed * dt));
            if (IsKeyDown(KEY_S)) freePos = Vector3Subtract(freePos, Vector3Scale(forwardDir, speed * dt));
            if (IsKeyDown(KEY_A)) freePos = Vector3Subtract(freePos, Vector3Scale(rightDir, speed * dt));
            if (IsKeyDown(KEY_D)) freePos = Vector3Add(freePos, Vector3Scale(rightDir, speed * dt));
            // Subir/bajar
            if (IsKeyDown(KEY_E)) freePos.y += speed * dt; // E sube
            if (IsKeyDown(KEY_Q)) freePos.y -= speed * dt; // Q baja

            // Fijar cámara
            camera.position = freePos;
            Vector3 lookTarget = Vector3Add(freePos, forwardDir);
            camera.target = lookTarget;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
        }

        // -------------------
        // Cálculo de frame del atlas según dirección de la cámara
        // -------------------
        Vector3 camDir = Vector3Subtract(camera.position, billboardPos);
        float camDist = Vector3Length(camDir);
        if (camDist < 0.0001f) camDist = 0.0001f;

        // Yaw horizontal [0,2PI)
        float yawAngle = atan2f(camDir.x, camDir.z);
        if (yawAngle < 0.0f) yawAngle += 2.0f * PI;

        // Pitch vertical [-PI/2,PI/2]
        float horizDist = sqrtf(camDir.x * camDir.x + camDir.z * camDir.z);
        float pitchAngle = atan2f(camDir.y, horizDist);

        // Mapear yaw -> columna
        int colIndex = (int)floorf((yawAngle / (2.0f * PI)) * (float)ATLAS_COLS);
        if (colIndex < 0) colIndex = 0;
        if (colIndex >= ATLAS_COLS) colIndex = ATLAS_COLS - 1;

        // Mapear pitch -> fila
        float pitchNorm = (pitchAngle + (PI * 0.5f)) / PI; // 0..1
        int rowIndex = (int)floorf(pitchNorm * (float)ATLAS_ROWS);
        if (rowIndex < 0) rowIndex = 0;
        if (rowIndex >= ATLAS_ROWS) rowIndex = ATLAS_ROWS - 1;

        int frameIndex = rowIndex * ATLAS_COLS + colIndex;

        // Calcular src
        float frameWidth = (float)texture.width / ATLAS_COLS;
        float frameHeight = (float)texture.height / ATLAS_ROWS;
        Rectangle src = {
            colIndex * frameWidth,
            rowIndex * frameHeight,
            frameWidth,
            frameHeight
        };

        // -------------------
        // Render
        // -------------------
        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
            DrawGrid(20, 1.0f);
            DrawBillboardFull(camera, texture, src, billboardPos, billboardSize);
            DrawSphere(billboardPos, 0.1f, RED);
        EndMode3D();

        // HUD / debug
        DrawText(TextFormat("Modo: %s  (Pulsa 1=Orbital, 2=Free)",
                  (camMode==1) ? "ORBITAL" : "FREE"), 10, 10, 20, BLACK);
        DrawText(TextFormat("Camera Pos: (%.2f, %.2f, %.2f)",
                  camera.position.x, camera.position.y, camera.position.z), 10, 40, 20, BLACK);
        DrawText(TextFormat("Billboard Pos: (%.2f, %.2f, %.2f)",
                  billboardPos.x, billboardPos.y, billboardPos.z), 10, 70, 20, BLACK);
        DrawText(TextFormat("FrameIdx: %d  (col %d, row %d)", frameIndex, colIndex, rowIndex), 10, 100, 20, BLACK);

        if (camMode == 1) {
            DrawText("Orbital: arrastra con BOTON IZQUIERDO, rueda para zoom", 10, 130, 20, DARKGRAY);
        } else {
            DrawText("Free: mantén BOTON DERECHO para mirar, WASD mover, E/Q subir/bajar, SHIFT sprint", 10, 130, 20, DARKGRAY);
        }

        DrawText("Flechas para mover billboard, SPACE/LEFT_SHIFT para altura del billboard", 10, 160, 20, DARKGRAY);

        EndDrawing();
    }

    // Cleanup
    UnloadTexture(texture);
    CloseWindow();

    return 0;
}
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

#define ATLAS_COLS 5
#define ATLAS_ROWS 5

Texture2D texture;
Vector3 billboardPos = {0.0f, 0.0f, 0.0f};
float billboardSize = 2.0f;

void DrawBillboardFull2D(Camera camera, Texture2D tex, Rectangle src, Vector3 pos, Vector2 size)
{
    Vector3 camDir = Vector3Subtract(camera.position, pos);
    float distance = Vector3Length(camDir);
    if (distance < 0.001f) {
        DrawBillboardPro(camera, tex, src, pos, camera.up, size, (Vector2){0.5f, 0.5f}, 0.0f, WHITE);
        return;
    }
    camDir = Vector3Scale(camDir, 1.0f / distance);
    Vector3 worldUp = {0.0f, 1.0f, 0.0f};
    Vector3 right, up;
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    right = Vector3CrossProduct(forward, camera.up);
    right = Vector3Normalize(right);
    if (fabsf(Vector3DotProduct(camDir, worldUp)) > 0.999f) {
        up = Vector3CrossProduct(camDir, right);
    } else {
        right = Vector3CrossProduct(worldUp, camDir);
        right = Vector3Normalize(right);
        up = Vector3CrossProduct(camDir, right);
    }
    DrawBillboardPro(camera, tex, src, pos, up, size, (Vector2){0.5f, 0.5f}, 0.0f, WHITE);
}

Rectangle GetAtlasCellSrc(Texture2D tex, int rawCol, int rowIndex)
{
    bool mirrored = false;
    int col = rawCol;
    if (rawCol >= ATLAS_COLS) {
        col = 2*ATLAS_COLS - 1 - rawCol;
        mirrored = true;
    }
    int atlasRow = ATLAS_ROWS - 1 - rowIndex;
    float cellW = (float)tex.width / ATLAS_COLS;
    float cellH = (float)tex.height / ATLAS_ROWS;
    float srcX = col * cellW;
    float srcY = atlasRow * cellH;
    if (mirrored) return (Rectangle){ srcX + cellW, srcY, -cellW, cellH };
    else          return (Rectangle){ srcX, srcY, cellW, cellH };
}

int main(void)
{
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "Billboard Atlas 5x5");
    SetTargetFPS(60);

    Camera camera = { 0 };
    camera.position = (Vector3){4.0f, 2.0f, 4.0f};
    camera.target   = (Vector3){0.0f, 1.0f, 0.0f};
    camera.up       = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy     = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int camMode = 1; 
    float orbitYaw, orbitPitch, orbitRadius;
    {
        Vector3 dir = Vector3Subtract(camera.position, camera.target);
        orbitRadius = Vector3Length(dir);
        orbitYaw = atan2f(dir.x, dir.z);
        orbitPitch = asinf(dir.y / orbitRadius);
    }

    Vector3 freePos = camera.position;
    float freeYaw, freePitch;
    {
        Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        freeYaw = atan2f(forward.x, forward.z);
        freePitch = asinf(forward.y);
    }

    Image img = LoadImage("tex.png");
    if (img.data == NULL) {
        int pw = 1280, ph = 800;
        img = GenImageColor(pw, ph, CLITERAL(Color){0,0,0,0});
        float cellW = (float)pw / ATLAS_COLS;
        float cellH = (float)ph / ATLAS_ROWS;
        for (int r = 0; r < ATLAS_ROWS; r++) {
            for (int c = 0; c < ATLAS_COLS; c++) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%02d", r*ATLAS_COLS + c);
                ImageDrawText(&img, buf, (int)(c*cellW)+8, (int)(r*cellH)+8, (int)(cellH/3), BLACK);
            }
        }
    }
    texture = LoadTextureFromImage(img);
    UnloadImage(img);

    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_ONE)) camMode = 1;
        if (IsKeyPressed(KEY_TWO)) camMode = 2;

        if (IsKeyDown(KEY_RIGHT)) billboardPos.x += 0.1f;
        if (IsKeyDown(KEY_LEFT))  billboardPos.x -= 0.1f;
        if (IsKeyDown(KEY_UP))    billboardPos.z -= 0.1f;
        if (IsKeyDown(KEY_DOWN))  billboardPos.z += 0.1f;
        if (IsKeyDown(KEY_SPACE)) billboardPos.y += 0.1f;
        if (IsKeyDown(KEY_LEFT_SHIFT)) billboardPos.y -= 0.1f;

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
                orbitRadius = Clamp(orbitRadius, 1.0f, 30.0f);
            }
            Vector3 target = billboardPos;
            float x = orbitRadius * cosf(orbitPitch) * sinf(orbitYaw);
            float y = orbitRadius * sinf(orbitPitch);
            float z = orbitRadius * cosf(orbitPitch) * cosf(orbitYaw);
            camera.position = (Vector3){ target.x + x, target.y + y, target.z + z };
            camera.target = target;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
        } else {
            Vector2 md = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                freeYaw   += md.x * 0.005f;
                freePitch += -md.y * 0.005f;
                freePitch = Clamp(freePitch, -1.49f, 1.49f);
            }
            Vector3 forwardDir = { sinf(freeYaw)*cosf(freePitch), sinf(freePitch), cosf(freeYaw)*cosf(freePitch) };
            forwardDir = Vector3Normalize(forwardDir);
            Vector3 rightDir = Vector3Normalize(Vector3CrossProduct((Vector3){0,1,0}, forwardDir));
            float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 12.5f : 5.0f;
            if (IsKeyDown(KEY_W)) freePos = Vector3Add(freePos, Vector3Scale(forwardDir, speed*dt));
            if (IsKeyDown(KEY_S)) freePos = Vector3Subtract(freePos, Vector3Scale(forwardDir, speed*dt));
            if (IsKeyDown(KEY_A)) freePos = Vector3Subtract(freePos, Vector3Scale(rightDir, speed*dt));
            if (IsKeyDown(KEY_D)) freePos = Vector3Add(freePos, Vector3Scale(rightDir, speed*dt));
            if (IsKeyDown(KEY_E)) freePos.y += speed*dt;
            if (IsKeyDown(KEY_Q)) freePos.y -= speed*dt;
            camera.position = freePos;
            camera.target = Vector3Add(freePos, forwardDir);
            camera.up = (Vector3){ 0,1,0 };
        }

        Vector3 camDir = Vector3Subtract(camera.position, billboardPos);
        float yawAngle = atan2f(camDir.x, camDir.z);
        if (yawAngle < 0.0f) yawAngle += 2.0f*PI;
        float horizDist = sqrtf(camDir.x*camDir.x + camDir.z*camDir.z);
        float pitchAngle = atan2f(camDir.y, horizDist);

        int rawCol = (int)floorf((yawAngle / (2.0f*PI)) * (float)(2*ATLAS_COLS));
        rawCol = Clamp(rawCol, 0, 2*ATLAS_COLS-1);

        float pitchNorm = (pitchAngle + (PI*0.5f)) / PI;
        int rowIndex = (int)floorf(pitchNorm * (float)ATLAS_ROWS);
        rowIndex = Clamp(rowIndex, 0, ATLAS_ROWS-1);

        Rectangle src = GetAtlasCellSrc(texture, rawCol, rowIndex);
        float cellW = (float)texture.width / ATLAS_COLS;
        float cellH = (float)texture.height / ATLAS_ROWS;
        float aspect = cellW / cellH;
        Vector2 worldSize = { billboardSize*aspect, billboardSize };

        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(camera);
                DrawGrid(20, 1.0f);
                DrawBillboardFull2D(camera, texture, src, billboardPos, worldSize);
                DrawSphere(billboardPos, 0.02f, RED);
            EndMode3D();
            DrawText(TextFormat("Modo: %s (1 Orbit / 2 Libre)", camMode==1?"Orbit":"Libre"), 10, 10, 20, DARKGRAY);
        EndDrawing();
    }

    UnloadTexture(texture);
    CloseWindow();

    return 0;
}
   


/*
#include <stdlib.h>
#include <stdio.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>

#define ATLAS_COLS 8
#define ATLAS_ROWS 5

#define LAYERS_COUNT 4

const char *layerFiles[LAYERS_COUNT] = { "tex0.png", "tex1.png", "tex2.png", "tex3.png" };

Texture2D layerTex[LAYERS_COUNT];
Vector3 billboardPos = {0.0f, 0.0f, 0.0f};
float billboardSize = 2.0f;

float layerDepths[LAYERS_COUNT]    = { -0.08f, -0.03f, 0.03f, 0.08f }; // back -> front
float layerScales[LAYERS_COUNT]    = { 1.05f, 1.02f, 1.02f, 1.08f };
float layerYOffsets[LAYERS_COUNT]  = { 0.08f, 0.0f, -0.02f, 0.05f };

void DrawBillboardFull(Camera camera, Texture2D tex, Rectangle src, Vector3 pos, float size)
{
    Vector3 camDir = Vector3Subtract(camera.position, pos);
    float distance = Vector3Length(camDir);

    if (distance < 0.001f) {
        DrawBillboardPro(camera, tex, src, pos, camera.up, (Vector2){size, size}, (Vector2){0.5f, 0.5f}, 0.0f, WHITE);
        return;
    }

    camDir = Vector3Scale(camDir, 1.0f / distance);
    Vector3 worldUp = {0.0f, 1.0f, 0.0f};
    Vector3 right, up;

    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    right = Vector3CrossProduct(forward, camera.up);
    right = Vector3Normalize(right);

    if (fabsf(Vector3DotProduct(camDir, worldUp)) > 0.999f) {
        up = Vector3CrossProduct(camDir, right);
    } else {
        right = Vector3CrossProduct(worldUp, camDir);
        right = Vector3Normalize(right);
        up = Vector3CrossProduct(camDir, right);
    }

    DrawBillboardPro(camera, tex, src, pos, up, (Vector2){size, size}, (Vector2){0.5f, 0.5f}, 0.0f, WHITE);
}

int main(void)
{
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "Capas separadas: tex0..tex3");
    SetTargetFPS(60);

    // Cámara base y vars (mismos modos que antes)
    Camera camera = {
        .position = {4.0f, 2.0f, 4.0f},
        .target = {0.0f, 1.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 60.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    // Orbital vars
    float orbitYaw, orbitPitch, orbitRadius;
    const float yawSensitivity = 0.01f;
    const float pitchSensitivity = 0.01f;
    const float zoomSensitivity = 0.5f;
    const float pitchMax = 1.4f;
    const float pitchMin = -1.4f;
    const float radiusMin = 1.0f;
    const float radiusMax = 30.0f;

    {
        Vector3 dir = Vector3Subtract(camera.position, camera.target);
        orbitRadius = Vector3Length(dir);
        if (orbitRadius <= 0.0001f) orbitRadius = 6.0f;
        orbitYaw = atan2f(dir.x, dir.z);
        orbitPitch = asinf(dir.y / orbitRadius);
    }

    // Free camera vars
    Vector3 freePos = camera.position;
    float freeYaw, freePitch;
    float freeSpeed = 5.0f;
    const float freeYawSens = 0.005f;
    const float freePitchSens = 0.005f;
    const float freeSprintMult = 2.5f;
    {
        Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        freeYaw = atan2f(forward.x, forward.z);
        freePitch = asinf(forward.y);
    }

    int camMode = 1; // 1 orbital, 2 free

    // -----------------------
    // Cargar las texturas por capa
    // -----------------------
    for (int i = 0; i < LAYERS_COUNT; i++) {
        Image img = LoadImage(layerFiles[i]);
        if (img.data == NULL) {
            // placeholder con texto claro para debug
            int pw = 512, ph = 512;
            img = GenImageColor(pw, ph, Fade(SKYBLUE, 0.6f));
            ImageDrawText(&img, layerFiles[i], 20, ph/2 - 10, 24, BLACK);
        }
        // Asegurarnos que la imagen tenga orientación "normal" para texturas
        ImageFlipVertical(&img);
        layerTex[i] = LoadTextureFromImage(img);
        UnloadImage(img);
        // activar filtrado bilinear si está disponible
        SetTextureFilter(layerTex[i], TEXTURE_FILTER_BILINEAR);
    }

    // -----------------------
    // Loop principal
    // -----------------------
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // Cambiar modo cámara con 1/2
        if (IsKeyPressed(KEY_ONE)) {
            camMode = 1;
            Vector3 dir = Vector3Subtract(camera.position, billboardPos);
            orbitRadius = Vector3Length(dir);
            if (orbitRadius <= 0.0001f) orbitRadius = 6.0f;
            orbitYaw = atan2f(dir.x, dir.z);
            orbitPitch = asinf(dir.y / orbitRadius);
        }
        if (IsKeyPressed(KEY_TWO)) {
            camMode = 2;
            freePos = camera.position;
            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            freeYaw = atan2f(forward.x, forward.z);
            freePitch = asinf(forward.y);
        }

        // Mover billboard (siempre)
        if (IsKeyDown(KEY_RIGHT)) billboardPos.x += 0.1f;
        if (IsKeyDown(KEY_LEFT))  billboardPos.x -= 0.1f;
        if (IsKeyDown(KEY_UP))    billboardPos.z -= 0.1f;
        if (IsKeyDown(KEY_DOWN))  billboardPos.z += 0.1f;
        if (IsKeyDown(KEY_SPACE) && !IsKeyDown(KEY_LEFT_CONTROL)) billboardPos.y += 0.1f;
        if (IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_LEFT_CONTROL)) billboardPos.y -= 0.1f;

        // Cámara: orbital o free
        if (camMode == 1) {
            Vector2 md = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                orbitYaw   += md.x * yawSensitivity;
                orbitPitch += -md.y * pitchSensitivity;
                if (orbitPitch > pitchMax) orbitPitch = pitchMax;
                if (orbitPitch < pitchMin) orbitPitch = pitchMin;
            }
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                orbitRadius -= wheel * zoomSensitivity;
                if (orbitRadius < radiusMin) orbitRadius = radiusMin;
                if (orbitRadius > radiusMax) orbitRadius = radiusMax;
            }
            Vector3 target = billboardPos;
            float x = orbitRadius * cosf(orbitPitch) * sinf(orbitYaw);
            float y = orbitRadius * sinf(orbitPitch);
            float z = orbitRadius * cosf(orbitPitch) * cosf(orbitYaw);
            camera.position = (Vector3){ target.x + x, target.y + y, target.z + z };
            camera.target = target;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
        } else {
            Vector2 md = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                freeYaw   += md.x * freeYawSens;
                freePitch += -md.y * freePitchSens;
                const float freePitchLimit = 1.49f;
                if (freePitch > freePitchLimit) freePitch = freePitchLimit;
                if (freePitch < -freePitchLimit) freePitch = -freePitchLimit;
            }
            Vector3 forwardDir = { sinf(freeYaw) * cosf(freePitch), sinf(freePitch), cosf(freeYaw) * cosf(freePitch) };
            forwardDir = Vector3Normalize(forwardDir);
            Vector3 rightDir = Vector3Normalize(Vector3CrossProduct((Vector3){0.0f,1.0f,0.0f}, forwardDir));
            float speed = freeSpeed;
            if (IsKeyDown(KEY_LEFT_SHIFT)) speed *= freeSprintMult;
            if (IsKeyDown(KEY_W)) freePos = Vector3Add(freePos, Vector3Scale(forwardDir, speed * dt));
            if (IsKeyDown(KEY_S)) freePos = Vector3Subtract(freePos, Vector3Scale(forwardDir, speed * dt));
            if (IsKeyDown(KEY_A)) freePos = Vector3Subtract(freePos, Vector3Scale(rightDir, speed * dt));
            if (IsKeyDown(KEY_D)) freePos = Vector3Add(freePos, Vector3Scale(rightDir, speed * dt));
            if (IsKeyDown(KEY_E)) freePos.y += speed * dt;
            if (IsKeyDown(KEY_Q)) freePos.y -= speed * dt;
            camera.position = freePos;
            Vector3 lookTarget = Vector3Add(freePos, forwardDir);
            camera.target = lookTarget;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
        }

        // -----------------------
        // Determinar columna/fila en función de la dirección cámara->billboard
        // -----------------------
        Vector3 camDir = Vector3Subtract(camera.position, billboardPos);
        float camDist = Vector3Length(camDir);
        if (camDist < 0.0001f) camDist = 0.0001f;

        float yawAngle = atan2f(camDir.x, camDir.z);
        if (yawAngle < 0.0f) yawAngle += 2.0f * PI;
        float horizDist = sqrtf(camDir.x*camDir.x + camDir.z*camDir.z);
        float pitchAngle = atan2f(camDir.y, horizDist);

        int rawCol = (int)floorf((yawAngle / (2.0f * PI)) * (float)ATLAS_COLS);
        if (rawCol < 0) rawCol = 0;
        if (rawCol >= ATLAS_COLS) rawCol = ATLAS_COLS - 1;

        float pitchNorm = (pitchAngle + (PI * 0.5f)) / PI; // 0..1
        int rowIndex = (int)floorf(pitchNorm * (float)ATLAS_ROWS);
        if (rowIndex < 0) rowIndex = 0;
        if (rowIndex >= ATLAS_ROWS) rowIndex = ATLAS_ROWS - 1;

        // Canonical col + flip (espejado horizontal)
        int half = ATLAS_COLS / 2;
        int canonicalCol;
        bool flipHoriz = false;
        if (rawCol <= half) {
            canonicalCol = rawCol;
            flipHoriz = false;
        } else {
            canonicalCol = ATLAS_COLS - rawCol;
            flipHoriz = true;
        }

        // -----------------------
        // Dibujar capas (back -> front)
        // -----------------------
        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
            DrawGrid(20, 1.0f);

            // viewDir desde billboard hacia la cámara (usado para desplazar capas)
            Vector3 viewDir = Vector3Normalize(Vector3Subtract(camera.position, billboardPos));

            // Desactivar depth test para dibujar capas con blending correcto (ordenadas back->front)
            rlDisableDepthTest();
            for (int i = 0; i < LAYERS_COUNT; i++) {
                // posición de la capa desplazada a lo largo de viewDir
                Vector3 layerPos = Vector3Add(billboardPos, Vector3Scale(viewDir, layerDepths[i]));
                layerPos.y += layerYOffsets[i];

                Texture2D tex = layerTex[i];

                // Determinar si la textura es un atlas (suficiente tamaño)
                bool isAtlas = ((int)tex.width >= ATLAS_COLS && (int)tex.height >= ATLAS_ROWS);

                Rectangle src;
                if (isAtlas) {
                    float frameW = (float)tex.width / ATLAS_COLS;
                    float frameH = (float)tex.height / ATLAS_ROWS;
                    if (!flipHoriz) {
                        src = (Rectangle){ canonicalCol * frameW, rowIndex * frameH, frameW, frameH };
                    } else {
                        // flip por negación de width
                        src = (Rectangle){ (canonicalCol + 1) * frameW, rowIndex * frameH, -frameW, frameH };
                    }
                } else {
                    // imagen única: usar toda la textura
                    if (!flipHoriz) {
                        src = (Rectangle){ 0.0f, 0.0f, (float)tex.width, (float)tex.height };
                    } else {
                        src = (Rectangle){ (float)tex.width, 0.0f, -(float)tex.width, (float)tex.height };
                    }
                }

                float scale = layerScales[i] * billboardSize;
                DrawBillboardFull(camera, tex, src, layerPos, scale);
            }
            rlEnableDepthTest();

            // Referencia
            DrawSphere(billboardPos, 0.02f, RED);

        EndMode3D();

        // HUD
        DrawText(TextFormat("Modo: %s  (Pulsa 1=Orbital, 2=Free)", (camMode==1) ? "ORBITAL" : "FREE"), 10, 10, 20, BLACK);
        DrawText(TextFormat("Camera: (%.2f,%.2f,%.2f)", camera.position.x, camera.position.y, camera.position.z), 10, 40, 20, BLACK);
        DrawText(TextFormat("Billboard: (%.2f,%.2f,%.2f)", billboardPos.x, billboardPos.y, billboardPos.z), 10, 70, 20, BLACK);
        DrawText(TextFormat("rawCol,row: %d,%d   canonicalCol: %d   flip:%s", rawCol, rowIndex, canonicalCol, flipHoriz ? "Y":"N"), 10, 100, 20, BLACK);
        DrawText("Flechas: mover billboard | 1=orbital 2=free | Izq raton=rotar orbita | Rueda=zoom", 10, 130, 20, DARKGRAY);

        EndDrawing();
    }

    // cleanup
    for (int i = 0; i < LAYERS_COUNT; i++) UnloadTexture(layerTex[i]);
    CloseWindow();

    return 0;
}
*/
