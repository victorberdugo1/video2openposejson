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
//static const float MAX_ORBIT_RADIUS = 20.0f;
static const float MIN_PITCH = -PI / 2.0f + 0.01f;
static const float MAX_PITCH = PI / 2.0f - 0.01f;
//static const float FPS_MIN_PITCH = -1.49f;
//static const float FPS_MAX_PITCH = 1.49f;
static const float BASE_SPEED = 5.0f;
static const float MOVEMENT_SPEED = 3.0f;
//static const float FAST_SPEED = 8.0f;
static const float VALID_POSITION_THRESHOLD = 0.01f;
static const float MIN_DISTANCE_THRESHOLD = 0.001f;

// Depth bias constants
static const float TORSO_BIAS = 0.001f;
static const float BONE_BIAS = 0.0f;
static const float HEAD_BIAS = -0.001f;
static const float INDEX_BIAS = -0.00001f;
static const float Z_FIGHTING_THRESHOLD = 0.01f;

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
 * | Function: ft_GetMouseDelta                                    |
 * |                                                               |
 * | Calculates mouse movement delta manually. This is a robust    |
 * | alternative to raylib's GetMouseDelta() on systems where it   |
 * | behaves incorrectly (like some Linux window managers).        |
 * |                                                               |
 * | It works by:                                                  |
 * |   1. Reading the current mouse position.                      |
 * |   2. Calculating the difference from the screen center.       |
 * |   3. Resetting the mouse position back to the center.         |
 * |   4. Returning the calculated difference as the delta.        |
 * |                                                               |
 * | NOTE: This requires DisableCursor() to be active to hide the  |
 * |       cursor, otherwise it will be visibly jumping.           |
 * +---------------------------------------------------------------+
 */
/*static Vector2 ft_GetMouseDelta(void)
{
    Vector2 mousePosition = GetMousePosition();
    Vector2 screenCenter = { (float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f };
    Vector2 delta = Vector2Subtract(mousePosition, screenCenter);
    SetMousePosition((int)screenCenter.x, (int)screenCenter.y);

    return (delta);
}*/

/*
 * +--------------------------------------------------------------+
 * | Function: FindPersonByBoneName                               |
 * |                                                              |
 * | Search an AnimationFrame for the Person that contains a bone |
 * | with the given name and return a pointer to that Person.     |
 * |                                                              |
 * | - Input:  AnimationFrame *frame, const char *boneName.       |
 * | - Output: Person* (or NULL if not found).                    |
 * +--------------------------------------------------------------+
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
 * +--------------------------------------------------------------+
 * | Function: DetectZFighting                                    |
 * |                                                              |
 * | Check pairs of render items to find very close distances to  |
 * | the camera and mark them as potential z-fighting conflicts.  |
 * |                                                              |
 * | - Input:  RenderItem *items, int itemCount.                  |
 * | - Output: bool (true if any conflict detected).              |
 * +--------------------------------------------------------------+
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
 * |                                                               |
 * | Sort render items by effective distance (distance + depthBias)|
 * | so farther objects are drawn first (painter's algorithm).     |
 * |                                                               |
 * | - Input: RenderItem *items, int itemCount.                    |
 * | - Effect: reorders array in-place (simple bubble sort used).  |
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
 * +--------------------------------------------------------------+
 * | Function: App_Init                                           |
 * |                                                              |
 * | Initialize the whole application: create window and camera,  |
 * | load texture configs, initialize animation system and load   |
 * | JSON data, set default render settings.                      |
 * |                                                              |
 * | - Input:  AppState *app                                      |
 * | - Output: bool (true on success, false on failure)           |
 * +--------------------------------------------------------------+
 */
