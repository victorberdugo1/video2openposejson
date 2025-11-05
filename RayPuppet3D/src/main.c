#include "bones_core.h"

#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080
#define UI_HEIGHT 200
#define TIMELINE_HEIGHT 100
#define BUTTON_SIZE 35
#define TIMELINE_MARGIN 60

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

typedef enum {
    TOOL_NONE,
    TOOL_SELECT,
    TOOL_DELETE,
    TOOL_DUPLICATE,
    TOOL_INTERPOLATE
} EditorTool;

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

    // Sistema de selección de huesos
    bool hasBoneSelected;
    char selectedBoneName[64];
    int selectedBonePersonIndex;
    Vector3 selectedBonePosition;
    bool isDraggingBone;
    Vector3 dragStartPos;
    Vector2 dragStartMouse;
} EditorState;

typedef struct {
    bool showBoneNames;
    bool showDebugSpheres;
    bool showConnections;
    bool showOrientation;  // Toggle para mostrar ejes de orientación
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

// ============================================================================
// FUNCIONES DE SELECCIÓN Y MOVIMIENTO DE HUESOS
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
// Encontrar el keyframe anterior más cercano
static int FindPreviousKeyframe(AppState* app, int fromFrame) {
    for (int i = fromFrame - 1; i >= 0; i--) {
        int frameIndex = FindFrameIndexByNumber(app, i);
        if (frameIndex != -1 && app->character->animation.frames[frameIndex].isOriginalKeyframe) {
            return i;
        }
    }
    return -1;
}

// Encontrar el siguiente keyframe más cercano
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

// Recalcular frames interpolados entre dos keyframes
static void RecalculateInterpolatedFrames(AppState* app, int keyframeA, int keyframeB) {
    if (keyframeA < 0 || keyframeB < 0 || keyframeB <= keyframeA) return;
    
    int indexA = FindFrameIndexByNumber(app, keyframeA);
    int indexB = FindFrameIndexByNumber(app, keyframeB);
    
    if (indexA == -1 || indexB == -1) return;
    
    AnimationFrame* frameA = &app->character->animation.frames[indexA];
    AnimationFrame* frameB = &app->character->animation.frames[indexB];
    
    // Recalcular cada frame interpolado entre A y B
    for (int frameNum = keyframeA + 1; frameNum < keyframeB; frameNum++) {
        int interpIndex = FindFrameIndexByNumber(app, frameNum);
        if (interpIndex == -1) continue;
        
        AnimationFrame* interpFrame = &app->character->animation.frames[interpIndex];
        
        // Solo recalcular si es un frame interpolado
        if (!interpFrame->isOriginalKeyframe) {
            float t = (float)(frameNum - keyframeA) / (float)(keyframeB - keyframeA);
            
            // Interpolar cada persona
            for (int p = 0; p < frameA->personCount && p < frameB->personCount; p++) {
                Person* personA = &frameA->persons[p];
                Person* personB = &frameB->persons[p];
                Person* interpPerson = &interpFrame->persons[p];
                
                if (!personA->active || !personB->active) continue;
                
                // Interpolar cada hueso
                for (int bA = 0; bA < personA->boneCount; bA++) {
                    Bone* boneA = &personA->bones[bA];
                    if (!boneA->position.valid) continue;
                    
                    // Buscar el hueso correspondiente en frameB
                    for (int bB = 0; bB < personB->boneCount; bB++) {
                        Bone* boneB = &personB->bones[bB];
                        if (strcmp(boneA->name, boneB->name) == 0 && boneB->position.valid) {
                            
                            // Buscar el hueso en el frame interpolado
                            for (int bI = 0; bI < interpPerson->boneCount; bI++) {
                                Bone* boneInterp = &interpPerson->bones[bI];
                                if (strcmp(boneInterp->name, boneA->name) == 0) {
                                    
                                    // Interpolar posición
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

// Recalcular todas las interpolaciones afectadas por el movimiento de un keyframe
static void RecalculateAffectedInterpolations(AppState* app, int movedKeyframe) {
    // Buscar el keyframe anterior
    int prevKeyframe = FindPreviousKeyframe(app, movedKeyframe);
    // Buscar el siguiente keyframe
    int nextKeyframe = FindNextKeyframe(app, movedKeyframe);
    
    // Recalcular interpolaciones entre previo y actual
    if (prevKeyframe != -1) {
        RecalculateInterpolatedFrames(app, prevKeyframe, movedKeyframe);
        TraceLog(LOG_INFO, "Recalculated interpolations between frames %d and %d", 
                 prevKeyframe, movedKeyframe);
    }
    
    // Recalcular interpolaciones entre actual y siguiente
    if (nextKeyframe != -1) {
        RecalculateInterpolatedFrames(app, movedKeyframe, nextKeyframe);
        TraceLog(LOG_INFO, "Recalculated interpolations between frames %d and %d", 
                 movedKeyframe, nextKeyframe);
    }
}
// Verificar si el frame actual es un keyframe
static bool IsCurrentFrameKeyframe(AppState* app) {
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return false;
    
    return app->character->animation.frames[currentFrame].isOriginalKeyframe;
}
// Encontrar el hueso más cercano al rayo del mouse
static bool FindBoneUnderMouse(AppState* app, char* outBoneName, int* outPersonIndex, Vector3* outBonePos) {
    if (!app->character->animation.isLoaded) return false;
    
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return false;
    
    const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
    Camera camera = app->character->renderer->camera;
    Vector2 mousePos = GetMousePosition();
    
    float closestDist = 50.0f; // Radio de detección en píxeles
    bool found = false;
    
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid) continue;
            
            // Proyectar el hueso a pantalla
            Vector2 screenPos = GetWorldToScreen(bone->position.position, camera);
            
            // Verificar si el mouse está cerca
            float dist = Vector2Distance(mousePos, screenPos);
            if (dist < closestDist) {
                closestDist = dist;
                strncpy(outBoneName, bone->name, 63);
                outBoneName[63] = '\0';
                *outPersonIndex = p;
                *outBonePos = bone->position.position;
                found = true;
            }
        }
    }
    
    return found;
}

// Calcular la intersección del rayo del mouse con un plano perpendicular a la cámara
static bool RayPlaneIntersection(Ray ray, Vector3 planePoint, Vector3 planeNormal, Vector3* outPoint) {
    float denom = Vector3DotProduct(planeNormal, ray.direction);
    
    if (fabsf(denom) < 0.0001f) return false; // Paralelo al plano
    
    Vector3 diff = Vector3Subtract(planePoint, ray.position);
    float t = Vector3DotProduct(diff, planeNormal) / denom;
    
    if (t < 0) return false; // Intersección detrás del rayo
    
    *outPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    return true;
}

static void MoveBoneWithMouse(AppState* app) {
    if (!app->editor.hasBoneSelected || !app->editor.isDraggingBone) return;
    if (!app->character->animation.isLoaded) return;
    
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
    
    AnimationFrame* frame = &app->character->animation.frames[currentFrame];
    Camera camera = app->character->renderer->camera;
    
    // Obtener el rayo del mouse actual
    Vector2 mousePos = GetMousePosition();
    Ray mouseRay = GetMouseRay(mousePos, camera);
    
    // Crear un plano perpendicular a la cámara que pasa por el hueso
    Vector3 camToPoint = Vector3Subtract(app->editor.selectedBonePosition, camera.position);
    Vector3 planeNormal = Vector3Normalize(camToPoint);
    
    // Calcular la intersección
    Vector3 newPosition;
    if (RayPlaneIntersection(mouseRay, app->editor.selectedBonePosition, planeNormal, &newPosition)) {
        // Actualizar la posición del hueso en el frame actual
        for (int p = 0; p < frame->personCount; p++) {
            Person* person = &frame->persons[p];
            if (!person->active) continue;
            
            for (int b = 0; b < person->boneCount; b++) {
                Bone* bone = &person->bones[b];
                if (strcmp(bone->name, app->editor.selectedBoneName) == 0) {
                    bone->position.position = newPosition;
                    app->editor.selectedBonePosition = newPosition;
                    app->editor.needsSave = true;
                    
                    // Forzar actualización del render
                    app->character->forceUpdate = true;
                    return;
                }
            }
        }
    }
}

static Rectangle GetTimelineRect(AppState* app) {
    return (Rectangle){
        TIMELINE_MARGIN,
        app->screenHeight - UI_HEIGHT + 50,
        app->screenWidth - TIMELINE_MARGIN * 2,
        40
    };
}

// Actualizar el estado de selección de huesos

static void UpdateBoneSelection(AppState* app) {
    if (app->camMode != 1) { // Solo en modo Orbit
        app->editor.hasBoneSelected = false;
        app->editor.isDraggingBone = false;
        return;
    }
    
    if (app->editor.isDraggingSlider) return; // No interferir con el timeline
    
    Vector2 mousePos = GetMousePosition();
    Rectangle timeline = GetTimelineRect(app);
    
    // Click izquierdo para seleccionar
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, timeline)) {
        char boneName[64];
        int personIndex;
        Vector3 bonePos;
        
        if (FindBoneUnderMouse(app, boneName, &personIndex, &bonePos)) {
            app->editor.hasBoneSelected = true;
            strncpy(app->editor.selectedBoneName, boneName, 63);
            app->editor.selectedBoneName[63] = '\0';
            app->editor.selectedBonePersonIndex = personIndex;
            app->editor.selectedBonePosition = bonePos;
        } else {
            app->editor.hasBoneSelected = false;
        }
    }
    
    // Click derecho para arrastrar
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && app->editor.hasBoneSelected) {
        if (!CheckCollisionPointRec(mousePos, timeline)) {
            char boneName[64];
            int personIndex;
            Vector3 bonePos;
            
            if (FindBoneUnderMouse(app, boneName, &personIndex, &bonePos)) {
                if (strcmp(boneName, app->editor.selectedBoneName) == 0) {
                    // Verificar si es un keyframe antes de permitir el movimiento
                    if (!IsCurrentFrameKeyframe(app)) {
                        TraceLog(LOG_WARNING, "Cannot move bones in interpolated frame %d", 
                                GetCurrentFrameNumber(app));
                        return;
                    }
                    
                    app->editor.isDraggingBone = true;
                    app->editor.dragStartPos = bonePos;
                    app->editor.dragStartMouse = mousePos;
                }
            }
        }
    }
    
    // Mover hueso mientras se arrastra
    if (app->editor.isDraggingBone && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        MoveBoneWithMouse(app);
    }
    
    // Soltar al liberar el botón
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && app->editor.isDraggingBone) {
        app->editor.isDraggingBone = false;
        
        // IMPORTANTE: Recalcular interpolaciones afectadas
        int currentFrameNumber = GetCurrentFrameNumber(app);
        RecalculateAffectedInterpolations(app, currentFrameNumber);
        
        TraceLog(LOG_INFO, "Bone movement completed, interpolations recalculated");
    }
}

// Dibujar feedback visual para el hueso seleccionado
static void DrawBoneSelectionFeedback(AppState* app) {
    if (!app->editor.hasBoneSelected) return;
    if (!app->character->animation.isLoaded) return;
    
    int currentFrame = app->character->currentFrame;
    if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
    
    const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
    Camera camera = app->character->renderer->camera;
    
    // Encontrar el hueso seleccionado
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (strcmp(bone->name, app->editor.selectedBoneName) == 0 && bone->position.valid) {
                Vector2 screenPos = GetWorldToScreen(bone->position.position, camera);
                
                // Dibujar esfera destacada en 3D
                BeginMode3D(camera);
                Color highlightColor = app->editor.isDraggingBone ? RED : ORANGE;
                DrawSphere(bone->position.position, 0.04f, highlightColor);
                DrawSphereWires(bone->position.position, 0.042f, 8, 8, WHITE);
                EndMode3D();
                
                // Dibujar nombre con fondo destacado
                int textWidth = MeasureText(bone->name, 12);
                DrawRectangle((int)screenPos.x - 2, (int)screenPos.y - 25, 
                            textWidth + 4, 16, highlightColor);
                DrawText(bone->name, (int)screenPos.x, (int)screenPos.y - 23, 12, WHITE);
                
                return;
            }
        }
    }
}


// Función para verificar si un frame número existe
static bool FrameExists(AppState* app, int frameNumber) {
    for (int i = 0; i < app->character->animation.frameCount; i++) {
        if (app->character->animation.frames[i].frameNumber == frameNumber) {
            return true;
        }
    }
    return false;
}





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
        
        Color frameColor;
        if (!frameExists) {
            frameColor = (Color){30, 30, 30, 255};
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
    // Verificar si el frame es interpolado
    int frameIndex = FindFrameIndexByNumber(app, i);
    bool isInterpolated = false;
    
    if (frameIndex != -1 && frameIndex < app->character->animation.frameCount) {
        isInterpolated = !app->character->animation.frames[frameIndex].isOriginalKeyframe;
    }
    
    // Color diferente según si es keyframe original o interpolado
    Color indicatorColor = isInterpolated ? BLUE : GREEN;
    
    DrawRectangle((int)(x + 2), (int)(timeline.y + timeline.height - 8), 
                 (int)(frameWidth - 4), 4, indicatorColor);
}
    }
    
    float markerX = timeline.x + currentFrameNumber * frameWidth + frameWidth / 2;
    DrawLineEx((Vector2){markerX, timeline.y}, 
               (Vector2){markerX, timeline.y + timeline.height}, 
               3, RED);
    DrawCircle((int)markerX, (int)(timeline.y + timeline.height + 5), 6, RED);
    
    if (CheckCollisionPointRec(GetMousePosition(), timeline)) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
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

