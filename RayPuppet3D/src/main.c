#include "bones_core.h"

#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080
#define UI_HEIGHT 200
#define TIMELINE_HEIGHT 100
#define BUTTON_SIZE 35
#define TIMELINE_MARGIN 60
#define MAX_UNDO_STACK 50
#define GIZMO_SIZE 120
#define GIZMO_MARGIN 20
#define GIZMO_CIRCLE_RADIUS 50
#define GIZMO_DOT_RADIUS 8
#define GIZMO_SNAP_THRESHOLD 0.15f 

static const float ORBIT_SENSITIVITY = 0.01f;
static const float FPS_SENSITIVITY = 0.003f;
static const float ZOOM_SENSITIVITY = 0.5f;
static const float MIN_ORBIT_RADIUS = 0.5f;
static const float MAX_ORBIT_RADIUS = 20.0f;
static const float MIN_PITCH = -85.0f * PI / 180.0f;
static const float MAX_PITCH = 81.0f * PI / 180.0f;
static const float BASE_SPEED = 5.0f;
static const float MOVEMENT_SPEED = 3.0f;
static const float AXIS_LENGTH = 0.05f;

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

typedef enum {
    TOOL_NONE,
    TOOL_SELECT,
    TOOL_DELETE,
    TOOL_DUPLICATE,
    TOOL_INTERPOLATE
} EditorTool;

typedef enum {
    UNDO_BONE_MOVE,
    UNDO_KEYFRAME_MOVE,
    UNDO_FRAME_PROMOTE
} UndoActionType;

typedef struct {
    UndoActionType type;
    char boneName[64];
    int frameNumber;
    Vector3 oldPosition;
    Vector3 newPosition;
    int oldFrameNumber;
    int newFrameNumber;
    int promotedFrameNumber;
} UndoAction;

typedef struct {
    UndoAction actions[MAX_UNDO_STACK];
    int count;
    int currentIndex;
} UndoHistory;

typedef struct {
    bool isPlaying;
    bool showTimeline;
    int selectedFrame;
    int selectionStart;
    int selectionEnd;
    bool isDraggingSlider;
    bool isSelecting;
    EditorTool currentTool;
    int interpolationCount;
    char exportPath[256];
    bool showExportDialog;
    bool needsSave;
    float playbackSpeed;
    bool hasBoneSelected;
    char selectedBoneName[64];
    int selectedBonePersonIndex;
    Vector3 selectedBonePosition;
    bool isDraggingBone;
    Vector3 dragStartPos;
    Vector2 dragStartMouse;
    bool isDraggingKeyframe;
    int draggedKeyframeNumber;
    Vector3 keyframeDragOffset;
    AnimationFrame draggedKeyframeData;
    UndoHistory undoHistory;
    Vector2 lastClickPos;
    float lastClickTime;
    int cycleIndex;
    bool isDraggingGizmo;
} EditorState;

typedef struct {
    bool showBoneNames;
    bool showDebugSpheres;
    bool showConnections;
    bool showOrientation;
} DebugOptions;

typedef struct {
    AnimatedCharacter* character;
    int camMode;
    float orbitYaw, orbitPitch, orbitRadius;
    bool showUI;
    char currentAnimation[64];
    EditorState editor;
    DebugOptions debug;
    int screenWidth;
    int screenHeight;
} AppState;

typedef struct {
    char boneName[64];
    int personIndex;
    Vector3 bonePos;
    float distance;
} BoneCandidate;

static void RecalculateAffectedInterpolations(AppState* app, int movedKeyframe);
static void MoveBoneInFrame(AppState* app, int frameNumber, const char* boneName, Vector3 newPosition);
static void MoveKeyframeInTimeline(AppState* app, int fromFrameNumber, int toFrameNumber);
static int FindFrameIndexByNumber(AppState* app, int frameNumber);
// ============================================================================
// UNDO/REDO SYSTEM
// ============================================================================

static void InitUndoHistory(UndoHistory* history) {
    memset(history, 0, sizeof(UndoHistory));
    history->count = 0;
    history->currentIndex = -1;
}

static void PushUndoAction(UndoHistory* history, UndoAction action) {
    if (history->currentIndex < history->count - 1) {
        history->count = history->currentIndex + 1;
    }
    if (history->count >= MAX_UNDO_STACK) {
        for (int i = 0; i < MAX_UNDO_STACK - 1; i++) {
            history->actions[i] = history->actions[i + 1];
        }
        history->count = MAX_UNDO_STACK - 1;
    }
    history->actions[history->count] = action;
    history->currentIndex = history->count;
    history->count++;
}

static bool PerformUndo(AppState* app) {
    UndoHistory* history = &app->editor.undoHistory;
    if (history->currentIndex < 0) return false;
    UndoAction* action = &history->actions[history->currentIndex];
    switch (action->type) {
        case UNDO_BONE_MOVE: {
            MoveBoneInFrame(app, action->frameNumber, action->boneName, action->oldPosition);
            RecalculateAffectedInterpolations(app, action->frameNumber);
            int frameIndex = FindFrameIndexByNumber(app, action->frameNumber);
            if (frameIndex != -1) {
                SetCharacterFrame(app->character, frameIndex);
            }
            break;
        }
        case UNDO_KEYFRAME_MOVE: {
            MoveKeyframeInTimeline(app, action->newFrameNumber, action->oldFrameNumber);
            break;
        }
        case UNDO_FRAME_PROMOTE: {
            int frameIndex = FindFrameIndexByNumber(app, action->promotedFrameNumber);
            if (frameIndex != -1) {
                app->character->animation.frames[frameIndex].isOriginalKeyframe = false;
                RecalculateAffectedInterpolations(app, action->promotedFrameNumber);
            }
            break;
        }
    }
    history->currentIndex--;
    app->editor.needsSave = true;
    return true;
}

static bool PerformRedo(AppState* app) {
    UndoHistory* history = &app->editor.undoHistory;
    if (history->currentIndex >= history->count - 1) return false;
    history->currentIndex++;
    UndoAction* action = &history->actions[history->currentIndex];
    switch (action->type) {
        case UNDO_BONE_MOVE: {
            MoveBoneInFrame(app, action->frameNumber, action->boneName, action->newPosition);
            RecalculateAffectedInterpolations(app, action->frameNumber);
            int frameIndex = FindFrameIndexByNumber(app, action->frameNumber);
            if (frameIndex != -1) {
                SetCharacterFrame(app->character, frameIndex);
            }
            break;
        }
        case UNDO_KEYFRAME_MOVE: {
            MoveKeyframeInTimeline(app, action->oldFrameNumber, action->newFrameNumber);
            break;
        }
        case UNDO_FRAME_PROMOTE: {
            int frameIndex = FindFrameIndexByNumber(app, action->promotedFrameNumber);
            if (frameIndex != -1) {
                app->character->animation.frames[frameIndex].isOriginalKeyframe = true;
            }
            break;
        }
    }
    app->editor.needsSave = true;
    return true;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static int GetCurrentFrameNumber(AppState* app) {
    if (app->character->currentFrame < 0 || 
        app->character->currentFrame >= app->character->animation.frameCount) {
        return 0;
    }
    return app->character->animation.frames[app->character->currentFrame].frameNumber;
}

static int FindFrameIndexByNumber(AppState* app, int frameNumber) {
    for (int i = 0; i < app->character->animation.frameCount; i++) {
        if (app->character->animation.frames[i].frameNumber == frameNumber) {
            return i;
        }
    }
    return -1;
}

static int FindMaxFrameNumber(AppState* app) {
    int maxFrame = 0;
    for (int i = 0; i < app->character->animation.frameCount; i++) {
        if (app->character->animation.frames[i].frameNumber > maxFrame) {
            maxFrame = app->character->animation.frames[i].frameNumber;
        }
    }
    return maxFrame;
}

static bool FrameExists(AppState* app, int frameNumber) {
    for (int i = 0; i < app->character->animation.frameCount; i++) {
        if (app->character->animation.frames[i].frameNumber == frameNumber) {
            return true;
        }
    }
    return false;
}

static int FindPreviousKeyframe(AppState* app, int fromFrame) {
    for (int i = fromFrame - 1; i >= 0; i--) {
        int frameIndex = FindFrameIndexByNumber(app, i);
        if (frameIndex != -1 && app->character->animation.frames[frameIndex].isOriginalKeyframe) {
            return i;
        }
    }
    return -1;
}

static int FindNextKeyframe(AppState* app, int fromFrame) {
    int maxFrameNumber = FindMaxFrameNumber(app);
    for (int i = fromFrame + 1; i <= maxFrameNumber; i++) {
        int frameIndex = FindFrameIndexByNumber(app, i);
        if (frameIndex != -1 && app->character->animation.frames[frameIndex].isOriginalKeyframe) {
            return i;
        }
    }
    return -1;
}

static bool IsCurrentFrameKeyframe(AppState* app) {
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return false;
    return app->character->animation.frames[currentFrame].isOriginalKeyframe;
}

static Rectangle GetTimelineRect(AppState* app) {
    return (Rectangle){
        TIMELINE_MARGIN,
        app->screenHeight - UI_HEIGHT + 50,
        app->screenWidth - TIMELINE_MARGIN * 2,
        40
    };
}

// ============================================================================
// CAMERA GIZMO
// ============================================================================

typedef struct {
    const char* name;
    float yaw;
    float pitch;
    Color color;
} SnapPoint;

static const SnapPoint SNAP_POINTS[] = {
    {"FRONT",  0.0f,        0.0f,           BLUE},
    {"BACK",   PI,          0.0f,           DARKBLUE},
    {"RIGHT",  PI/2,        0.0f,           RED},
    {"LEFT",   -PI/2,       0.0f,           DARKPURPLE},
    {"TOP",    0.0f,        MAX_PITCH,      GREEN},
    {"BOTTOM", 0.0f,        MIN_PITCH,      DARKGREEN},
};

static const int SNAP_POINT_COUNT = 6;

static Rectangle GetGizmoRect(AppState* app) {
    int gizmoSize = app->screenHeight * 0.23f;
    if (gizmoSize < 120) gizmoSize = 120;
    if (gizmoSize > 250) gizmoSize = 250;
    int centerY = app->screenHeight / 2;
    return (Rectangle){
        GIZMO_MARGIN,
        centerY - gizmoSize,
        gizmoSize,
        gizmoSize
    };
}

static void SnapToNearestPoint(AppState* app) {
    float bestDistance = GIZMO_SNAP_THRESHOLD;
    int bestSnapIndex = -1;
    for (int i = 0; i < SNAP_POINT_COUNT; i++) {
        float yawDiff = fabsf(app->orbitYaw - SNAP_POINTS[i].yaw);
        float pitchDiff = fabsf(app->orbitPitch - SNAP_POINTS[i].pitch);
        if (yawDiff > PI) yawDiff = 2*PI - yawDiff;
        float normalizedYaw = yawDiff / PI;
        float normalizedPitch = pitchDiff / MAX_PITCH;
        float distance = sqrtf(normalizedYaw * normalizedYaw + normalizedPitch * normalizedPitch);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestSnapIndex = i;
        }
    }
    if (bestSnapIndex != -1) {
        app->orbitYaw = SNAP_POINTS[bestSnapIndex].yaw;
        app->orbitPitch = SNAP_POINTS[bestSnapIndex].pitch;
    }
}

