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

// ====== NUEVO: Sistema de eventos de animación ======
#define BONES_ANIMATION_EVENTS_IMPLEMENTATION
#include "bones_anim_events.h"

#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080
#define MAX_TEXTURES 13
#define MAX_RENDER_ITEMS 512

// Constants
static const float AUTO_PLAY_LERP = 0.15f;
static const float ORBIT_SENSITIVITY = 0.01f;
static const float FPS_SENSITIVITY = 0.003f;
static const float ZOOM_SENSITIVITY = 0.5f;
static const float MIN_ORBIT_RADIUS = 0.5f;
static const float MAX_ORBIT_RADIUS = 20.0f;
static const float MIN_PITCH = -85.0f * PI / 180.0f;
static const float MAX_PITCH = 81.0f * PI / 180.0f;
static const float BASE_SPEED = 5.0f;
static const float MOVEMENT_SPEED = 3.0f;
static const float VALID_POSITION_THRESHOLD = 0.01f;
static const float MIN_DISTANCE_THRESHOLD = 0.001f;

// Depth bias constants
static const float TORSO_BIAS = 0.001f;
static const float BONE_BIAS = 0.0f;
static const float HEAD_BIAS = -0.001f;
static const float INDEX_BIAS = -0.00001f;
static const float Z_FIGHTING_THRESHOLD = 0.01f;

// ====== NUEVO: Globales para sistema de animación ======
TextureSetCollection* g_textureSets = NULL;
static AnimationController* g_animController = NULL;

typedef struct {
    Camera camera;
    int camMode;
    float orbitYaw, orbitPitch, orbitRadius;
    
    // Core systems
    SimpleTextureSystem textureSystem;
    BoneConfig* boneConfigs;
    int boneConfigCount;
    BonesAnimation animation;
    BonesRenderConfig renderConfig;
    
    // Render data
    BoneRenderData* renderBones;
    int renderBonesCount;
    int renderBonesCapacity;
    HeadRenderData* renderHeads;
    int renderHeadsCount;
    int renderHeadsCapacity;
    TorsoRenderData* renderTorsos;
    int renderTorsosCount;
    int renderTorsosCapacity;
    
    // Textures
    Texture2D textures[MAX_TEXTURES];
    char texturePaths[MAX_TEXTURES][MAX_FILE_PATH_LENGTH];
    int textureCount;
    
    // Animation
    int physCols, physRows;
    int currentFrame;
    int maxFrames;
    bool autoPlay;
    float autoPlayTimer;
    float autoPlaySpeed;
    
    // Auto center
    Vector3 autoCenter;
    bool autoCenterCalculated;
    
    // Control flags
    bool renderHeadBillboards;
    bool renderTorsoBillboards;
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

/*
 * +---------------------------------------------------------------+
 * | Function: FindPersonByBoneName                               |
 * +---------------------------------------------------------------+
 */
static const Person* FindPersonByBoneName(const AnimationFrame* frame, const char* boneName) {
    if (!frame || !boneName) return NULL;
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            if (strcmp(person->bones[b].name, boneName) == 0) return person;
        }
    }
    return NULL;
}

/*
 * +---------------------------------------------------------------+
 * | Function: DetectZFighting                                    |
 * +---------------------------------------------------------------+
 */
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

/*
 * +---------------------------------------------------------------+
 * | Function: SortRenderItems                                     |
 * +---------------------------------------------------------------+
 */
static void SortRenderItems(RenderItem* items, int itemCount) {
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
        if (!swapped) break;
    }
}

/*
 * +---------------------------------------------------------------+
 * | Function: App_Init                                            |
 * +---------------------------------------------------------------+
 */
