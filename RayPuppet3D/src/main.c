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

    // Head system
    HeadRenderData* renderHeads;
    int renderHeadsCount;
    int renderHeadsCapacity;
    bool renderHeadBillboards;

    // Torso system
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

static bool App_Init(AppState* app);
static void App_Shutdown(AppState* app);
static int App_GetTextureIndex(AppState* app, const char* path);
static void App_HandleInput(AppState* app, float dt);
static void App_UpdateCamera(AppState* app, float dt);
static void App_UpdateAutoCenter(AppState* app);
static void App_PrepareRenderBones(AppState* app);
static void App_PrepareRenderHeads(AppState* app);
static void App_PrepareRenderTorsos(AppState* app);
static void App_Draw(AppState* app);

static bool App_Init(AppState* app) {
    if (!app) return false;
    memset(app, 0, sizeof(*app));
    CheckBoneNameLength();
    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - System with Morphing, Head & Torso Billboards");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
#if defined(__linux__)
    for (int i = 0; i < 5; i++) PollInputEvents();
#endif
    MaximizeWindow();
    SetTargetFPS(60);

    app->camera = (Camera){ 0 };
    app->camera.position = (Vector3){ 0.0f, 0.6f, 2.5f };
    app->camera.target = (Vector3){ 0.0f, 0.6f, 0.0f };
    app->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    app->camera.fovy = 45.0f;
    app->camera.projection = CAMERA_PERSPECTIVE;

    app->camMode = 1;
    app->orbitYaw = 0.0f;
    app->orbitPitch = -0.2f;
    app->orbitRadius = 2.5f;
    app->cameraMouseControl = false;
    app->renderHeadBillboards = true;
    app->renderTorsoBillboards = true;
    app->physCols = 8;
    app->physRows = 8;

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
    app->currentFrame = 0;
    app->autoPlay = false;
    app->autoPlayTimer = 0.0f;
    app->autoPlaySpeed = 0.1f;
    app->autoCenter = (Vector3){ 0,0,0 };
    app->autoCenterCalculated = false;
    app->lastProcessedFrame = -1;
    app->forceUpdate = false;

    return true;
}

static void App_Shutdown(AppState* app) {
    if (!app) return;
    if (app->renderBones) free(app->renderBones);
    if (app->renderHeads) free(app->renderHeads);
    if (app->renderTorsos) free(app->renderTorsos);
    CleanupTextureSystem(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);
    for (int i = 0; i < app->textureCount; i++) UnloadTexture(app->textures[i]);
    BonesFree(&app->animation);
    CloseWindow();
}