static void DrawCameraGizmo(AppState* app) {
    if (!app->showUI || app->camMode != 1) return;
    Rectangle gizmoRect = GetGizmoRect(app);
    Vector2 center = {
        gizmoRect.x + gizmoRect.width / 2,
        gizmoRect.y + gizmoRect.height / 2
    };
    float circleRadius = gizmoRect.width * 0.4f;
    float dotRadius = gizmoRect.width * 0.04f;
    float snapPointRadius = gizmoRect.width * 0.035f;
    DrawRectangleRec(gizmoRect, (Color){40, 40, 40, 200});
    DrawRectangleLinesEx(gizmoRect, 2, BLACK);
    DrawCircleV(center, circleRadius, (Color){60, 60, 60, 255});
    DrawCircleLines((int)center.x, (int)center.y, circleRadius, LIGHTGRAY);
    for (int i = 0; i < SNAP_POINT_COUNT; i++) {
        float normalizedYaw = SNAP_POINTS[i].yaw / PI;
        float normalizedPitch = SNAP_POINTS[i].pitch / MAX_PITCH;
        float snapX = center.x + normalizedYaw * (circleRadius - snapPointRadius * 2);
        float snapY = center.y - normalizedPitch * (circleRadius - snapPointRadius * 2);
        float yawDiff = fabsf(app->orbitYaw - SNAP_POINTS[i].yaw);
        float pitchDiff = fabsf(app->orbitPitch - SNAP_POINTS[i].pitch);
        if (yawDiff > PI) yawDiff = 2*PI - yawDiff;
        bool isNear = (yawDiff < GIZMO_SNAP_THRESHOLD * 2) && 
                      (pitchDiff < GIZMO_SNAP_THRESHOLD * 2);
        float currentSnapRadius = isNear ? snapPointRadius * 1.3f : snapPointRadius;
        DrawCircleV((Vector2){snapX, snapY}, currentSnapRadius, 
                   isNear ? ColorBrightness(SNAP_POINTS[i].color, 0.3f) : SNAP_POINTS[i].color);
        DrawCircleLines((int)snapX, (int)snapY, currentSnapRadius, WHITE);
        if (isNear) {
            int textSize = (int)(gizmoRect.width * 0.045f);
            if (textSize < 8) textSize = 8;
            if (textSize > 12) textSize = 12;
            int textW = MeasureText(SNAP_POINTS[i].name, textSize);
            DrawRectangle((int)snapX - textW/2 - 2, (int)snapY - 18, 
                         textW + 4, 14, (Color){0, 0, 0, 180});
            DrawText(SNAP_POINTS[i].name, (int)snapX - textW/2, (int)snapY - 16, textSize, WHITE);
        }
    }
    DrawLineEx(
        (Vector2){center.x - circleRadius, center.y},
        (Vector2){center.x + circleRadius, center.y},
        2, (Color){100, 100, 100, 255}
    );
    DrawLineEx(
        (Vector2){center.x, center.y - circleRadius},
        (Vector2){center.x, center.y + circleRadius},
        2, (Color){100, 100, 100, 255}
    );
    float normalizedYaw = app->orbitYaw / PI;
    float normalizedPitch = app->orbitPitch / MAX_PITCH;
    float dotX = center.x + normalizedYaw * (circleRadius - dotRadius);
    float dotY = center.y - normalizedPitch * (circleRadius - dotRadius);
    Vector2 dotPos = {dotX, dotY};
    float distFromCenter = Vector2Distance(dotPos, center);
    if (distFromCenter > circleRadius - dotRadius) {
        Vector2 direction = Vector2Normalize(Vector2Subtract(dotPos, center));
        dotPos = Vector2Add(center, Vector2Scale(direction, circleRadius - dotRadius));
    }
    Color dotColor = app->editor.isDraggingGizmo ? ORANGE : SKYBLUE;
    DrawCircleV(dotPos, dotRadius, dotColor);
    DrawCircleLines((int)dotPos.x, (int)dotPos.y, dotRadius, WHITE);
    int titleSize = (int)(gizmoRect.width * 0.055f);
    if (titleSize < 10) titleSize = 10;
    if (titleSize > 16) titleSize = 16;
    DrawText("CAMERA", (int)(gizmoRect.x + 8), (int)(gizmoRect.y + 8), titleSize, YELLOW);
    int angleTextSize = (int)(gizmoRect.width * 0.04f);
    if (angleTextSize < 8) angleTextSize = 8;
    if (angleTextSize > 12) angleTextSize = 12;
    char yawText[32], pitchText[32];
    snprintf(yawText, sizeof(yawText), "Yaw: %.0f°", app->orbitYaw * 180.0f / PI);
    snprintf(pitchText, sizeof(pitchText), "Pitch: %.0f°", app->orbitPitch * 180.0f / PI);
    DrawText(yawText, (int)(gizmoRect.x + 8), (int)(gizmoRect.y + gizmoRect.height - 35), angleTextSize, WHITE);
    DrawText(pitchText, (int)(gizmoRect.x + 8), (int)(gizmoRect.y + gizmoRect.height - 20), angleTextSize, WHITE);
}

static void UpdateCameraGizmo(AppState* app) {
    if (!app->showUI || app->camMode != 1) {
        app->editor.isDraggingGizmo = false;
        return;
    }
    Rectangle gizmoRect = GetGizmoRect(app);
    Vector2 mousePos = GetMousePosition();
    float circleRadius = gizmoRect.width * 0.4f;
    float dotRadius = gizmoRect.width * 0.04f;
    float snapPointRadius = gizmoRect.width * 0.035f;
    Vector2 center = {
        gizmoRect.x + gizmoRect.width / 2,
        gizmoRect.y + gizmoRect.height / 2
    };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, gizmoRect)) {
        for (int i = 0; i < SNAP_POINT_COUNT; i++) {
            float normalizedYaw = SNAP_POINTS[i].yaw / PI;
            float normalizedPitch = SNAP_POINTS[i].pitch / MAX_PITCH;
            float snapX = center.x + normalizedYaw * (circleRadius - snapPointRadius * 2);
            float snapY = center.y - normalizedPitch * (circleRadius - snapPointRadius * 2);
            Vector2 snapPos = {snapX, snapY};
            float distToSnap = Vector2Distance(mousePos, snapPos);
            float detectionRadius = snapPointRadius * 2.0f;
            if (distToSnap < detectionRadius) {
                app->orbitYaw = SNAP_POINTS[i].yaw;
                app->orbitPitch = SNAP_POINTS[i].pitch;
                return;
            }
        }
        app->editor.isDraggingGizmo = true;
    }
    if (app->editor.isDraggingGizmo && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 offset = Vector2Subtract(mousePos, center);
        float dist = Vector2Length(offset);
        if (dist > circleRadius - dotRadius) {
            offset = Vector2Scale(Vector2Normalize(offset), circleRadius - dotRadius);
        }
        float normalizedX = offset.x / (circleRadius - dotRadius);
        float normalizedY = -offset.y / (circleRadius - dotRadius);
        app->orbitYaw = normalizedX * PI;
        app->orbitPitch = Clamp(normalizedY * MAX_PITCH, MIN_PITCH, MAX_PITCH);
        SnapToNearestPoint(app);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if (app->editor.isDraggingGizmo) {
            SnapToNearestPoint(app);
        }
        app->editor.isDraggingGizmo = false;
    }
}

// ============================================================================
// KEYFRAME MANAGEMENT FUNCTIONS
// ============================================================================

static void PromoteFrameToKeyframe(AppState* app) {
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
    AnimationFrame* frame = &app->character->animation.frames[currentFrame];
    if (frame->isOriginalKeyframe) return;
    UndoAction action = {0};
    action.type = UNDO_FRAME_PROMOTE;
    action.promotedFrameNumber = frame->frameNumber;
    PushUndoAction(&app->editor.undoHistory, action);
    frame->isOriginalKeyframe = true;
    app->editor.needsSave = true;
}