static bool App_Init(AppState* app) {
    if (!app) return false;
    memset(app, 0, sizeof(*app));

    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - System with Morphing, Head & Torso Billboards");
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

    // Load configurations
    LoadSimpleTextureConfig(&app->textureSystem, "bone_textures.txt");
    LoadBoneConfigurations(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);

    // Initialize animation system
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

/*
 * +--------------------------------------------------------------+
 * | Function: App_Shutdown                                       |
 * |                                                              |
 * | Free all resources allocated by the application: free memory,|
 * | unload textures, cleanup animation system and close window.  |
 * |                                                              |
 * | - Input: AppState *app                                       |
 * | - Effect: app becomes uninitialized and window closed.       |
 * +--------------------------------------------------------------+
 */
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

/*
 * +--------------------------------------------------------------+
 * | Function: App_GetTextureIndex                                |
 * |                                                              |
 * | Return the index for a texture path; if not loaded, load it, |
 * | store it in the texture array, and return its index. Uses a  |
 * | fallback image when the file cannot be loaded.               |
 * |                                                              |
 * | - Input: AppState *app, const char *path                     |
 * | - Output: int texture index                                  |
 * +--------------------------------------------------------------+
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
 * +--------------------------------------------------------------+
 * | Function: App_HandleInput                                    |
 * |                                                              |
 * | Read keyboard and mouse input and update application state:  |
 * | frame navigation (left/right/home/end), play/pause, reload   |
 * | textures, toggle head/torso billboards, switch camera modes. |
 * |                                                              |
 * | - Input: AppState *app, float dt                             |
 * +--------------------------------------------------------------+
 */
static void App_HandleInput(AppState* app, float dt) {
    if (!app) return;

	static int framesSinceLastPress = 0;

    // Frame navigation
    if (app->animation.isLoaded && app->maxFrames > 0) {
        bool frameChanged = false;
        int newFrame = app->currentFrame;

		framesSinceLastPress++;

        if (IsKeyDown(KEY_LEFT) && newFrame > 0 && framesSinceLastPress > 15) { newFrame--; frameChanged = true; framesSinceLastPress = 0; }
        if (IsKeyDown(KEY_RIGHT) && newFrame < app->maxFrames - 1 && framesSinceLastPress > 15) { newFrame++; frameChanged = true; framesSinceLastPress = 0; }
        if (IsKeyPressed(KEY_HOME)) { newFrame = 0; frameChanged = true; }
        if (IsKeyPressed(KEY_END) && app->maxFrames > 0) { newFrame = app->maxFrames - 1; frameChanged = true; }

        if (frameChanged) {
            app->currentFrame = newFrame;
            BonesSetFrame(&app->animation, app->currentFrame);
            app->autoCenterCalculated = false;
        }

        if (IsKeyPressed(KEY_SPACE)) app->autoPlay = !app->autoPlay;

        // Auto play
        if (app->autoPlay && app->maxFrames > 1) {
            app->autoPlayTimer += dt;
            if (app->autoPlayTimer >= app->autoPlaySpeed) {
                app->autoPlayTimer = 0.0f;
                app->currentFrame = (app->currentFrame + 1) % app->maxFrames;
                BonesSetFrame(&app->animation, app->currentFrame);
            }
        }
    }

    // Other controls
    if (IsKeyPressed(KEY_F5)) {
        if (LoadSimpleTextureConfig(&app->textureSystem, "bone_textures.txt")) {
            LoadBoneConfigurations(&app->textureSystem, &app->boneConfigs, &app->boneConfigCount);
        }
    }

    // Camera mode switching
    if (IsKeyPressed(KEY_ONE)) {
        app->camMode = 1;
		Vector3 target = app->autoCenterCalculated ? app->autoCenter : (Vector3) { 0, 0.6f, 0 };
		Vector3 dir = Vector3Subtract(app->camera.position, target);
		float orbit_distance = Vector3Length(dir);
		app->orbitYaw = atan2f(dir.z, dir.x);
		app->orbitPitch = asinf(Clamp(dir.y / orbit_distance, -1.0f, 1.0f));
		app->orbitRadius = orbit_distance; 
		app->camera.target = target; 
		EnableCursor();
	}
    if (IsKeyPressed(KEY_TWO)) {
        app->camMode = 2;
		Vector3 target = app->autoCenterCalculated ? app->autoCenter : (Vector3) { 0, 0.6f, 0 };
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
 * +--------------------------------------------------------------+
 * | Function: App_UpdateCamera                                   |
 * |                                                              |
 * | Update camera position and orientation depending on camera   |
 * | mode (orbit or FPS). Orbit uses mouse drag + wheel for zoom; |
 * | FPS uses mouse look + WASD movement.                         |
 * |                                                              |
 * | - Input: AppState *app, float dt                             |
 * | - Effect: updates app->camera.position and .target           |
 * +--------------------------------------------------------------+
 */
static void App_UpdateCamera(AppState* app, float dt) {
    if (!app) return;

    Vector3 cameraTarget = app->autoCenterCalculated ? app->autoCenter : (Vector3) { 0, 0.6f, 0 };

    if (app->camMode == 1) {
        // Orbit camera
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			HideCursor();
            Vector2 mouseDelta = GetMouseDelta();
            app->orbitYaw += mouseDelta.x * ORBIT_SENSITIVITY;
            app->orbitPitch = Clamp(app->orbitPitch - mouseDelta.y * ORBIT_SENSITIVITY, MIN_PITCH, MAX_PITCH);
        }
		else
		{
			ShowCursor();
		}
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
			app->orbitRadius -= wheel * ZOOM_SENSITIVITY;
			if (app->orbitRadius < MIN_ORBIT_RADIUS)
				app->orbitRadius = MIN_ORBIT_RADIUS;
        }

        float cosP = cosf(app->orbitPitch);
        float sinP = sinf(app->orbitPitch);
        float cosY = cosf(app->orbitYaw);
        float sinY = sinf(app->orbitYaw);

        app->camera.position = (Vector3){
            cameraTarget.x + app->orbitRadius * cosP * cosY,
            cameraTarget.y + app->orbitRadius * sinP,
            cameraTarget.z + app->orbitRadius * cosP * sinY
        };
        app->camera.target = cameraTarget;
    }
    else {
        // FPS camera
		Vector2 mouse_delta = GetMouseDelta();
		app->orbitYaw += mouse_delta.x * FPS_SENSITIVITY;
		app->orbitPitch -= mouse_delta.y * FPS_SENSITIVITY;
		app->orbitPitch = Clamp(app->orbitPitch, MIN_PITCH, MAX_PITCH);
		Vector3   forward;
		forward.x = cosf(app->orbitPitch) * cosf(app->orbitYaw);
		forward.y = sinf(app->orbitPitch);
		forward.z = cosf(app->orbitPitch) * sinf(app->orbitYaw);
		forward = Vector3Normalize(forward);
		Vector3 right_dir = Vector3Normalize(Vector3CrossProduct(forward, app->camera.up));
		float speed = BASE_SPEED * dt;
		if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
			speed *= MOVEMENT_SPEED;
		if (IsKeyDown(KEY_W))
			app->camera.position = Vector3Add(app->camera.position,
				Vector3Scale(forward, speed));
		if (IsKeyDown(KEY_S))
			app->camera.position = Vector3Subtract(app->camera.position,
				Vector3Scale(forward, speed));
		if (IsKeyDown(KEY_D))
			app->camera.position = Vector3Add(app->camera.position,
				Vector3Scale(right_dir, speed));
		if (IsKeyDown(KEY_A))
			app->camera.position = Vector3Subtract(app->camera.position,
				Vector3Scale(right_dir, speed));
		if (IsKeyDown(KEY_SPACE))
			app->camera.position = Vector3Add(app->camera.position,
				Vector3Scale(app->camera.up, speed));
		if (IsKeyDown(KEY_LEFT_CONTROL))
			app->camera.position = Vector3Subtract(app->camera.position,
				Vector3Scale(app->camera.up, speed));
		app->camera.target = Vector3Add(app->camera.position, forward);
	}
}

/*
 * +--------------------------------------------------------------+
 * | Function: App_UpdateAutoCenter                               |
 * |                                                              |
 * | Compute an automatic center point (autoCenter) by averaging  |
 * | relevant bone positions (head, chest, hip, spine) across all |
 * | active persons in the current frame. Smoothly lerp if needed.|
 * |                                                              |
 * | - Input: AppState *app                                       |
 * | - Effect: updates app->autoCenter and flag calculated.       |
 * +--------------------------------------------------------------+
 */
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
        }
        else {
            app->autoCenter = newCenter;
        }

        app->autoCenterCalculated = true;
    }
}