static void DrawControlPanel(AppState* app) {
    if (!app->editor.showTimeline) return;
    
    int panelY = app->screenHeight - UI_HEIGHT + 100;
    int buttonX = 10;
    
    int maxFrameNumber = FindMaxFrameNumber(app);
    int currentFrameNumber = GetCurrentFrameNumber(app);
    
    // Botones de reproducción
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
    
    // Herramientas de edición
if (Button((Rectangle){(float)buttonX, (float)panelY, 80, BUTTON_SIZE}, 
           "DELETE", RED)) {
    if (app->editor.selectionStart != -1 && FrameExists(app, app->editor.selectionStart)) {
        int frameIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
        if (frameIndex != -1) {
            int deletedFrameNumber = app->editor.selectionStart;
            
            BonesDeleteFrame(&app->character->animation, frameIndex);
            app->character->maxFrames = app->character->animation.frameCount;
            app->editor.needsSave = true;
            
            // Buscar el siguiente frame válido desde el frame eliminado
            int nextValidFrame = -1;
            for (int i = deletedFrameNumber; i <= maxFrameNumber; i++) {
                if (FrameExists(app, i)) {
                    nextValidFrame = i;
                    break;
                }
            }
            
            // Si no hay siguiente, buscar el anterior
            if (nextValidFrame == -1) {
                for (int i = deletedFrameNumber - 1; i >= 0; i--) {
                    if (FrameExists(app, i)) {
                        nextValidFrame = i;
                        break;
                    }
                }
            }
            
            // Mover a ese frame y mantener la selección
            if (nextValidFrame != -1) {
                int newIndex = FindFrameIndexByNumber(app, nextValidFrame);
                if (newIndex != -1) {
                    SetCharacterFrame(app->character, newIndex);
                    app->editor.selectionStart = nextValidFrame;
                    app->editor.selectionEnd = nextValidFrame;
                }
            } else {
                // No quedan frames
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
    snprintf(infoText, sizeof(infoText), "Frame: %d/%d | Selected: %d-%d | Existing: %d/%d", 
             currentFrameNumber, 
             maxFrameNumber,
             app->editor.selectionStart,
             app->editor.selectionEnd,
             existingFrames,
             totalFrames);
    DrawText(infoText, buttonX + 10, panelY + 10, 16, WHITE);
}

// Helpers usados por la exportación
static const Person* GetPrimaryPerson(const AnimationFrame* frame) {
	if (!frame) return NULL;
	for (int p = 0; p < frame->personCount; p++) {
		if (frame->persons[p].active) return &frame->persons[p];
	}
	return NULL;
}



bool BonesExportToJSON(BonesAnimation* animation, const char* filepath, int startIdx, int endIdx) {
    if (!animation || !filepath || startIdx < 0 || endIdx < 0 || startIdx >= animation->frameCount) {
        TraceLog(LOG_ERROR, "BONES: Invalid export parameters - startIdx: %d, endIdx: %d, frameCount: %d", 
                 startIdx, endIdx, animation->frameCount);
        return false;
    }
    if (endIdx >= animation->frameCount) endIdx = animation->frameCount - 1;
    if (startIdx > endIdx) {
        TraceLog(LOG_ERROR, "BONES: startIdx > endIdx (%d > %d)", startIdx, endIdx);
        return false;
    }

    FILE* file = fopen(filepath, "w");
    if (!file) {
        TraceLog(LOG_ERROR, "BONES: Failed to open file for export: %s", filepath);
        return false;
    }

    TraceLog(LOG_INFO, "BONES: Starting export with %d frames (indices %d to %d)", 
             animation->frameCount, startIdx, endIdx);

    fprintf(file, "{\n");
    bool firstPrinted = true;
    int exportedCount = 0;

    for (int idx = startIdx; idx <= endIdx; idx++) {
        AnimationFrame* frame = &animation->frames[idx];
        if (!frame->valid) continue;

        // Solo exportar keyframes originales
        if (!frame->isOriginalKeyframe) continue;

        const Person* person = GetPrimaryPerson(frame);
        if (!person) continue;

        if (!firstPrinted) fprintf(file, ",\n");
        firstPrinted = false;

        fprintf(file, "  \"frame_%04d\": {\n", frame->frameNumber);
        fprintf(file, "    \"person_0\": {\n");

        bool firstBone = true;
        int boneCount = 0;
        
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid) continue;
            if (!firstBone) fprintf(file, ",\n");
            firstBone = false;

            // REVERTIR LA TRANSFORMACIÓN PARA OBTENER COORDENADAS ORIGINALES
            float x_transformed = bone->position.position.x;
            float y_transformed = bone->position.position.y;
            float z_transformed = bone->position.position.z;

            // Aplicar transformación inversa
            float x_original = (x_transformed + 1.0f) * 0.5f;
            float y_original = 1.0f - y_transformed;
            float z_original = (z_transformed + 1.0f) * 0.5f;

            // DEBUG: Para verificar la transformación
            if (exportedCount == 0 && boneCount == 0 && strcmp(bone->name, "Nose") == 0) {
                TraceLog(LOG_INFO, "BONES: Transformation debug for Nose:");
                TraceLog(LOG_INFO, "  Transformed: (%.6f, %.6f, %.6f)", x_transformed, y_transformed, z_transformed);
                TraceLog(LOG_INFO, "  Original:    (%.6f, %.6f, %.6f)", x_original, y_original, z_original);
                TraceLog(LOG_INFO, "  Expected:    (%.6f, %.6f, %.6f)", 0.8440054059f, 0.2510614097f, 0.0346111879f);
            }

            fprintf(file, "      \"%s\": {\"x\": %.10f, \"y\": %.10f, \"z\": %.10f}",
                    bone->name, x_original, y_original, z_original);
            
            boneCount++;
        }

        fprintf(file, "\n    }\n  }");
        exportedCount++;
        
        TraceLog(LOG_DEBUG, "BONES: Exported frame %d with %d bones", frame->frameNumber, boneCount);
    }

    fprintf(file, "\n}\n");
    fclose(file);

    TraceLog(LOG_INFO, "BONES: Export completed - %d frames exported", exportedCount);

    if (exportedCount == 0) {
        TraceLog(LOG_WARNING, "BONES: No frames were exported!");
        return false;
    }

    return true;
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

	/* BOTÓN EXPORT */
	if (Button((Rectangle){(float)(dialogX + 20), (float)(dialogY + 140), 100, 40}, "EXPORT", GREEN)) {
		if (strlen(app->editor.exportPath) > 0) {
			/* Si la selección existe y start != end la usamos; en caso contrario exportamos todo */
			bool hasSelection = (app->editor.selectionStart != -1 && app->editor.selectionEnd != -1 && app->editor.selectionStart != app->editor.selectionEnd);
			int startFrame = hasSelection ? app->editor.selectionStart : 0;
			int endFrame = hasSelection ? app->editor.selectionEnd :
				(app->character && app->character->animation.frameCount > 0 ? app->character->animation.frameCount - 1 : 0);

			/* Clamp de seguridad */
			if (startFrame < 0) startFrame = 0;
			if (endFrame < 0) endFrame = 0;
			if (app->character && app->character->animation.frameCount > 0) {
				if (startFrame >= app->character->animation.frameCount) startFrame = app->character->animation.frameCount - 1;
				if (endFrame >= app->character->animation.frameCount) endFrame = app->character->animation.frameCount - 1;
			}

			/* DEBUG: mostrar lo que realmente vamos a pasar al exportador */
			TraceLog(LOG_DEBUG, "DEBUG: Export request: selectionStart=%d selectionEnd=%d hasSelection=%d -> start=%d end=%d currentFrame=%d framesTotal=%d",
				app->editor.selectionStart, app->editor.selectionEnd, hasSelection ? 1 : 0,
				startFrame, endFrame,
				app->character ? app->character->currentFrame : -1,
				app->character ? app->character->animation.frameCount : 0);

			if (BonesExportToJSON(&app->character->animation,
			                      app->editor.exportPath,
			                      startFrame,
			                      endFrame)) {
				TraceLog(LOG_INFO, "Animation exported successfully!");
				app->editor.needsSave = false;
			} else {
				TraceLog(LOG_ERROR, "Failed to export animation");
			}

			app->editor.showExportDialog = false;
		}
	}

	/* BOTÓN CANCEL */
	if (Button((Rectangle){(float)(dialogX + 140), (float)(dialogY + 140), 100, 40}, "CANCEL", RED)) {
		app->editor.showExportDialog = false;
	}

	/* BOTÓN BROWSE (rellena ejemplo de path) */
	if (Button((Rectangle){(float)(dialogX + dialogW - 120), (float)(dialogY + 140), 100, 40}, "BROWSE", BLUE)) {
		strcpy(app->editor.exportPath, "data/poses/exported.json");
	}
}

// ============================================================================
// FUNCIÓN PARA DIBUJAR ORIENTACIÓN DE LOS BONES
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
				if (strncmp(uniqueBones[u].name, bone->name, MAX_BONE_NAME_LENGTH) == 0) { exists = true; break; }
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

		DrawLine3D(pos, Vector3Add(pos, right), RED); DrawSphere(Vector3Add(pos, right), 0.0035f, RED);
		DrawLine3D(pos, Vector3Add(pos, up), GREEN); DrawSphere(Vector3Add(pos, up), 0.0035f, GREEN);
		DrawLine3D(pos, Vector3Add(pos, forward), BLUE); DrawSphere(Vector3Add(pos, forward), 0.0035f, BLUE);
		DrawSphere(pos, 0.005f, YELLOW);
	}

	EndMode3D();
}