static void RecalculateInterpolatedFrames(AppState* app, int keyframeA, int keyframeB) {
    if (keyframeA < 0 || keyframeB < 0 || keyframeB <= keyframeA) return;
    int indexA = FindFrameIndexByNumber(app, keyframeA);
    int indexB = FindFrameIndexByNumber(app, keyframeB);
    if (indexA == -1 || indexB == -1) return;
    AnimationFrame* frameA = &app->character->animation.frames[indexA];
    AnimationFrame* frameB = &app->character->animation.frames[indexB];
    for (int frameNum = keyframeA + 1; frameNum < keyframeB; frameNum++) {
        int interpIndex = FindFrameIndexByNumber(app, frameNum);
        if (interpIndex == -1) continue;
        AnimationFrame* interpFrame = &app->character->animation.frames[interpIndex];
        if (!interpFrame->isOriginalKeyframe) {
            float t = (float)(frameNum - keyframeA) / (float)(keyframeB - keyframeA);
            for (int p = 0; p < frameA->personCount && p < frameB->personCount; p++) {
                Person* personA = &frameA->persons[p];
                Person* personB = &frameB->persons[p];
                Person* interpPerson = &interpFrame->persons[p];
                if (!personA->active || !personB->active) continue;
                for (int bA = 0; bA < personA->boneCount; bA++) {
                    Bone* boneA = &personA->bones[bA];
                    if (!boneA->position.valid) continue;
                    for (int bB = 0; bB < personB->boneCount; bB++) {
                        Bone* boneB = &personB->bones[bB];
                        if (strcmp(boneA->name, boneB->name) == 0 && boneB->position.valid) {
                            for (int bI = 0; bI < interpPerson->boneCount; bI++) {
                                Bone* boneInterp = &interpPerson->bones[bI];
                                if (strcmp(boneInterp->name, boneA->name) == 0) {
                                    boneInterp->position.position = Vector3Lerp(
                                        boneA->position.position,
                                        boneB->position.position,
                                        t
                                    );
                                    boneInterp->position.valid = true;
                                    boneInterp->position.confidence = 
                                        boneA->position.confidence * (1.0f - t) + 
                                        boneB->position.confidence * t;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

static void RecalculateAffectedInterpolations(AppState* app, int movedKeyframe) {
    int prevKeyframe = FindPreviousKeyframe(app, movedKeyframe);
    int nextKeyframe = FindNextKeyframe(app, movedKeyframe);
    if (prevKeyframe != -1) {
        RecalculateInterpolatedFrames(app, prevKeyframe, movedKeyframe);
    }
    if (nextKeyframe != -1) {
        RecalculateInterpolatedFrames(app, movedKeyframe, nextKeyframe);
    }
}

// ============================================================================
// GEOMETRY HELPER FUNCTIONS
// ============================================================================

static bool RayPlaneIntersection(Ray ray, Vector3 planePoint, Vector3 planeNormal, Vector3* outPoint) {
    float denom = Vector3DotProduct(planeNormal, ray.direction);
    if (fabsf(denom) < 0.0001f) return false;
    Vector3 diff = Vector3Subtract(planePoint, ray.position);
    float t = Vector3DotProduct(diff, planeNormal) / denom;
    if (t < 0) return false;
    *outPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    return true;
}

// ============================================================================
// TIMELINE KEYFRAME DRAGGING
// ============================================================================

static void EnsureFrameExists(AppState* app, int frameNumber) {
    int existingIndex = FindFrameIndexByNumber(app, frameNumber);
    if (existingIndex != -1) return;
    int prevKeyframe = -1;
    int nextKeyframe = -1;
    for (int i = frameNumber - 1; i >= 0; i--) {
        int idx = FindFrameIndexByNumber(app, i);
        if (idx != -1 && app->character->animation.frames[idx].isOriginalKeyframe) {
            prevKeyframe = i;
            break;
        }
    }
    int maxFrame = FindMaxFrameNumber(app);
    for (int i = frameNumber + 1; i <= maxFrame; i++) {
        int idx = FindFrameIndexByNumber(app, i);
        if (idx != -1 && app->character->animation.frames[idx].isOriginalKeyframe) {
            nextKeyframe = i;
            break;
        }
    }
    if (prevKeyframe == -1 || nextKeyframe == -1) return;
    int prevIdx = FindFrameIndexByNumber(app, prevKeyframe);
    int nextIdx = FindFrameIndexByNumber(app, nextKeyframe);
    if (prevIdx == -1 || nextIdx == -1) return;
    if (app->character->animation.frameCount >= app->character->animation.maxFrames) return;
    AnimationFrame* prevFrame = &app->character->animation.frames[prevIdx];
    AnimationFrame* nextFrame = &app->character->animation.frames[nextIdx];
    int insertPos = app->character->animation.frameCount;
    for (int i = 0; i < app->character->animation.frameCount; i++) {
        if (app->character->animation.frames[i].frameNumber > frameNumber) {
            insertPos = i;
            break;
        }
    }
    for (int i = app->character->animation.frameCount; i > insertPos; i--) {
        app->character->animation.frames[i] = app->character->animation.frames[i - 1];
    }
    AnimationFrame* newFrame = &app->character->animation.frames[insertPos];
    memset(newFrame, 0, sizeof(AnimationFrame));
    newFrame->frameNumber = frameNumber;
    newFrame->valid = true;
    newFrame->isOriginalKeyframe = false;
    newFrame->personCount = prevFrame->personCount;
    float t = (float)(frameNumber - prevKeyframe) / (float)(nextKeyframe - prevKeyframe);
    for (int p = 0; p < prevFrame->personCount && p < nextFrame->personCount; p++) {
        Person* prevPerson = &prevFrame->persons[p];
        Person* nextPerson = &nextFrame->persons[p];
        Person* newPerson = &newFrame->persons[p];
        newPerson->active = prevPerson->active && nextPerson->active;
        newPerson->boneCount = prevPerson->boneCount;
        for (int b = 0; b < prevPerson->boneCount; b++) {
            Bone* prevBone = &prevPerson->bones[b];
            strncpy(newPerson->bones[b].name, prevBone->name, MAX_BONE_NAME_LENGTH - 1);
            for (int nb = 0; nb < nextPerson->boneCount; nb++) {
                Bone* nextBone = &nextPerson->bones[nb];
                if (strcmp(prevBone->name, nextBone->name) == 0 && 
                    prevBone->position.valid && nextBone->position.valid) {
                    newPerson->bones[b].position.position = Vector3Lerp(
                        prevBone->position.position,
                        nextBone->position.position,
                        t
                    );
                    newPerson->bones[b].position.valid = true;
                    newPerson->bones[b].position.confidence = 
                        prevBone->position.confidence * (1.0f - t) + 
                        nextBone->position.confidence * t;
                    break;
                }
            }
        }
    }
    app->character->animation.frameCount++;
    app->character->maxFrames = app->character->animation.frameCount;
}

static void MoveKeyframeInTimeline(AppState* app, int fromFrameNumber, int toFrameNumber) {
    if (fromFrameNumber == toFrameNumber) return;
    int fromIndex = FindFrameIndexByNumber(app, fromFrameNumber);
    if (fromIndex == -1) return;
    AnimationFrame* sourceFrame = &app->character->animation.frames[fromIndex];
    if (!sourceFrame->isOriginalKeyframe) return;
    EnsureFrameExists(app, toFrameNumber);
    int toIndex = FindFrameIndexByNumber(app, toFrameNumber);
    if (toIndex == -1) return;
    AnimationFrame* targetFrame = &app->character->animation.frames[toIndex];
    if (targetFrame->isOriginalKeyframe && toIndex != fromIndex) return;
    targetFrame->isOriginalKeyframe = true;
    targetFrame->frameNumber = toFrameNumber;
    for (int p = 0; p < sourceFrame->personCount && p < MAX_PERSONS; p++) {
        Person* sourcePerson = &sourceFrame->persons[p];
        Person* targetPerson = &targetFrame->persons[p];
        targetPerson->active = sourcePerson->active;
        targetPerson->boneCount = sourcePerson->boneCount;
        for (int b = 0; b < sourcePerson->boneCount && b < MAX_BONES_PER_PERSON; b++) {
            targetPerson->bones[b] = sourcePerson->bones[b];
        }
    }
    if (fromIndex != toIndex) {
        sourceFrame->isOriginalKeyframe = false;
        int prevKeyframe = FindPreviousKeyframe(app, fromFrameNumber);
        int nextKeyframe = FindNextKeyframe(app, fromFrameNumber);
        if (prevKeyframe != -1 && nextKeyframe != -1) {
            RecalculateInterpolatedFrames(app, prevKeyframe, nextKeyframe);
        } else {
            BonesDeleteFrame(&app->character->animation, fromIndex);
            app->character->maxFrames = app->character->animation.frameCount;
        }
    }
    app->character->forceUpdate = true;
}

static void UpdateTimelineKeyframeDragging(AppState* app) {
    if (!app->editor.showTimeline || !app->character->animation.isLoaded) return;
    Rectangle timeline = GetTimelineRect(app);
    Vector2 mousePos = GetMousePosition();
    int maxFrameNumber = FindMaxFrameNumber(app);
    float frameWidth = timeline.width / (float)(maxFrameNumber + 1);
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mousePos, timeline)) {
            int clickedFrameNumber = (int)((mousePos.x - timeline.x) / frameWidth);
            if (clickedFrameNumber >= 0 && clickedFrameNumber <= maxFrameNumber) {
                int frameIndex = FindFrameIndexByNumber(app, clickedFrameNumber);
                if (frameIndex != -1) {
                    AnimationFrame* frame = &app->character->animation.frames[frameIndex];
                    if (frame->isOriginalKeyframe) {
                        app->editor.isDraggingKeyframe = true;
                        app->editor.draggedKeyframeNumber = clickedFrameNumber;
                        app->editor.draggedKeyframeData = *frame;
                    }
                }
            }
        }
    }
    if (app->editor.isDraggingKeyframe && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (mousePos.x >= timeline.x && mousePos.x <= timeline.x + timeline.width) {
            int targetFrameNumber = (int)((mousePos.x - timeline.x) / frameWidth);
            if (targetFrameNumber >= 0 && targetFrameNumber <= maxFrameNumber) {
                if (targetFrameNumber != app->editor.draggedKeyframeNumber) {
                    int targetIndex = FindFrameIndexByNumber(app, targetFrameNumber);
                    bool canMove = true;
                    if (targetIndex != -1) {
                        AnimationFrame* targetFrame = &app->character->animation.frames[targetIndex];
                        if (targetFrame->isOriginalKeyframe) {
                            canMove = false;
                        }
                    } else {
                        EnsureFrameExists(app, targetFrameNumber);
                        targetIndex = FindFrameIndexByNumber(app, targetFrameNumber);
                        if (targetIndex == -1) {
                            canMove = false;
                        }
                    }
                    if (canMove) {
                        MoveKeyframeInTimeline(app, app->editor.draggedKeyframeNumber, targetFrameNumber);
                        app->editor.draggedKeyframeNumber = targetFrameNumber;
                    }
                }
            }
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && app->editor.isDraggingKeyframe) {
        int oldFrameNumber = app->editor.draggedKeyframeData.frameNumber;
        int newFrameNumber = app->editor.draggedKeyframeNumber;
        if (oldFrameNumber != newFrameNumber) {
            UndoAction action = {0};
            action.type = UNDO_KEYFRAME_MOVE;
            action.oldFrameNumber = oldFrameNumber;
            action.newFrameNumber = newFrameNumber;
            PushUndoAction(&app->editor.undoHistory, action);
        }
        app->editor.isDraggingKeyframe = false;
        int prevKeyframe = FindPreviousKeyframe(app, app->editor.draggedKeyframeNumber);
        int nextKeyframe = FindNextKeyframe(app, app->editor.draggedKeyframeNumber);
        if (prevKeyframe != -1) {
            RecalculateInterpolatedFrames(app, prevKeyframe, app->editor.draggedKeyframeNumber);
        }
        if (nextKeyframe != -1) {
            RecalculateInterpolatedFrames(app, app->editor.draggedKeyframeNumber, nextKeyframe);
        }
        int frameIndex = FindFrameIndexByNumber(app, app->editor.draggedKeyframeNumber);
        if (frameIndex != -1) {
            SetCharacterFrame(app->character, frameIndex);
        }
        app->editor.needsSave = true;
    }
}

// ============================================================================
// BONE MANIPULATION FUNCTIONS
// ============================================================================

static void MoveBoneInFrame(AppState* app, int frameNumber, const char* boneName, Vector3 newPosition) {
    int frameIndex = FindFrameIndexByNumber(app, frameNumber);
    if (frameIndex == -1) return;
    AnimationFrame* frame = &app->character->animation.frames[frameIndex];
    for (int p = 0; p < frame->personCount; p++) {
        Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            Bone* bone = &person->bones[b];
            if (strcmp(bone->name, boneName) == 0 && bone->position.valid) {
                bone->position.position = newPosition;
                app->editor.needsSave = true;
                app->character->forceUpdate = true;
                return;
            }
        }
    }
}

static void MoveBoneWithMouse(AppState* app) {
    if (!app->editor.hasBoneSelected || !app->editor.isDraggingBone) return;
    if (!app->character->animation.isLoaded) return;
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
    Camera camera = app->character->renderer->camera;
    Vector2 mousePos = GetMousePosition();
    Ray mouseRay = GetMouseRay(mousePos, camera);
    Vector3 camToPoint = Vector3Subtract(app->editor.selectedBonePosition, camera.position);
    Vector3 planeNormal = Vector3Normalize(camToPoint);
    Vector3 newPosition;
    if (RayPlaneIntersection(mouseRay, app->editor.selectedBonePosition, planeNormal, &newPosition)) {
        int currentFrameNumber = GetCurrentFrameNumber(app);
        MoveBoneInFrame(app, currentFrameNumber, app->editor.selectedBoneName, newPosition);
        app->editor.selectedBonePosition = newPosition;
        if (IsCurrentFrameKeyframe(app)) {
            RecalculateAffectedInterpolations(app, currentFrameNumber);
        }
    }
}

static int FindAllBonesUnderMouse(AppState* app, BoneCandidate* candidates, int maxCandidates) {
    if (!app->character->animation.isLoaded) return 0;
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return 0;
    const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
    Camera camera = app->character->renderer->camera;
    Vector2 mousePos = GetMousePosition();
    int candidateCount = 0;
    const float SELECTION_RADIUS = 50.0f;
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid) continue;
            Vector2 screenPos = GetWorldToScreen(bone->position.position, camera);
            float dist = Vector2Distance(mousePos, screenPos);
            if (dist < SELECTION_RADIUS && candidateCount < maxCandidates) {
                candidates[candidateCount].distance = dist;
                strncpy(candidates[candidateCount].boneName, bone->name, 63);
                candidates[candidateCount].boneName[63] = '\0';
                candidates[candidateCount].personIndex = p;
                candidates[candidateCount].bonePos = bone->position.position;
                candidateCount++;
            }
        }
    }
    for (int i = 0; i < candidateCount - 1; i++) {
        for (int j = 0; j < candidateCount - i - 1; j++) {
            if (candidates[j].distance > candidates[j + 1].distance) {
                BoneCandidate temp = candidates[j];
                candidates[j] = candidates[j + 1];
                candidates[j + 1] = temp;
            }
        }
    }
    return candidateCount;
}

static void UpdateBoneSelection(AppState* app) {
    if (app->camMode != 1) {
        app->editor.hasBoneSelected = false;
        app->editor.isDraggingBone = false;
        return;
    }
    if (app->editor.isDraggingSlider || app->editor.isDraggingKeyframe) return;
    Vector2 mousePos = GetMousePosition();
    Rectangle timeline = GetTimelineRect(app);
    Rectangle gizmoRect = GetGizmoRect(app);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && 
        !CheckCollisionPointRec(mousePos, timeline) && 
        !CheckCollisionPointRec(mousePos, gizmoRect) &&
        !IsKeyDown(KEY_LEFT_CONTROL)) {
        BoneCandidate candidates[100];
        int candidateCount = FindAllBonesUnderMouse(app, candidates, 100);
        if (candidateCount > 0) {
            float currentTime = GetTime();
            float timeSinceLastClick = currentTime - app->editor.lastClickTime;
            float distanceFromLastClick = Vector2Distance(mousePos, app->editor.lastClickPos);
            const float DOUBLE_CLICK_TIME = 0.5f;
            const float DOUBLE_CLICK_DISTANCE = 10.0f;
            if (timeSinceLastClick < DOUBLE_CLICK_TIME && 
                distanceFromLastClick < DOUBLE_CLICK_DISTANCE &&
                candidateCount > 1) {
                app->editor.cycleIndex = (app->editor.cycleIndex + 1) % candidateCount;
            } else {
                app->editor.cycleIndex = 0;
            }
            BoneCandidate* selected = &candidates[app->editor.cycleIndex];
            app->editor.hasBoneSelected = true;
            strncpy(app->editor.selectedBoneName, selected->boneName, 63);
            app->editor.selectedBoneName[63] = '\0';
            app->editor.selectedBonePersonIndex = selected->personIndex;
            app->editor.selectedBonePosition = selected->bonePos;
            app->editor.lastClickPos = mousePos;
            app->editor.lastClickTime = currentTime;
        } else {
            app->editor.hasBoneSelected = false;
            app->editor.cycleIndex = 0;
        }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && app->editor.hasBoneSelected) {
        if (!CheckCollisionPointRec(mousePos, timeline) && !CheckCollisionPointRec(mousePos, gizmoRect)) {
            BoneCandidate candidates[100];
            int candidateCount = FindAllBonesUnderMouse(app, candidates, 100);
            for (int i = 0; i < candidateCount; i++) {
                if (strcmp(candidates[i].boneName, app->editor.selectedBoneName) == 0) {
                    if (!IsCurrentFrameKeyframe(app)) {
                        PromoteFrameToKeyframe(app);
                    }
                    app->editor.isDraggingBone = true;
                    app->editor.dragStartPos = candidates[i].bonePos;
                    app->editor.dragStartMouse = mousePos;
                    break;
                }
            }
        }
    }
    if (app->editor.isDraggingBone && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        MoveBoneWithMouse(app);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && app->editor.isDraggingBone) {
        int currentFrameNumber = GetCurrentFrameNumber(app);
        UndoAction action = {0};
        action.type = UNDO_BONE_MOVE;
        strncpy(action.boneName, app->editor.selectedBoneName, 63);
        action.frameNumber = currentFrameNumber;
        action.oldPosition = app->editor.dragStartPos;
        action.newPosition = app->editor.selectedBonePosition;
        PushUndoAction(&app->editor.undoHistory, action);
        app->editor.isDraggingBone = false;
        RecalculateAffectedInterpolations(app, currentFrameNumber);
    }
}

// ============================================================================
// UI DRAWING FUNCTIONS
// ============================================================================

static bool Button(Rectangle bounds, const char* text, Color color) {
    bool isHovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    bool isPressed = isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Color bgColor = isPressed ? ColorBrightness(color, -0.3f) : 
                    isHovered ? ColorBrightness(color, -0.1f) : color;
    DrawRectangleRec(bounds, bgColor);
    DrawRectangleLinesEx(bounds, 2, BLACK);
    int textWidth = MeasureText(text, 16);
    DrawText(text, 
             (int)(bounds.x + (bounds.width - textWidth) / 2),
             (int)(bounds.y + (bounds.height - 16) / 2),
             16, WHITE);
    return isPressed;
}

static bool ToggleButton(Rectangle bounds, const char* text, bool isActive, Color activeColor, Color inactiveColor) {
    bool isHovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    bool isPressed = isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Color baseColor = isActive ? activeColor : inactiveColor;
    Color bgColor = isPressed ? ColorBrightness(baseColor, -0.3f) : 
                    isHovered ? ColorBrightness(baseColor, -0.1f) : baseColor;
    DrawRectangleRec(bounds, bgColor);
    DrawRectangleLinesEx(bounds, 2, isActive ? ColorBrightness(activeColor, -0.2f) : BLACK);
    int textWidth = MeasureText(text, 14);
    DrawText(text, 
             (int)(bounds.x + (bounds.width - textWidth) / 2),
             (int)(bounds.y + (bounds.height - 14) / 2),
             14, WHITE);
    return isPressed;
}

static bool IconButton(Rectangle bounds, const char* icon, Color color) {
    bool isHovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    bool isPressed = isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Color bgColor = isPressed ? ColorBrightness(color, -0.3f) : 
                    isHovered ? ColorBrightness(color, -0.1f) : color;
    DrawRectangleRec(bounds, bgColor);
    DrawRectangleLinesEx(bounds, 2, BLACK);
    int textWidth = MeasureText(icon, 20);
    DrawText(icon, 
             (int)(bounds.x + (bounds.width - textWidth) / 2),
             (int)(bounds.y + (bounds.height - 20) / 2 - 2),
             20, WHITE);
    return isPressed;
}

static void DrawTextField(Rectangle bounds, const char* text, bool active) {
    DrawRectangleRec(bounds, active ? (Color){240, 240, 255, 255} : LIGHTGRAY);
    DrawRectangleLinesEx(bounds, 2, active ? BLUE : DARKGRAY);
    DrawText(text, (int)(bounds.x + 5), (int)(bounds.y + 8), 16, BLACK);
}

static void DrawBoneSelectionFeedback(AppState* app) {
    if (!app->editor.hasBoneSelected) return;
    if (!app->character->animation.isLoaded) return;
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
    const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
    Camera camera = app->character->renderer->camera;
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (strcmp(bone->name, app->editor.selectedBoneName) == 0 && bone->position.valid) {
                Vector2 screenPos = GetWorldToScreen(bone->position.position, camera);
                BeginMode3D(camera);
                Color highlightColor;
                const char* modeText;
                if (app->editor.isDraggingKeyframe) {
                    highlightColor = PURPLE;
                    modeText = "[DRAGGING KEYFRAME]";
                } else if (app->editor.isDraggingBone) {
                    highlightColor = RED;
                    modeText = "[DRAGGING BONE]";
                } else if (IsCurrentFrameKeyframe(app)) {
                    highlightColor = GREEN;
                    modeText = "[KEYFRAME]";
                } else {
                    highlightColor = ORANGE;
                    modeText = "[INTERPOLATED]";
                }
                DrawSphere(bone->position.position, 0.04f, highlightColor);
                DrawSphereWires(bone->position.position, 0.042f, 8, 8, WHITE);
                EndMode3D();
                int textWidth = MeasureText(bone->name, 12);
                DrawRectangle((int)screenPos.x - 2, (int)screenPos.y - 40, 
                            textWidth + 4, 16, highlightColor);
                DrawText(bone->name, (int)screenPos.x, (int)screenPos.y - 38, 12, WHITE);
                int modeWidth = MeasureText(modeText, 10);
                DrawRectangle((int)screenPos.x - 2, (int)screenPos.y - 23, 
                            modeWidth + 4, 13, Fade(highlightColor, 0.8f));
                DrawText(modeText, (int)screenPos.x, (int)screenPos.y - 21, 10, WHITE);
                return;
            }
        }
    }
}

