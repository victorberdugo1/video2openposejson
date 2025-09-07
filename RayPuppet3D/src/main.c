#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "bonetile.h"
#include "bones3d.h"
#include "head_billboard.h"
#include "torso_billboard.h"

#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080
#define MAX_TEXTURES 10
#define MAX_RENDER_ITEMS 512

// Constantes optimizadas
static const float AUTO_PLAY_LERP = 0.15f;
static const float ORBIT_SENSITIVITY = 0.01f;
static const float FPS_SENSITIVITY = 0.003f;
static const float ZOOM_SENSITIVITY = 0.5f;
static const float MIN_ORBIT_RADIUS = 0.5f;
static const float MAX_ORBIT_RADIUS = 20.0f;
static const float MIN_PITCH = -1.4f;
static const float MAX_PITCH = 1.4f;
static const float FPS_MIN_PITCH = -1.49f;
static const float FPS_MAX_PITCH = 1.49f;
static const float MOVEMENT_SPEED = 3.0f;
static const float FAST_SPEED = 8.0f;
static const float VALID_POSITION_THRESHOLD = 0.01f;
static const float MIN_DISTANCE_THRESHOLD = 0.001f;

// Bias constants para Z-fighting
static const float TORSO_BIAS = 0.001f;
static const float BONE_BIAS = 0.0f;
static const float HEAD_BIAS = -0.001f;
static const float INDEX_BIAS = -0.00001f;
static const float Z_FIGHTING_THRESHOLD = 0.01f;

typedef struct {
    Camera camera;
    int camMode;
    float orbitYaw, orbitPitch, orbitRadius;
    bool cameraMouseControl;
    SimpleTextureSystem textureSystem;
    BoneConfig* boneConfigs;
    int boneConfigCount;
    BoneRenderData* renderBones;
    int renderBonesCount;
    int renderBonesCapacity;

    HeadRenderData* renderHeads;
    int renderHeadsCount;
    int renderHeadsCapacity;
    bool renderHeadBillboards;

    TorsoRenderData* renderTorsos;
    int renderTorsosCount;
    int renderTorsosCapacity;
    bool renderTorsoBillboards;

    BonesAnimation animation;
    BonesRenderConfig renderConfig;
    Texture2D textures[MAX_TEXTURES];
    char texturePaths[MAX_TEXTURES][MAX_FILE_PATH_LENGTH];
    int textureCount;
    int physCols, physRows;
    int currentFrame;
    int maxFrames;
    bool autoPlay;
    float autoPlayTimer;
    float autoPlaySpeed;
    Vector3 autoCenter;
    bool autoCenterCalculated;
    int lastProcessedFrame;
    bool forceUpdate;
} AppState;

typedef struct {
    int type; // 0=torso, 1=bone, 2=head
    int index;
    float distance;
    float depthBias;
    bool hasZFighting;
} RenderItem;

// Prototipos de funciones optimizados
static bool App_Init(AppState* app);
static void App_Shutdown(AppState* app);
static int App_GetTextureIndex(AppState* app, const char* path);
static void App_HandleInput(AppState* app, float dt);
static void App_UpdateCamera(AppState* app, float dt);
static void App_UpdateAutoCenter(AppState* app);
static void App_PrepareRenderData(AppState* app);
static bool DetectZFighting(RenderItem* items, int itemCount);
static void SortRenderItems(RenderItem* items, int itemCount);
static void App_Draw(AppState* app);