static int App_GetTextureIndex(AppState* app, const char* path) {
    if (!app || !path) return 0;

    for (int i = 0; i < app->textureCount; i++) {
        if (strcmp(app->texturePaths[i], path) == 0) return i;
    }

    if (app->textureCount >= MAX_TEXTURES) return 0;

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

    if (app->animation.isLoaded && app->maxFrames > 0) {
        bool frameChanged = false;

        if (IsKeyPressed(KEY_LEFT) && app->currentFrame > 0) {
            app->currentFrame--;
            BonesSetFrame(&app->animation, app->currentFrame);
            frameChanged = true;
        }
        if (IsKeyPressed(KEY_RIGHT) && app->currentFrame < app->maxFrames - 1) {
            app->currentFrame++;
            BonesSetFrame(&app->animation, app->currentFrame);
            frameChanged = true;
        }
        if (IsKeyPressed(KEY_HOME)) {
            app->currentFrame = 0;
            BonesSetFrame(&app->animation, app->currentFrame);
            frameChanged = true;
        }
        if (IsKeyPressed(KEY_END) && app->maxFrames > 0) {
            app->currentFrame = app->maxFrames - 1;
            BonesSetFrame(&app->animation, app->currentFrame);
            frameChanged = true;
        }

        if (frameChanged) app->autoCenterCalculated = false;

        if (IsKeyPressed(KEY_SPACE)) app->autoPlay = !app->autoPlay;

        if (app->autoPlay && app->maxFrames > 1) {
            app->autoPlayTimer += dt;
            if (app->autoPlayTimer >= app->autoPlaySpeed) {
                app->autoPlayTimer = 0.0f;
                app->currentFrame = (app->currentFrame + 1) % app->maxFrames;
                BonesSetFrame(&app->animation, app->currentFrame);
            }
        }
    }

    if (IsKeyPressed(KEY_F5)) {
        if (LoadSimpleTextureConfig(&app->textureSystem, "bone_textures.txt")) {
            LoadBoneConfigurations(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);
        }
    }

    if (IsKeyPressed(KEY_ONE)) {
        app->camMode = 1; app->cameraMouseControl = false; EnableCursor();
    }
    if (IsKeyPressed(KEY_TWO)) {
        app->camMode = 2; app->cameraMouseControl = true; DisableCursor();
        Vector3 target = app->autoCenterCalculated ? app->autoCenter : (Vector3) { 0, 0.6f, 0 };
        app->camera.position = (Vector3){ target.x, target.y + 0.5f, target.z + 2.0f };
        app->camera.target = target;
        Vector3 direction = Vector3Subtract(target, app->camera.position);
        app->orbitYaw = atan2f(direction.x, direction.z);
        app->orbitPitch = atan2f(direction.y, sqrtf(direction.x * direction.x + direction.z * direction.z));
    }

    if (IsKeyPressed(KEY_C)) {
        app->cameraMouseControl = !app->cameraMouseControl;
        if (app->cameraMouseControl) DisableCursor(); else EnableCursor();
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
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mouseDelta = GetMouseDelta();
            app->orbitYaw += mouseDelta.x * 0.01f;
            app->orbitPitch += -mouseDelta.y * 0.01f;
            app->orbitPitch = Clamp(app->orbitPitch, -1.4f, 1.4f);
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            app->orbitRadius -= wheel * 0.5f;
            app->orbitRadius = Clamp(app->orbitRadius, 0.5f, 20.0f);
        }

        float x = app->orbitRadius * cosf(app->orbitPitch) * sinf(app->orbitYaw);
        float y = app->orbitRadius * sinf(app->orbitPitch);
        float z = app->orbitRadius * cosf(app->orbitPitch) * cosf(app->orbitYaw);
        app->camera.position = (Vector3){ cameraTarget.x + x, cameraTarget.y + y, cameraTarget.z + z };
        app->camera.target = cameraTarget;
    }
    else {
        if (app->cameraMouseControl) {
            Vector2 mouseDelta = GetMouseDelta();
            app->orbitYaw -= mouseDelta.x * 0.003f;
            app->orbitPitch -= mouseDelta.y * 0.003f;
            app->orbitPitch = Clamp(app->orbitPitch, -1.49f, 1.49f);
        }

        Vector3 forward = { sinf(app->orbitYaw) * cosf(app->orbitPitch),
                           sinf(app->orbitPitch),
                           cosf(app->orbitYaw) * cosf(app->orbitPitch) };
        forward = Vector3Normalize(forward);
        Vector3 right = { sinf(app->orbitYaw - PI / 2), 0, cosf(app->orbitYaw - PI / 2) };
        right = Vector3Normalize(right);

        float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 8.0f : 3.0f;

        if (IsKeyDown(KEY_W)) app->camera.position = Vector3Add(app->camera.position, Vector3Scale(forward, speed * dt));
        if (IsKeyDown(KEY_S)) app->camera.position = Vector3Subtract(app->camera.position, Vector3Scale(forward, speed * dt));
        if (IsKeyDown(KEY_A)) app->camera.position = Vector3Subtract(app->camera.position, Vector3Scale(right, speed * dt));
        if (IsKeyDown(KEY_D)) app->camera.position = Vector3Add(app->camera.position, Vector3Scale(right, speed * dt));
        if (IsKeyDown(KEY_SPACE)) app->camera.position.y += speed * dt;
        if (IsKeyDown(KEY_LEFT_CONTROL)) app->camera.position.y -= speed * dt;

        app->camera.target = Vector3Add(app->camera.position, forward);
    }
}