static void DrawTimeline(AppState* app) {
    if (!app->editor.showTimeline || !app->character->animation.isLoaded) return;
    Rectangle timeline = GetTimelineRect(app);
    int maxFrameNumber = FindMaxFrameNumber(app);
    if (maxFrameNumber == 0 && app->character->animation.frameCount == 0) return;
    DrawRectangleRec(timeline, (Color){40, 40, 40, 255});
    DrawRectangleLinesEx(timeline, 2, BLACK);
    float frameWidth = timeline.width / (float)(maxFrameNumber + 1);
    int currentFrameNumber = GetCurrentFrameNumber(app);
    for (int i = 0; i <= maxFrameNumber; i++) {
        float x = timeline.x + i * frameWidth;
        Rectangle frameRect = {x, timeline.y, frameWidth, timeline.height};
        bool frameExists = FrameExists(app, i);
        bool isCurrentFrame = (i == currentFrameNumber);
        bool isSelected = (i >= app->editor.selectionStart && i <= app->editor.selectionEnd);
        bool isDraggedKeyframe = (app->editor.isDraggingKeyframe && i == app->editor.draggedKeyframeNumber);
        Color frameColor;
        if (!frameExists) {
            frameColor = (Color){30, 30, 30, 255};
        } else if (isDraggedKeyframe) {
            frameColor = PURPLE;
        } else if (isCurrentFrame) {
            frameColor = ORANGE;
        } else if (isSelected) {
            frameColor = SKYBLUE;
        } else {
            frameColor = (i % 5 == 0) ? (Color){60, 60, 60, 255} : (Color){50, 50, 50, 255};
        }
        DrawRectangleRec(frameRect, frameColor);
        DrawRectangleLinesEx(frameRect, 1, BLACK);
        if (i % 5 == 0) {
            char frameNum[16];
            snprintf(frameNum, sizeof(frameNum), "%d", i);
            DrawText(frameNum, (int)(x + 2), (int)(timeline.y - 15), 10, 
                    !frameExists ? DARKGRAY : LIGHTGRAY);
        }
        if (frameExists) {
            int frameIndex = FindFrameIndexByNumber(app, i);
            bool isInterpolated = false;
            if (frameIndex != -1 && frameIndex < app->character->animation.frameCount) {
                isInterpolated = !app->character->animation.frames[frameIndex].isOriginalKeyframe;
            }
            Color indicatorColor = isInterpolated ? BLUE : GREEN;
            if (isDraggedKeyframe) {
                indicatorColor = PURPLE;
                DrawRectangle((int)(x + 1), (int)(timeline.y + timeline.height - 10), 
                             (int)(frameWidth - 2), 6, indicatorColor);
            } else {
                DrawRectangle((int)(x + 2), (int)(timeline.y + timeline.height - 8), 
                             (int)(frameWidth - 4), 4, indicatorColor);
            }
        }
    }
    float markerX = timeline.x + currentFrameNumber * frameWidth + frameWidth / 2;
    DrawLineEx((Vector2){markerX, timeline.y}, 
               (Vector2){markerX, timeline.y + timeline.height}, 
               3, RED);
    DrawCircle((int)markerX, (int)(timeline.y + timeline.height + 5), 6, RED);
    if (app->editor.isDraggingKeyframe) {
        Vector2 mousePos = GetMousePosition();
        if (CheckCollisionPointRec(mousePos, timeline)) {
            int targetFrame = (int)((mousePos.x - timeline.x) / frameWidth);
            if (targetFrame >= 0 && targetFrame <= maxFrameNumber) {
                float targetX = timeline.x + targetFrame * frameWidth;
                DrawRectangleLinesEx(
                    (Rectangle){targetX, timeline.y, frameWidth, timeline.height},
                    3, PURPLE
                );
                char dragText[64];
                snprintf(dragText, sizeof(dragText), "Move to frame %d", targetFrame);
                int textW = MeasureText(dragText, 12);
                DrawRectangle((int)mousePos.x - textW/2 - 5, (int)mousePos.y - 30, 
                            textW + 10, 20, (Color){128, 0, 128, 200});
                DrawText(dragText, (int)mousePos.x - textW/2, (int)mousePos.y - 27, 12, WHITE);
            }
        }
    }
    if (!app->editor.isDraggingKeyframe) {
        if (CheckCollisionPointRec(GetMousePosition(), timeline)) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsKeyDown(KEY_LEFT_CONTROL)) {
                app->editor.isDraggingSlider = true;
            }
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                int newFrameNumber = currentFrameNumber - (int)wheel;
                if (newFrameNumber >= 0 && newFrameNumber <= maxFrameNumber) {
                    if (FrameExists(app, newFrameNumber)) {
                        int frameIndex = FindFrameIndexByNumber(app, newFrameNumber);
                        if (frameIndex != -1) {
                            SetCharacterFrame(app->character, frameIndex);
                        }
                    }
                }
            }
        }
        if (app->editor.isDraggingSlider) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 mousePos = GetMousePosition();
                if (mousePos.x >= timeline.x && mousePos.x <= timeline.x + timeline.width) {
                    int clickedFrameNumber = (int)((mousePos.x - timeline.x) / frameWidth);
                    if (clickedFrameNumber >= 0 && clickedFrameNumber <= maxFrameNumber) {
                        if (FrameExists(app, clickedFrameNumber)) {
                            int frameIndex = FindFrameIndexByNumber(app, clickedFrameNumber);
                            if (frameIndex != -1) {
                                SetCharacterFrame(app->character, frameIndex);
                                if (IsKeyDown(KEY_LEFT_SHIFT)) {
                                    if (app->editor.selectionStart == -1) {
                                        app->editor.selectionStart = clickedFrameNumber;
                                    }
                                    app->editor.selectionEnd = clickedFrameNumber;
                                    if (app->editor.selectionStart > app->editor.selectionEnd) {
                                        int temp = app->editor.selectionStart;
                                        app->editor.selectionStart = app->editor.selectionEnd;
                                        app->editor.selectionEnd = temp;
                                    }
                                } else {
                                    app->editor.selectionStart = clickedFrameNumber;
                                    app->editor.selectionEnd = clickedFrameNumber;
                                }
                            }
                        }
                    }
                }
            } else {
                app->editor.isDraggingSlider = false;
            }
        }
    }
}

