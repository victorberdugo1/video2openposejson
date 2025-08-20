#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "bonetile.h"
#include "bones3d.h"

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

// Función para calcular datos de atlas basado en ángulo de cámara
void CalculateBoneRenderData(Vector3 bonePos, Camera camera, 
                             int* outChosenIndex, float* outRotation, bool* outMirrored) 
{
    const int indices[3][8] = {
        {  0,  4,  5,  6,  7,  6,  5,  4 },
        {  2, 12, 13, 14, 15, 14, 13, 12 },
        {  1,  8,  9, 10, 11, 10,  9,  8 }
    };
    const int topdownIndex = 3;
    const float sectorAngles[8] = {0,45,90,135,180,225,270,315};
    const float TOPDOWN_ANGLE = 70.0f;
    const float HIGH_THRESHOLD = 22.5f;
    const float MAIN_THRESHOLD = -22.5f;

    // Dirección de la cámara hacia el hueso
    Vector3 camDir = Vector3Subtract(camera.position, bonePos);
    float yaw = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;
    float yawDeg = yaw * RAD2DEG;

    float horiz = sqrtf(camDir.x*camDir.x + camDir.z*camDir.z);
    float pitch = atan2f(camDir.y, horiz);
    float pitchDeg = pitch * RAD2DEG;

    // Determinar fila
    int chosenRow = -1;
    bool useTopdown = false;
    bool isTopView = false;

    if (pitchDeg >= TOPDOWN_ANGLE) {
        useTopdown = true;
        isTopView = true;
    } else if (pitchDeg >= HIGH_THRESHOLD) {
        chosenRow = 2;
    } else if (pitchDeg >= MAIN_THRESHOLD) {
        chosenRow = 0;
    } else if (pitchDeg >= -TOPDOWN_ANGLE) {
        chosenRow = 1;
    } else {
        useTopdown = true;
        isTopView = false;
    }

    // Sector más cercano
    int sector = 0;
    float minDiff = 360.0f;
    for (int i = 0; i < 8; i++) {
        float diff = fabsf(yawDeg - sectorAngles[i]);
        if (diff > 180.0f) diff = 360.0f - diff;
        if (diff < minDiff) { minDiff = diff; sector = i; }
    }

    // Asignar resultados
    if (useTopdown) {
        *outChosenIndex = topdownIndex;

        if (isTopView) {
            *outRotation = sectorAngles[sector] + 180.0f;
            *outMirrored = false;
        } else { // bottom view
            *outRotation = 360.0f - sectorAngles[sector];
            *outMirrored = true;
        }
    } else {
        *outChosenIndex = indices[chosenRow][sector];
        *outRotation = 0.0f;
        *outMirrored = !(sector >= 5 && sector <= 7);
    }
}