static bool App_Init(AppState* app) {
    if (!app) return false;
    memset(app, 0, sizeof(*app));

    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - System with Morphing, Head & Torso Billboards");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
#if defined(__linux__)
    for (int i = 0; i < 5; i++) PollInputEvents();
#endif
    MaximizeWindow();
    SetTargetFPS(60);

    // Inicialización optimizada de cámara
    app->camera = (Camera){
        .position = {0.0f, 0.6f, 2.5f},
        .target = {0.0f, 0.6f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    // Estado inicial
    app->camMode = 1;
    app->orbitRadius = 2.5f;
    app->orbitPitch = -0.2f;
    app->renderHeadBillboards = true;
    app->renderTorsoBillboards = true;
    app->physCols = 8;
    app->physRows = 8;
    app->autoPlaySpeed = 0.1f;
    app->lastProcessedFrame = -1;

    // Cargar configuraciones
    LoadSimpleTextureConfig(&app->textureSystem, "bone_textures.txt");
    LoadBoneConfigurations(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);

    if (BonesInit(&app->animation, 1000) != BONES_SUCCESS) {
        CloseWindow();
        return false;
    }

    BonesLoadFromJSON(&app->animation, "test.json");
    app->renderConfig = BonesGetDefaultRenderConfig();
    app->renderConfig.drawDebugSpheres = true;
    app->renderConfig.debugColor = GREEN;
    app->renderConfig.debugSphereRadius = 0.035f;
    app->renderConfig.enableDepthSorting = true;
    BonesSetRenderConfig(&app->renderConfig);

    app->maxFrames = BonesGetFrameCount(&app->animation);
    return true;
}

static void App_Shutdown(AppState* app) {
    if (!app) return;

    free(app->renderBones);
    free(app->renderHeads);
    free(app->renderTorsos);
    CleanupTextureSystem(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);

    for (int i = 0; i < app->textureCount; i++) {
        UnloadTexture(app->textures[i]);
    }

    BonesFree(&app->animation);
    CloseWindow();
}

static int App_GetTextureIndex(AppState* app, const char* path) {
    if (!app || !path) return 0;

    // Buscar textura existente
    for (int i = 0; i < app->textureCount; i++) {
        if (strcmp(app->texturePaths[i], path) == 0) return i;
    }

    if (app->textureCount >= MAX_TEXTURES) return 0;

    // Cargar nueva textura
    Image img = LoadImage(path);
    if (img.data == NULL) {
        img = GenImageColor(1024, 1024, CLITERAL(Color){60, 120, 220, 255});
        ImageDrawText(&img, path, 8, 8, 128, WHITE);
    }

    app->textures[app->textureCount] = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(app->textures[app->textureCount], TEXTURE_FILTER_POINT);
    strncpy(app->texturePaths[app->textureCount], path, MAX_FILE_PATH_LENGTH - 1);
    app->texturePaths[app->textureCount][MAX_FILE_PATH_LENGTH - 1] = '\0';

    return app->textureCount++;
}

static void App_HandleInput(AppState* app, float dt) {
    if (!app) return;

    // Frame control optimizado
    if (app->animation.isLoaded && app->maxFrames > 0) {
        bool frameChanged = false;
        int newFrame = app->currentFrame;

        if (IsKeyPressed(KEY_LEFT) && newFrame > 0) {
            newFrame--;
            frameChanged = true;
        }
        if (IsKeyPressed(KEY_RIGHT) && newFrame < app->maxFrames - 1) {
            newFrame++;
            frameChanged = true;
        }
        if (IsKeyPressed(KEY_HOME)) {
            newFrame = 0;
            frameChanged = true;
        }
        if (IsKeyPressed(KEY_END) && app->maxFrames > 0) {
            newFrame = app->maxFrames - 1;
            frameChanged = true;
        }

        if (frameChanged) {
            app->currentFrame = newFrame;
            BonesSetFrame(&app->animation, app->currentFrame);
            app->autoCenterCalculated = false;
        }

        if (IsKeyPressed(KEY_SPACE)) app->autoPlay = !app->autoPlay;

        // Auto-play optimizado
        if (app->autoPlay && app->maxFrames > 1) {
            app->autoPlayTimer += dt;
            if (app->autoPlayTimer >= app->autoPlaySpeed) {
                app->autoPlayTimer = 0.0f;
                app->currentFrame = (app->currentFrame + 1) % app->maxFrames;
                BonesSetFrame(&app->animation, app->currentFrame);
            }
        }
    }

    // Controles de configuración
    if (IsKeyPressed(KEY_F5)) {
        if (LoadSimpleTextureConfig(&app->textureSystem, "bone_textures.txt")) {
            LoadBoneConfigurations(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);
        }
    }

    // Camera mode switching optimizado
    if (IsKeyPressed(KEY_ONE)) {
        app->camMode = 1;
        app->cameraMouseControl = false;
        EnableCursor();
    }
    if (IsKeyPressed(KEY_TWO)) {
        app->camMode = 2;
        app->cameraMouseControl = true;
        DisableCursor();

        Vector3 target = app->autoCenterCalculated ? app->autoCenter : (Vector3) { 0, 0.6f, 0 };
        app->camera.position = (Vector3){ target.x, target.y + 0.5f, target.z + 2.0f };
        app->camera.target = target;

        Vector3 direction = Vector3Subtract(target, app->camera.position);
        app->orbitYaw = atan2f(direction.x, direction.z);
        app->orbitPitch = atan2f(direction.y, sqrtf(direction.x * direction.x + direction.z * direction.z));
    }

    if (IsKeyPressed(KEY_C)) {
        app->cameraMouseControl = !app->cameraMouseControl;
        if (app->cameraMouseControl) DisableCursor();
        else EnableCursor();
    }

    if (IsKeyPressed(KEY_H)) {
        app->renderHeadBillboards = !app->renderHeadBillboards;
        TraceLog(LOG_INFO, "Head Billboards %s", app->renderHeadBillboards ? "ENABLED" : "DISABLED");
    }

    if (IsKeyPressed(KEY_T)) {
        app->renderTorsoBillboards = !app->renderTorsoBillboards;
        TraceLog(LOG_INFO, "Torso Billboards %s", app->renderTorsoBillboards ? "ENABLED" : "DISABLED");
    }
}

static void App_UpdateCamera(AppState* app, float dt) {
    if (!app) return;

    Vector3 cameraTarget = app->autoCenterCalculated ? app->autoCenter : (Vector3) { 0, 0.6f, 0 };

    if (app->camMode == 1) {
        // Orbit camera optimizada
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mouseDelta = GetMouseDelta();
            app->orbitYaw += mouseDelta.x * ORBIT_SENSITIVITY;
            app->orbitPitch = Clamp(app->orbitPitch - mouseDelta.y * ORBIT_SENSITIVITY, MIN_PITCH, MAX_PITCH);
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            app->orbitRadius = Clamp(app->orbitRadius - wheel * ZOOM_SENSITIVITY, MIN_ORBIT_RADIUS, MAX_ORBIT_RADIUS);
        }

        // Cálculo optimizado de posición
        float cosP = cosf(app->orbitPitch);
        float sinP = sinf(app->orbitPitch);
        float cosY = cosf(app->orbitYaw);
        float sinY = sinf(app->orbitYaw);

        app->camera.position = (Vector3){
            cameraTarget.x + app->orbitRadius * cosP * sinY,
            cameraTarget.y + app->orbitRadius * sinP,
            cameraTarget.z + app->orbitRadius * cosP * cosY
        };
        app->camera.target = cameraTarget;
    }
    else {
        // FPS camera optimizada
        if (app->cameraMouseControl) {
            Vector2 mouseDelta = GetMouseDelta();
            app->orbitYaw -= mouseDelta.x * FPS_SENSITIVITY;
            app->orbitPitch = Clamp(app->orbitPitch - mouseDelta.y * FPS_SENSITIVITY, FPS_MIN_PITCH, FPS_MAX_PITCH);
        }

        // Precalcular vectores de dirección (ARREGLADO)
        float cosP = cosf(app->orbitPitch);
        float sinP = sinf(app->orbitPitch);
        float cosY = cosf(app->orbitYaw);
        float sinY = sinf(app->orbitYaw);

        Vector3 forward = { sinY * cosP, sinP, cosY * cosP };
        // ARREGLO: Cálculo correcto del vector right para movimiento lateral
        Vector3 right = { cosY, 0, -sinY };  // Perpendicular al forward en el plano XZ

        forward = Vector3Normalize(forward);
        right = Vector3Normalize(right);

        float speed = IsKeyDown(KEY_LEFT_SHIFT) ? FAST_SPEED : MOVEMENT_SPEED;
        speed *= dt;

        // Movimiento optimizado
        if (IsKeyDown(KEY_W)) app->camera.position = Vector3Add(app->camera.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_S)) app->camera.position = Vector3Subtract(app->camera.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_A)) app->camera.position = Vector3Subtract(app->camera.position, Vector3Scale(right, speed));
        if (IsKeyDown(KEY_D)) app->camera.position = Vector3Add(app->camera.position, Vector3Scale(right, speed));
        if (IsKeyDown(KEY_SPACE)) app->camera.position.y += speed;
        if (IsKeyDown(KEY_LEFT_CONTROL)) app->camera.position.y -= speed;

        app->camera.target = Vector3Add(app->camera.position, forward);
    }
}