static void DrawControlPanel(AppState* app) {
    if (!app->editor.showTimeline) return;
    int panelY = app->screenHeight - UI_HEIGHT + 100;
    int buttonX = 10;
    int maxFrameNumber = FindMaxFrameNumber(app);
    int currentFrameNumber = GetCurrentFrameNumber(app);
    if (IconButton((Rectangle){(float)buttonX, (float)panelY, BUTTON_SIZE, BUTTON_SIZE}, 
                   "|<", DARKBLUE)) {
        for (int i = 0; i <= maxFrameNumber; i++) {
            if (FrameExists(app, i)) {
                int frameIndex = FindFrameIndexByNumber(app, i);
                if (frameIndex != -1) {
                    SetCharacterFrame(app->character, frameIndex);
                }
                break;
            }
        }
    }
    buttonX += BUTTON_SIZE + 5;
    if (IconButton((Rectangle){(float)buttonX, (float)panelY, BUTTON_SIZE, BUTTON_SIZE}, 
                   "<", DARKBLUE)) {
        for (int i = currentFrameNumber - 1; i >= 0; i--) {
            if (FrameExists(app, i)) {
                int frameIndex = FindFrameIndexByNumber(app, i);
                if (frameIndex != -1) {
                    SetCharacterFrame(app->character, frameIndex);
                }
                break;
            }
        }
    }
    buttonX += BUTTON_SIZE + 5;
    const char* playIcon = app->editor.isPlaying ? "||" : ">";
    if (IconButton((Rectangle){(float)buttonX, (float)panelY, BUTTON_SIZE, BUTTON_SIZE}, 
                   playIcon, app->editor.isPlaying ? DARKGREEN : GREEN)) {
        app->editor.isPlaying = !app->editor.isPlaying;
        SetCharacterAutoPlay(app->character, app->editor.isPlaying);
    }
    buttonX += BUTTON_SIZE + 5;
    if (IconButton((Rectangle){(float)buttonX, (float)panelY, BUTTON_SIZE, BUTTON_SIZE}, 
                   ">", DARKBLUE)) {
        for (int i = currentFrameNumber + 1; i <= maxFrameNumber; i++) {
            if (FrameExists(app, i)) {
                int frameIndex = FindFrameIndexByNumber(app, i);
                if (frameIndex != -1) {
                    SetCharacterFrame(app->character, frameIndex);
                }
                break;
            }
        }
    }
    buttonX += BUTTON_SIZE + 5;
    if (IconButton((Rectangle){(float)buttonX, (float)panelY, BUTTON_SIZE, BUTTON_SIZE}, 
                   ">|", DARKBLUE)) {
        for (int i = maxFrameNumber; i >= 0; i--) {
            if (FrameExists(app, i)) {
                int frameIndex = FindFrameIndexByNumber(app, i);
                if (frameIndex != -1) {
                    SetCharacterFrame(app->character, frameIndex);
                }
                break;
            }
        }
    }
    buttonX += BUTTON_SIZE + 15;
    bool canUndo = app->editor.undoHistory.currentIndex >= 0;
    if (IconButton((Rectangle){(float)buttonX, (float)panelY, BUTTON_SIZE, BUTTON_SIZE}, 
                   "<-", canUndo ? PURPLE : DARKGRAY)) {
        if (canUndo) PerformUndo(app);
    }
    buttonX += BUTTON_SIZE + 5;
    bool canRedo = app->editor.undoHistory.currentIndex < app->editor.undoHistory.count - 1;
    if (IconButton((Rectangle){(float)buttonX, (float)panelY, BUTTON_SIZE, BUTTON_SIZE}, 
                   "->", canRedo ? PURPLE : DARKGRAY)) {
        if (canRedo) PerformRedo(app);
    }
    buttonX += BUTTON_SIZE + 15;
    if (Button((Rectangle){(float)buttonX, (float)panelY, 80, BUTTON_SIZE}, 
               "DELETE", RED)) {
        if (app->editor.selectionStart != -1 && FrameExists(app, app->editor.selectionStart)) {
            int frameIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
            if (frameIndex != -1) {
                int deletedFrameNumber = app->editor.selectionStart;
                BonesDeleteFrame(&app->character->animation, frameIndex);
                app->character->maxFrames = app->character->animation.frameCount;
                app->editor.needsSave = true;
                int nextValidFrame = -1;
                for (int i = deletedFrameNumber; i <= maxFrameNumber; i++) {
                    if (FrameExists(app, i)) {
                        nextValidFrame = i;
                        break;
                    }
                }
                if (nextValidFrame == -1) {
                    for (int i = deletedFrameNumber - 1; i >= 0; i--) {
                        if (FrameExists(app, i)) {
                            nextValidFrame = i;
                            break;
                        }
                    }
                }
                if (nextValidFrame != -1) {
                    int newIndex = FindFrameIndexByNumber(app, nextValidFrame);
                    if (newIndex != -1) {
                        SetCharacterFrame(app->character, newIndex);
                        app->editor.selectionStart = nextValidFrame;
                        app->editor.selectionEnd = nextValidFrame;
                    }
                } else {
                    app->editor.selectionStart = -1;
                    app->editor.selectionEnd = -1;
                }
            }
        }
    }
    buttonX += 85;
    if (Button((Rectangle){(float)buttonX, (float)panelY, 80, BUTTON_SIZE}, 
               "DUPLICATE", BLUE)) {
        if (app->editor.selectionStart != -1 && FrameExists(app, app->editor.selectionStart)) {
            int frameIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
            if (frameIndex != -1) {
                BonesDuplicateFrame(&app->character->animation, frameIndex);
                app->character->maxFrames = app->character->animation.frameCount;
                app->editor.needsSave = true;
            }
        }
    }
    buttonX += 85;
    if (Button((Rectangle){(float)buttonX, (float)panelY, 100, BUTTON_SIZE}, 
               "INTERPOLATE", PURPLE)) {
        if (app->editor.selectionStart != -1 && app->editor.selectionEnd != -1 &&
            app->editor.selectionEnd > app->editor.selectionStart) {
            int startIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
            int endIndex = FindFrameIndexByNumber(app, app->editor.selectionEnd);
            if (startIndex != -1 && endIndex != -1) {
                BonesInterpolateFrames(&app->character->animation, 
                                      startIndex, 
                                      endIndex, 
                                      app->editor.interpolationCount);
                app->character->maxFrames = app->character->animation.frameCount;
                app->editor.needsSave = true;
            }
        }
    }
    buttonX += 105;
    char interpText[32];
    snprintf(interpText, sizeof(interpText), "Frames: %d", app->editor.interpolationCount);
    DrawTextField((Rectangle){(float)buttonX, (float)panelY, 100, BUTTON_SIZE}, interpText, false);
    if (IsKeyPressed(KEY_KP_ADD) || (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_EQUAL))) {
        app->editor.interpolationCount++;
    }
    if (IsKeyPressed(KEY_KP_SUBTRACT) || (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_MINUS))) {
        if (app->editor.interpolationCount > 1) app->editor.interpolationCount--;
    }
    buttonX += 105;
    if (Button((Rectangle){(float)buttonX, (float)panelY, 80, BUTTON_SIZE}, 
               "EXPORT", app->editor.needsSave ? ORANGE : DARKGREEN)) {
        app->editor.showExportDialog = !app->editor.showExportDialog;
    }
    buttonX += 85;
    int totalFrames = maxFrameNumber + 1;
    int existingFrames = app->character->animation.frameCount;
    char infoText[128];
    snprintf(infoText, sizeof(infoText), "Frame: %d/%d | Undo: %d Redo: %d | Frames: %d/%d", 
             currentFrameNumber, 
             maxFrameNumber,
             app->editor.undoHistory.currentIndex + 1,
             app->editor.undoHistory.count - app->editor.undoHistory.currentIndex - 1,
             existingFrames,
             totalFrames);
    DrawText(infoText, buttonX + 10, panelY + 10, 16, WHITE);
}

