#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "bonetile.h"
#include "bones3d.h"

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
    Vector3 lastCameraPos;
    bool useMorphing; // NUEVO CAMPO PARA TOGGLE
} AppState;

/* ---------- Prototypes ---------- */
static bool App_Init(AppState* app);
static void App_Shutdown(AppState* app);
static int App_GetTextureIndex(AppState* app, const char* path);
static void App_HandleInput(AppState* app, float dt);
static void App_UpdateCamera(AppState* app, float dt);
static void App_UpdateAutoCenter(AppState* app);
static void App_PrepareRenderBones(AppState* app);
static void App_Draw(AppState* app);

/* ---------- Implementations ---------- */

/* ************************************************************************** */
/* Application initialization                                                  */
/* - Parameters: AppState* app (output)                                        */
/* - Returns: true if initialization succeeded, false on failure.             */
/* - Actions:                                                                  */
/*   * Zeroes the state struct.                                                */
/*   * Initializes the window, camera and texture system.                      */
/*   * Loads the Bones animation from JSON and configures the renderer.        */
/*   * Sets default values (frames, autoplay, auto-center).                    */
/* - Note: if BonesInit fails, closes the window and returns false.           */
/* ************************************************************************** */
static bool App_Init(AppState* app) {
    if (!app) return false;
    memset(app, 0, sizeof(*app));
    CheckBoneNameLength();
    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - System with Morphing");
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
    app->orbitYaw = 0.0f; app->orbitPitch = -0.2f; app->orbitRadius = 2.5f;
    app->cameraMouseControl = false;
    app->boneConfigs = NULL;
    app->boneConfigCount = 0;
    app->renderBones = NULL;
    app->renderBonesCount = 0;
    app->renderBonesCapacity = 0;
    app->textureCount = 0;
    app->physCols = 8; app->physRows = 8;
    app->useMorphing = false; // INICIALIZAR EN FALSE (método clásico por defecto)

    // Load texture config (try, but continue if it fails)
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
    app->lastCameraPos = app->camera.position;
    return true;
}

/* ************************************************************************** */
/* Application shutdown / cleanup                                              */
/* - Parameters: AppState* app                                                 */
/* - Returns: void                                                             */
/* - Actions:                                                                  */
/*   * Frees render-related memory.                                            */
/*   * Cleans up texture system and unloads GPU textures.                      */
/*   * Frees the Bones animation and closes the window.                        */
/* - Note: safely handles null pointers.                                       */
/* ************************************************************************** */
static void App_Shutdown(AppState* app) {
    if (!app) return;
    if (app->renderBones) free(app->renderBones);
    CleanupTextureSystem(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);
    for (int i = 0; i < app->textureCount; i++) UnloadTexture(app->textures[i]);
    BonesFree(&app->animation);
    CloseWindow();
}

/* ************************************************************************** */
/* Find or load a texture                                                      */
/* - Parameters: AppState* app, const char* path                               */
/* - Returns: index (int) of the texture in app->textures; 0 on failure/limit. */
/* - Actions:                                                                  */
/*   * Searches if the path has already been loaded; returns its index.        */
/*   * If not present and there's room, attempts to load the image from disk.  */
/*   * If loading fails, generates a placeholder image with the path drawn.    */
/*   * Sets texture filter and stores the path in app->texturePaths.           */
/* - Note: does not unload old textures here; central management assumed.      */
/* ************************************************************************** */
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

