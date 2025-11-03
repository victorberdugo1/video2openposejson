#include "raylib.h"
#include "bones_core.h"

// ============================================================================
// CONSTANTES DE LA APLICACIÓN
// ============================================================================

#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080

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

// ============================================================================
// ESTRUCTURA DE LA APLICACIÓN
// ============================================================================

typedef struct {
    BonesRenderer* renderer;
    
    SimpleTextureSystem textureSystem;
    BoneConfig* boneConfigs;
    int boneConfigCount;
    BonesAnimation animation;
    BonesRenderConfig renderConfig;
    
    BoneRenderData* renderBones;
    int renderBonesCount;
    int renderBonesCapacity;
    HeadRenderData* renderHeads;
    int renderHeadsCount;
    int renderHeadsCapacity;
    TorsoRenderData* renderTorsos;
    int renderTorsosCount;
    int renderTorsosCapacity;
    
    int currentFrame;
    int maxFrames;
    bool autoPlay;
    float autoPlaySpeed;
    
    Vector3 autoCenter;
    bool autoCenterCalculated;
    
    bool renderHeadBillboards;
    bool renderTorsoBillboards;
    int lastProcessedFrame;
    bool forceUpdate;
    
    // Control de cámara
    int camMode;
    float orbitYaw, orbitPitch, orbitRadius;
} AppState;

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

TextureSetCollection* g_textureSets = NULL;
static AnimationController* g_animController = NULL;

// ============================================================================
// INICIALIZACIÓN Y FINALIZACIÓN
// ============================================================================

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

    // Inicializar renderizador
    app->renderer = BonesRenderer_Create();
    if (!app->renderer || !BonesRenderer_Init(app->renderer)) {
        TraceLog(LOG_ERROR, "Failed to initialize renderer");
        CloseWindow();
        return false;
    }

    app->camMode = 1;
    app->orbitRadius = 2.5f;
    app->orbitPitch = -0.2f;
    app->renderHeadBillboards = true;
    app->renderTorsoBillboards = true;
    app->autoPlaySpeed = 0.1f;
    app->lastProcessedFrame = -1;
    app->autoPlay = true;

    // Inicializar sistemas de texturas
    g_textureSets = BonesTextureSets_Create();
    if (!BonesTextureSets_LoadFromFile(g_textureSets, "data/textures/texture_sets.txt")) {
        TraceLog(LOG_WARNING, "TEXTURE_SETS: No texture sets loaded, using defaults");
    }

    LoadSimpleTextureConfig(&app->textureSystem, "data/textures/bone_textures.txt");
    LoadBoneConfigurations(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);

    if (BonesInit(&app->animation, 1000) != BONES_SUCCESS) {
        TraceLog(LOG_ERROR, "Failed to initialize bones animation system");
        CloseWindow();
        return false;
    }

    BonesLoadFromJSON(&app->animation, "data/poses/idle.json");
    
    app->renderConfig = BonesGetDefaultRenderConfig();
    app->renderConfig.drawDebugSpheres = true;
    app->renderConfig.debugColor = GREEN;
    app->renderConfig.debugSphereRadius = 0.035f;
    app->renderConfig.enableDepthSorting = true;
    BonesSetRenderConfig(&app->renderConfig);

    app->maxFrames = BonesGetFrameCount(&app->animation);

    g_animController = AnimController_Create(&app->animation, g_textureSets);
    if (!g_animController) {
        TraceLog(LOG_ERROR, "Failed to create animation controller");
        CloseWindow();
        return false;
    }

    if (AnimController_LoadClipMetadata(g_animController, "data/animations/idle.anim")) {
        if (AnimController_PlayClip(g_animController, "idle")) {
            TraceLog(LOG_INFO, "ANIM_CONTROLLER: Animation 'idle' loaded and playing");
        }
    }

    return true;
}

static void App_Shutdown(AppState* app) {
    if (!app) return;

    AnimController_Free(g_animController);
    BonesTextureSets_Free(g_textureSets);
    g_animController = NULL;
    g_textureSets = NULL;

    BonesRenderer_Free(app->renderer);
    free(app->renderBones);
    free(app->renderHeads);
    free(app->renderTorsos);
    CleanupTextureSystem(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);

    BonesFree(&app->animation);
    CloseWindow();
}

// ============================================================================
// MANEJO DE ENTRADA
// ============================================================================