int main(void) {
    const int screenW = 2056;
    const int screenH = 1504;
    
    InitWindow(screenW, screenH, "Bones3D - Bonetiles System");
    SetTargetFPS(60);
    
    // Configurar cámara
    Camera camera = {0};
    camera.position = (Vector3){0.0f, 0.6f, 2.5f};
    camera.target   = (Vector3){0.0f, 0.6f, 0.0f};
    camera.up       = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy     = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    
    int camMode = 1;
    float orbitYaw = 0.0f, orbitPitch = -0.2f, orbitRadius = 2.5f;
    
    // ========================================================================
    // INICIALIZAR SISTEMA BONES3D
    // ========================================================================
    BonesAnimation animation;
    BonesError result = BonesInit(&animation, 100);
    
    if (result != BONES_SUCCESS) {
        TraceLog(LOG_ERROR, "Error inicializando Bones3D: %s", BonesGetErrorString(result));
        CloseWindow();
        return -1;
    }
    
    // Cargar datos desde archivo JSON
    result = BonesLoadFromJSON(&animation, "test.json");
    if (result != BONES_SUCCESS) {
        TraceLog(LOG_WARNING, "No se pudo cargar test_data.json: %s", BonesGetErrorString(result));
        TraceLog(LOG_INFO, "Usando posiciones hardcodeadas como fallback");
        
        // Fallback: usar posiciones hardcodeadas como antes
        // (mantenemos compatibilidad con tu código original)
    } else {
        TraceLog(LOG_INFO, "Bones3D cargado exitosamente: %d frames", BonesGetFrameCount(&animation));
        BonesSetFrame(&animation, 0); // Usar primer frame
    }
    
    // Configurar renderizado
    BonesRenderConfig config = BonesGetDefaultRenderConfig();
    config.drawDebugSpheres = true;
    config.debugColor = GREEN;
    config.debugSphereRadius = 0.035f;
    BonesSetRenderConfig(&config);
    
    // ========================================================================
    // CARGAR TEXTURAS
    // ========================================================================
    float bonetileSize = 0.35f;
    Texture2D texA = {0}, texB = {0};
    
    // Textura A (Neck)
    Image imgA = LoadImage("texA.png");
    if (imgA.data == NULL) {
        imgA = GenImageColor(1024, 1024, CLITERAL(Color){60,120,220,255});
        ImageDrawText(&imgA, "NECK", 8, 8, 128, WHITE);
    }
    texA = LoadTextureFromImage(imgA); 
    UnloadImage(imgA);
    SetTextureFilter(texA, TEXTURE_FILTER_POINT);
    SetTextureWrap(texA, TEXTURE_WRAP_CLAMP);
    
    // Textura B (Nose)
    Image imgB = LoadImage("texB.png");
    if (imgB.data == NULL) {
        imgB = GenImageColor(1024, 1024, CLITERAL(Color){120,200,80,255});
        ImageDrawText(&imgB, "NOSE", 8, 8, 128, BLACK);
    }
    texB = LoadTextureFromImage(imgB); 
    UnloadImage(imgB);
    SetTextureFilter(texB, TEXTURE_FILTER_POINT);
    SetTextureWrap(texB, TEXTURE_WRAP_CLAMP);
    
    const int physColsA = 8, physRowsA = 8;
    const int physColsB = 8, physRowsB = 8;
    
    // Variables de render
    Rectangle srcNeck = {0}, srcNose = {0};
    float rotNeck = 0.0f, rotNose = 0.0f;
    bool finalMirrNeck = false, finalMirrNose = false;
    
    // Variables para información
    bool usingBones3D = animation.isLoaded;
    Vector3 neckPos = {0}, nosePos = {0};
    
    // Fallback hardcodeado (compatibilidad)
    Vector3 fallbackNeckPos = {
        0.753111869096756f * 2.0f - 1.0f,
        1.0f - 0.26760193705558777f,
        0.210185244679451f * 2.0f - 1.0f
    };
    Vector3 fallbackNosePos = {
        0.7485565543174744f * 2.0f - 1.0f,
        1.0f - 0.18256297707557678f,
        0.25478237867355347f * 2.0f - 1.0f
    };
    
    // ========================================================================
    // LOOP PRINCIPAL
    // ========================================================================
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // Cambio de modo de cámara
        if (IsKeyPressed(KEY_ONE)) camMode = 1;
        if (IsKeyPressed(KEY_TWO)) camMode = 2;
        
        // ====================================================================
        // OBTENER POSICIONES DE BONES
        // ====================================================================
        bool neckValid = false, noseValid = false;
        
        if (usingBones3D) {
            // Usar sistema Bones3D
            BonesError neckResult = BonesGetBonePosition(&animation, BonesGetCurrentFrame(&animation), 
                                                        "person_0", "Neck", &neckPos);
            BonesError noseResult = BonesGetBonePosition(&animation, BonesGetCurrentFrame(&animation), 
                                                        "person_0", "Nose", &nosePos);
            
            neckValid = (neckResult == BONES_SUCCESS);
            noseValid = (noseResult == BONES_SUCCESS);
        } 
        
        // Fallback a posiciones hardcodeadas si es necesario
        if (!neckValid) {
            neckPos = fallbackNeckPos;
            neckValid = true;
        }
        if (!noseValid) {
            nosePos = fallbackNosePos;
            noseValid = true;
        }
        
        // ====================================================================
        // CONTROL DE CÁMARA
        // ====================================================================
        if (camMode == 1) {
            // Modo órbita alrededor del cuello
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
            // Modo vuelo libre
            Vector2 md = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                orbitYaw += md.x * 0.002f;
                orbitPitch += -md.y * 0.002f;
                orbitPitch = Clamp(orbitPitch, -1.49f, 1.49f);
            }
            
            Vector3 forwardDir = { 
                sinf(orbitYaw) * cosf(orbitPitch), 
                sinf(orbitPitch), 
                cosf(orbitYaw) * cosf(orbitPitch) 
            };
            forwardDir = Vector3Normalize(forwardDir);
            Vector3 rightDir = Vector3Normalize(Vector3CrossProduct((Vector3){0,1,0}, forwardDir));
            
            float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 6.0f : 2.5f;
            if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(forwardDir, speed*dt));
            if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(forwardDir, speed*dt));
            if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(rightDir, speed*dt));
            if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(rightDir, speed*dt));
            
            camera.target = Vector3Add(camera.position, forwardDir);
        }
        
        // ====================================================================
        // CALCULAR DATOS DE RENDER PARA CADA BONE
        // ====================================================================
        int chosenIndexN = 0, chosenIndexO = 0;
        
        if (neckValid) {
            CalculateBoneRenderData(neckPos, camera, &chosenIndexN, &rotNeck, &finalMirrNeck);
            int logicalColNeck = chosenIndexN % ATLAS_COLS;
            int logicalRowNeck = chosenIndexN / ATLAS_COLS;
            srcNeck = SrcFromLogical(texA, logicalColNeck, logicalRowNeck, physColsA, physRowsA, 
                                   finalMirrNeck, &finalMirrNeck);
        }
        
        if (noseValid) {
            CalculateBoneRenderData(nosePos, camera, &chosenIndexO, &rotNose, &finalMirrNose);
            int logicalColNose = chosenIndexO % ATLAS_COLS;
            int logicalRowNose = chosenIndexO / ATLAS_COLS;
            srcNose = SrcFromLogical(texB, logicalColNose, logicalRowNose, physColsB, physRowsB, 
                                   finalMirrNose, &finalMirrNose);
        }
        
        // Calcular tamaño en mundo
        float physCellW = (float)texA.width / (float)physColsA;
        float physCellH = (float)texA.height / (float)physRowsA;
        float logicalCellW = physCellW * (physColsA / ATLAS_COLS);
        float logicalCellH = physCellH * (physRowsA / ATLAS_ROWS);
        float aspect = logicalCellW / logicalCellH;
        Vector2 worldSize = (Vector2){ bonetileSize * aspect, bonetileSize };
        
        // ====================================================================
        // RENDERIZADO
        // ====================================================================
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            BeginMode3D(camera);
                DrawGrid(24, 0.5f);
                
                // Dibujar esferas de debug
                if (neckValid) DrawSphereWires(neckPos, 0.035f, 8, 8, RED);
                if (noseValid) DrawSphereWires(nosePos, 0.035f, 8, 8, BLUE);
                
                // Dibujar billboards con depth sorting
                if (neckValid && noseValid) {
                    float dNeck = Vector3Distance(camera.position, neckPos);
                    float dNose = Vector3Distance(camera.position, nosePos);
                    
                    rlDisableDepthTest();
                    BeginBlendMode(BLEND_ALPHA);
                    
                    // Dibujar el más lejano primero
                    if (dNeck > dNose) {
                        DrawBonetileCustom(texA, camera, srcNeck, neckPos, worldSize, rotNeck, finalMirrNeck);
                        DrawBonetileCustom(texB, camera, srcNose, nosePos, worldSize, rotNose, finalMirrNose);
                    } else {
                        DrawBonetileCustom(texB, camera, srcNose, nosePos, worldSize, rotNose, finalMirrNose);
                        DrawBonetileCustom(texA, camera, srcNeck, neckPos, worldSize, rotNeck, finalMirrNeck);
                    }
                    
                    EndBlendMode();
                    rlEnableDepthMask();
                }
                
            EndMode3D();
            
            // ================================================================
            // UI OVERLAY - ATLAS GRID
            // ================================================================
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
                    
                    Color bg = LIGHTGRAY;
                    if (neckValid && idx == chosenIndexN) { 
                        bg = Fade(RED, 0.85f); 
                    } else if (noseValid && idx == chosenIndexO) { 
                        bg = Fade(BLUE, 0.85f); 
                    }
                    
                    DrawRectangleRec(rcell, bg);
                    DrawRectangleLines((int)rcell.x, (int)rcell.y, (int)rcell.width, (int)rcell.height, GRAY);
                }
            }
            
            // ================================================================
            // INFORMACIÓN DE DEBUG
            // ================================================================
            DrawText("BONES3D SYSTEM", 20, 20, 20, DARKGREEN);
            
            // Estado del sistema
            const char* systemStatus = usingBones3D ? "LOADED from JSON" : "FALLBACK (hardcoded)";
            Color statusColor = usingBones3D ? DARKGREEN : ORANGE;
            DrawText(TextFormat("Data Source: %s", systemStatus), 20, 50, 16, statusColor);
            
            if (usingBones3D) {
                DrawText(TextFormat("Frames: %d | Current: %d", 
                        BonesGetFrameCount(&animation), BonesGetCurrentFrame(&animation)), 
                        20, 70, 16, DARKGRAY);
            }
            
            // Info de bones
            DrawText(TextFormat("Neck: idx %02d rot %.1f mirror %s", 
                               chosenIndexN, rotNeck, finalMirrNeck ? "YES" : "NO"),
                     20, screenH - 80, 16, RED);
            DrawText(TextFormat("Nose: idx %02d rot %.1f mirror %s", 
                               chosenIndexO, rotNose, finalMirrNose ? "YES" : "NO"),
                     20, screenH - 60, 16, BLUE);
            
            // Controles
            DrawText("Controls: 1=Orbit Camera | 2=Free Camera | Mouse+WASD", 20, screenH - 40, 14, DARKGRAY);
            DrawText(TextFormat("Camera Mode: %s", camMode == 1 ? "ORBIT" : "FREE"), 20, screenH - 20, 14, DARKGRAY);
            
        EndDrawing();
    }
    
    // ========================================================================
    // LIMPIEZA
    // ========================================================================
    UnloadTexture(texA);
    UnloadTexture(texB);
    BonesFree(&animation);
    CloseWindow();
    
    return 0;
}