static void DrawDebugPanel(AppState* app) {
    if (!app->showUI) return;
    int panelX = app->screenWidth - 220;
    int panelY = 10;
    int panelWidth = 210;
    int panelHeight = 200;
    int buttonWidth = 190;
    int buttonY = panelY + 40;
    DrawRectangle(panelX, panelY, panelWidth, panelHeight, (Color){40, 40, 40, 220});
    DrawRectangleLinesEx((Rectangle){(float)panelX, (float)panelY, (float)panelWidth, (float)panelHeight}, 2, BLACK);
    DrawText("DEBUG OPTIONS", panelX + 10, panelY + 10, 16, YELLOW);
    if (ToggleButton((Rectangle){(float)(panelX + 10), (float)buttonY, (float)buttonWidth, 30}, 
                     "Bone Names", app->debug.showBoneNames, GREEN, DARKGRAY)) {
        app->debug.showBoneNames = !app->debug.showBoneNames;
    }
    buttonY += 35;
    if (ToggleButton((Rectangle){(float)(panelX + 10), (float)buttonY, (float)buttonWidth, 30}, 
                     "Debug Spheres", app->debug.showDebugSpheres, GREEN, DARKGRAY)) {
        app->debug.showDebugSpheres = !app->debug.showDebugSpheres;
    }
    buttonY += 35;
    if (ToggleButton((Rectangle){(float)(panelX + 10), (float)buttonY, (float)buttonWidth, 30}, 
                     "Connections", app->debug.showConnections, GREEN, DARKGRAY)) {
        app->debug.showConnections = !app->debug.showConnections;
    }
    buttonY += 35;
    if (ToggleButton((Rectangle){(float)(panelX + 10), (float)buttonY, (float)buttonWidth, 30}, 
                     "Orientation", app->debug.showOrientation, GREEN, DARKGRAY)) {
        app->debug.showOrientation = !app->debug.showOrientation;
    }
}

// ============================================================================
// DEBUG VISUALIZATION FUNCTIONS
// ============================================================================

static void DrawBoneOrientation(AppState* app) {
    if (!app->debug.showOrientation || !app->character->animation.isLoaded) return;
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
    const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
    typedef struct {
        char name[MAX_BONE_NAME_LENGTH];
        Vector3 position;
        BoneOrientation orientation;
    } UniqueBone;
    UniqueBone uniqueBones[MAX_BONES_PER_PERSON * MAX_PERSONS];
    int uniqueCount = 0;
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid) continue;
            bool exists = false;
            for (int u = 0; u < uniqueCount; u++) {
                if (strncmp(uniqueBones[u].name, bone->name, MAX_BONE_NAME_LENGTH) == 0) { 
                    exists = true; 
                    break; 
                }
            }
            if (!exists && uniqueCount < (MAX_BONES_PER_PERSON * MAX_PERSONS)) {
                strncpy(uniqueBones[uniqueCount].name, bone->name, MAX_BONE_NAME_LENGTH - 1);
                uniqueBones[uniqueCount].name[MAX_BONE_NAME_LENGTH - 1] = '\0';
                uniqueBones[uniqueCount].position = bone->position.position;
                uniqueBones[uniqueCount].orientation = CalculateBoneOrientation(bone->name, person, bone->position.position);
                uniqueCount++;
            }
        }
    }
    BeginMode3D(app->character->renderer->camera);
    for (int i = 0; i < uniqueCount; i++) {
        if (!uniqueBones[i].orientation.valid) continue;
        Vector3 pos = uniqueBones[i].position;
        BoneOrientation orient = uniqueBones[i].orientation;
        Vector3 forward = Vector3Scale(orient.forward, AXIS_LENGTH);
        Vector3 right = Vector3Scale(orient.right, AXIS_LENGTH);
        Vector3 up = Vector3Scale(orient.up, AXIS_LENGTH);
        DrawLine3D(pos, Vector3Add(pos, right), RED); 
        DrawSphere(Vector3Add(pos, right), 0.0035f, RED);
        DrawLine3D(pos, Vector3Add(pos, up), GREEN); 
        DrawSphere(Vector3Add(pos, up), 0.0035f, GREEN);
        DrawLine3D(pos, Vector3Add(pos, forward), BLUE); 
        DrawSphere(Vector3Add(pos, forward), 0.0035f, BLUE);
        DrawSphere(pos, 0.005f, YELLOW);
    }
    EndMode3D();
}

