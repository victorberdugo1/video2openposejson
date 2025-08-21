#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "bonetile.h"
#include "bones3d.h"
#include <string.h>
// Tamaño base para diseño responsive
#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080

// Estructura para datos de renderizado de un bone
typedef struct {
    Vector3 position;
    int atlasIndex;
    float rotation;
    bool mirrored;
    float distance; // Para depth sorting
    char boneName[MAX_BONE_NAME_LENGTH];
    char personId[16];
    int textureIndex;
    bool valid;
} BoneRenderData;

// Array dinámico para almacenar datos de render
static BoneRenderData* renderBones = NULL;
static int renderBonesCount = 0;
static int renderBonesCapacity = 0;

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

// Función para escalar valores según el tamaño de la pantalla
float ScaleValue(float value, int screenDimension, int baseDimension) {
    return value * ((float)screenDimension / (float)baseDimension);
}

// Función para escalar fuentes
int ScaleFontSize(int baseSize, int screenHeight) {
    return (int)(baseSize * ((float)screenHeight / BASE_HEIGHT));
}

// Función para redimensionar el array de render bones
bool ResizeRenderBonesArray(int newCapacity) {
    if (newCapacity <= renderBonesCapacity) return true;
    
    BoneRenderData* newArray = (BoneRenderData*)realloc(renderBones, 
                                                       sizeof(BoneRenderData) * newCapacity);
    if (!newArray) return false;
    
    renderBones = newArray;
    renderBonesCapacity = newCapacity;
    return true;
}

// Función de comparación para sorting por profundidad
int CompareBonesByDistance(const void* a, const void* b) {
    const BoneRenderData* boneA = (const BoneRenderData*)a;
    const BoneRenderData* boneB = (const BoneRenderData*)b;
    
    // Ordenar de mayor a menor distancia (los más lejanos primero)
    if (boneA->distance > boneB->distance) return -1;
    if (boneA->distance < boneB->distance) return 1;
    return 0;
}

// Función para determinar el índice de textura basado en el nombre del bone
int GetTextureIndexForBone(const char* boneName) {
    // Puedes personalizar esto según tus necesidades
    // Por ejemplo, diferentes texturas para diferentes tipos de huesos
    
    // Huesos de la cabeza/cara
    if (strstr(boneName, "Nose") || strstr(boneName, "Eye") || 
        strstr(boneName, "Ear") || strstr(boneName, "Head")) {
        return 1; // texB
    }
    
    // Huesos del torso/cuello
    if (strstr(boneName, "Neck") || strstr(boneName, "Shoulder") ||
        strstr(boneName, "Chest") || strstr(boneName, "Spine")) {
        return 0; // texA  
    }
    
    // Huesos de extremidades
    if (strstr(boneName, "Elbow") || strstr(boneName, "Wrist") ||
        strstr(boneName, "Knee") || strstr(boneName, "Ankle") ||
        strstr(boneName, "Hip")) {
        return 2; // texC (si tienes más texturas)
    }
    
    // Por defecto, usar texA
    return 0;
}

