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

// ---- Dibujar quad texturizado en 3D desde 4 vértices ----
static void DrawQuadTextured3D(Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
                               float u0, float v0t, float u1, float v1t)
{
    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        rlTexCoord2f(u0, v0t); rlVertex3f(v0.x, v0.y, v0.z);
        rlTexCoord2f(u1, v0t); rlVertex3f(v1.x, v1.y, v1.z);
        rlTexCoord2f(u1, v1t); rlVertex3f(v2.x, v2.y, v2.z);
        rlTexCoord2f(u0, v1t); rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
    rlSetTexture(0);
}

// ---- Billboard custom con rotación ----
void DrawBillboardCustom(Camera camera, Rectangle src, Vector3 pos, Vector2 size, float rotationDeg, bool mirrored)
{
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    float a = rotationDeg * (PI / 180.0f);
    float ca = cosf(a);
    float sa = sinf(a);

    Vector3 newRight = Vector3Subtract(Vector3Scale(right, ca), Vector3Scale(up, sa));
    Vector3 newUp    = Vector3Add(Vector3Scale(right, sa), Vector3Scale(up, ca));

    Vector3 halfX = Vector3Scale(newRight, size.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp,    size.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(pos, halfX), halfY);
    Vector3 p1 = Vector3Add   (Vector3Subtract(pos, halfY), halfX);
    Vector3 p2 = Vector3Add   (Vector3Add(pos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(pos, halfY), halfX);

    float texW = (float)texture.width;
    float texH = (float)texture.height;
    float u_left  = src.x / texW;
    float u_right = (src.x + src.width) / texW;
    float v_top   = src.y / texH;
    float v_bottom= (src.y + src.height) / texH;

    if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
    if (src.height < 0){ float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

    float v0t = v_bottom;
    float v1t = v_top;

    if (mirrored) {
        float tmp = u_left; u_left = u_right; u_right = tmp;
    }

    DrawQuadTextured3D(p0, p1, p2, p3, u_left, v0t, u_right, v1t);
}

// Obtener la src rect de una celda del atlas
Rectangle GetAtlasCellSrcPos(Texture2D tex, int col, int rowIndex, bool mirrored, bool *outMirrored)
{
    int atlasRow = ATLAS_ROWS - 1 - rowIndex;
    float cellW = (float)tex.width / ATLAS_COLS;
    float cellH = (float)tex.height / ATLAS_ROWS;
    float srcX = col * cellW;
    float srcY = atlasRow * cellH;

    if (outMirrored) *outMirrored = mirrored;

    return (Rectangle){ srcX, srcY, cellW, cellH };
}

int main(void)
{
    const int screenWidth = 1920;
    const int screenHeight = 1440;
    InitWindow(screenWidth, screenHeight, "Billboard con transiciones perfectas a 22.5°");
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
        float yawDeg = yawAngle * RAD2DEG;

        float horizDist = sqrtf(camDir.x*camDir.x + camDir.z*camDir.z);
        float pitchAngle = atan2f(camDir.y, horizDist);
        float pitchNorm = (pitchAngle + (PI*0.5f)) / PI;
        int rowIndex = (int)floorf(pitchNorm * (float)ATLAS_ROWS);
        rowIndex = Clamp(rowIndex, 0, ATLAS_ROWS-1);

        bool useFixedTop = (rowIndex == 0);
        bool useFixedBottom = (rowIndex == ATLAS_ROWS - 1);

        // MODIFICACIÓN PRINCIPAL: Transiciones perfectas a 22.5°
        int baseCol = 0;
        bool mirrored = false;

        // Ángulos centrales de cada sector
        const float sectorAngles[] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f};

        // Encontrar el sector más cercano
        int sector = 0;
        float minDiff = 360.0f;
        for (int i = 0; i < 8; i++) {
            float diff = fabsf(yawDeg - sectorAngles[i]);
            if (diff > 180.0f) diff = 360.0f - diff;
            if (diff < minDiff) {
                minDiff = diff;
                sector = i;
            }
        }

        // Determinar textura basada en el sector
        switch(sector) {
            case 0: baseCol = 0; mirrored = false; break;
            case 1: baseCol = 1; mirrored = true; break;  // Derecha-frontal (mirrored)
            case 2: baseCol = 2; mirrored = true; break;  // Derecha (mirrored)
            case 3: baseCol = 3; mirrored = true; break;  // Derecha-atrás (mirrored)
            case 4: baseCol = 4; mirrored = false; break; // Atrás
            case 5: baseCol = 3; mirrored = false; break; // Izquierda-atrás
            case 6: baseCol = 2; mirrored = false; break; // Izquierda
            case 7: baseCol = 1; mirrored = false; break; // Izquierda-frontal
        }

        Rectangle src;
        float rotationDeg = 0.0f;
        bool finalMirrored = false;

        if (useFixedTop || useFixedBottom) {
            src = GetAtlasCellSrcPos(texture, 0, useFixedTop ? 0 : ATLAS_ROWS-1, false, &finalMirrored);
            rotationDeg = sectorAngles[sector]; // Usar el ángulo central del sector

            // Para la vista inferior, ajustar la orientación
			if (useFixedBottom) {
				rotationDeg = 360.0f - rotationDeg;
				if (rotationDeg >= 360.0f)
					rotationDeg -= 360.0f;
			}
        } else {
            src = GetAtlasCellSrcPos(texture, baseCol, rowIndex, mirrored, &finalMirrored);
            rotationDeg = 0.0f;
        }

        float cellW = (float)texture.width / ATLAS_COLS;
        float cellH = (float)texture.height / ATLAS_ROWS;
        float aspect = cellW / cellH;
        Vector2 worldSize = (Vector2){ billboardSize * aspect, billboardSize };

        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(camera);
                DrawGrid(20, 1.0f);
                DrawBillboardCustom(camera, src, billboardPos, worldSize, rotationDeg, finalMirrored);
            EndMode3D();

            DrawText(TextFormat("Modo: %s (1 Orbit / 2 Libre)", camMode==1?"Orbit":"Libre"), 10, 10, 20, DARKGRAY);
            DrawText(TextFormat("Yaw: %.1f° | Sector: %d (%d°) | Row: %d | Rotación: %.1f°",
                               yawDeg, sector, (int)sectorAngles[sector], rowIndex, rotationDeg), 10, 40, 20, DARKGRAY);
        EndDrawing();
    }

    UnloadTexture(texture);
    CloseWindow();
    return 0;
}