static void DrawDebugVisuals(AppState* app) {
    if (!app->character->animation.isLoaded) return;
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
    const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
    typedef struct {
        char name[64];
        Vector3 worldPos;
        Vector2 screenPos;
    } DrawnBone;
    static DrawnBone drawnBones[MAX_BONES_PER_PERSON * MAX_PERSONS];
    int drawnCount = 0;
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid) continue;
            bool alreadyDrawn = false;
            for (int d = 0; d < drawnCount; d++) {
                if (strcmp(drawnBones[d].name, bone->name) == 0) {
                    alreadyDrawn = true;
                    break;
                }
            }
            if (!alreadyDrawn && drawnCount < MAX_BONES_PER_PERSON * MAX_PERSONS) {
                Vector2 screenPos = GetWorldToScreen(bone->position.position, 
                                                     app->character->renderer->camera);
                if (screenPos.x >= 0 && screenPos.x < app->screenWidth &&
                    screenPos.y >= 0 && screenPos.y < app->screenHeight) {
                    strncpy(drawnBones[drawnCount].name, bone->name, 63);
                    drawnBones[drawnCount].name[63] = '\0';
                    drawnBones[drawnCount].worldPos = bone->position.position;
                    drawnBones[drawnCount].screenPos = screenPos;
                    drawnCount++;
                }
            }
        }
    }
    if (app->debug.showDebugSpheres && drawnCount > 0) {
        BeginMode3D(app->character->renderer->camera);
        for (int i = 0; i < drawnCount; i++) {
            DrawSphere(drawnBones[i].worldPos, 0.028f, (Color){80, 160, 255, 140});
            DrawSphereWires(drawnBones[i].worldPos, 0.031f, 8, 8, (Color){235, 235, 235, 255});
        }
        EndMode3D();
    }
    if (app->debug.showConnections && drawnCount > 0) {
        const char* connections[][2] = {
            {"Neck", "LShoulder"}, {"Neck", "RShoulder"},
            {"LShoulder", "LElbow"}, {"LElbow", "LWrist"},
            {"RShoulder", "RElbow"}, {"RElbow", "RWrist"},
            {"Neck", "LHip"}, {"Neck", "RHip"},
            {"LHip", "LKnee"}, {"LKnee", "LAnkle"},
            {"RHip", "RKnee"}, {"RKnee", "RAnkle"},
            {"LHip", "RHip"}, {"LShoulder", "RShoulder"},
            {NULL, NULL}
        };
        BeginMode3D(app->character->renderer->camera);
        for (int c = 0; connections[c][0] != NULL; c++) {
            Vector3 pos1 = {0};
            Vector3 pos2 = {0};
            bool found1 = false, found2 = false;
            for (int i = 0; i < drawnCount; i++) {
                if (strcmp(drawnBones[i].name, connections[c][0]) == 0) {
                    pos1 = drawnBones[i].worldPos;
                    found1 = true;
                }
                if (strcmp(drawnBones[i].name, connections[c][1]) == 0) {
                    pos2 = drawnBones[i].worldPos;
                    found2 = true;
                }
                if (found1 && found2) break;
            }
            if (found1 && found2 && Vector3Length(pos1) > 0.01f && Vector3Length(pos2) > 0.01f) {
                DrawLine3D(pos1, pos2, LIME);
            }
        }
        EndMode3D();
    }
    if (app->debug.showBoneNames && drawnCount > 0) {
        for (int i = 0; i < drawnCount; i++) {
            int textWidth = MeasureText(drawnBones[i].name, 10);
            DrawRectangle((int)drawnBones[i].screenPos.x - 2, 
                         (int)drawnBones[i].screenPos.y - 22, 
                         textWidth + 4, 14, 
                         (Color){0, 0, 0, 180});
            DrawText(drawnBones[i].name, 
                    (int)drawnBones[i].screenPos.x, 
                    (int)drawnBones[i].screenPos.y - 20, 
                    10, YELLOW);
        }
    }
}

// ============================================================================
// EXPORT FUNCTIONS
// ============================================================================

static const Person* GetPrimaryPerson(const AnimationFrame* frame) {
    if (!frame) return NULL;
    for (int p = 0; p < frame->personCount; p++) {
        if (frame->persons[p].active) return &frame->persons[p];
    }
    return NULL;
}

bool BonesExportToJSON(BonesAnimation* animation, const char* filepath, int startIdx, int endIdx) {
    if (!animation || !filepath || startIdx < 0 || endIdx < 0 || startIdx >= animation->frameCount) {
        return false;
    }
    if (endIdx >= animation->frameCount) endIdx = animation->frameCount - 1;
    if (startIdx > endIdx) {
        return false;
    }
    FILE* file = fopen(filepath, "w");
    if (!file) {
        return false;
    }
    fprintf(file, "{\n");
    bool firstPrinted = true;
    int exportedCount = 0;
    for (int idx = startIdx; idx <= endIdx; idx++) {
        AnimationFrame* frame = &animation->frames[idx];
        if (!frame->valid) continue;
        if (!frame->isOriginalKeyframe) continue;
        const Person* person = GetPrimaryPerson(frame);
        if (!person) continue;
        if (!firstPrinted) fprintf(file, ",\n");
        firstPrinted = false;
        fprintf(file, "  \"frame_%04d\": {\n", frame->frameNumber);
        fprintf(file, "    \"person_0\": {\n");
        bool firstBone = true;
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid) continue;
            if (!firstBone) fprintf(file, ",\n");
            firstBone = false;
            double x_transformed = (double)bone->position.position.x;
            double y_transformed = (double)bone->position.position.y;
            double z_transformed = (double)bone->position.position.z;
            double x_original = (x_transformed + 1.0) * 0.5;
            double y_original = 1.0 - y_transformed;
            double z_original = (z_transformed + 1.0) * 0.5;
            fprintf(file, "      \"%s\": {\"x\": %.17g, \"y\": %.17g, \"z\": %.17g}",
                    bone->name, x_original, y_original, z_original);
        }
        fprintf(file, "\n    }\n  }");
        exportedCount++;
    }
    fprintf(file, "\n}\n");
    fclose(file);
    return exportedCount > 0;
}

static void DrawExportDialog(AppState* app) {
    if (!app->editor.showExportDialog) return;
    int dialogW = 500;
    int dialogH = 200;
    int dialogX = (app->screenWidth - dialogW) / 2;
    int dialogY = (app->screenHeight - dialogH) / 2;
    DrawRectangle(0, 0, app->screenWidth, app->screenHeight, (Color){0, 0, 0, 150});
    DrawRectangle(dialogX, dialogY, dialogW, dialogH, RAYWHITE);
    DrawRectangleLinesEx((Rectangle){(float)dialogX, (float)dialogY, (float)dialogW, (float)dialogH}, 3, BLACK);
    DrawText("Export Animation", dialogX + 20, dialogY + 20, 20, BLACK);
    DrawText("Export Path:", dialogX + 20, dialogY + 60, 16, DARKGRAY);
    DrawTextField((Rectangle){(float)(dialogX + 20), (float)(dialogY + 80), (float)(dialogW - 40), 30},
        app->editor.exportPath, true);
    if (Button((Rectangle){(float)(dialogX + 20), (float)(dialogY + 140), 100, 40}, "EXPORT", GREEN)) {
        if (strlen(app->editor.exportPath) > 0) {
            bool hasSelection = (app->editor.selectionStart != -1 && app->editor.selectionEnd != -1 && 
                                app->editor.selectionStart != app->editor.selectionEnd);
            int startFrame = hasSelection ? app->editor.selectionStart : 0;
            int endFrame = hasSelection ? app->editor.selectionEnd :
                (app->character && app->character->animation.frameCount > 0 ? app->character->animation.frameCount - 1 : 0);
            if (startFrame < 0) startFrame = 0;
            if (endFrame < 0) endFrame = 0;
            if (app->character && app->character->animation.frameCount > 0) {
                if (startFrame >= app->character->animation.frameCount) startFrame = app->character->animation.frameCount - 1;
                if (endFrame >= app->character->animation.frameCount) endFrame = app->character->animation.frameCount - 1;
            }
            if (BonesExportToJSON(&app->character->animation,
                                  app->editor.exportPath,
                                  startFrame,
                                  endFrame)) {
                app->editor.needsSave = false;
            }
            app->editor.showExportDialog = false;
        }
    }
    if (Button((Rectangle){(float)(dialogX + 140), (float)(dialogY + 140), 100, 40}, "CANCEL", RED)) {
        app->editor.showExportDialog = false;
    }
    if (Button((Rectangle){(float)(dialogX + dialogW - 120), (float)(dialogY + 140), 100, 40}, "BROWSE", BLUE)) {
        strcpy(app->editor.exportPath, "data/poses/exported.json");
    }
}

// ============================================================================
// MAIN UI DRAWING
// ============================================================================

static void App_DrawUI(AppState* app) {
    if (!app->showUI) return;
    int maxFrameNumber = FindMaxFrameNumber(app);
    int existingFrames = app->character->animation.frameCount;
    int currentFrameNumber = GetCurrentFrameNumber(app);
    DrawText("BONES3D ANIMATION EDITOR", 10, 10, 20, BLUE);
    DrawText("SPACE: Play/Pause | LEFT/RIGHT: Frame | Ctrl+Z: Undo | Ctrl+Y: Redo", 10, 35, 14, DARKGRAY);
    DrawText("1: Orbit | 2: FPS | 3-6: Load Anims | H/T: Billboards | F1: Toggle UI", 10, 52, 14, DARKGRAY);
    DrawText("LEFT CLICK: Select | RIGHT CLICK: Move | CTRL+LEFT (timeline): Drag keyframe", 10, 69, 14, DARKGRAY);
    char frameText[128];
    snprintf(frameText, sizeof(frameText), "Animation: %s | Frame: %d/%d (%d existing) %s %s", 
             app->currentAnimation, 
             currentFrameNumber, 
             maxFrameNumber,
             existingFrames,
             app->editor.isPlaying ? "[PLAYING]" : "[PAUSED]",
             app->editor.needsSave ? "[*]" : "");
    DrawText(frameText, 10, 89, 16, app->editor.needsSave ? ORANGE : DARKGRAY);
    if (app->editor.isDraggingKeyframe) {
        DrawText("[DRAGGING KEYFRAME] Release to confirm", 10, 106, 16, PURPLE);
    } else if (app->editor.hasBoneSelected) {
        bool isKeyframe = IsCurrentFrameKeyframe(app);
        char selectionText[256];
        if (app->editor.isDraggingBone) {
            snprintf(selectionText, sizeof(selectionText), "[DRAGGING BONE] %s | Live preview", 
                    app->editor.selectedBoneName);
            DrawText(selectionText, 10, 106, 16, RED);
        } else if (isKeyframe) {
            snprintf(selectionText, sizeof(selectionText), "[KEYFRAME] %s | Right-click: move", 
                    app->editor.selectedBoneName);
            DrawText(selectionText, 10, 106, 16, GREEN);
        } else {
            snprintf(selectionText, sizeof(selectionText), "[INTERPOLATED] %s | Right-click: convert & move", 
                    app->editor.selectedBoneName);
            DrawText(selectionText, 10, 106, 16, ORANGE);
        }
    }
}