static bool App_Init(AppState* app) {
    if (!app) return false;
    memset(app, 0, sizeof(*app));

    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - Unified Animation System");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
#if defined(__linux__)
    for (int i = 0; i < 5; i++) PollInputEvents();
#endif
    MaximizeWindow();
    SetTargetFPS(120);

    // Initialize camera
    app->camera = (Camera){
        .position = {0.0f, 0.6f, 2.5f},
        .target = {0.0f, 0.6f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    // Initialize settings
    app->camMode = 1;
    app->orbitRadius = 2.5f;
    app->orbitPitch = -0.2f;
    app->renderHeadBillboards = true;
    app->renderTorsoBillboards = true;
    app->physCols = 4;
    app->physRows = 4;
    app->autoPlaySpeed = 0.1f;
    app->lastProcessedFrame = -1;
    app->autoPlay = true; // Start with autoplay ON

    // ====== NUEVO: Cargar texture sets ======
    g_textureSets = BonesTextureSets_Create();
    if (!BonesTextureSets_LoadFromFile(g_textureSets, "data/textures/texture_sets.txt")) {
        TraceLog(LOG_WARNING, "TEXTURE_SETS: No texture sets loaded, using defaults");
    } else {
        TraceLog(LOG_INFO, "TEXTURE_SETS: Successfully loaded");
    }

    // Load configurations
    LoadSimpleTextureConfig(&app->textureSystem, "data/textures/bone_textures.txt");
    LoadBoneConfigurations(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);

    // Initialize animation system
    if (BonesInit(&app->animation, 1000) != BONES_SUCCESS) {
        TraceLog(LOG_ERROR, "Failed to initialize bones animation system");
        CloseWindow();
        return false;
    }

    // ====== MODIFICADO: Cargar pose JSON inicial ======
    BonesLoadFromJSON(&app->animation, "data/poses/idle.json");
    
    app->renderConfig = BonesGetDefaultRenderConfig();
    app->renderConfig.drawDebugSpheres = true;
    app->renderConfig.debugColor = GREEN;
    app->renderConfig.debugSphereRadius = 0.035f;
    app->renderConfig.enableDepthSorting = true;
    BonesSetRenderConfig(&app->renderConfig);

    app->maxFrames = BonesGetFrameCount(&app->animation);

    // ====== NUEVO: Crear animation controller ======
    g_animController = AnimController_Create(&app->animation, g_textureSets);
    if (!g_animController) {
        TraceLog(LOG_ERROR, "Failed to create animation controller");
        CloseWindow();
        return false;
    }

    // ====== NUEVO: Cargar metadata de animación ======
    if (AnimController_LoadClipMetadata(g_animController, "data/animations/idle.anim")) {
        if (AnimController_PlayClip(g_animController, "idle")) {
            TraceLog(LOG_INFO, "ANIM_CONTROLLER: Animation 'idle' loaded and playing");
        } else {
            TraceLog(LOG_WARNING, "ANIM_CONTROLLER: Failed to play 'idle' clip");
        }
    } else {
        TraceLog(LOG_WARNING, "ANIM_CONTROLLER: Failed to load animation metadata from idle.anim");
    }

    return true;
}

/*
 * +---------------------------------------------------------------+
 * | Function: App_Shutdown                                        |
 * +---------------------------------------------------------------+
 */
static void App_Shutdown(AppState* app) {
    if (!app) return;

    // ====== NUEVO: Cleanup animation system ======
    AnimController_Free(g_animController);
    BonesTextureSets_Free(g_textureSets);
    g_animController = NULL;
    g_textureSets = NULL;

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

/*
 * +---------------------------------------------------------------+
 * | Function: App_GetTextureIndex                                 |
 * +---------------------------------------------------------------+
 */
static int App_GetTextureIndex(AppState* app, const char* path) {
    if (!app || !path) return 0;

    // Check if texture already loaded
    for (int i = 0; i < app->textureCount; i++) {
        if (strcmp(app->texturePaths[i], path) == 0) return i;
    }

    if (app->textureCount >= MAX_TEXTURES) return 0;

    // Load new texture
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

/*
 * +---------------------------------------------------------------+
 * | Function: App_HandleInput                                     |
 * +---------------------------------------------------------------+
 */
static void App_HandleInput(AppState* app) {
    if (!app) return;

    static int framesSinceLastPress = 0;
    framesSinceLastPress++;

    // ====== NUEVO: Controles de animación ======
    if (IsKeyPressed(KEY_P)) {
        if (g_animController && g_animController->playing) {
            AnimController_Pause(g_animController);
            TraceLog(LOG_INFO, "ANIM: PAUSED");
        } else if (g_animController) {
            AnimController_Resume(g_animController);
            TraceLog(LOG_INFO, "ANIM: RESUMED");
        }
    }

    // ====== NUEVO: Cambiar entre animaciones ======
    if (IsKeyPressed(KEY_ONE)) {
        TraceLog(LOG_INFO, "Loading IDLE animation...");
        BonesLoadFromJSON(&app->animation, "data/poses/idle.json");
        app->maxFrames = BonesGetFrameCount(&app->animation);
        if (AnimController_LoadClipMetadata(g_animController, "data/animations/idle.anim")) {
            AnimController_PlayClip(g_animController, "idle");
        }
        app->currentFrame = 0;
        app->autoCenterCalculated = false;
    }

    if (IsKeyPressed(KEY_TWO)) {
        TraceLog(LOG_INFO, "Loading TALK animation...");
        BonesLoadFromJSON(&app->animation, "data/poses/talk.json");
        app->maxFrames = BonesGetFrameCount(&app->animation);
        if (AnimController_LoadClipMetadata(g_animController, "data/animations/talk.anim")) {
            AnimController_PlayClip(g_animController, "talk");
        }
        app->currentFrame = 0;
        app->autoCenterCalculated = false;
    }

    // Frame navigation
    if (app->animation.isLoaded && app->maxFrames > 0) {
        bool frameChanged = false;
        int newFrame = app->currentFrame;

        if (IsKeyDown(KEY_LEFT) && newFrame > 0 && framesSinceLastPress > 15) { 
            newFrame--; frameChanged = true; framesSinceLastPress = 0; 
        }
        if (IsKeyDown(KEY_RIGHT) && newFrame < app->maxFrames - 1 && framesSinceLastPress > 15) { 
            newFrame++; frameChanged = true; framesSinceLastPress = 0; 
        }
        if (IsKeyPressed(KEY_HOME)) { newFrame = 0; frameChanged = true; }
        if (IsKeyPressed(KEY_END) && app->maxFrames > 0) { 
            newFrame = app->maxFrames - 1; frameChanged = true; 
        }

        if (frameChanged) {
            app->currentFrame = newFrame;
            BonesSetFrame(&app->animation, app->currentFrame);
            app->autoCenterCalculated = false;
        }

        if (IsKeyPressed(KEY_SPACE)) app->autoPlay = !app->autoPlay;
    }

    // Other controls
    if (IsKeyPressed(KEY_F5)) {
        TraceLog(LOG_INFO, "Reloading configurations...");
        if (LoadSimpleTextureConfig(&app->textureSystem, "bone_textures.txt")) {
            LoadBoneConfigurations(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);
        }
        // ====== NUEVO: Recargar texture sets ======
        BonesTextureSets_LoadFromFile(g_textureSets, "data/textures/texture_sets.txt");
    }

    // Camera mode switching (MODIFICADO: ahora 3 y 4 para cámara)
    if (IsKeyPressed(KEY_THREE)) {
        app->camMode = 1;
        Vector3 target = app->autoCenterCalculated ? app->autoCenter : (Vector3){0, 0.6f, 0};
        Vector3 dir = Vector3Subtract(app->camera.position, target);
        float orbit_distance = Vector3Length(dir);
        app->orbitYaw = atan2f(dir.x, dir.z);
        app->orbitPitch = asinf(Clamp(dir.y / orbit_distance, -1.0f, 1.0f));
        app->orbitRadius = orbit_distance;
        app->camera.target = target;
        EnableCursor();
        TraceLog(LOG_INFO, "Camera mode: ORBIT");
    }
    if (IsKeyPressed(KEY_FOUR)) {
        app->camMode = 2;
        Vector3 target = app->autoCenterCalculated ? app->autoCenter : (Vector3){0, 0.6f, 0};
        Vector3 dir = Vector3Subtract(app->camera.position, target);
        float orbit_distance = Vector3Length(dir);
        float orbit_yaw = atan2f(dir.z, dir.x);
        float orbit_pitch = asinf(Clamp(dir.y / orbit_distance, -1.0f, 1.0f));
        app->camera.position.x = orbit_distance * cosf(orbit_pitch) * cosf(orbit_yaw) + target.x;
        app->camera.position.y = orbit_distance * sinf(orbit_pitch) + target.y;
        app->camera.position.z = orbit_distance * cosf(orbit_pitch) * sinf(orbit_yaw) + target.z;
        app->camera.target = target;
        app->orbitYaw = orbit_yaw + PI;
        app->orbitPitch = -orbit_pitch;
        DisableCursor();
        TraceLog(LOG_INFO, "Camera mode: FPS");
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

/*
 * +---------------------------------------------------------------+
 * | Function: App_UpdateCamera                                    |
 * +---------------------------------------------------------------+
 */
static void App_UpdateCamera(AppState* app, float dt) {
    if (!app) return;

    Vector3 cameraTarget = app->autoCenterCalculated ? app->autoCenter : (Vector3){0, 0.6f, 0};

    if (app->camMode == 1) {
        // Orbit camera
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mouseDelta = GetMouseDelta();
            app->orbitYaw += mouseDelta.x * ORBIT_SENSITIVITY;
            app->orbitPitch = Clamp(app->orbitPitch - mouseDelta.y * ORBIT_SENSITIVITY, MIN_PITCH, MAX_PITCH);
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            app->orbitRadius = Clamp(app->orbitRadius - wheel * ZOOM_SENSITIVITY, MIN_ORBIT_RADIUS, MAX_ORBIT_RADIUS);
        }

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
        // FPS camera
        Vector2 mouse_delta = GetMouseDelta();
        app->orbitYaw += mouse_delta.x * FPS_SENSITIVITY;
        app->orbitPitch -= mouse_delta.y * FPS_SENSITIVITY;
        app->orbitPitch = Clamp(app->orbitPitch, MIN_PITCH, MAX_PITCH);
        
        Vector3 forward;
        forward.x = cosf(app->orbitPitch) * cosf(app->orbitYaw);
        forward.y = sinf(app->orbitPitch);
        forward.z = cosf(app->orbitPitch) * sinf(app->orbitYaw);
        forward = Vector3Normalize(forward);
        
        Vector3 right_dir = Vector3Normalize(Vector3CrossProduct(forward, app->camera.up));
        float speed = BASE_SPEED * dt;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
            speed *= MOVEMENT_SPEED;
        
        if (IsKeyDown(KEY_W))
            app->camera.position = Vector3Add(app->camera.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_S))
            app->camera.position = Vector3Subtract(app->camera.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_D))
            app->camera.position = Vector3Add(app->camera.position, Vector3Scale(right_dir, speed));
        if (IsKeyDown(KEY_A))
            app->camera.position = Vector3Subtract(app->camera.position, Vector3Scale(right_dir, speed));
        if (IsKeyDown(KEY_Q))
            app->camera.position = Vector3Add(app->camera.position, Vector3Scale(app->camera.up, speed));
        if (IsKeyDown(KEY_LEFT_CONTROL))
            app->camera.position = Vector3Subtract(app->camera.position, Vector3Scale(app->camera.up, speed));
        
        app->camera.target = Vector3Add(app->camera.position, forward);
    }
}

/*
 * +---------------------------------------------------------------+
 * | Function: App_UpdateAutoCenter                                |
 * +---------------------------------------------------------------+
 */
static void App_UpdateAutoCenter(AppState* app) {
    if (!app || !app->animation.isLoaded || !BonesIsValidFrame(&app->animation, app->currentFrame)) return;

    const AnimationFrame* frame = &app->animation.frames[app->currentFrame];
    Vector3 totalPos = {0, 0, 0};
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

        if (hasValidHead) { totalPos = Vector3Add(totalPos, headPos); validBoneCount++; }
        if (hasValidChest) { totalPos = Vector3Add(totalPos, chestPos); validBoneCount++; }
        if (hasValidHip) { totalPos = Vector3Add(totalPos, hipPos); validBoneCount++; }
    }

    if (validBoneCount > 0) {
        Vector3 newCenter = Vector3Scale(totalPos, 1.0f / validBoneCount);

        if (app->camMode == 1 && app->autoPlay && app->autoCenterCalculated) {
            app->autoCenter = Vector3Lerp(app->autoCenter, newCenter, AUTO_PLAY_LERP);
        } else {
            app->autoCenter = newCenter;
        }

        app->autoCenterCalculated = true;
    }
}

/*
 * +---------------------------------------------------------------+
 * | Function: App_PrepareRenderData                               |
 * +---------------------------------------------------------------+
 */
static void App_PrepareRenderData(AppState* app) {
    if (!app) return;

    if (app->currentFrame != app->lastProcessedFrame || app->forceUpdate) {
        CollectBonesForRendering(&app->animation, app->camera, &app->renderBones, &app->renderBonesCount,
            &app->renderBonesCapacity, app->boneConfigs, app->boneConfigCount);
        app->lastProcessedFrame = app->currentFrame;
        app->forceUpdate = false;
    }

    if (app->renderHeadBillboards) {
        CollectHeadsForRendering(&app->animation, &app->renderHeads, &app->renderHeadsCount,
            &app->renderHeadsCapacity, app->boneConfigs, app->boneConfigCount);
    } else {
        app->renderHeadsCount = 0;
    }

    if (app->renderTorsoBillboards) {
        CollectTorsosForRendering(&app->animation, &app->renderTorsos, &app->renderTorsosCount,
            &app->renderTorsosCapacity, app->boneConfigs, app->boneConfigCount);
    } else {
        app->renderTorsosCount = 0;
    }
}

/*
 * +---------------------------------------------------------------+
 * | Function: RenderBone                                          |
 * +---------------------------------------------------------------+
 */
static void RenderBone(AppState* app, const BoneRenderData* bone, Vector3 renderPosition, const AnimationFrame* frame) {
    int texIndex = App_GetTextureIndex(app, bone->texturePath);
    Texture2D currentTex = app->textures[texIndex];

    bool isWrist = IsWristBone(bone->boneName);
    int usedCols = isWrist ? 5 : app->physCols;
    int usedRows = isWrist ? 5 : app->physRows;

    float physCellW = (float)currentTex.width / (float)usedCols;
    float physCellH = (float)currentTex.height / (float)usedRows;
    float aspect = physCellW / physCellH;
    Vector2 worldSize = {bone->size * aspect, bone->size};

    int chosenIndex = 0;
    float rotation = 0.0f;
    bool mirrored = false;

    const Person* bonePerson = frame ? FindPersonByBoneName(frame, bone->boneName) : NULL;

    if (isWrist) {
        CalculateHandBoneRenderData(bone->position, app->camera, &chosenIndex, &rotation, &mirrored, bone->boneName);
    } else if (bone->orientation.valid) {
        CalculateLimbBoneRenderData(bone, bonePerson, app->camera, &chosenIndex, &rotation, &mirrored);
    }

    int maxIndex = usedCols * usedRows - 1;
    if (chosenIndex < 0) chosenIndex = 0;
    if (chosenIndex > maxIndex) chosenIndex %= (maxIndex + 1);

    int logicalCol = chosenIndex % usedCols;
    int logicalRow = chosenIndex / usedCols;
    bool finalMirror = isWrist ? false : mirrored;
    Rectangle src = SrcFromLogical(currentTex, logicalCol, logicalRow, usedCols, usedRows, finalMirror, &finalMirror);

    // Find neighbor for roll calculation
    char conns[3][32];
    float prios[3];
    Vector3 neighborPos = {0};
    bool haveNeighbor = false;
    if (GetBoneConnectionsWithPriority(bone->boneName, conns, prios)) {
        for (int k = 0; k < 3 && !haveNeighbor; k++) {
            if (conns[k][0] == '\0') continue;
            BoneRenderData* nb = FindRenderBoneByName(app->renderBones, app->renderBonesCount, conns[k]);
            if (nb && nb->valid && nb->visible && Vector3Distance(bone->position, nb->position) > MIN_DISTANCE_THRESHOLD) {
                neighborPos = nb->position;
                haveNeighbor = true;
            }
        }
    }

    DrawBonetileCustomWithRoll(currentTex, app->camera, src, renderPosition, worldSize, rotation,
        finalMirror, haveNeighbor, neighborPos, bone, bonePerson);
}

/*
 * +---------------------------------------------------------------+
 * | Function: App_Draw                                            |
 * +---------------------------------------------------------------+
 */
static void App_Draw(AppState* app) {
    if (!app) return;

    const AnimationFrame* frame = NULL;
    if (app->animation.isLoaded && BonesIsValidFrame(&app->animation, app->currentFrame)) {
        frame = &app->animation.frames[app->currentFrame];
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    // ====== MODIFICADO: UI con información de animación ======
    DrawText("UNIFIED ANIMATION SYSTEM", 10, 10, 20, BLUE);
    DrawText("1: Idle | 2: Talk | 3: Orbit Cam | 4: FPS Cam | P: Pause | Space: Play/Pause", 10, 35, 16, DARKGRAY);
    DrawText("H: Toggle Heads | T: Toggle Torsos | F5: Reload | Left/Right: Frame Nav", 10, 55, 16, DARKGRAY);

    char frameText[128];
    const char* animStatus = app->autoPlay ? "(Playing)" : "(Paused)";
    const char* currentAnim = "unknown";
    if (g_animController && g_animController->currentClipIndex >= 0) {
        currentAnim = g_animController->clips[g_animController->currentClipIndex].name;
    }
    snprintf(frameText, sizeof(frameText), "Animation: %s | Frame: %d/%d %s", 
             currentAnim, app->currentFrame + 1, app->maxFrames, animStatus);
    DrawText(frameText, 10, 75, 16, DARKGRAY);

    char statsText[256];
    snprintf(statsText, sizeof(statsText), "Bones: %d | Heads: %s (%d) | Torsos: %s (%d) | Camera: %s",
        app->renderBonesCount, 
        app->renderHeadBillboards ? "ON" : "OFF", app->renderHeadsCount,
        app->renderTorsoBillboards ? "ON" : "OFF", app->renderTorsosCount,
        app->camMode == 1 ? "ORBIT" : "FPS");
    DrawText(statsText, 10, 95, 16, DARKGRAY);

    // ====== NUEVO: Mostrar variante activa de Head ======
    if (g_textureSets) {
        const char* activeVariant = BonesTextureSets_GetActiveVariantName(g_textureSets, "Head");
        if (activeVariant) {
            char variantText[128];
            snprintf(variantText, sizeof(variantText), "Head Texture: %s", activeVariant);
            DrawText(variantText, 10, 115, 16, DARKGREEN);
        }
    }

    BeginMode3D(app->camera);
    DrawGrid(24, 0.5f);
    if (app->autoCenterCalculated) {
        DrawSphereWires(app->autoCenter, 0.05f, 8, 8, ORANGE);
    }

    // Prepare render items
    int totalItems = app->renderBonesCount + app->renderHeadsCount + app->renderTorsosCount;
    if (totalItems > 0) {
        static RenderItem renderItems[MAX_RENDER_ITEMS];
        int itemCount = 0;
        Vector3 camPos = app->camera.position;

        // Collect render items
        for (int i = 0; i < app->renderTorsosCount && itemCount < MAX_RENDER_ITEMS; i++) {
            const TorsoRenderData* torso = &app->renderTorsos[i];
            if (!torso->valid || !torso->visible) continue;
            renderItems[itemCount++] = (RenderItem){
                .type = 0, .index = i,
                .distance = Vector3Distance(camPos, torso->position),
                .depthBias = TORSO_BIAS + (INDEX_BIAS * i)
            };
        }

        for (int i = 0; i < app->renderBonesCount && itemCount < MAX_RENDER_ITEMS; i++) {
            const BoneRenderData* bone = &app->renderBones[i];
            if (!bone->valid || !bone->visible) continue;
            renderItems[itemCount++] = (RenderItem){
                .type = 1, .index = i,
                .distance = Vector3Distance(camPos, bone->position),
                .depthBias = BONE_BIAS + (INDEX_BIAS * i)
            };
        }

        for (int i = 0; i < app->renderHeadsCount && itemCount < MAX_RENDER_ITEMS; i++) {
            const HeadRenderData* head = &app->renderHeads[i];
            if (!head->valid || !head->visible) continue;
            renderItems[itemCount++] = (RenderItem){
                .type = 2, .index = i,
                .distance = Vector3Distance(camPos, head->position),
                .depthBias = HEAD_BIAS + (INDEX_BIAS * i)
            };
        }

        DetectZFighting(renderItems, itemCount);
        SortRenderItems(renderItems, itemCount);

        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthTest();

        // Single pass: Render all items in order, with special neck handling for heads
        for (int i = 0; i < itemCount; i++) {
            RenderItem* item = &renderItems[i];

            // Special handling: If current item is a head, first render any neck bones behind it
            if (item->type == 2) {
                const HeadRenderData* currentHead = &app->renderHeads[item->index];

                // Look for neck bones that should be rendered before this head
                for (int j = 0; j < app->renderBonesCount; j++) {
                    const BoneRenderData* bone = &app->renderBones[j];
                    if (!bone->valid || !bone->visible) continue;
                    if (strcmp(bone->boneName, "Neck") != 0) continue;

                    // Check if this neck belongs to the same character/group as the head
                    float neckHeadDistance = Vector3Distance(bone->position, currentHead->position);
                    if (neckHeadDistance < 2.0f) {
                        Vector3 toCam = Vector3Subtract(camPos, bone->position);
                        float distance = Vector3Length(toCam);
                        Vector3 renderOffset = {0};
                        if (distance > MIN_DISTANCE_THRESHOLD) {
                            Vector3 toCamNorm = Vector3Normalize(toCam);
                            renderOffset = Vector3Scale(toCamNorm, BONE_BIAS + (INDEX_BIAS * j));
                        }
                        Vector3 renderPosition = Vector3Add(bone->position, renderOffset);
                        RenderBone(app, bone, renderPosition, frame);
                    }
                }
            }

            // Render the current item
            Vector3 toCam = Vector3Subtract(camPos,
                item->type == 0 ? app->renderTorsos[item->index].position :
                item->type == 1 ? app->renderBones[item->index].position :
                app->renderHeads[item->index].position);

            float distance = Vector3Length(toCam);
            Vector3 renderOffset = {0};
            if (distance > MIN_DISTANCE_THRESHOLD) {
                Vector3 toCamNorm = Vector3Normalize(toCam);
                renderOffset = Vector3Scale(toCamNorm, item->depthBias);
            }

            if (item->type == 0) {
                // Render torso
                const TorsoRenderData* torso = &app->renderTorsos[item->index];
                TorsoRenderData adjusted = *torso;
                adjusted.position = Vector3Add(torso->position, renderOffset);
                int texIndex = App_GetTextureIndex(app, torso->texturePath);
                DrawTorsoBillboard(app->textures[texIndex], app->camera, &adjusted, app->physCols, app->physRows);
            }
            else if (item->type == 1) {
                // Render bone (skip necks here since they're handled above)
                const BoneRenderData* bone = &app->renderBones[item->index];
                if (strcmp(bone->boneName, "Neck") != 0) {
                    Vector3 renderPosition = Vector3Add(bone->position, renderOffset);
                    RenderBone(app, bone, renderPosition, frame);
                }
            }
            else if (item->type == 2) {
                // Render head
                const HeadRenderData* head = &app->renderHeads[item->index];
                HeadRenderData adjusted = *head;
                adjusted.position = Vector3Add(head->position, renderOffset);
                int texIndex = App_GetTextureIndex(app, head->texturePath);
                DrawHeadBillboard(app->textures[texIndex], app->camera, &adjusted, app->physCols, app->physRows);
            }
        }

        EndBlendMode();
    }

    EndMode3D();
    EndDrawing();
}

/*
 * +---------------------------------------------------------------+
 * | Function: main                                                |
 * +---------------------------------------------------------------+
 */
int main(void) {
    AppState app;
    if (!App_Init(&app)) return -1;
    
    TraceLog(LOG_INFO, "===========================================");
    TraceLog(LOG_INFO, "Unified Animation System Initialized");
    TraceLog(LOG_INFO, "===========================================");
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        // ====== NUEVO: Update animation controller ======
        if (g_animController && app.autoPlay) {
            AnimController_Update(g_animController, dt);
            
            // Sincronizar frame del JSON con el controller
            int frameFromController = AnimController_GetCurrentFrame(g_animController);
            BonesSetFrame(&app.animation, frameFromController);
            app.currentFrame = frameFromController;
        }
        
        App_HandleInput(&app);
        App_UpdateCamera(&app, dt);
        App_UpdateAutoCenter(&app);
        App_PrepareRenderData(&app);
        App_Draw(&app);
    }
    
    TraceLog(LOG_INFO, "Shutting down animation system...");
    App_Shutdown(&app);
    return 0;
}