/* ************************************************************************** */
/* Input handling (keyboard/mouse)                                             */
/* - Parameters: AppState* app, float dt                                       */
/* - Returns: void                                                             */
/* - Actions:                                                                  */
/*   * Frame control (left/right, HOME/END, space for autoplay).               */
/*   * Reload texture configuration (F5).                                      */
/*   * Switch camera modes and toggle mouse/keyboard control (1,2,C).          */
/*   * Toggle morphing mode (M).                                               */
/* - Side effects: modifies app->currentFrame, app->autoPlay, etc.             */
/* ************************************************************************** */
static void App_HandleInput(AppState* app, float dt) {
    if (!app) return;

    // Frame control / autoplay
    if (app->animation.isLoaded && app->maxFrames > 0) {
        bool manualFrameChange = false;

        if (IsKeyPressed(KEY_LEFT) && app->currentFrame > 0) {
            app->currentFrame--;
            BonesSetFrame(&app->animation, app->currentFrame);
            manualFrameChange = true;
        }
        if (IsKeyPressed(KEY_RIGHT) && app->currentFrame < app->maxFrames - 1) {
            app->currentFrame++;
            BonesSetFrame(&app->animation, app->currentFrame);
            manualFrameChange = true;
        }
        if (IsKeyPressed(KEY_HOME)) {
            app->currentFrame = 0;
            BonesSetFrame(&app->animation, app->currentFrame);
            manualFrameChange = true;
        }
        if (IsKeyPressed(KEY_END) && app->maxFrames > 0) {
            app->currentFrame = app->maxFrames - 1;
            BonesSetFrame(&app->animation, app->currentFrame);
            manualFrameChange = true;
        }

        if (manualFrameChange) {
            app->autoCenterCalculated = false;
        }

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

    // Camera mode controls
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

    // Mouse control toggle (cambié de M a C para evitar conflictos)
    if (IsKeyPressed(KEY_C)) {
        app->cameraMouseControl = !app->cameraMouseControl;
        if (app->cameraMouseControl) DisableCursor(); else EnableCursor();
    }

    // NUEVO: Toggle morphing con la tecla M
    if (IsKeyPressed(KEY_M)) {
        app->useMorphing = !app->useMorphing;
        app->forceUpdate = true; // Forzar recálculo de render data

        // Mostrar mensaje en consola para debug
        TraceLog(LOG_INFO, "Morphing %s", app->useMorphing ? "ENABLED" : "DISABLED");
    }
}

/* ************************************************************************** */
/* Updates the camera based on mode and input                                  */
/* - Parameters: AppState* app, float dt                                       */
/* - Returns: void                                                             */
/* - Actions:                                                                  */
/*   * Mode 1: orbital camera around autoCenter (rotation and zoom).          */
/*   * Mode 2: "fly" camera with keyboard and mouse control.                  */
/*   * Adjusts position, target and pitch/radius limits.                      */
/* - Note: uses GetMouseDelta() / GetMouseWheelMove() for interaction.        */
/* ************************************************************************** */
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
        if (wheel != 0.0f) { app->orbitRadius -= wheel * 0.5f; app->orbitRadius = Clamp(app->orbitRadius, 0.5f, 20.0f); }
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
        Vector3 forward = { sinf(app->orbitYaw) * cosf(app->orbitPitch), sinf(app->orbitPitch), cosf(app->orbitYaw) * cosf(app->orbitPitch) };
        forward = Vector3Normalize(forward);
        Vector3 right = { sinf(app->orbitYaw - PI / 2), 0, cosf(app->orbitYaw - PI / 2) }; right = Vector3Normalize(right);
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

/* ************************************************************************** */
/* Calculate automatic scene center (autoCenter)                               */
/* - Parameters: AppState* app                                                */
/* - Returns: void                                                             */
/* - Actions:                                                                  */
/*   * Iterates bones in current frame and averages reference positions       */
/*     (Spine, Chest, Neck, Hip) to center the view.                          */
/*   * Marks app->autoCenterCalculated when computed.                         */
/* - Note: does not recalculate if already computed or frame invalid.         */
/* ************************************************************************** */
static void App_UpdateAutoCenter(AppState* app) {
    if (!app) return;
    if (!app->animation.isLoaded) return;
    if (!BonesIsValidFrame(&app->animation, app->currentFrame)) return;

    const AnimationFrame* frame = &app->animation.frames[app->currentFrame];
    Vector3 totalPos = { 0,0,0 };
    int validBoneCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
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
    }

    if (validBoneCount > 0) {
        Vector3 newCenter = Vector3Scale(totalPos, 1.0f / validBoneCount);

        if (app->camMode == 1 && app->autoPlay && app->autoCenterCalculated) {

            float lerpFactor = 0.15f;
            app->autoCenter = Vector3Lerp(app->autoCenter, newCenter, lerpFactor);
        }
        else {
            app->autoCenter = newCenter;
        }

        app->autoCenterCalculated = true;
    }
}

/* ************************************************************************** */
/* Prepare list of bones to render                                              */
/* - Parameters: AppState* app                                                */
/* - Returns: void                                                             */
/* - Actions:                                                                  */
/*   * If frame changed or forced update, calls function that collects        */
/*     bone render data and updates counts/capacities.                        */
/*   * If camera moved enough, marks forceUpdate to reprocess.                */
/* - Note: optimizes by avoiding unnecessary recompute per frame/camera move. */
/* ************************************************************************** */
static void App_PrepareRenderBones(AppState* app) {
    if (!app) return;
    if (app->currentFrame != app->lastProcessedFrame || app->forceUpdate) {
        CollectBonesForRendering(&app->animation, app->camera, &app->renderBones, &app->renderBonesCount, &app->renderBonesCapacity, app->boneConfigs, app->boneConfigCount);
        app->lastProcessedFrame = app->currentFrame;
        app->forceUpdate = false;
    }
    float cameraMoved = Vector3Distance(app->camera.position, app->lastCameraPos);
    if (cameraMoved > 0.5f) {
        app->forceUpdate = true;
        app->lastCameraPos = app->camera.position;
    }
}