static void App_UpdateAutoCenter(AppState* app) {
    if (!app || !app->animation.isLoaded || !BonesIsValidFrame(&app->animation, app->currentFrame)) return;

    const AnimationFrame* frame = &app->animation.frames[app->currentFrame];
    Vector3 totalPos = { 0, 0, 0 };
    int validBoneCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;

        Vector3 headPos = CalculateHeadPosition(person);
        Vector3 chestPos = CalculateChestPosition(person);
        Vector3 hipPos = CalculateHipPosition(person);

        bool hasValidHead = Vector3Length(headPos) > VALID_POSITION_THRESHOLD;
        bool hasValidChest = Vector3Length(chestPos) > VALID_POSITION_THRESHOLD;
        bool hasValidHip = Vector3Length(hipPos) > VALID_POSITION_THRESHOLD;

        // Bones centrales
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (bone->position.valid && BonesIsPositionValid(bone->position.position)) {
                const char* name = bone->name;
                if (strstr(name, "Spine") || strstr(name, "Chest") || strstr(name, "Neck") || strstr(name, "Hip")) {
                    totalPos = Vector3Add(totalPos, bone->position.position);
                    validBoneCount++;
                }
            }
        }

        // Añadir posiciones calculadas
        if (hasValidHead) { totalPos = Vector3Add(totalPos, headPos); validBoneCount++; }
        if (hasValidChest) { totalPos = Vector3Add(totalPos, chestPos); validBoneCount++; }
        if (hasValidHip) { totalPos = Vector3Add(totalPos, hipPos); validBoneCount++; }
    }

    if (validBoneCount > 0) {
        Vector3 newCenter = Vector3Scale(totalPos, 1.0f / validBoneCount);

        if (app->camMode == 1 && app->autoPlay && app->autoCenterCalculated) {
            app->autoCenter = Vector3Lerp(app->autoCenter, newCenter, AUTO_PLAY_LERP);
        }
        else {
            app->autoCenter = newCenter;
        }

        app->autoCenterCalculated = true;
    }
}