static void App_UpdateAutoCenter(AppState* app) {
    if (!app || !app->animation.isLoaded) return;
    if (!BonesIsValidFrame(&app->animation, app->currentFrame)) return;

    const AnimationFrame* frame = &app->animation.frames[app->currentFrame];
    Vector3 totalPos = { 0,0,0 };
    int validBoneCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;

        Vector3 headPos = CalculateHeadPosition(person);
        bool hasValidHead = Vector3Length(headPos) > 0.01f;

        Vector3 chestPos = CalculateChestPosition(person);
        Vector3 hipPos = CalculateHipPosition(person);
        bool hasValidChest = Vector3Length(chestPos) > 0.01f;
        bool hasValidHip = Vector3Length(hipPos) > 0.01f;

        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (bone->position.valid && BonesIsPositionValid(bone->position.position)) {
                if (strstr(bone->name, "Spine") || strstr(bone->name, "Chest") ||
                    strstr(bone->name, "Neck") || strstr(bone->name, "Hip")) {
                    totalPos = Vector3Add(totalPos, bone->position.position);
                    validBoneCount++;
                }
            }
        }

        if (hasValidHead) {
            totalPos = Vector3Add(totalPos, headPos);
            validBoneCount++;
        }

        if (hasValidChest) {
            totalPos = Vector3Add(totalPos, chestPos);
            validBoneCount++;
        }
        if (hasValidHip) {
            totalPos = Vector3Add(totalPos, hipPos);
            validBoneCount++;
        }
    }

    if (validBoneCount > 0) {
        Vector3 newCenter = Vector3Scale(totalPos, 1.0f / validBoneCount);

        if (app->camMode == 1 && app->autoPlay && app->autoCenterCalculated) {
            app->autoCenter = Vector3Lerp(app->autoCenter, newCenter, 0.15f);
        }
        else {
            app->autoCenter = newCenter;
        }

        app->autoCenterCalculated = true;
    }
}

static void App_PrepareRenderBones(AppState* app) {
    if (!app) return;

    if (app->currentFrame != app->lastProcessedFrame || app->forceUpdate) {
        CollectBonesForRendering(&app->animation, app->camera, &app->renderBones, &app->renderBonesCount,
            &app->renderBonesCapacity, app->boneConfigs, app->boneConfigCount);
        app->lastProcessedFrame = app->currentFrame;
        app->forceUpdate = false;
    }
}

static void App_PrepareRenderHeads(AppState* app) {
    if (!app || !app->renderHeadBillboards) {
        if (app) app->renderHeadsCount = 0;
        return;
    }

    CollectHeadsForRendering(&app->animation, &app->renderHeads,
        &app->renderHeadsCount, &app->renderHeadsCapacity,
        app->boneConfigs, app->boneConfigCount);
}

static void App_PrepareRenderTorsos(AppState* app) {
    if (!app || !app->renderTorsoBillboards) {
        if (app) app->renderTorsosCount = 0;
        return;
    }

    CollectTorsosForRendering(&app->animation, &app->renderTorsos,
        &app->renderTorsosCount, &app->renderTorsosCapacity,
        app->boneConfigs, app->boneConfigCount);
}



// Estructura para renderizado con detección de Z-fighting
typedef struct {
    int type; // 0=torso, 1=bone, 2=head
    int index;
    float distance;
    float depthBias;
    bool hasZFighting; // Flag para Z-fighting detectado
} RenderItem;

// Función para detectar Z-fighting entre elementos cercanos
static bool DetectZFighting(RenderItem* items, int itemCount, float threshold) {
    bool hasZFighting = false;

    for (int i = 0; i < itemCount; i++) {
        items[i].hasZFighting = false;

        for (int j = i + 1; j < itemCount; j++) {
            // Calcular diferencia de profundidad real (sin bias)
            float depthDiff = fabs(items[i].distance - items[j].distance);

            // Si están muy cerca en profundidad, hay potencial Z-fighting
            if (depthDiff < threshold) {
                items[i].hasZFighting = true;
                items[j].hasZFighting = true;
                hasZFighting = true;
            }
        }
    }

    return hasZFighting;
}