/* ************************************************************************** */
/* Main drawing                                                                */
/* - Parameters: AppState* app                                                */
/* - Returns: void                                                             */
/* - Actions:                                                                  */
/*   * Clears screen, sets 3D mode and draws grid and center sphere.          */
/*   * Shows UI with current mode and controls.                               */
/*   * Iterates app->renderBones and draws each bone using selected method.   */
/*   * Optionally draws debug spheres around each bone.                       */
/* - Note: manages blend mode and depth test for correct rendering.          */
/* ************************************************************************** */
static void App_Draw(AppState* app) {
    if (!app) return;
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Mostrar el estado actual en pantalla
    const char* modeText = app->useMorphing ? "MORPHING MODE" : "CLASSIC MODE";
    const char* controlsText = "M: Toggle Morphing | C: Mouse Control | 1/2: Camera Mode | Space: Play/Pause";
    DrawText(modeText, 10, 10, 20, app->useMorphing ? GREEN : BLUE);
    DrawText(controlsText, 10, 35, 16, DARKGRAY);

    char frameText[64];
    snprintf(frameText, sizeof(frameText), "Frame: %d/%d %s", app->currentFrame + 1, app->maxFrames, app->autoPlay ? "(Playing)" : "(Paused)");
    DrawText(frameText, 10, 55, 16, DARKGRAY);

    char statsText[128];
    snprintf(statsText, sizeof(statsText), "Bones: %d | Camera Mode: %d | Mouse Control: %s",
        app->renderBonesCount, app->camMode, app->cameraMouseControl ? "ON" : "OFF");
    DrawText(statsText, 10, 75, 16, DARKGRAY);

    BeginMode3D(app->camera);
    DrawGrid(24, 0.5f);
    if (app->autoCenterCalculated) DrawSphereWires(app->autoCenter, 0.05f, 8, 8, ORANGE);

    if (app->renderBonesCount > 0) {
        rlDisableDepthTest();
        BeginBlendMode(BLEND_ALPHA);

        for (int i = 0; i < app->renderBonesCount; i++) {
            const BoneRenderData* bone = &app->renderBones[i];
            if (!bone->valid || !bone->visible) continue;

            int texIndex = App_GetTextureIndex(app, bone->texturePath);
            Texture2D currentTex = app->textures[texIndex];

            float physCellW = (float)currentTex.width / (float)app->physCols;
            float physCellH = (float)currentTex.height / (float)app->physRows;
            float logicalCellW = physCellW * (app->physCols / ATLAS_COLS);
            float logicalCellH = physCellH * (app->physRows / ATLAS_ROWS);
            float aspect = logicalCellW / logicalCellH;
            Vector2 worldSize = (Vector2){ bone->size * aspect, bone->size };
            if (app->useMorphing) {
                // MÉTODO NUEVO: Usar morphing
                DrawBonetileWithMorphing(currentTex, app->camera, bone->morphData, bone->position, worldSize, app->physCols, app->physRows);
            }
            else {
                // MÉTODO CLÁSICO: Sin morphing - recalcular datos en tiempo real
                int outChosenIndex;
                float outRotation;
                bool outMirrored;

                CalculateBoneRenderData(bone->position, app->camera, &outChosenIndex, &outRotation, &outMirrored);

                int logicalCol = outChosenIndex % ATLAS_COLS;
                int logicalRow = outChosenIndex / ATLAS_COLS;
                bool finalMirror = false;
                Rectangle src = SrcFromLogical(currentTex, logicalCol, logicalRow, app->physCols, app->physRows, outMirrored, &finalMirror);
                DrawBonetileCustom(currentTex, app->camera, src, bone->position, worldSize, outRotation, finalMirror);
            }

            if (app->renderConfig.drawDebugSpheres) {
                Color debugCol = (texIndex == 0) ? RED : (texIndex == 1) ? BLUE : (texIndex == 2) ? PURPLE : GREEN;
                DrawSphereWires(bone->position, app->renderConfig.debugSphereRadius, 8, 8, debugCol);
            }
        }

        EndBlendMode();
        rlEnableDepthMask();
    }

    EndMode3D();
    EndDrawing();
}

/* ************************************************************************** */
/* Entry point (main)                                                          */
/* - Returns: 0 on success, -1 on initialization failure                        */
/* - Actions:                                                                  */
/*   * Initializes AppState, runs main loop (input, update, render).          */
/*   * On exit, frees resources via App_Shutdown.                              */
/* ************************************************************************** */
int main(void) {
    AppState app;
    if (!App_Init(&app)) return -1;
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        App_HandleInput(&app, dt);
        App_UpdateCamera(&app, dt);
        App_UpdateAutoCenter(&app);
        App_PrepareRenderBones(&app);
        App_Draw(&app);
    }
    App_Shutdown(&app);
    return 0;
}