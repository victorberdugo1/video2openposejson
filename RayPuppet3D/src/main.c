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
    bool useMorphing;
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
    app->useMorphing = false;

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

    if (IsKeyPressed(KEY_M)) {
        app->useMorphing = !app->useMorphing;
        app->forceUpdate = true;
        TraceLog(LOG_INFO, "Morphing %s", app->useMorphing ? "ENABLED" : "DISABLED");
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

static void App_Draw(AppState* app) {
    if (!app) return;

    BeginDrawing();
    ClearBackground(RAYWHITE);

    const char* modeText = app->useMorphing ? "MORPHING MODE" : "CLASSIC MODE";
    const char* controlsText = "M: Toggle Morphing | H: Toggle Heads | T: Toggle Torsos | C: Mouse Control | 1/2: Camera Mode | Space: Play/Pause";
    DrawText(modeText, 10, 10, 20, app->useMorphing ? GREEN : BLUE);
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
        rlDisableDepthTest();
        BeginBlendMode(BLEND_ALPHA);

        // Render bones
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
                DrawBonetileWithMorphing(currentTex, app->camera, bone->morphData, bone->position, worldSize, app->physCols, app->physRows);
            }
            else {
                int chosenIndex;
                float rotation;
                bool mirrored;

                CalculateBoneRenderData(bone->position, app->camera, &chosenIndex, &rotation, &mirrored);

                int logicalCol = chosenIndex % ATLAS_COLS;
                int logicalRow = chosenIndex / ATLAS_COLS;
                bool finalMirror = false;
                Rectangle src = SrcFromLogical(currentTex, logicalCol, logicalRow, app->physCols, app->physRows, mirrored, &finalMirror);
                DrawBonetileCustom(currentTex, app->camera, src, bone->position, worldSize, rotation, finalMirror);
            }

            if (app->renderConfig.drawDebugSpheres) {
                Color debugCol = (texIndex == 0) ? RED : (texIndex == 1) ? BLUE : (texIndex == 2) ? PURPLE : GREEN;
                DrawSphereWires(bone->position, app->renderConfig.debugSphereRadius, 8, 8, debugCol);
            }
        }

        // Render heads
        if (app->renderHeadBillboards && app->renderHeadsCount > 0) {
            for (int i = 0; i < app->renderHeadsCount; i++) {
                const HeadRenderData* head = &app->renderHeads[i];
                if (!head->valid || !head->visible) continue;

                int texIndex = App_GetTextureIndex(app, head->texturePath);
                Texture2D currentTex = app->textures[texIndex];

                DrawHeadBillboard(currentTex, app->camera, head, app->physCols, app->physRows);

                if (app->renderConfig.drawDebugSpheres) {
                    DrawSphereWires(head->position, 0.05f, 8, 8, ORANGE);

                    if (head->orientation.valid) {
                        Vector3 forwardEnd = Vector3Add(head->position, Vector3Scale(head->orientation.forward, 0.1f));
                        Vector3 upEnd = Vector3Add(head->position, Vector3Scale(head->orientation.up, 0.1f));
                        Vector3 rightEnd = Vector3Add(head->position, Vector3Scale(head->orientation.right, 0.1f));

                        DrawLine3D(head->position, forwardEnd, BLUE);
                        DrawLine3D(head->position, upEnd, GREEN);
                        DrawLine3D(head->position, rightEnd, RED);
                    }
                }
            }
        }

        // Render torsos
        if (app->renderTorsoBillboards && app->renderTorsosCount > 0) {
            for (int i = 0; i < app->renderTorsosCount; i++) {
                const TorsoRenderData* torso = &app->renderTorsos[i];
                if (!torso->valid || !torso->visible) continue;

                int texIndex = App_GetTextureIndex(app, torso->texturePath);
                Texture2D currentTex = app->textures[texIndex];

                DrawTorsoBillboard(currentTex, app->camera, torso, app->physCols, app->physRows);

                if (app->renderConfig.drawDebugSpheres) {
                    Color torsoColor = (torso->type == TORSO_CHEST) ? YELLOW : PURPLE;
                    DrawSphereWires(torso->position, 0.06f, 8, 8, torsoColor);

                    if (torso->orientation.valid) {
                        Vector3 forwardEnd = Vector3Add(torso->position, Vector3Scale(torso->orientation.forward, 0.12f));
                        Vector3 upEnd = Vector3Add(torso->position, Vector3Scale(torso->orientation.up, 0.12f));
                        Vector3 rightEnd = Vector3Add(torso->position, Vector3Scale(torso->orientation.right, 0.12f));

                        DrawLine3D(torso->position, forwardEnd, DARKBLUE);
                        DrawLine3D(torso->position, upEnd, DARKGREEN);
                        DrawLine3D(torso->position, rightEnd, MAROON);
                    }
                }
            }
        }

        EndBlendMode();
        rlEnableDepthMask();
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