static void App_Draw(AppState* app) {
    if (!app) return;

    BeginDrawing();
    ClearBackground(RAYWHITE);

    const char* modeText = "CLASSIC MODE";
    const char* controlsText = "M: Toggle Morphing | H: Toggle Heads | T: Toggle Torsos | C: Mouse Control | 1/2: Camera Mode | Space: Play/Pause";
    DrawText(modeText, 10, 10, 20, BLUE);
    DrawText(controlsText, 10, 35, 16, DARKGRAY);

    char frameText[64];
    snprintf(frameText, sizeof(frameText), "Frame: %d/%d %s", app->currentFrame + 1, app->maxFrames, app->autoPlay ? "(Playing)" : "(Paused)");
    DrawText(frameText, 10, 55, 16, DARKGRAY);

    char statsText[256];
    snprintf(statsText, sizeof(statsText), "Bones: %d | Heads: %s (%d) | Torsos: %s (%d) | Camera Mode: %d | Mouse Control: %s",
        app->renderBonesCount, app->renderHeadBillboards ? "ON" : "OFF", app->renderHeadsCount,
        app->renderTorsoBillboards ? "ON" : "OFF", app->renderTorsosCount,
        app->camMode, app->cameraMouseControl ? "ON" : "OFF");
    DrawText(statsText, 10, 75, 16, DARKGRAY);

    BeginMode3D(app->camera);
    DrawGrid(24, 0.5f);
    if (app->autoCenterCalculated) DrawSphereWires(app->autoCenter, 0.05f, 8, 8, ORANGE);

    if (app->renderBonesCount > 0 || app->renderHeadsCount > 0 || app->renderTorsosCount > 0) {
        // Crear array combinado para ordenar
        RenderItem renderItems[512];
        int itemCount = 0;

        Vector3 camPos = app->camera.position;

        // Bias más suaves para evitar problemas visuales
        const float TORSO_BIAS = 0.001f;
        const float BONE_BIAS = 0.0f;
        const float HEAD_BIAS = -0.001f;
        const float INDEX_BIAS = -0.00001f;

        // Umbral de Z-fighting
        const float Z_FIGHTING_THRESHOLD = 0.01f; // 1cm

        // Agregar torsos
        for (int i = 0; i < app->renderTorsosCount; i++) {
            const TorsoRenderData* torso = &app->renderTorsos[i];
            if (!torso->valid || !torso->visible) continue;

            renderItems[itemCount].type = 0;
            renderItems[itemCount].index = i;
            renderItems[itemCount].distance = Vector3Distance(camPos, torso->position);
            renderItems[itemCount].depthBias = TORSO_BIAS + (INDEX_BIAS * i);
            renderItems[itemCount].hasZFighting = false;
            itemCount++;
        }

        // Agregar bones
        for (int i = 0; i < app->renderBonesCount; i++) {
            const BoneRenderData* bone = &app->renderBones[i];
            if (!bone->valid || !bone->visible) continue;

            renderItems[itemCount].type = 1;
            renderItems[itemCount].index = i;
            renderItems[itemCount].distance = Vector3Distance(camPos, bone->position);
            renderItems[itemCount].depthBias = BONE_BIAS + (INDEX_BIAS * i);
            renderItems[itemCount].hasZFighting = false;
            itemCount++;
        }

        // Agregar heads
        for (int i = 0; i < app->renderHeadsCount; i++) {
            const HeadRenderData* head = &app->renderHeads[i];
            if (!head->valid || !head->visible) continue;

            renderItems[itemCount].type = 2;
            renderItems[itemCount].index = i;
            renderItems[itemCount].distance = Vector3Distance(camPos, head->position);
            renderItems[itemCount].depthBias = HEAD_BIAS + (INDEX_BIAS * i);
            renderItems[itemCount].hasZFighting = false;
            itemCount++;
        }

        // Detectar Z-fighting
        DetectZFighting(renderItems, itemCount, Z_FIGHTING_THRESHOLD);

        // Ordenar por distancia + bias (más lejos primero para renderizado back-to-front)
        for (int i = 0; i < itemCount - 1; i++) {
            for (int j = 0; j < itemCount - i - 1; j++) {
                float distanceA = renderItems[j].distance + renderItems[j].depthBias;
                float distanceB = renderItems[j + 1].distance + renderItems[j + 1].depthBias;

                if (distanceA < distanceB) {
                    RenderItem temp = renderItems[j];
                    renderItems[j] = renderItems[j + 1];
                    renderItems[j + 1] = temp;
                }
            }
        }

        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthTest();


        // Renderizar todos los elementos en orden
        for (int i = 0; i < itemCount; i++) {
            RenderItem* item = &renderItems[i];

            if (item->type == 0) { // Torso
                const TorsoRenderData* torso = &app->renderTorsos[item->index];

                // Calcular posición de renderizado con bias muy pequeño
                Vector3 renderPosition = torso->position;
                Vector3 toCam = Vector3Subtract(camPos, torso->position);
                float distance = Vector3Length(toCam);

                if (distance > 0.001f) {
                    Vector3 toCamNorm = Vector3Normalize(toCam);
                    renderPosition = Vector3Add(torso->position, Vector3Scale(toCamNorm, item->depthBias));
                }

                int texIndex = App_GetTextureIndex(app, torso->texturePath);
                Texture2D currentTex = app->textures[texIndex];

                TorsoRenderData adjustedTorso = *torso;
                adjustedTorso.position = renderPosition;
                DrawTorsoBillboard(currentTex, app->camera, &adjustedTorso, app->physCols, app->physRows);
            }
            else if (item->type == 1) { // Bone
                const BoneRenderData* bone = &app->renderBones[item->index];

                // Calcular posición de renderizado con bias
                Vector3 renderPosition = bone->position;
                Vector3 toCam = Vector3Subtract(camPos, bone->position);
                float distance = Vector3Length(toCam);

                if (distance > 0.001f) {
                    Vector3 toCamNorm = Vector3Normalize(toCam);
                    renderPosition = Vector3Add(bone->position, Vector3Scale(toCamNorm, item->depthBias));
                }

                int texIndex = App_GetTextureIndex(app, bone->texturePath);
                Texture2D currentTex = app->textures[texIndex];

                float physCellW = (float)currentTex.width / (float)app->physCols;
                float physCellH = (float)currentTex.height / (float)app->physRows;
                float logicalCellW = physCellW * (app->physCols / ATLAS_COLS);
                float logicalCellH = physCellH * (app->physRows / ATLAS_ROWS);
                float aspect = logicalCellW / logicalCellH;
                Vector2 worldSize = (Vector2){ bone->size * aspect, bone->size };

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

                char conns[3][32];
                float prios[3];
                Vector3 neighborPos = { 0.0f, 0.0f, 0.0f };
                bool haveNeighbor = false;

                if (GetBoneConnectionsWithPriority(bone->boneName, conns, prios)) {
                    for (int k = 0; k < 3; k++) {
                        if (conns[k][0] == '\0') continue;
                        BoneRenderData* nb = FindRenderBoneByName(app->renderBones, app->renderBonesCount, conns[k]);
                        if (nb && nb->valid && nb->visible) {
                            float d = Vector3Distance(bone->position, nb->position);
                            if (d > 0.001f) {
                                neighborPos = nb->position;
                                haveNeighbor = true;
                                break;
                            }
                        }
                    }
                }

                DrawBonetileCustomWithRoll(currentTex, app->camera, src, renderPosition, worldSize,
                    rotation, finalMirror, true, haveNeighbor, neighborPos, bone->boneName);
            }
            else if (item->type == 2) { // Head
                const HeadRenderData* head = &app->renderHeads[item->index];

                // Calcular posición de renderizado con bias
                Vector3 renderPosition = head->position;
                Vector3 toCam = Vector3Subtract(camPos, head->position);
                float distance = Vector3Length(toCam);

                if (distance > 0.001f) {
                    Vector3 toCamNorm = Vector3Normalize(toCam);
                    renderPosition = Vector3Add(head->position, Vector3Scale(toCamNorm, item->depthBias));
                }

                int texIndex = App_GetTextureIndex(app, head->texturePath);
                Texture2D currentTex = app->textures[texIndex];

                HeadRenderData adjustedHead = *head;
                adjustedHead.position = renderPosition;
                DrawHeadBillboard(currentTex, app->camera, &adjustedHead, app->physCols, app->physRows);
            }
        }

        EndBlendMode();
        //rlEnableDepthTest();
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
        App_PrepareRenderBones(&app);
        App_PrepareRenderHeads(&app);
        App_PrepareRenderTorsos(&app);
        App_Draw(&app);
    }

    App_Shutdown(&app);
    return 0;
}
