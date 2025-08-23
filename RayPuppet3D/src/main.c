#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "bonetile.h"
#include "bones3d.h"
#include <string.h>

#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080
#define MAX_TEXTURES 10

int main(void) {
    CheckBoneNameLength();

    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - System");
    SetWindowState(FLAG_WINDOW_RESIZABLE);

#if defined(__linux__)
    for (int i = 0; i < 5; i++) {
        PollInputEvents();
    }
#endif

    MaximizeWindow();
    SetTargetFPS(60);

    Camera camera = { 0 };
    camera.position = (Vector3){ 0.0f, 0.6f, 2.5f };
    camera.target = (Vector3){ 0.0f, 0.6f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int camMode = 1;
    float orbitYaw = 0.0f, orbitPitch = -0.2f, orbitRadius = 2.5f;
    bool cameraMouseControl = false;

    SimpleTextureSystem textureSystem = { 0 };
    BoneConfig* boneConfigs = NULL;
    int boneConfigCount = 0;
    BoneRenderData* renderBones = NULL;
    int renderBonesCount = 0;
    int renderBonesCapacity = 0;

    if (!LoadSimpleTextureConfig(&textureSystem, "bone_textures.txt")) {
        LoadSimpleTextureConfig(&textureSystem, "bone_textures.txt");
    }

    LoadBoneConfigurations(&textureSystem, &boneConfigs, &boneConfigCount);

    BonesAnimation animation;
    BonesError result = BonesInit(&animation, 1000);

    if (result != BONES_SUCCESS) {
        CloseWindow();
        return -1;
    }

    result = BonesLoadFromJSON(&animation, "test.json");

    BonesRenderConfig config = BonesGetDefaultRenderConfig();
    config.drawDebugSpheres = true;
    config.debugColor = GREEN;
    config.debugSphereRadius = 0.035f;
    config.enableDepthSorting = true;
    BonesSetRenderConfig(&config);

    Texture2D textures[MAX_TEXTURES];
    char texturePaths[MAX_TEXTURES][MAX_FILE_PATH_LENGTH];
    int textureCount = 0;

    int GetTextureIndex(const char* path) {
        for (int i = 0; i < textureCount; i++) {
            if (strcmp(texturePaths[i], path) == 0) {
                return i;
            }
        }

        if (textureCount >= MAX_TEXTURES) {
            return 0;
        }

        Image img = LoadImage(path);
        if (img.data == NULL) {
            img = GenImageColor(1024, 1024, CLITERAL(Color){60, 120, 220, 255});
            ImageDrawText(&img, path, 8, 8, 128, WHITE);
        }

        textures[textureCount] = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(textures[textureCount], TEXTURE_FILTER_POINT);

        strncpy(texturePaths[textureCount], path, MAX_FILE_PATH_LENGTH - 1);
        texturePaths[textureCount][MAX_FILE_PATH_LENGTH - 1] = '\0';

        return textureCount++;
    }

    const int physCols = 8, physRows = 8;

    int maxFrames = BonesGetFrameCount(&animation);
    int currentFrame = 0;
    bool autoPlay = false;
    float autoPlayTimer = 0.0f;
    float autoPlaySpeed = 0.1f;

    Vector3 autoCenter = { 0 };
    bool autoCenterCalculated = false;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

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

        if (IsKeyPressed(KEY_F5)) {
            if (LoadSimpleTextureConfig(&textureSystem, "bone_textures.txt")) {
                LoadBoneConfigurations(&textureSystem, &boneConfigs, &boneConfigCount);
            }
        }

        if (animation.isLoaded && !autoCenterCalculated) {
            if (BonesIsValidFrame(&animation, currentFrame)) {
                const AnimationFrame* frame = &animation.frames[currentFrame];
                Vector3 totalPos = { 0 };
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
                    autoCenter = Vector3Scale(totalPos, 1.0f / validBoneCount);
                    autoCenterCalculated = true;
                }
            }
        }

        if (IsKeyPressed(KEY_ONE)) {
            camMode = 1;
            cameraMouseControl = false;
            EnableCursor();
        }
        if (IsKeyPressed(KEY_TWO)) {
            camMode = 2;
            cameraMouseControl = true;
            DisableCursor();
            Vector3 target = autoCenterCalculated ? autoCenter : (Vector3) { 0, 0.6f, 0 };
            camera.position = (Vector3){ target.x, target.y + 0.5f, target.z + 2.0f };
            camera.target = target;
            Vector3 direction = Vector3Subtract(target, camera.position);
            orbitYaw = atan2f(direction.x, direction.z);
            orbitPitch = atan2f(direction.y, sqrtf(direction.x * direction.x + direction.z * direction.z));
        }

        if (IsKeyPressed(KEY_M)) {
            cameraMouseControl = !cameraMouseControl;
            if (cameraMouseControl) {
                DisableCursor();
            }
            else {
                EnableCursor();
            }
        }

        Vector3 cameraTarget = autoCenterCalculated ? autoCenter : (Vector3) { 0, 0.6f, 0 };

        if (camMode == 1) {
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
        }
        else {
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
                sinf(orbitYaw - PI / 2),
                0,
                cosf(orbitYaw - PI / 2)
            };
            right = Vector3Normalize(right);

            float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 8.0f : 3.0f;
            if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(forward, speed * dt));
            if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(forward, speed * dt));
            if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(right, speed * dt));
            if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(right, speed * dt));

            if (IsKeyDown(KEY_SPACE)) camera.position.y += speed * dt;
            if (IsKeyDown(KEY_LEFT_CONTROL)) camera.position.y -= speed * dt;

            camera.target = Vector3Add(camera.position, forward);
        }

        static int lastProcessedFrame = -1;
        static bool forceUpdate = false;

        if (currentFrame != lastProcessedFrame || forceUpdate) {
            CollectBonesForRendering(&animation, camera, &renderBones, &renderBonesCount, &renderBonesCapacity, boneConfigs, boneConfigCount);
            lastProcessedFrame = currentFrame;
            forceUpdate = false;
        }

        static Vector3 lastCameraPos = { 0 };
        float cameraMoved = Vector3Distance(camera.position, lastCameraPos);
        if (cameraMoved > 0.5f) {
            forceUpdate = true;
            lastCameraPos = camera.position;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
        DrawGrid(24, 0.5f);

        if (autoCenterCalculated) {
            DrawSphereWires(autoCenter, 0.05f, 8, 8, ORANGE);
        }

        if (renderBonesCount > 0) {
            rlDisableDepthTest();
            BeginBlendMode(BLEND_ALPHA);

            for (int i = 0; i < renderBonesCount; i++) {
                const BoneRenderData* bone = &renderBones[i];
                if (!bone->valid || !bone->visible) continue;

                int texIndex = GetTextureIndex(bone->texturePath);
                Texture2D currentTex = textures[texIndex];

                float physCellW = (float)currentTex.width / (float)physCols;
                float physCellH = (float)currentTex.height / (float)physRows;
                float logicalCellW = physCellW * (physCols / ATLAS_COLS);
                float logicalCellH = physCellH * (physRows / ATLAS_ROWS);
                float aspect = logicalCellW / logicalCellH;
                Vector2 worldSize = (Vector2){ bone->size * aspect, bone->size };

                int logicalCol = bone->atlasIndex % ATLAS_COLS;
                int logicalRow = bone->atlasIndex / ATLAS_COLS;
                bool finalMirror = false;
                Rectangle src = SrcFromLogical(currentTex, logicalCol, logicalRow,
                    physCols, physRows, bone->mirrored, &finalMirror);

                DrawBonetileCustom(currentTex, camera, src, bone->position,
                    worldSize, bone->rotation, finalMirror);

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

        EndDrawing();
    }

    if (renderBones) {
        free(renderBones);
    }

    CleanupTextureSystem(&textureSystem, &boneConfigs, &boneConfigCount);

    for (int i = 0; i < textureCount; i++) {
        UnloadTexture(textures[i]);
    }

    BonesFree(&animation);
    CloseWindow();

    return 0;
}