static void App_PrepareRenderData(AppState* app) {
    if (!app) return;

    // Preparar bones si es necesario
    if (app->currentFrame != app->lastProcessedFrame || app->forceUpdate) {
        CollectBonesForRendering(&app->animation, app->camera, &app->renderBones, &app->renderBonesCount,
            &app->renderBonesCapacity, app->boneConfigs, app->boneConfigCount);
        app->lastProcessedFrame = app->currentFrame;
        app->forceUpdate = false;
    }

    // Preparar heads
    if (app->renderHeadBillboards) {
        CollectHeadsForRendering(&app->animation, &app->renderHeads, &app->renderHeadsCount,
            &app->renderHeadsCapacity, app->boneConfigs, app->boneConfigCount);
    }
    else {
        app->renderHeadsCount = 0;
    }

    // Preparar torsos
    if (app->renderTorsoBillboards) {
        CollectTorsosForRendering(&app->animation, &app->renderTorsos, &app->renderTorsosCount,
            &app->renderTorsosCapacity, app->boneConfigs, app->boneConfigCount);
    }
    else {
        app->renderTorsosCount = 0;
    }
}

static bool DetectZFighting(RenderItem* items, int itemCount) {
    bool hasZFighting = false;

    for (int i = 0; i < itemCount; i++) {
        items[i].hasZFighting = false;
        for (int j = i + 1; j < itemCount; j++) {
            if (fabs(items[i].distance - items[j].distance) < Z_FIGHTING_THRESHOLD) {
                items[i].hasZFighting = items[j].hasZFighting = true;
                hasZFighting = true;
            }
        }
    }
    return hasZFighting;
}