/*
 * +--------------------------------------------------------------+
 * | Function: App_PrepareRenderData                              |
 * |                                                              |
 * | Collect and update data structures used for drawing: bones,  |
 * | heads and torsos. Only refresh bones when frame changed or   |
 * | forced; collect heads/torsos only if their billboards active.|
 * |                                                              |
 * | - Input: AppState *app                                       |
 * +--------------------------------------------------------------+
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
    }
    else {
        app->renderHeadsCount = 0;
    }

    if (app->renderTorsoBillboards) {
        CollectTorsosForRendering(&app->animation, &app->renderTorsos, &app->renderTorsosCount,
            &app->renderTorsosCapacity, app->boneConfigs, app->boneConfigCount);
    }
    else {
        app->renderTorsosCount = 0;
    }
}

/*
 * +---------------------------------------------------------------+
 * | Function: RenderBone                                          |
 * |                                                               |
 * | Render a single bone "bonetile": choose texture cell, compute |
 * | world size and aspect, determine rotation and mirroring, find |
 * | neighbor for roll calculation, then call the final draw call. |
 * |                                                               |
 * | - Input: AppState*, BoneRenderData*, Vector3 renderPosition,  |
 * |          const AnimationFrame* (optional)                     |
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
    Vector2 worldSize = { bone->size * aspect, bone->size };

    int chosenIndex = 0;
    float rotation = 0.0f;
    bool mirrored = false;

    const Person* bonePerson = frame ? FindPersonByBoneName(frame, bone->boneName) : NULL;

    if (isWrist) {
        CalculateHandBoneRenderData(bone->position, app->camera, &chosenIndex, &rotation, &mirrored, bone->boneName);
    }
    else if (bone->orientation.valid) {

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
    Vector3 neighborPos = { 0 };
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
 * | Function: DrawOpenPoseSkeleton                                |
 * |                                                               |
 * | Draw a simple OpenPose-style debug skeleton for the first     |
 * | active person: draw lines for predefined connections and tiny |
 * | spheres at bone positions to visualize pose correctness.      |
 * |                                                               |
 * | - Input: AppState *app                                        |
 * +---------------------------------------------------------------+
 */