// ============================================================================
// FUNCIÓN PARA DIBUJAR NOMBRES, BONES Y CONEXIONES SIN DUPLICADOS
// ============================================================================

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
    
    // Recolectar bones únicos
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
    
    // Dibujar debug spheres en 3D (si está activado)
    if (app->debug.showDebugSpheres && drawnCount > 0) {
        BeginMode3D(app->character->renderer->camera);
        for (int i = 0; i < drawnCount; i++) {
            DrawSphere(drawnBones[i].worldPos, 0.028f, (Color){80, 160, 255, 140});
            DrawSphereWires(drawnBones[i].worldPos, 0.031f, 8, 8, (Color){235, 235, 235, 255});
        }
        EndMode3D();
    }
    
    // Dibujar conexiones en 3D (si está activado)
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
    
    // Dibujar nombres en 2D (si está activado)
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

static void DrawDebugPanel(AppState* app) {
    if (!app->showUI) return;
    
    int panelX = app->screenWidth - 220;
    int panelY = 10;
    int panelWidth = 210;
    int panelHeight = 200;
    int buttonWidth = 190;
    int buttonY = panelY + 40;
    
    // Panel de fondo
    DrawRectangle(panelX, panelY, panelWidth, panelHeight, (Color){40, 40, 40, 220});
    DrawRectangleLinesEx((Rectangle){(float)panelX, (float)panelY, (float)panelWidth, (float)panelHeight}, 2, BLACK);
    
    DrawText("DEBUG OPTIONS", panelX + 10, panelY + 10, 16, YELLOW);
    
    // Toggle Bone Names
    if (ToggleButton((Rectangle){(float)(panelX + 10), (float)buttonY, (float)buttonWidth, 30}, 
                     "Bone Names", app->debug.showBoneNames, GREEN, DARKGRAY)) {
        app->debug.showBoneNames = !app->debug.showBoneNames;
    }
    buttonY += 35;
    
    // Toggle Debug Spheres
    if (ToggleButton((Rectangle){(float)(panelX + 10), (float)buttonY, (float)buttonWidth, 30}, 
                     "Debug Spheres", app->debug.showDebugSpheres, GREEN, DARKGRAY)) {
        app->debug.showDebugSpheres = !app->debug.showDebugSpheres;
    }
    buttonY += 35;
    
    // Toggle Connections
    if (ToggleButton((Rectangle){(float)(panelX + 10), (float)buttonY, (float)buttonWidth, 30}, 
                     "Connections", app->debug.showConnections, GREEN, DARKGRAY)) {
        app->debug.showConnections = !app->debug.showConnections;
    }
    buttonY += 35;
    
    // Toggle Orientation
    if (ToggleButton((Rectangle){(float)(panelX + 10), (float)buttonY, (float)buttonWidth, 30}, 
                     "Orientation", app->debug.showOrientation, GREEN, DARKGRAY)) {
        app->debug.showOrientation = !app->debug.showOrientation;
    }
    buttonY += 35;
}