static void SortRenderItems(RenderItem* items, int itemCount) {
    // Bubble sort optimizado (pocos elementos normalmente)
    for (int i = 0; i < itemCount - 1; i++) {
        bool swapped = false;
        for (int j = 0; j < itemCount - i - 1; j++) {
            float distanceA = items[j].distance + items[j].depthBias;
            float distanceB = items[j + 1].distance + items[j + 1].depthBias;

            if (distanceA < distanceB) {
                RenderItem temp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = temp;
                swapped = true;
            }
        }
        if (!swapped) break; // Early exit si ya está ordenado
    }
}

static void App_Draw(AppState* app) {
    if (!app) return;

    BeginDrawing();
    ClearBackground(RAYWHITE);

    // UI optimizada
    const char* modeText = "CLASSIC MODE";
    const char* controlsText = "M: Toggle Morphing | H: Toggle Heads | T: Toggle Torsos | C: Mouse Control | 1/2: Camera Mode | Space: Play/Pause";

    DrawText(modeText, 10, 10, 20, BLUE);
    DrawText(controlsText, 10, 35, 16, DARKGRAY);

    char frameText[64];
    snprintf(frameText, sizeof(frameText), "Frame: %d/%d %s",
        app->currentFrame + 1, app->maxFrames, app->autoPlay ? "(Playing)" : "(Paused)");
    DrawText(frameText, 10, 55, 16, DARKGRAY);

    char statsText[256];
    snprintf(statsText, sizeof(statsText),
        "Bones: %d | Heads: %s (%d) | Torsos: %s (%d) | Camera Mode: %d | Mouse Control: %s",
        app->renderBonesCount,
        app->renderHeadBillboards ? "ON" : "OFF", app->renderHeadsCount,
        app->renderTorsoBillboards ? "ON" : "OFF", app->renderTorsosCount,
        app->camMode, app->cameraMouseControl ? "ON" : "OFF");
    DrawText(statsText, 10, 75, 16, DARKGRAY);

    BeginMode3D(app->camera);
    DrawGrid(24, 0.5f);
    if (app->autoCenterCalculated) {
        DrawSphereWires(app->autoCenter, 0.05f, 8, 8, ORANGE);
    }

    int totalItems = app->renderBonesCount + app->renderHeadsCount + app->renderTorsosCount;
    if (totalItems > 0) {
        static RenderItem renderItems[MAX_RENDER_ITEMS];
        int itemCount = 0;
        Vector3 camPos = app->camera.position;

        // Crear items de renderizado
        for (int i = 0; i < app->renderTorsosCount && itemCount < MAX_RENDER_ITEMS; i++) {
            const TorsoRenderData* torso = &app->renderTorsos[i];
            if (!torso->valid || !torso->visible) continue;

            renderItems[itemCount] = (RenderItem){
                .type = 0, .index = i,
                .distance = Vector3Distance(camPos, torso->position),
                .depthBias = TORSO_BIAS + (INDEX_BIAS * i),
                .hasZFighting = false
            };
            itemCount++;
        }

        for (int i = 0; i < app->renderBonesCount && itemCount < MAX_RENDER_ITEMS; i++) {
            const BoneRenderData* bone = &app->renderBones[i];
            if (!bone->valid || !bone->visible) continue;

            renderItems[itemCount] = (RenderItem){
                .type = 1, .index = i,
                .distance = Vector3Distance(camPos, bone->position),
                .depthBias = BONE_BIAS + (INDEX_BIAS * i),
                .hasZFighting = false
            };
            itemCount++;
        }

        for (int i = 0; i < app->renderHeadsCount && itemCount < MAX_RENDER_ITEMS; i++) {
            const HeadRenderData* head = &app->renderHeads[i];
            if (!head->valid || !head->visible) continue;

            renderItems[itemCount] = (RenderItem){
                .type = 2, .index = i,
                .distance = Vector3Distance(camPos, head->position),
                .depthBias = HEAD_BIAS + (INDEX_BIAS * i),
                .hasZFighting = false
            };
            itemCount++;
        }

        DetectZFighting(renderItems, itemCount);
        SortRenderItems(renderItems, itemCount);

        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthTest();

        // Renderizado optimizado
        for (int i = 0; i < itemCount; i++) {
            RenderItem* item = &renderItems[i];
            Vector3 toCam = Vector3Subtract(camPos,
                item->type == 0 ? app->renderTorsos[item->index].position :
                item->type == 1 ? app->renderBones[item->index].position :
                app->renderHeads[item->index].position);

            float distance = Vector3Length(toCam);
            Vector3 renderOffset = { 0, 0, 0 };

            if (distance > MIN_DISTANCE_THRESHOLD) {
                Vector3 toCamNorm = Vector3Normalize(toCam);
                renderOffset = Vector3Scale(toCamNorm, item->depthBias);
            }

            if (item->type == 0) { // Torso
                const TorsoRenderData* torso = &app->renderTorsos[item->index];
                TorsoRenderData adjustedTorso = *torso;
                adjustedTorso.position = Vector3Add(torso->position, renderOffset);

                int texIndex = App_GetTextureIndex(app, torso->texturePath);
                DrawTorsoBillboard(app->textures[texIndex], app->camera, &adjustedTorso, app->physCols, app->physRows);
            }
            else if (item->type == 1) { // Bone
                const BoneRenderData* bone = &app->renderBones[item->index];
                Vector3 renderPosition = Vector3Add(bone->position, renderOffset);

                int texIndex = App_GetTextureIndex(app, bone->texturePath);
                Texture2D currentTex = app->textures[texIndex];

                // Calcular tamaño y orientación
                float physCellW = (float)currentTex.width / app->physCols;
                float physCellH = (float)currentTex.height / app->physRows;
                float logicalCellW = physCellW * (app->physCols / ATLAS_COLS);
                float logicalCellH = physCellH * (app->physRows / ATLAS_ROWS);
                float aspect = logicalCellW / logicalCellH;
                Vector2 worldSize = { bone->size * aspect, bone->size };

                int chosenIndex = 0;
                float rotation = 0.0f;
                bool mirrored = false;

                if (bone->orientation.valid) {
                    CalculateEnhancedBoneRenderData(bone, app->camera, &chosenIndex, &rotation, &mirrored);
                }
                else {
                    CalculateBoneRenderData(bone->position, app->camera, &chosenIndex, &rotation, &mirrored, bone->boneName);
                }

                int logicalCol = chosenIndex % ATLAS_COLS;
                int logicalRow = chosenIndex / ATLAS_COLS;
                bool finalMirror = false;
                Rectangle src = SrcFromLogical(currentTex, logicalCol, logicalRow, app->physCols, app->physRows, mirrored, &finalMirror);

                // Buscar neighbor optimizado
                char conns[3][32];
                float prios[3];
                Vector3 neighborPos = { 0, 0, 0 };
                bool haveNeighbor = false;

                if (GetBoneConnectionsWithPriority(bone->boneName, conns, prios)) {
                    for (int k = 0; k < 3 && !haveNeighbor; k++) {
                        if (conns[k][0] == '\0') continue;
                        BoneRenderData* nb = FindRenderBoneByName(app->renderBones, app->renderBonesCount, conns[k]);
                        if (nb && nb->valid && nb->visible &&
                            Vector3Distance(bone->position, nb->position) > MIN_DISTANCE_THRESHOLD) {
                            neighborPos = nb->position;
                            haveNeighbor = true;
                        }
                    }
                }

                DrawBonetileCustomWithRoll(currentTex, app->camera, src, renderPosition, worldSize,
                    rotation, finalMirror, haveNeighbor, neighborPos, bone->boneName);
            }
            else if (item->type == 2) { // Head
                const HeadRenderData* head = &app->renderHeads[item->index];
                HeadRenderData adjustedHead = *head;
                adjustedHead.position = Vector3Add(head->position, renderOffset);

                int texIndex = App_GetTextureIndex(app, head->texturePath);
                DrawHeadBillboard(app->textures[texIndex], app->camera, &adjustedHead, app->physCols, app->physRows);
            }
        }

        EndBlendMode();
    }

    EndMode3D();
    EndDrawing();
}

int main(void) {
    AppState app;
    if (!App_Init(&app)) return -1;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        App_HandleInput(&app, dt);
        App_UpdateCamera(&app, dt);
        App_UpdateAutoCenter(&app);
        App_PrepareRenderData(&app);
        App_Draw(&app);
    }

    App_Shutdown(&app);
    return 0;
}