// Función para recopilar todos los bones visibles para renderizado
void CollectBonesForRendering(const BonesAnimation* animation, Camera camera) {
    renderBonesCount = 0;
    
    if (!animation->isLoaded) return;
    
    int currentFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, currentFrame)) return;
    
    const AnimationFrame* frame = &animation->frames[currentFrame];
    
    // Estimar capacidad necesaria
    int estimatedBones = 0;
    for (int p = 0; p < frame->personCount; p++) {
        if (frame->persons[p].active) {
            estimatedBones += frame->persons[p].boneCount;
        }
    }
    
    if (!ResizeRenderBonesArray(estimatedBones + 10)) {
        TraceLog(LOG_ERROR, "No se pudo redimensionar array de render bones");
        return;
    }
    
    // Recopilar todos los bones válidos
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid || !bone->visible) continue;
            
            // Validar posición
            if (!BonesIsPositionValid(bone->position.position)) continue;
            
            // Calcular distancia a la cámara
            float distance = Vector3Distance(camera.position, bone->position.position);
            
            // Filtrar bones muy lejanos si es necesario
            if (distance > 50.0f) continue;
            
            // Calcular datos de atlas
            int atlasIndex;
            float rotation;
            bool mirrored;
            CalculateBoneRenderData(bone->position.position, camera, 
                                  &atlasIndex, &rotation, &mirrored);
            
            // Añadir al array de render
            BoneRenderData* renderBone = &renderBones[renderBonesCount];
            renderBone->position = bone->position.position;
            renderBone->atlasIndex = atlasIndex;
            renderBone->rotation = rotation;
            renderBone->mirrored = mirrored;
            renderBone->distance = distance;
            renderBone->textureIndex = GetTextureIndexForBone(bone->name);
            renderBone->valid = true;
            
            strncpy(renderBone->boneName, bone->name, MAX_BONE_NAME_LENGTH - 1);
            renderBone->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
            
            strncpy(renderBone->personId, person->personId, 15);
            renderBone->personId[15] = '\0';
            
            renderBonesCount++;
        }
    }
    
    // Ordenar por distancia para renderizado correcto
    if (renderBonesCount > 1) {
        qsort(renderBones, renderBonesCount, sizeof(BoneRenderData), CompareBonesByDistance);
    }
}