static bool App_Init(AppState* app) {
    if (!app) return false;
    memset(app, 0, sizeof(*app));

    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - Animation Editor");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    
    #if defined(__linux__)
    for (int i = 0; i < 5; i++) PollInputEvents();
    #endif
    
    MaximizeWindow();
    SetTargetFPS(120);

    app->character = CreateAnimatedCharacter("data/textures/bone_textures.txt", 
                                           "data/textures/texture_sets.txt");
    if (!app->character) {
        TraceLog(LOG_ERROR, "Failed to create animated character");
        CloseWindow();
        return false;
    }

    if (LoadAnimation(app->character, "data/poses/idle.json", "data/animations/idle.anim")) {
        strcpy(app->currentAnimation, "idle");
        TraceLog(LOG_INFO, "Successfully loaded default idle animation");
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
    strcpy(app->editor.exportPath, "data/poses/exported.json");

    app->debug.showBoneNames = false;
    app->debug.showDebugSpheres = false;
    app->debug.showConnections = false;
    app->debug.showOrientation = false;

    app->screenWidth = GetScreenWidth();
    app->screenHeight = GetScreenHeight();

    TraceLog(LOG_INFO, "Animation Editor initialized successfully");
    return true;
}

static void App_Shutdown(AppState* app) {
    if (!app) return;
    DestroyAnimatedCharacter(app->character);
    CloseWindow();
}

static void App_HandleInput(AppState* app) {
    if (!app) return;

    int maxFrameNumber = FindMaxFrameNumber(app);
    int currentFrameNumber = GetCurrentFrameNumber(app);

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
    }
    if (IsKeyPressed(KEY_FOUR)) {
        LoadAnimation(app->character, "data/poses/talk.json", "data/animations/talk.anim");
        strcpy(app->currentAnimation, "talk");
    }
    if (IsKeyPressed(KEY_FIVE)) {
        LoadAnimation(app->character, "data/poses/walk.json", "data/animations/walk.anim");
        strcpy(app->currentAnimation, "walk");
    }
    if (IsKeyPressed(KEY_SIX)) {
        LoadAnimation(app->character, "data/poses/jump.json", "data/animations/jump.anim");
        strcpy(app->currentAnimation, "jump");
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
        int maxFrameNumber = FindMaxFrameNumber(app);
        
        BonesDeleteFrame(&app->character->animation, frameIndex);
        app->character->maxFrames = app->character->animation.frameCount;
        app->editor.needsSave = true;
        
        // Buscar el siguiente frame válido desde el frame eliminado
        int nextValidFrame = -1;
        for (int i = deletedFrameNumber; i <= maxFrameNumber; i++) {
            if (FrameExists(app, i)) {
                nextValidFrame = i;
                break;
            }
        }
        
        // Si no hay siguiente, buscar el anterior
        if (nextValidFrame == -1) {
            for (int i = deletedFrameNumber - 1; i >= 0; i--) {
                if (FrameExists(app, i)) {
                    nextValidFrame = i;
                    break;
                }
            }
        }
        
        // Mover a ese frame y mantener la selección
        if (nextValidFrame != -1) {
            int newIndex = FindFrameIndexByNumber(app, nextValidFrame);
            if (newIndex != -1) {
                SetCharacterFrame(app->character, newIndex);
                app->editor.selectionStart = nextValidFrame;
                app->editor.selectionEnd = nextValidFrame;
            }
        } else {
            // No quedan frames
            app->editor.selectionStart = -1;
            app->editor.selectionEnd = -1;
        }
    }
}
}

