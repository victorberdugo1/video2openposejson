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

