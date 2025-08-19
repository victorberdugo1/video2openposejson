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
#include "bonetile.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

int main(void) {
    const int screenWidth = 1920;
    const int screenHeight = 1440;
    InitWindow(screenWidth, screenHeight, "Bonetile con transiciones perfectas a 22.5°");
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

    Image img = LoadImage("tex0.png");
    if (img.data == NULL) {
        int pw = 1280, ph = 800;
        img = GenImageColor(pw, ph, CLITERAL(Color){0,0,0,0});
        float cellW = (float)pw / AXIS_YAW;
        float cellH = (float)ph / AXIS_PITCH;
        for (int r = 0; r < AXIS_PITCH; r++) {
            for (int c = 0; c < AXIS_YAW; c++) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%02d", r*AXIS_YAW + c);
                ImageDrawText(&img, buf, (int)(c*cellW)+8, (int)(r*cellH)+8, (int)(cellH/3), BLACK);
            }
        }
    }
    texture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, TEXTURE_WRAP_CLAMP);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_ONE)) camMode = 1;
        if (IsKeyPressed(KEY_TWO)) camMode = 2;
        if (IsKeyDown(KEY_RIGHT)) bonetilePos.x += 0.1f;
        if (IsKeyDown(KEY_LEFT))  bonetilePos.x -= 0.1f;
        if (IsKeyDown(KEY_UP))    bonetilePos.z -= 0.1f;
        if (IsKeyDown(KEY_DOWN))  bonetilePos.z += 0.1f;
        if (IsKeyDown(KEY_SPACE)) bonetilePos.y += 0.1f;
        if (IsKeyDown(KEY_LEFT_SHIFT)) bonetilePos.y -= 0.1f;

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
            Vector3 target = bonetilePos;
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

        Vector3 camDir = Vector3Subtract(camera.position, bonetilePos);
        float yawAngle = atan2f(camDir.x, camDir.z);
        if (yawAngle < 0.0f) yawAngle += 2.0f*PI;
        float yawDeg = yawAngle * RAD2DEG;

        float horizDist = sqrtf(camDir.x*camDir.x + camDir.z*camDir.z);
        float pitchAngle = atan2f(camDir.y, horizDist);
        float pitchNorm = (pitchAngle + (PI*0.5f)) / PI;
        int rowIndex = (int)floorf(pitchNorm * (float)AXIS_PITCH);
        rowIndex = Clamp(rowIndex, 0, AXIS_PITCH-1);

        bool useFixedTop = (rowIndex == 0);
        bool useFixedBottom = (rowIndex == AXIS_PITCH - 1);

        // Transiciones perfectas a 22.5°
        int baseCol = 0;
        bool mirrored = false;
        const float sectorAngles[] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f};

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

        switch(sector) {
            case 0: baseCol = 0; mirrored = false; break;
            case 1: baseCol = 1; mirrored = true; break;
            case 2: baseCol = 2; mirrored = true; break;
            case 3: baseCol = 3; mirrored = true; break;
            case 4: baseCol = 4; mirrored = false; break;
            case 5: baseCol = 3; mirrored = false; break;
            case 6: baseCol = 2; mirrored = false; break;
            case 7: baseCol = 1; mirrored = false; break;
        }

        Rectangle src;
        float rotationDeg = 0.0f;
        bool finalMirrored = false;

        if (useFixedTop || useFixedBottom) {
            src = GetAtlasCellSrcPos(texture, 0, useFixedTop ? 0 : AXIS_PITCH-1, false, &finalMirrored);
            rotationDeg = sectorAngles[sector];
            if (useFixedBottom) {
                rotationDeg = 360.0f - rotationDeg;
                if (rotationDeg >= 360.0f) rotationDeg -= 360.0f;
            }
        } else {
            src = GetAtlasCellSrcPos(texture, baseCol, rowIndex, mirrored, &finalMirrored);
        }

        float cellW = (float)texture.width / AXIS_YAW;
        float cellH = (float)texture.height / AXIS_PITCH;
        float aspect = cellW / cellH;
        Vector2 worldSize = (Vector2){ bonetileSize * aspect, bonetileSize };

        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(camera);
                DrawGrid(20, 1.0f);
                DrawBonetileCustom(camera, src, bonetilePos, worldSize, rotationDeg, finalMirrored);
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
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "bonetile.h"   // contiene AXIS_YAW/ROWS y prototipos