static void DrawOpenPoseSkeleton(AppState* app) {
    if (!app || !app->animation.isLoaded || !BonesIsValidFrame(&app->animation, app->currentFrame)) return;

    const AnimationFrame* frame = &app->animation.frames[app->currentFrame];

    const char* connections[][2] = {
        {"Neck", "Nose"}, {"Neck", "LShoulder"}, {"Neck", "RShoulder"}, {"LShoulder", "RShoulder"},
        {"LShoulder", "LElbow"}, {"LElbow", "LWrist"}, {"RShoulder", "RElbow"}, {"RElbow", "RWrist"},
        {"Neck", "MidHip"}, {"MidHip", "LHip"}, {"MidHip", "RHip"}, {"LHip", "LKnee"}, {"LKnee", "LAnkle"},
        {"RHip", "RKnee"}, {"RKnee", "RAnkle"}, {"LAnkle", "LBigToe"}, {"LAnkle", "LSmallToe"},
        {"LBigToe", "LSmallToe"}, {"RAnkle", "RBigToe"}, {"RAnkle", "RSmallToe"}, {"RBigToe", "RSmallToe"},
        {"Nose", "LEye"}, {"Nose", "REye"}, {"LEye", "LEar"}, {"REye", "REar"}
    };

    int numConnections = sizeof(connections) / sizeof(connections[0]);

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;

        for (int c = 0; c < numConnections; c++) {
            Vector3 pos1 = { 0 }, pos2 = { 0 };
            bool found1 = false, found2 = false;

            for (int b = 0; b < person->boneCount; b++) {
                const Bone* bone = &person->bones[b];
                if (!bone->position.valid) continue;

                if (strcmp(bone->name, connections[c][0]) == 0) {
                    pos1 = bone->position.position;
                    found1 = true;
                }
                else if (strcmp(bone->name, connections[c][1]) == 0) {
                    pos2 = bone->position.position;
                    found2 = true;
                }

                if (found1 && found2) break;
            }

            if (found1 && found2) DrawLine3D(pos1, pos2, RED);
        }

        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (bone->position.valid) DrawSphere(bone->position.position, 0.002f, BLUE);
        }

        break;
    }
}