int main(void) {
    // Inicializar ventana en tamaño base pero permitir redimensionamiento
    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - Dynamic Multi-bone System");
    
    // Configurar ventana para que sea redimensionable y maximizada
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    
    // Solución específica para Linux - maximizar después de un breve retraso
    #if defined(__linux__)
        // Pequeña espera para asegurar que la ventana esté completamente inicializada
        for (int i = 0; i < 5; i++) {
            PollInputEvents();
        }
    #endif
    
    MaximizeWindow();
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
    
    // Variables para control de cámara mejorado
    bool cameraMouseControl = false;
    
    // ========================================================================
    // INICIALIZAR SISTEMA BONES3D
    // ========================================================================
    BonesAnimation animation;
    BonesError result = BonesInit(&animation, 1000); // Aumentar capacidad
    
    if (result != BONES_SUCCESS) {
        TraceLog(LOG_ERROR, "Error inicializando Bones3D: %s", BonesGetErrorString(result));
        CloseWindow();
        return -1;
    }
    
    // Cargar datos desde archivo JSON
    result = BonesLoadFromJSON(&animation, "test.json");
    if (result != BONES_SUCCESS) {
        TraceLog(LOG_WARNING, "No se pudo cargar test.json: %s", BonesGetErrorString(result));
        TraceLog(LOG_INFO, "Sistema funcionará sin datos de bones");
    } else {
        TraceLog(LOG_INFO, "Bones3D cargado exitosamente: %d frames", BonesGetFrameCount(&animation));
        BonesSetFrame(&animation, 0);
        
        // Debug: imprimir información del primer frame
        BonesPrintFrameInfo(&animation, 0);
    }
    
    // Configurar renderizado
    BonesRenderConfig config = BonesGetDefaultRenderConfig();
    config.drawDebugSpheres = true;
    config.debugColor = GREEN;
    config.debugSphereRadius = 0.035f;
    config.enableDepthSorting = true;
    BonesSetRenderConfig(&config);
    
    // ========================================================================
    // CARGAR TEXTURAS DINÁMICAMENTE
    // ========================================================================
    const int MAX_TEXTURES = 4;
    Texture2D textures[MAX_TEXTURES];
    int textureCount = 0;
    
    // Textura A (para huesos del torso/cuello)
    Image imgA = LoadImage("texA.png");
    if (imgA.data == NULL) {
        imgA = GenImageColor(1024, 1024, CLITERAL(Color){60,120,220,255});
        ImageDrawText(&imgA, "TORSO", 8, 8, 128, WHITE);
    }
    textures[0] = LoadTextureFromImage(imgA); 
    UnloadImage(imgA);
    SetTextureFilter(textures[0], TEXTURE_FILTER_POINT);
    SetTextureWrap(textures[0], TEXTURE_WRAP_CLAMP);
    textureCount++;
    
    // Textura B (para huesos de la cabeza)
    Image imgB = LoadImage("texB.png");
    if (imgB.data == NULL) {
        imgB = GenImageColor(1024, 1024, CLITERAL(Color){120,200,80,255});
        ImageDrawText(&imgB, "HEAD", 8, 8, 128, BLACK);
    }
    textures[1] = LoadTextureFromImage(imgB); 
    UnloadImage(imgB);
    SetTextureFilter(textures[1], TEXTURE_FILTER_POINT);
    SetTextureWrap(textures[1], TEXTURE_WRAP_CLAMP);
    textureCount++;
    
    // Textura C (para extremidades) - opcional
    Image imgC = LoadImage("texC.png");
    if (imgC.data == NULL) {
        imgC = GenImageColor(1024, 1024, CLITERAL(Color){220,80,120,255});
        ImageDrawText(&imgC, "LIMBS", 8, 8, 128, WHITE);
    }
    textures[2] = LoadTextureFromImage(imgC); 
    UnloadImage(imgC);
    SetTextureFilter(textures[2], TEXTURE_FILTER_POINT);
    SetTextureWrap(textures[2], TEXTURE_WRAP_CLAMP);
    textureCount++;
    
    const int physCols = 8, physRows = 8;
    float bonetileSize = 0.35f;
    
    // Variables para navegación de frames
    int maxFrames = BonesGetFrameCount(&animation);
    int currentFrame = 0;
    bool autoPlay = false;
    float autoPlayTimer = 0.0f;
    float autoPlaySpeed = 0.1f; // segundos entre frames
    
    // Calcular centro automático basado en los bones
    Vector3 autoCenter = {0};
    bool autoCenterCalculated = false;
    
    // ========================================================================
    // LOOP PRINCIPAL
    // ========================================================================
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int currentScreenWidth = GetScreenWidth();
        int currentScreenHeight = GetScreenHeight();
        
        // ====================================================================
        // CONTROLES DE NAVEGACIÓN DE FRAMES
        // ====================================================================
        if (animation.isLoaded && maxFrames > 0) {
            if (IsKeyPressed(KEY_LEFT) && currentFrame > 0) {
                currentFrame--;
                BonesSetFrame(&animation, currentFrame);
                autoCenterCalculated = false;
            }
            if (IsKeyPressed(KEY_RIGHT) && currentFrame < maxFrames - 1) {
                currentFrame++;
                BonesSetFrame(&animation, currentFrame);
                autoCenterCalculated = false;
            }
            if (IsKeyPressed(KEY_HOME)) {
                currentFrame = 0;
                BonesSetFrame(&animation, currentFrame);
                autoCenterCalculated = false;
            }
            if (IsKeyPressed(KEY_END) && maxFrames > 0) {
                currentFrame = maxFrames - 1;
                BonesSetFrame(&animation, currentFrame);
                autoCenterCalculated = false;
            }
            if (IsKeyPressed(KEY_SPACE)) {
                autoPlay = !autoPlay;
            }
            
            // Auto-reproducir
            if (autoPlay && maxFrames > 1) {
                autoPlayTimer += dt;
                if (autoPlayTimer >= autoPlaySpeed) {
                    autoPlayTimer = 0.0f;
                    currentFrame = (currentFrame + 1) % maxFrames;
                    BonesSetFrame(&animation, currentFrame);
                    autoCenterCalculated = false;
                }
            }
        }
        
        // ====================================================================
        // CALCULAR CENTRO AUTOMÁTICO
        // ====================================================================
        if (animation.isLoaded && !autoCenterCalculated) {
            if (BonesIsValidFrame(&animation, currentFrame)) {
                const AnimationFrame* frame = &animation.frames[currentFrame];
                Vector3 totalPos = {0};
                int validBoneCount = 0;
                
                for (int p = 0; p < frame->personCount; p++) {
                    const Person* person = &frame->persons[p];
                    if (!person->active) continue;
                    
                    for (int b = 0; b < person->boneCount; b++) {
                        const Bone* bone = &person->bones[b];
                        if (bone->position.valid && BonesIsPositionValid(bone->position.position)) {
                            totalPos = Vector3Add(totalPos, bone->position.position);
                            validBoneCount++;
                        }
                    }
                }
                
                if (validBoneCount > 0) {
                    autoCenter = Vector3Scale(totalPos, 1.0f / validBoneCount);
                    autoCenterCalculated = true;
                }
            }
        }
        
        // Cambio de modo de cámara
        if (IsKeyPressed(KEY_ONE)) {
            camMode = 1;
            cameraMouseControl = false;
            EnableCursor();
        }
        if (IsKeyPressed(KEY_TWO)) {
            camMode = 2;
            cameraMouseControl = true;
            DisableCursor();
            
            // Usar el centro calculado automáticamente
            Vector3 target = autoCenterCalculated ? autoCenter : (Vector3){0, 0.6f, 0};
            camera.position = (Vector3){target.x, target.y + 0.5f, target.z + 2.0f};
            camera.target = target;
            
            // Calcular ángulos iniciales para la cámara
            Vector3 direction = Vector3Subtract(target, camera.position);
            orbitYaw = atan2f(direction.x, direction.z);
            orbitPitch = atan2f(direction.y, sqrtf(direction.x*direction.x + direction.z*direction.z));
        }
        
        // Alternar control de ratón
        if (IsKeyPressed(KEY_M)) {
            cameraMouseControl = !cameraMouseControl;
            if (cameraMouseControl) {
                DisableCursor();
            } else {
                EnableCursor();
            }
        }
        
        // ====================================================================
        // CONTROL DE CÁMARA
        // ====================================================================
        Vector3 cameraTarget = autoCenterCalculated ? autoCenter : (Vector3){0, 0.6f, 0};
        
        if (camMode == 1) {
            // Modo órbita
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 mouseDelta = GetMouseDelta();
                orbitYaw += mouseDelta.x * 0.01f;
                orbitPitch += -mouseDelta.y * 0.01f;
                orbitPitch = Clamp(orbitPitch, -1.4f, 1.4f);
            }
            
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                orbitRadius -= wheel * 0.5f;
                orbitRadius = Clamp(orbitRadius, 0.5f, 20.0f);
            }
            
            float x = orbitRadius * cosf(orbitPitch) * sinf(orbitYaw);
            float y = orbitRadius * sinf(orbitPitch);
            float z = orbitRadius * cosf(orbitPitch) * cosf(orbitYaw);
            
            camera.position = (Vector3){ cameraTarget.x + x, cameraTarget.y + y, cameraTarget.z + z };
            camera.target = cameraTarget;
        } else {
            // Modo FPS
            if (cameraMouseControl) {
                Vector2 mouseDelta = GetMouseDelta();
                orbitYaw -= mouseDelta.x * 0.003f;
                orbitPitch -= mouseDelta.y * 0.003f;
                orbitPitch = Clamp(orbitPitch, -1.49f, 1.49f);
            }
            
            Vector3 forward = {
                sinf(orbitYaw) * cosf(orbitPitch),
                sinf(orbitPitch),
                cosf(orbitYaw) * cosf(orbitPitch)
            };
            forward = Vector3Normalize(forward);
            
            Vector3 right = {
                sinf(orbitYaw - PI/2),
                0,
                cosf(orbitYaw - PI/2)
            };
            right = Vector3Normalize(right);
            
            // Movimiento de la cámara
            float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 8.0f : 3.0f;
            if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(forward, speed*dt));
            if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(forward, speed*dt));
            if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(right, speed*dt));
            if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(right, speed*dt));
            
            // Movimiento vertical
            if (IsKeyDown(KEY_SPACE)) camera.position.y += speed * dt;
            if (IsKeyDown(KEY_LEFT_CONTROL)) camera.position.y -= speed * dt;
            
            camera.target = Vector3Add(camera.position, forward);
        }
        
        // ====================================================================
        // RECOPILAR BONES PARA RENDERIZADO
        // ====================================================================
        CollectBonesForRendering(&animation, camera);
        
        // ====================================================================
        // RENDERIZADO
        // ====================================================================
        BeginDrawing();
            ClearBackground(RAYWHITE);
            
            BeginMode3D(camera);
                DrawGrid(24, 0.5f);
                
                // Dibujar centro automático
                if (autoCenterCalculated) {
                    DrawSphereWires(autoCenter, 0.05f, 8, 8, ORANGE);
                }
                
                // Renderizar todos los bones dinámicamente
                if (renderBonesCount > 0) {
                    rlDisableDepthTest();
                    BeginBlendMode(BLEND_ALPHA);
                    
                    // Calcular tamaño del mundo una vez
                    float physCellW = (float)textures[0].width / (float)physCols;
                    float physCellH = (float)textures[0].height / (float)physRows;
                    float logicalCellW = physCellW * (physCols / ATLAS_COLS);
                    float logicalCellH = physCellH * (physRows / ATLAS_ROWS);
                    float aspect = logicalCellW / logicalCellH;
                    Vector2 worldSize = (Vector2){ bonetileSize * aspect, bonetileSize };
                    
                    for (int i = 0; i < renderBonesCount; i++) {
                        const BoneRenderData* bone = &renderBones[i];
                        if (!bone->valid) continue;
                        
                        // Seleccionar textura
                        int texIndex = bone->textureIndex;
                        if (texIndex >= textureCount) texIndex = 0;
                        Texture2D currentTex = textures[texIndex];
                        
                        // Calcular rectángulo fuente
                        int logicalCol = bone->atlasIndex % ATLAS_COLS;
                        int logicalRow = bone->atlasIndex / ATLAS_COLS;
                        bool finalMirror = false;
                        Rectangle src = SrcFromLogical(currentTex, logicalCol, logicalRow, 
                                                     physCols, physRows, bone->mirrored, &finalMirror);
                        
                        // Dibujar bone
                        DrawBonetileCustom(currentTex, camera, src, bone->position, 
                                         worldSize, bone->rotation, finalMirror);
                        
                        // Debug sphere
                        if (config.drawDebugSpheres) {
                            Color debugCol = (texIndex == 0) ? RED : 
                                           (texIndex == 1) ? BLUE : 
                                           (texIndex == 2) ? PURPLE : GREEN;
                            DrawSphereWires(bone->position, config.debugSphereRadius, 8, 8, debugCol);
                        }
                    }
                    
                    EndBlendMode();
                    rlEnableDepthMask();
                }
                
            EndMode3D();
            
            // ================================================================
            // UI OVERLAY (Responsive)
            // ================================================================
            int baseFontSize = ScaleFontSize(16, currentScreenHeight);
            int titleSize = ScaleFontSize(20, currentScreenHeight);
            int smallFontSize = ScaleFontSize(12, currentScreenHeight);
            
            // Información principal
            DrawText("BONES3D DYNAMIC SYSTEM", 
                    ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                    ScaleValue(20, currentScreenHeight, BASE_HEIGHT), 
                    titleSize, DARKGREEN);
            
            // Estado del sistema
            if (animation.isLoaded) {
                DrawText(TextFormat("JSON LOADED | Frames: %d | Current: %d", 
                        maxFrames, currentFrame), 
                        ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                        ScaleValue(50, currentScreenHeight, BASE_HEIGHT), 
                        baseFontSize, DARKGREEN);
                
                DrawText(TextFormat("Bones Rendered: %d | Auto-play: %s", 
                        renderBonesCount, autoPlay ? "ON" : "OFF"), 
                        ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                        ScaleValue(70, currentScreenHeight, BASE_HEIGHT), 
                        baseFontSize, DARKBLUE);
            } else {
                DrawText("NO DATA LOADED", 
                        ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                        ScaleValue(50, currentScreenHeight, BASE_HEIGHT), 
                        baseFontSize, ORANGE);
            }
            
            // Lista de bones activos (solo algunos para no saturar)
            int yOffset = ScaleValue(100, currentScreenHeight, BASE_HEIGHT);
            int maxBonesToShow = 10;
            for (int i = 0; i < renderBonesCount && i < maxBonesToShow; i++) {
                const BoneRenderData* bone = &renderBones[i];
                Color boneColor = (bone->textureIndex == 0) ? RED : 
                                (bone->textureIndex == 1) ? BLUE : PURPLE;
                
                DrawText(TextFormat("%s[%s]: idx%02d d%.1f", 
                        bone->boneName, bone->personId, bone->atlasIndex, bone->distance),
                        ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                        yOffset + i * ScaleValue(18, currentScreenHeight, BASE_HEIGHT), 
                        smallFontSize, boneColor);
            }
            
            if (renderBonesCount > maxBonesToShow) {
                DrawText(TextFormat("... and %d more bones", renderBonesCount - maxBonesToShow),
                        ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                        yOffset + maxBonesToShow * ScaleValue(18, currentScreenHeight, BASE_HEIGHT), 
                        smallFontSize, GRAY);
            }
            
            // ================================================================
            // ATLAS GRID OVERLAY (Responsive)
            // ================================================================
            const int gridCols = ATLAS_COLS, gridRows = ATLAS_ROWS;
            const int cellW = ScaleValue(220, currentScreenWidth, BASE_WIDTH);
            const int cellH = ScaleValue(28, currentScreenHeight, BASE_HEIGHT);
            const int margin = ScaleValue(6, currentScreenWidth, BASE_WIDTH);
            const int startX = currentScreenWidth - (cellW + ScaleValue(20, currentScreenWidth, BASE_WIDTH));
            const int startY = ScaleValue(20, currentScreenHeight, BASE_HEIGHT);
            
            // Título del atlas
            DrawText("Atlas Grid 4x4", startX, startY - ScaleValue(20, currentScreenHeight, BASE_HEIGHT), 
                    ScaleFontSize(18, currentScreenHeight), DARKGRAY);
            
            // Contar bones por índice de atlas
            int atlasCount[16] = {0}; // Para 4x4 = 16 indices
            for (int i = 0; i < renderBonesCount; i++) {
                if (renderBones[i].atlasIndex >= 0 && renderBones[i].atlasIndex < 16) {
                    atlasCount[renderBones[i].atlasIndex]++;
                }
            }
            
            // Dibujar cuadrícula del atlas con contadores
            for (int r = 0; r < gridRows; r++) {
                for (int c = 0; c < gridCols; c++) {
                    int idx = r * gridCols + c;
                    int xcell = startX + c * ((cellW / gridCols) + margin);
                    int ycell = startY + r * (cellH + margin);
                    Rectangle rcell = { (float)xcell, (float)ycell, 
                                       (float)(cellW / gridCols - margin), (float)cellH };
                    
                    Color bg = LIGHTGRAY;
                    if (atlasCount[idx] > 0) {
                        // Color basado en cantidad de bones usando este índice
                        float intensity = (float)atlasCount[idx] / 5.0f; // Normalizar
                        if (intensity > 1.0f) intensity = 1.0f;
                        bg = ColorFromHSV(240.0f - intensity * 120.0f, 0.7f, 0.9f); // De azul a rojo
                    }
                    
                    DrawRectangleRec(rcell, bg);
                    DrawRectangleLines((int)rcell.x, (int)rcell.y, (int)rcell.width, (int)rcell.height, GRAY);
                    
                    // Mostrar contador si hay bones
                    if (atlasCount[idx] > 0) {
                        DrawText(TextFormat("%d", atlasCount[idx]),
                                xcell + 2, ycell + 2, ScaleFontSize(12, currentScreenHeight), BLACK);
                    }
                    
                    // Mostrar índice en esquina
                    DrawText(TextFormat("%d", idx),
                            xcell + (cellW/gridCols) - margin - 15, ycell + cellH - 15, 
                            ScaleFontSize(10, currentScreenHeight), DARKGRAY);
                }
            }
            
            // ================================================================
            // CONTROLES EN PANTALLA
            // ================================================================
            int controlsY = currentScreenHeight - ScaleValue(120, currentScreenHeight, BASE_HEIGHT);
            
            DrawText("DYNAMIC CONTROLS:", 
                    ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                    controlsY, 
                    ScaleFontSize(16, currentScreenHeight), DARKGREEN);
            
            DrawText("Camera: 1=Orbit | 2=FPS | M=Toggle Mouse | WASD=Move | Shift=Sprint", 
                    ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                    controlsY + ScaleValue(20, currentScreenHeight, BASE_HEIGHT), 
                    ScaleFontSize(14, currentScreenHeight), DARKGRAY);
            
            DrawText("Animation: ←→=Frame | Home/End=First/Last | Space=Auto-play", 
                    ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                    controlsY + ScaleValue(40, currentScreenHeight, BASE_HEIGHT), 
                    ScaleFontSize(14, currentScreenHeight), DARKGRAY);
            
            DrawText("Mouse: Left-click+drag=Orbit | Wheel=Zoom | Right-click=Context", 
                    ScaleValue(20, currentScreenWidth, BASE_WIDTH), 
                    controlsY + ScaleValue(60, currentScreenHeight, BASE_HEIGHT), 
                    ScaleFontSize(14, currentScreenHeight), DARKGRAY);
            
            // Status bar en la parte inferior
            DrawRectangle(0, currentScreenHeight - ScaleValue(25, currentScreenHeight, BASE_HEIGHT), 
                         currentScreenWidth, ScaleValue(25, currentScreenHeight, BASE_HEIGHT), 
                         Fade(BLACK, 0.8f));
            
            DrawText(TextFormat("Frame: %d/%d | Bones: %d | Camera: %s | FPS: %d", 
                               currentFrame + 1, maxFrames, renderBonesCount, 
                               (camMode == 1) ? "ORBIT" : "FPS", GetFPS()),
                     ScaleValue(10, currentScreenWidth, BASE_WIDTH), 
                     currentScreenHeight - ScaleValue(20, currentScreenHeight, BASE_HEIGHT), 
                     ScaleFontSize(14, currentScreenHeight), WHITE);
            
            // Indicador de auto-play
            if (autoPlay) {
                const char* playText = "AUTO-PLAY ON";
                int playTextWidth = MeasureText(playText, ScaleFontSize(14, currentScreenHeight));
                DrawText(playText,
                        currentScreenWidth - playTextWidth - ScaleValue(10, currentScreenWidth, BASE_WIDTH),
                        currentScreenHeight - ScaleValue(20, currentScreenHeight, BASE_HEIGHT),
                        ScaleFontSize(14, currentScreenHeight), LIME);
            }
            
        EndDrawing();
    }
    
    // ========================================================================
    // LIMPIEZA
    // ========================================================================
    
    // Liberar array dinámico
    if (renderBones) {
        free(renderBones);
        renderBones = NULL;
        renderBonesCount = 0;
        renderBonesCapacity = 0;
    }
    
    // Liberar texturas
    for (int i = 0; i < textureCount; i++) {
        UnloadTexture(textures[i]);
    }
    
    // Liberar sistema bones3d
    BonesFree(&animation);
    CloseWindow();
    
    return 0;
}