static void App_HandleInput(AppState* app) {
    if (!app) return;

    static int framesSinceLastPress = 0;
    framesSinceLastPress++;

    if (IsKeyPressed(KEY_P)) {
        if (g_animController && g_animController->playing) {
            AnimController_Pause(g_animController);
            TraceLog(LOG_INFO, "ANIM: PAUSED");
        } else if (g_animController) {
            AnimController_Resume(g_animController);
            TraceLog(LOG_INFO, "ANIM: RESUMED");
        }
    }

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

    if (IsKeyPressed(KEY_F5)) {
        TraceLog(LOG_INFO, "Reloading configurations...");
        if (LoadSimpleTextureConfig(&app->textureSystem, "bone_textures.txt")) {
            LoadBoneConfigurations(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);
        }
        BonesTextureSets_LoadFromFile(g_textureSets, "data/textures/texture_sets.txt");
    }

    if (IsKeyPressed(KEY_THREE)) {
        app->camMode = 1;
        Vector3 target = app->autoCenterCalculated ? app->autoCenter : (Vector3){0, 0.6f, 0};
        Vector3 dir = Vector3Subtract(app->renderer->camera.position, target);
        float orbit_distance = Vector3Length(dir);
        app->orbitYaw = atan2f(dir.x, dir.z);
        app->orbitPitch = asinf(Clamp(dir.y / orbit_distance, -1.0f, 1.0f));
        app->orbitRadius = orbit_distance;
        app->renderer->camera.target = target;
        EnableCursor();
        TraceLog(LOG_INFO, "Camera mode: ORBIT");
    }
    
    if (IsKeyPressed(KEY_FOUR)) {
        app->camMode = 2;
        Vector3 target = app->autoCenterCalculated ? app->autoCenter : (Vector3){0, 0.6f, 0};
        Vector3 dir = Vector3Subtract(app->renderer->camera.position, target);
        float orbit_distance = Vector3Length(dir);
        float orbit_yaw = atan2f(dir.z, dir.x);
        float orbit_pitch = asinf(Clamp(dir.y / orbit_distance, -1.0f, 1.0f));
        app->renderer->camera.position.x = orbit_distance * cosf(orbit_pitch) * cosf(orbit_yaw) + target.x;
        app->renderer->camera.position.y = orbit_distance * sinf(orbit_pitch) + target.y;
        app->renderer->camera.position.z = orbit_distance * cosf(orbit_pitch) * sinf(orbit_yaw) + target.z;
        app->renderer->camera.target = target;
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

// ============================================================================
// ACTUALIZACIÓN DE CÁMARA
// ============================================================================

static void App_UpdateCamera(AppState* app, float dt) {
    if (!app || !app->renderer) return;

    Vector3 cameraTarget = app->autoCenterCalculated ? app->autoCenter : (Vector3){0, 0.6f, 0};

    if (app->camMode == 1) {
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

        app->renderer->camera.position = (Vector3){
            cameraTarget.x + app->orbitRadius * cosP * sinY,
            cameraTarget.y + app->orbitRadius * sinP,
            cameraTarget.z + app->orbitRadius * cosP * cosY
        };
        app->renderer->camera.target = cameraTarget;
    }
    else {
        Vector2 mouse_delta = GetMouseDelta();
        app->orbitYaw += mouse_delta.x * FPS_SENSITIVITY;
        app->orbitPitch -= mouse_delta.y * FPS_SENSITIVITY;
        app->orbitPitch = Clamp(app->orbitPitch, MIN_PITCH, MAX_PITCH);
        
        Vector3 forward;
        forward.x = cosf(app->orbitPitch) * cosf(app->orbitYaw);
        forward.y = sinf(app->orbitPitch);
        forward.z = cosf(app->orbitPitch) * sinf(app->orbitYaw);
        forward = Vector3Normalize(forward);
        
        Vector3 right_dir = Vector3Normalize(Vector3CrossProduct(forward, app->renderer->camera.up));
        float speed = BASE_SPEED * dt;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
            speed *= MOVEMENT_SPEED;
        
        if (IsKeyDown(KEY_W))
            app->renderer->camera.position = Vector3Add(app->renderer->camera.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_S))
            app->renderer->camera.position = Vector3Subtract(app->renderer->camera.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_D))
            app->renderer->camera.position = Vector3Add(app->renderer->camera.position, Vector3Scale(right_dir, speed));
        if (IsKeyDown(KEY_A))
            app->renderer->camera.position = Vector3Subtract(app->renderer->camera.position, Vector3Scale(right_dir, speed));
        if (IsKeyDown(KEY_Q))
            app->renderer->camera.position = Vector3Add(app->renderer->camera.position, Vector3Scale(app->renderer->camera.up, speed));
        if (IsKeyDown(KEY_LEFT_CONTROL))
            app->renderer->camera.position = Vector3Subtract(app->renderer->camera.position, Vector3Scale(app->renderer->camera.up, speed));
        
        app->renderer->camera.target = Vector3Add(app->renderer->camera.position, forward);
    }
}

// ============================================================================
// ACTUALIZACIÓN DEL CENTRO AUTOMÁTICO
// ============================================================================

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

// ============================================================================
// PREPARACIÓN DE DATOS DE RENDERIZADO
// ============================================================================

static void App_PrepareRenderData(AppState* app) {
    if (!app) return;

    if (app->currentFrame != app->lastProcessedFrame || app->forceUpdate) {
        CollectBonesForRendering(&app->animation, app->renderer->camera, &app->renderBones, &app->renderBonesCount,
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

// ============================================================================
// INTERFAZ DE USUARIO
// ============================================================================

static void App_DrawUI(AppState* app) {
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

    if (g_textureSets) {
        const char* activeVariant = BonesTextureSets_GetActiveVariantName(g_textureSets, "Head");
        if (activeVariant) {
            char variantText[128];
            snprintf(variantText, sizeof(variantText), "Head Texture: %s", activeVariant);
            DrawText(variantText, 10, 115, 16, DARKGREEN);
        }
    }
}

// ============================================================================
// RENDERIZADO PRINCIPAL
// ============================================================================

static void App_Draw(AppState* app) {
    if (!app) return;

    BeginDrawing();
    ClearBackground(RAYWHITE);

    App_DrawUI(app);

    // Renderizar el frame usando el sistema de renderizado unificado
    BonesRenderer_RenderFrame(app->renderer,
                             app->renderBones, app->renderBonesCount,
                             app->renderHeads, app->renderHeadsCount,
                             app->renderTorsos, app->renderTorsosCount,
                             app->autoCenter, app->autoCenterCalculated);

    EndDrawing();
}

// ============================================================================
// FUNCIÓN PRINCIPAL
// ============================================================================

int main(void) {
    AppState app;
    if (!App_Init(&app)) return -1;
    
    TraceLog(LOG_INFO, "===========================================");
    TraceLog(LOG_INFO, "Unified Animation System Initialized");
    TraceLog(LOG_INFO, "===========================================");
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        if (g_animController && app.autoPlay) {
            AnimController_Update(g_animController, dt);
            
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