/*
 * +---------------------------------------------------------------+
 * | Function: App_Draw                                            |
 * |                                                               |
 * | Main per-frame draw routine: draws 2D UI, enters 3D mode,     |
 * | builds render items (torsos/bones/heads), detects z-fighting, |
 * | disables depth-test and renders everything with blending in   |
 * | correct order. Handles neck/vs-head special case.             |
 * |                                                               |
 * | - Input: AppState *app                                        |
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

    // Draw UI
    DrawText("CLASSIC MODE", 10, 10, 20, BLUE);
    DrawText("M: Toggle Morphing | H: Toggle Heads | T: Toggle Torsos | C: Mouse Control | 1/2: Camera Mode | Space: Play/Pause", 10, 35, 16, DARKGRAY);

    char frameText[64];
    snprintf(frameText, sizeof(frameText), "Frame: %d/%d %s", app->currentFrame + 1, app->maxFrames, app->autoPlay ? "(Playing)" : "(Paused)");
    DrawText(frameText, 10, 55, 16, DARKGRAY);

    char statsText[256];
    snprintf(statsText, sizeof(statsText), "Bones: %d | Heads: %s (%d) | Torsos: %s (%d) | Camera Mode: %d",
        app->renderBonesCount, app->renderHeadBillboards ? "ON" : "OFF", app->renderHeadsCount,
        app->renderTorsoBillboards ? "ON" : "OFF", app->renderTorsosCount,
        app->camMode);
    DrawText(statsText, 10, 75, 16, DARKGRAY);

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
                    // You might need to adjust this logic based on your character grouping
                    // For now, we'll use proximity as a heuristic
                    float neckHeadDistance = Vector3Distance(bone->position, currentHead->position);
                    if (neckHeadDistance < 2.0f) { // Adjust threshold as needed
                        Vector3 toCam = Vector3Subtract(camPos, bone->position);
                        float distance = Vector3Length(toCam);
                        Vector3 renderOffset = { 0 };
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
            Vector3 renderOffset = { 0 };
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

    DrawOpenPoseSkeleton(app);
    EndMode3D();
    EndDrawing();
}

/*
 * +---------------------------------------------------------------+
 * | Function: main                                                |
 * |                                                               |
 * | Application entry point: initialize AppState via App_Init,    |
 * | run main loop (handle input, update camera/center/data, draw) |
 * | until window closes, then cleanup with App_Shutdown.          |
 * |                                                               |
 * | - Typical flow: Init -> Loop -> Shutdown                      |
 * +---------------------------------------------------------------+
 */
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