static void App_UpdateCamera(AppState* app, float dt) {
    if (!app || !app->character) return;

    Vector3 cameraTarget = app->character->autoCenterCalculated ? 
                          app->character->autoCenter : (Vector3){0, 0.6f, 0};

    if (app->camMode == 1) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !app->editor.isDraggingSlider) {
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
    UpdateBoneSelection(app);
}

static void App_DrawUI(AppState* app) {
    if (!app->showUI) return;

    int maxFrameNumber = FindMaxFrameNumber(app);
    int existingFrames = app->character->animation.frameCount;
    int currentFrameNumber = GetCurrentFrameNumber(app);

    DrawText("BONES3D ANIMATION EDITOR", 10, 10, 20, BLUE);
    DrawText("SPACE: Play/Pause | LEFT/RIGHT: Frame | DEL: Delete | CTRL+S: Export", 10, 35, 14, DARKGRAY);
    DrawText("1: Orbit | 2: FPS | 3-6: Load Anims | H/T: Billboards | F1: Toggle UI", 10, 52, 14, DARKGRAY);
    DrawText("F2: Spheres | F3: Names | F4: Connections | F5: Orientation", 10, 69, 14, DARKGRAY);

    char frameText[128];
    snprintf(frameText, sizeof(frameText), "Animation: %s | Frame: %d/%d (%d existing) %s %s", 
             app->currentAnimation, 
             currentFrameNumber, 
             maxFrameNumber,
             existingFrames,
             app->editor.isPlaying ? "[PLAYING]" : "[PAUSED]",
             app->editor.needsSave ? "[*]" : "");
    DrawText(frameText, 10, 89, 16, app->editor.needsSave ? ORANGE : DARKGRAY);

   if (app->editor.hasBoneSelected) {
        bool isKeyframe = IsCurrentFrameKeyframe(app);
        char selectionText[128];
        
        if (app->editor.isDraggingBone) {
            snprintf(selectionText, sizeof(selectionText), "[DRAGGING] Bone: %s", 
                    app->editor.selectedBoneName);
            DrawText(selectionText, 10, 106, 16, RED);
        } else if (isKeyframe) {
            snprintf(selectionText, sizeof(selectionText), "[SELECTED - KEYFRAME] Bone: %s (Can move)", 
                    app->editor.selectedBoneName);
            DrawText(selectionText, 10, 106, 16, GREEN);
        } else {
            snprintf(selectionText, sizeof(selectionText), "[SELECTED - INTERPOLATED] Bone: %s (Cannot move)", 
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
    
    // Dibujar orientación de bones (3D) - independiente
    DrawBoneOrientation(app);
    
    // Dibujar todos los elementos de debug (nombres, esferas, conexiones)
    DrawDebugVisuals(app);
        DrawBoneSelectionFeedback(app);
    App_DrawUI(app);
    DrawDebugPanel(app);
    DrawTimeline(app);
    DrawControlPanel(app);
    DrawExportDialog(app);

    EndDrawing();
}

int main(void) {
    AppState app;
    if (!App_Init(&app)) return -1;
    
    TraceLog(LOG_INFO, "=== BONES3D ANIMATION EDITOR ===");
    TraceLog(LOG_INFO, "Press SPACE to play/pause");
    TraceLog(LOG_INFO, "Use LEFT/RIGHT arrows to navigate frames");
    TraceLog(LOG_INFO, "Select frames with mouse + SHIFT for range");
    TraceLog(LOG_INFO, "Press CTRL+S to export animation");
    TraceLog(LOG_INFO, "Press F2 to toggle debug spheres");
    TraceLog(LOG_INFO, "Press F3 to toggle bone names");
    TraceLog(LOG_INFO, "Press F4 to toggle skeleton connections");
    TraceLog(LOG_INFO, "Press F5 to toggle bone orientation axes");
    TraceLog(LOG_INFO, "Use DEBUG OPTIONS panel (top-right) for visual toggles");
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        app.screenWidth = GetScreenWidth();
        app.screenHeight = GetScreenHeight();
        
        App_HandleInput(&app);
        App_UpdateCamera(&app, dt);
        UpdateAnimatedCharacter(app.character, dt);
        App_Draw(&app);
    }
    
    if (app.editor.needsSave) {
        TraceLog(LOG_WARNING, "Closing with unsaved changes!");
    }
    
    App_Shutdown(&app);
    return 0;
}