static void App_Draw(AppState* app) {
    if (!app) return;
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawAnimatedCharacter(app->character, app->character->renderer->camera);
    DrawBoneOrientation(app);
    DrawDebugVisuals(app);
    DrawBoneSelectionFeedback(app);
    DrawCameraGizmo(app);
    App_DrawUI(app);
    DrawDebugPanel(app);
    DrawTimeline(app);
    DrawControlPanel(app);
    DrawExportDialog(app);
    EndDrawing();
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

static void App_HandleInput(AppState* app) {
    if (!app) return;
    int maxFrameNumber = FindMaxFrameNumber(app);
    int currentFrameNumber = GetCurrentFrameNumber(app);
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z)) {
        PerformUndo(app);
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y)) {
        PerformRedo(app);
    }
    if (IsKeyPressed(KEY_SPACE)) {
        app->editor.isPlaying = !app->editor.isPlaying;
        SetCharacterAutoPlay(app->character, app->editor.isPlaying);
    }
    if (IsKeyPressed(KEY_LEFT)) {
        for (int i = currentFrameNumber - 1; i >= 0; i--) {
            if (FrameExists(app, i)) {
                int frameIndex = FindFrameIndexByNumber(app, i);
                if (frameIndex != -1) {
                    SetCharacterFrame(app->character, frameIndex);
                }
                break;
            }
        }
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        for (int i = currentFrameNumber + 1; i <= maxFrameNumber; i++) {
            if (FrameExists(app, i)) {
                int frameIndex = FindFrameIndexByNumber(app, i);
                if (frameIndex != -1) {
                    SetCharacterFrame(app->character, frameIndex);
                }
                break;
            }
        }
    }
    if (IsKeyPressed(KEY_HOME)) {
        for (int i = 0; i <= maxFrameNumber; i++) {
            if (FrameExists(app, i)) {
                int frameIndex = FindFrameIndexByNumber(app, i);
                if (frameIndex != -1) {
                    SetCharacterFrame(app->character, frameIndex);
                }
                break;
            }
        }
    }
    if (IsKeyPressed(KEY_END)) {
        for (int i = maxFrameNumber; i >= 0; i--) {
            if (FrameExists(app, i)) {
                int frameIndex = FindFrameIndexByNumber(app, i);
                if (frameIndex != -1) {
                    SetCharacterFrame(app->character, frameIndex);
                }
                break;
            }
        }
    }
    if (IsKeyPressed(KEY_ONE)) {
        app->camMode = 1;
        EnableCursor();
    }
    if (IsKeyPressed(KEY_TWO)) {
        app->camMode = 2;
        DisableCursor();
    }
    if (IsKeyPressed(KEY_THREE)) {
        LoadAnimation(app->character, "data/poses/idle.json", "data/animations/idle.anim");
        strcpy(app->currentAnimation, "idle");
        InitUndoHistory(&app->editor.undoHistory);
    }
    if (IsKeyPressed(KEY_FOUR)) {
        LoadAnimation(app->character, "data/poses/talk.json", "data/animations/talk.anim");
        strcpy(app->currentAnimation, "talk");
        InitUndoHistory(&app->editor.undoHistory);
    }
    if (IsKeyPressed(KEY_FIVE)) {
        LoadAnimation(app->character, "data/poses/walk.json", "data/animations/walk.anim");
        strcpy(app->currentAnimation, "walk");
        InitUndoHistory(&app->editor.undoHistory);
    }
    if (IsKeyPressed(KEY_SIX)) {
        LoadAnimation(app->character, "data/poses/jump.json", "data/animations/jump.anim");
        strcpy(app->currentAnimation, "jump");
        InitUndoHistory(&app->editor.undoHistory);
    }
    if (IsKeyPressed(KEY_H)) {
        SetCharacterBillboards(app->character, 
                              !app->character->renderHeadBillboards, 
                              app->character->renderTorsoBillboards);
    }
    if (IsKeyPressed(KEY_T)) {
        SetCharacterBillboards(app->character, 
                              app->character->renderHeadBillboards,
                              !app->character->renderTorsoBillboards);
    }
    if (IsKeyPressed(KEY_F1)) {
        app->showUI = !app->showUI;
        app->editor.showTimeline = app->showUI;
    }
    if (IsKeyPressed(KEY_F2)) {
        app->debug.showDebugSpheres = !app->debug.showDebugSpheres;
    }
    if (IsKeyPressed(KEY_F3)) {
        app->debug.showBoneNames = !app->debug.showBoneNames;
    }
    if (IsKeyPressed(KEY_F4)) {
        app->debug.showConnections = !app->debug.showConnections;
    }
    if (IsKeyPressed(KEY_F5)) {
        app->debug.showOrientation = !app->debug.showOrientation;
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
        app->editor.showExportDialog = true;
    }
    if (IsKeyPressed(KEY_DELETE) && app->editor.selectionStart != -1 && FrameExists(app, app->editor.selectionStart)) {
        int frameIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
        if (frameIndex != -1) {
            int deletedFrameNumber = app->editor.selectionStart;
            BonesDeleteFrame(&app->character->animation, frameIndex);
            app->character->maxFrames = app->character->animation.frameCount;
            app->editor.needsSave = true;
            int nextValidFrame = -1;
            for (int i = deletedFrameNumber; i <= maxFrameNumber; i++) {
                if (FrameExists(app, i)) {
                    nextValidFrame = i;
                    break;
                }
            }
            if (nextValidFrame == -1) {
                for (int i = deletedFrameNumber - 1; i >= 0; i--) {
                    if (FrameExists(app, i)) {
                        nextValidFrame = i;
                        break;
                    }
                }
            }
            if (nextValidFrame != -1) {
                int newIndex = FindFrameIndexByNumber(app, nextValidFrame);
                if (newIndex != -1) {
                    SetCharacterFrame(app->character, newIndex);
                    app->editor.selectionStart = nextValidFrame;
                    app->editor.selectionEnd = nextValidFrame;
                }
            } else {
                app->editor.selectionStart = -1;
                app->editor.selectionEnd = -1;
            }
        }
    }
}

// ============================================================================
// CAMERA UPDATE
// ============================================================================

static void App_UpdateCamera(AppState* app, float dt) {
    if (!app || !app->character) return;
    Vector3 cameraTarget = app->character->autoCenterCalculated ? 
                          app->character->autoCenter : (Vector3){0, 0.6f, 0};
    if (app->camMode == 1) {
        UpdateCameraGizmo(app);
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && 
            !app->editor.isDraggingSlider && 
            !app->editor.isDraggingKeyframe &&
            !app->editor.isDraggingGizmo &&
            !CheckCollisionPointRec(GetMousePosition(), GetTimelineRect(app)) &&
            !CheckCollisionPointRec(GetMousePosition(), GetGizmoRect(app))) {
            Vector2 mouseDelta = GetMouseDelta();
            app->orbitYaw += mouseDelta.x * ORBIT_SENSITIVITY;
            app->orbitPitch = Clamp(app->orbitPitch - mouseDelta.y * ORBIT_SENSITIVITY, 
                                   MIN_PITCH, MAX_PITCH);
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f && !CheckCollisionPointRec(GetMousePosition(), GetTimelineRect(app))) {
            app->orbitRadius = Clamp(app->orbitRadius - wheel * ZOOM_SENSITIVITY, 
                                    MIN_ORBIT_RADIUS, MAX_ORBIT_RADIUS);
        }
        float cosP = cosf(app->orbitPitch);
        float sinP = sinf(app->orbitPitch);
        float cosY = cosf(app->orbitYaw);
        float sinY = sinf(app->orbitYaw);
        app->character->renderer->camera.position = (Vector3){
            cameraTarget.x + app->orbitRadius * cosP * sinY,
            cameraTarget.y + app->orbitRadius * sinP,
            cameraTarget.z + app->orbitRadius * cosP * cosY
        };
        app->character->renderer->camera.target = cameraTarget;
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
        Vector3 right_dir = Vector3Normalize(Vector3CrossProduct(forward, 
                                app->character->renderer->camera.up));
        float speed = BASE_SPEED * dt;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
            speed *= MOVEMENT_SPEED;
        if (IsKeyDown(KEY_W))
            app->character->renderer->camera.position = 
                Vector3Add(app->character->renderer->camera.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_S))
            app->character->renderer->camera.position = 
                Vector3Subtract(app->character->renderer->camera.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_D))
            app->character->renderer->camera.position = 
                Vector3Add(app->character->renderer->camera.position, Vector3Scale(right_dir, speed));
        if (IsKeyDown(KEY_A))
            app->character->renderer->camera.position = 
                Vector3Subtract(app->character->renderer->camera.position, Vector3Scale(right_dir, speed));
        app->character->renderer->camera.target = 
            Vector3Add(app->character->renderer->camera.position, forward);
    }
    UpdateTimelineKeyframeDragging(app);
    if (!app->editor.isDraggingKeyframe) {
        UpdateBoneSelection(app);
    }
}

// ============================================================================
// INITIALIZATION AND SHUTDOWN
// ============================================================================

static bool App_Init(AppState* app) {
    if (!app) return false;
    memset(app, 0, sizeof(*app));
    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - Animation Editor with Camera Gizmo");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    #if defined(__linux__)
    for (int i = 0; i < 5; i++) PollInputEvents();
    #endif
    MaximizeWindow();
    SetTargetFPS(120);
    app->character = CreateAnimatedCharacter("data/textures/bone_textures.txt", 
                                           "data/textures/texture_sets.txt");
    if (!app->character) {
        CloseWindow();
        return false;
    }
    if (LoadAnimation(app->character, "data/poses/idle.json", "data/animations/idle.anim")) {
        strcpy(app->currentAnimation, "idle");
    }
    app->camMode = 1;
    app->orbitRadius = 2.5f;
    app->orbitPitch = -0.2f;
    app->showUI = true;
    app->editor.showTimeline = true;
    app->editor.isPlaying = true;
    app->editor.selectedFrame = 0;
    app->editor.selectionStart = -1;
    app->editor.selectionEnd = -1;
    app->editor.interpolationCount = 5;
    app->editor.playbackSpeed = 1.0f;
    app->editor.isDraggingKeyframe = false;
    app->editor.draggedKeyframeNumber = -1;
    app->editor.isDraggingGizmo = false;
    strcpy(app->editor.exportPath, "data/poses/exported.json");
    InitUndoHistory(&app->editor.undoHistory);
    app->debug.showBoneNames = false;
    app->debug.showDebugSpheres = false;
    app->debug.showConnections = false;
    app->debug.showOrientation = false;
    app->screenWidth = GetScreenWidth();
    app->screenHeight = GetScreenHeight();
    return true;
}

static void App_Shutdown(AppState* app) {
    if (!app) return;
    DestroyAnimatedCharacter(app->character);
    CloseWindow();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

int main(void) {
    AppState app;
    if (!App_Init(&app)) return -1;
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        app.screenWidth = GetScreenWidth();
        app.screenHeight = GetScreenHeight();
        App_HandleInput(&app);
        App_UpdateCamera(&app, dt);
        UpdateAnimatedCharacter(app.character, dt);
        App_Draw(&app);
    }
    App_Shutdown(&app);
    return 0;
}