int main(void) {
    const int screenWidth = 2056;
    const int screenHeight = 1504;
    InitWindow(screenWidth, screenHeight, "Bonetiles en posiciones específicas");
    SetTargetFPS(60);

    // Cámara
    Camera camera = { 0 };
    camera.position = (Vector3){0.0f, 0.5f, 4.0f};
    camera.target   = (Vector3){0.0f, 0.5f, 0.0f};
    camera.up       = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy     = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int camMode = 1;  // 1 = Orbital, 2 = Libre

    // Variables para modo orbital
    float orbitYaw = 0.0f;
    float orbitPitch = 0.0f;
    float orbitRadius = 4.0f;
    //Vector3 orbitTarget = {0.0f, 0.5f, 0.0f};

    // Variables para modo libre
    Vector3 freePos = camera.position;
    float freeYaw, freePitch;
    {
        Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        freeYaw = atan2f(forward.x, forward.z);
        freePitch = asinf(forward.y);
    }

    // Posiciones fijas de los bones
    Vector3 neckPos = {
        0.753111869096756f * 2.0f - 1.0f,   // X: [0,1] -> [-1,1]
        1.0f - 0.26760193705558777f,        // Y: invertido (Y=0 es abajo)
        0.210185244679451f * 2.0f - 1.0f    // Z: [0,1] -> [-1,1]
    };

    Vector3 nosePos = {
        0.7485565543174744f * 2.0f - 1.0f,  // X: [0,1] -> [-1,1]
        1.0f - 0.18256297707557678f,        // Y: invertido
        0.25478237867355347f * 2.0f - 1.0f  // Z: [0,1] -> [-1,1]
    };

    // Tamaño de los bonetiles
    float bonetileSize = 0.3f;

    // Cargar dos texturas distintas
    Texture2D textureA = {0};
    Texture2D textureB = {0};

    // Cargar textura A
    Image imgA = LoadImage("tex1.png");
    if (imgA.data == NULL) {
        int pw = 1280, ph = 800;
        imgA = GenImageColor(pw, ph, CLITERAL(Color){40, 120, 200, 255});
        float cellW = (float)pw / AXIS_YAW;
        float cellH = (float)ph / AXIS_PITCH;
        for (int r = 0; r < AXIS_PITCH; r++) {
            for (int c = 0; c < AXIS_YAW; c++) {
                char buf[8];
                snprintf(buf, sizeof(buf), "A%02d", r*AXIS_YAW + c);
                ImageDrawText(&imgA, buf, (int)(c*cellW)+8, (int)(r*cellH)+8, (int)(cellH/4), BLACK);
            }
        }
    }
    textureA = LoadTextureFromImage(imgA);
    UnloadImage(imgA);
    SetTextureFilter(textureA, TEXTURE_FILTER_POINT);
    SetTextureWrap(textureA, TEXTURE_WRAP_CLAMP);

    // Cargar textura B
    Image imgB = LoadImage("tex0.png");
    if (imgB.data == NULL) {
        int pw = 1280, ph = 800;
        imgB = GenImageColor(pw, ph, CLITERAL(Color){120, 200, 90, 255});
        float cellW = (float)pw / AXIS_YAW;
        float cellH = (float)ph / AXIS_PITCH;
        for (int r = 0; r < AXIS_PITCH; r++) {
            for (int c = 0; c < AXIS_YAW; c++) {
                char buf[8];
                snprintf(buf, sizeof(buf), "B%02d", r*AXIS_YAW + c);
                ImageDrawText(&imgB, buf, (int)(c*cellW)+8, (int)(r*cellH)+8, (int)(cellH/4), BLACK);
            }
        }
    }
    textureB = LoadTextureFromImage(imgB);
    UnloadImage(imgB);
    SetTextureFilter(textureB, TEXTURE_FILTER_POINT);
    SetTextureWrap(textureB, TEXTURE_WRAP_CLAMP);

    // Variables auxiliares para el atlas
    Rectangle srcNeck, srcNose;
    float rotationDegNeck = 0.0f, rotationDegNose = 0.0f;
    bool finalMirroredNeck = false, finalMirroredNose = false;

    // Main loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Cambiar modo de cámara
        if (IsKeyPressed(KEY_ONE)) camMode = 1;
        if (IsKeyPressed(KEY_TWO)) camMode = 2;

        // Control de cámara según modo
        if (camMode == 1) {
            // Modo orbital alrededor del cuello
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

            float x = orbitRadius * cosf(orbitPitch) * sinf(orbitYaw);
            float y = orbitRadius * sinf(orbitPitch);
            float z = orbitRadius * cosf(orbitPitch) * cosf(orbitYaw);
            camera.position = (Vector3){ neckPos.x + x, neckPos.y + y, neckPos.z + z };
            camera.target = neckPos;
            camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
        } else {
            // Modo libre
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

        // Calcular parámetros para cada bonetile
        const float sectorAngles[] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f};
        float cellW = (float)textureA.width / AXIS_YAW;
        float cellH = (float)textureA.height / AXIS_PITCH;
        float aspect = cellW / cellH;
        Vector2 worldSize = (Vector2){ bonetileSize * aspect, bonetileSize };

        // --- Para el cuello (textureA) ---
        Vector3 camDirNeck = Vector3Subtract(camera.position, neckPos);
        float yawAngleNeck = atan2f(camDirNeck.x, camDirNeck.z);
        if (yawAngleNeck < 0.0f) yawAngleNeck += 2.0f*PI;
        float yawDegNeck = yawAngleNeck * RAD2DEG;

        float horizDistNeck = sqrtf(camDirNeck.x*camDirNeck.x + camDirNeck.z*camDirNeck.z);
        float pitchAngleNeck = atan2f(camDirNeck.y, horizDistNeck);
        float pitchNormNeck = (pitchAngleNeck + (PI*0.5f)) / PI;
        int rowIndexNeck = (int)floorf(pitchNormNeck * (float)AXIS_PITCH);
        rowIndexNeck = Clamp(rowIndexNeck, 0, AXIS_PITCH-1);

        bool useFixedTopNeck = (rowIndexNeck == 0);
        bool useFixedBottomNeck = (rowIndexNeck == AXIS_PITCH - 1);

        int baseColNeck = 0;
        bool mirroredNeck = false;

        int sectorNeck = 0;
        float minDiffNeck = 360.0f;
        for (int i = 0; i < 8; i++) {
            float diff = fabsf(yawDegNeck - sectorAngles[i]);
            if (diff > 180.0f) diff = 360.0f - diff;
            if (diff < minDiffNeck) { minDiffNeck = diff; sectorNeck = i; }
        }

        switch(sectorNeck) {
            case 0: baseColNeck = 0; mirroredNeck = false; break;
            case 1: baseColNeck = 1; mirroredNeck = true; break;
            case 2: baseColNeck = 2; mirroredNeck = true; break;
            case 3: baseColNeck = 3; mirroredNeck = true; break;
            case 4: baseColNeck = 4; mirroredNeck = false; break;
            case 5: baseColNeck = 3; mirroredNeck = false; break;
            case 6: baseColNeck = 2; mirroredNeck = false; break;
            case 7: baseColNeck = 1; mirroredNeck = false; break;
        }

        if (useFixedTopNeck || useFixedBottomNeck) {
            srcNeck = GetAtlasCellSrcPos(textureA, 0, useFixedTopNeck ? 0 : AXIS_PITCH-1, false, &finalMirroredNeck);
            rotationDegNeck = sectorAngles[sectorNeck];
            if (useFixedBottomNeck) {
                rotationDegNeck = 360.0f - rotationDegNeck;
                if (rotationDegNeck >= 360.0f) rotationDegNeck -= 360.0f;
            }
        } else {
            srcNeck = GetAtlasCellSrcPos(textureA, baseColNeck, rowIndexNeck, mirroredNeck, &finalMirroredNeck);
            rotationDegNeck = 0.0f;
        }

        // --- Para la nariz (textureB) ---
        Vector3 camDirNose = Vector3Subtract(camera.position, nosePos);
        float yawAngleNose = atan2f(camDirNose.x, camDirNose.z);
        if (yawAngleNose < 0.0f) yawAngleNose += 2.0f*PI;
        float yawDegNose = yawAngleNose * RAD2DEG;

        float horizDistNose = sqrtf(camDirNose.x*camDirNose.x + camDirNose.z*camDirNose.z);
        float pitchAngleNose = atan2f(camDirNose.y, horizDistNose);
        float pitchNormNose = (pitchAngleNose + (PI*0.5f)) / PI;
        int rowIndexNose = (int)floorf(pitchNormNose * (float)AXIS_PITCH);
        rowIndexNose = Clamp(rowIndexNose, 0, AXIS_PITCH-1);

        bool useFixedTopNose = (rowIndexNose == 0);
        bool useFixedBottomNose = (rowIndexNose == AXIS_PITCH - 1);

        int baseColNose = 0;
        bool mirroredNose = false;

        int sectorNose = 0;
        float minDiffNose = 360.0f;
        for (int i = 0; i < 8; i++) {
            float diff = fabsf(yawDegNose - sectorAngles[i]);
            if (diff > 180.0f) diff = 360.0f - diff;
            if (diff < minDiffNose) { minDiffNose = diff; sectorNose = i; }
        }

        switch(sectorNose) {
            case 0: baseColNose = 0; mirroredNose = false; break;
            case 1: baseColNose = 1; mirroredNose = true; break;
            case 2: baseColNose = 2; mirroredNose = true; break;
            case 3: baseColNose = 3; mirroredNose = true; break;
            case 4: baseColNose = 4; mirroredNose = false; break;
            case 5: baseColNose = 3; mirroredNose = false; break;
            case 6: baseColNose = 2; mirroredNose = false; break;
            case 7: baseColNose = 1; mirroredNose = false; break;
        }

        if (useFixedTopNose || useFixedBottomNose) {
            srcNose = GetAtlasCellSrcPos(textureB, 0, useFixedTopNose ? 0 : AXIS_PITCH-1, false, &finalMirroredNose);
            rotationDegNose = sectorAngles[sectorNose];
            if (useFixedTopNose) {
                rotationDegNose = 360.0f - rotationDegNose;
                if (rotationDegNose >= 360.0f) rotationDegNose -= 360.0f;
            }
        } else {
            srcNose = GetAtlasCellSrcPos(textureB, baseColNose, rowIndexNose, mirroredNose, &finalMirroredNose);
            rotationDegNose = 0.0f;
        }

        // Dibujado
        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(camera);
                DrawGrid(20, 1.0f);

                // Dibujar marcadores de posición
                DrawSphereWires(neckPos, 0.05f, 8, 8, RED);
                DrawSphereWires(nosePos, 0.05f, 8, 8, BLUE);

                // Dibujar bonetiles en orden de profundidad
                float dNeck = Vector3Distance(camera.position, neckPos);
                float dNose = Vector3Distance(camera.position, nosePos);

                rlDisableDepthTest();
                BeginBlendMode(BLEND_ALPHA);

                    if (dNeck > dNose) {
                        DrawBonetileCustom(textureA, camera, srcNeck, neckPos,
								worldSize, rotationDegNeck, finalMirroredNeck);
                        DrawBonetileCustom(textureB, camera, srcNose, nosePos,
								worldSize, rotationDegNose, finalMirroredNose);
                    } else {
                        DrawBonetileCustom(textureB, camera, srcNose, nosePos,
								worldSize, rotationDegNose, finalMirroredNose);
                        DrawBonetileCustom(textureA, camera, srcNeck, neckPos,
								worldSize, rotationDegNeck, finalMirroredNeck);
                    }

                rlEnableDepthMask();
                EndBlendMode();

            EndMode3D();

            // Información en pantalla
            DrawText(TextFormat("Modo: %s (1 Orbit / 2 Libre)", camMode==1?"Orbit":"Libre"), 10, 10, 20, DARKGRAY);
            DrawText(TextFormat("Cuello: (%.2f, %.2f, %.2f)", neckPos.x, neckPos.y, neckPos.z), 10, 40, 20, RED);
            DrawText(TextFormat("Nariz: (%.2f, %.2f, %.2f)", nosePos.x, nosePos.y, nosePos.z), 10, 70, 20, BLUE);
            DrawText(TextFormat("Camara: (%.2f, %.2f, %.2f)", camera.position.x, camera.position.y, camera.position.z), 10, 100, 20, DARKGRAY);
            DrawText(TextFormat("Cuello: Yaw %.1f°, Pitch %.1f°, Sector %d", yawDegNeck, pitchAngleNeck*RAD2DEG, sectorNeck), 10, 130, 20, RED);
            DrawText(TextFormat("Nariz: Yaw %.1f°, Pitch %.1f°, Sector %d", yawDegNose, pitchAngleNose*RAD2DEG, sectorNose), 10, 160, 20, BLUE);
        EndDrawing();
    }

    // Liberar recursos
    UnloadTexture(textureA);
    UnloadTexture(textureB);

    CloseWindow();
    return 0;
}
