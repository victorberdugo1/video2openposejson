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
} EditorState;

typedef struct {
    AnimatedCharacter* character;
    
    int camMode;
    float orbitYaw, orbitPitch, orbitRadius;
    
    bool showUI;
    char currentAnimation[64];
    
    EditorState editor;
    
    int screenWidth;
    int screenHeight;
} AppState;

// Función para encontrar el frame máximo real en la animación
static int FindMaxFrameNumber(AppState* app) {
    int maxFrame = 0;
    for (int i = 0; i < app->character->animation.frameCount; i++) {
        if (app->character->animation.frames[i].frameNumber > maxFrame) {
            maxFrame = app->character->animation.frames[i].frameNumber;
        }
    }
    return maxFrame;
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

// Función para encontrar el índice de un frame por su número
static int FindFrameIndexByNumber(AppState* app, int frameNumber) {
    for (int i = 0; i < app->character->animation.frameCount; i++) {
        if (app->character->animation.frames[i].frameNumber == frameNumber) {
            return i;
        }
    }
    return -1;
}

// Función para obtener el número de frame actual (visual)
static int GetCurrentFrameNumber(AppState* app) {
    if (app->character->currentFrame < 0 || 
        app->character->currentFrame >= app->character->animation.frameCount) {
        return 0;
    }
    return app->character->animation.frames[app->character->currentFrame].frameNumber;
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

static Rectangle GetTimelineRect(AppState* app) {
    return (Rectangle){
        TIMELINE_MARGIN,
        app->screenHeight - UI_HEIGHT + 50,
        app->screenWidth - TIMELINE_MARGIN * 2,
        40
    };
}

static void DrawTimeline(AppState* app) {
    if (!app->editor.showTimeline || !app->character->animation.isLoaded) return;
    
    Rectangle timeline = GetTimelineRect(app);
    
    // Encontrar el frame máximo real en lugar de usar maxFrames
    int maxFrameNumber = FindMaxFrameNumber(app);
    if (maxFrameNumber == 0 && app->character->animation.frameCount == 0) return;
    
    // Fondo del timeline
    DrawRectangleRec(timeline, (Color){40, 40, 40, 255});
    DrawRectangleLinesEx(timeline, 2, BLACK);
    
    float frameWidth = timeline.width / (float)(maxFrameNumber + 1);
    
    // Obtener el número de frame actual (visual)
    int currentFrameNumber = GetCurrentFrameNumber(app);
    
    // Dibujar todos los frames desde 0 hasta maxFrameNumber
    for (int i = 0; i <= maxFrameNumber; i++) {
        float x = timeline.x + i * frameWidth;
        Rectangle frameRect = {x, timeline.y, frameWidth, timeline.height};
        
        // Verificar si este frame número existe realmente
        bool frameExists = FrameExists(app, i);
        
        bool isCurrentFrame = (i == currentFrameNumber);
        bool isSelected = (i >= app->editor.selectionStart && i <= app->editor.selectionEnd);
        
        Color frameColor;
        if (!frameExists) {
            frameColor = (Color){30, 30, 30, 255}; // Frame faltante - más oscuro
        } else if (isCurrentFrame) {
            frameColor = ORANGE;
        } else if (isSelected) {
            frameColor = SKYBLUE;
        } else {
            frameColor = (i % 5 == 0) ? (Color){60, 60, 60, 255} : (Color){50, 50, 50, 255};
        }
        
        DrawRectangleRec(frameRect, frameColor);
        DrawRectangleLinesEx(frameRect, 1, BLACK);
        
        // Números de frame cada 5 frames
        if (i % 5 == 0) {
            char frameNum[16];
            snprintf(frameNum, sizeof(frameNum), "%d", i);
            DrawText(frameNum, (int)(x + 2), (int)(timeline.y - 15), 10, 
                    !frameExists ? DARKGRAY : LIGHTGRAY);
        }
        
        // Indicador visual para frames que existen
        if (frameExists) {
            DrawRectangle((int)(x + 2), (int)(timeline.y + timeline.height - 8), 
                         (int)(frameWidth - 4), 4, GREEN);
        }
    }
    
    // Marcador de frame actual - usar el número de frame visual
    float markerX = timeline.x + currentFrameNumber * frameWidth + frameWidth / 2;
    DrawLineEx((Vector2){markerX, timeline.y}, 
               (Vector2){markerX, timeline.y + timeline.height}, 
               3, RED);
    DrawCircle((int)markerX, (int)(timeline.y + timeline.height + 5), 6, RED);
    
    // Interacción del timeline
    if (CheckCollisionPointRec(GetMousePosition(), timeline)) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            app->editor.isDraggingSlider = true;
        }
        
        // Scroll del mouse para cambiar frames
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            int newFrameNumber = currentFrameNumber - (int)wheel;
            if (newFrameNumber >= 0 && newFrameNumber <= maxFrameNumber) {
                // Solo cambiar si el frame existe
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
                    // Solo cambiar si el frame existe
                    if (FrameExists(app, clickedFrameNumber)) {
                        int frameIndex = FindFrameIndexByNumber(app, clickedFrameNumber);
                        if (frameIndex != -1) {
                            SetCharacterFrame(app->character, frameIndex);
                            
                            // Selección múltiple con Shift
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
        // Ir al primer frame que existe
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
        // Frame anterior que existe
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
        // Frame siguiente que existe
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
        // Ir al último frame que existe
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
                BonesDeleteFrame(&app->character->animation, frameIndex);
                app->character->maxFrames = app->character->animation.frameCount;
                app->editor.needsSave = true;
                
                // Ajustar frame actual si es necesario
                if (currentFrameNumber >= app->character->maxFrames) {
                    for (int i = app->character->maxFrames - 1; i >= 0; i--) {
                        if (FrameExists(app, i)) {
                            int newIndex = FindFrameIndexByNumber(app, i);
                            if (newIndex != -1) {
                                SetCharacterFrame(app->character, newIndex);
                            }
                            break;
                        }
                    }
                }
                app->editor.selectionStart = -1;
                app->editor.selectionEnd = -1;
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
    
    // Input para frames de interpolación
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
    
    // Botón de guardar/exportar
    if (Button((Rectangle){(float)buttonX, (float)panelY, 80, BUTTON_SIZE}, 
               "EXPORT", app->editor.needsSave ? ORANGE : DARKGREEN)) {
        app->editor.showExportDialog = !app->editor.showExportDialog;
    }
    buttonX += 85;
    
    // Info de frames
    int totalFrames = maxFrameNumber + 1; // Total de números de frame (incluyendo huecos)
    int existingFrames = app->character->animation.frameCount; // Frames que realmente existen
    
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

static void DrawExportDialog(AppState* app) {
    if (!app->editor.showExportDialog) return;
    
    int dialogW = 500;
    int dialogH = 200;
    int dialogX = (app->screenWidth - dialogW) / 2;
    int dialogY = (app->screenHeight - dialogH) / 2;
    
    // Fondo oscuro
    DrawRectangle(0, 0, app->screenWidth, app->screenHeight, (Color){0, 0, 0, 150});
    
    // Diálogo
    DrawRectangle(dialogX, dialogY, dialogW, dialogH, RAYWHITE);
    DrawRectangleLinesEx((Rectangle){(float)dialogX, (float)dialogY, (float)dialogW, (float)dialogH}, 3, BLACK);
    
    DrawText("Export Animation", dialogX + 20, dialogY + 20, 20, BLACK);
    
    // Path de exportación
    DrawText("Export Path:", dialogX + 20, dialogY + 60, 16, DARKGRAY);
    DrawTextField((Rectangle){(float)(dialogX + 20), (float)(dialogY + 80), (float)(dialogW - 40), 30}, 
                  app->editor.exportPath, true);
    
    // Botones
    if (Button((Rectangle){(float)(dialogX + 20), (float)(dialogY + 140), 100, 40}, 
               "EXPORT", GREEN)) {
        if (strlen(app->editor.exportPath) > 0) {
            // Exportar todo el rango o selección
            int startFrame = app->editor.selectionStart != -1 ? app->editor.selectionStart : 0;
            int endFrame = app->editor.selectionEnd != -1 ? app->editor.selectionEnd : FindMaxFrameNumber(app);
            
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
    
    if (Button((Rectangle){(float)(dialogX + 140), (float)(dialogY + 140), 100, 40}, 
               "CANCEL", RED)) {
        app->editor.showExportDialog = false;
    }
    
    if (Button((Rectangle){(float)(dialogX + dialogW - 120), (float)(dialogY + 140), 100, 40}, 
               "BROWSE", BLUE)) {
        // Aquí podrías implementar un file browser
        strcpy(app->editor.exportPath, "data/poses/exported.json");
    }
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
    app->editor.isPlaying = false;
    app->editor.selectedFrame = 0;
    app->editor.selectionStart = -1;
    app->editor.selectionEnd = -1;
    app->editor.interpolationCount = 5;
    app->editor.playbackSpeed = 1.0f;
    strcpy(app->editor.exportPath, "data/poses/exported.json");

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

    // Play/Pause
    if (IsKeyPressed(KEY_SPACE)) {
        app->editor.isPlaying = !app->editor.isPlaying;
        SetCharacterAutoPlay(app->character, app->editor.isPlaying);
    }

    // Navegación de frames - solo a frames que existen
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

    // Modos de cámara
    if (IsKeyPressed(KEY_ONE)) {
        app->camMode = 1;
        EnableCursor();
    }
    if (IsKeyPressed(KEY_TWO)) {
        app->camMode = 2;
        DisableCursor();
    }

    // Cambio de animaciones
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

    // Billboards
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

    // UI toggle
    if (IsKeyPressed(KEY_F1)) {
        app->showUI = !app->showUI;
        app->editor.showTimeline = app->showUI;
    }
    
    // Quick save
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
        app->editor.showExportDialog = true;
    }
    
    // Delete selected frame
    if (IsKeyPressed(KEY_DELETE) && app->editor.selectionStart != -1 && FrameExists(app, app->editor.selectionStart)) {
        int frameIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
        if (frameIndex != -1) {
            BonesDeleteFrame(&app->character->animation, frameIndex);
            app->character->maxFrames = app->character->animation.frameCount;
            app->editor.needsSave = true;
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
        // Cámara FPS (sin cambios)
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
}

static void App_DrawUI(AppState* app) {
    if (!app->showUI) return;

    int maxFrameNumber = FindMaxFrameNumber(app);
    int existingFrames = app->character->animation.frameCount;
    int currentFrameNumber = GetCurrentFrameNumber(app);

    DrawText("BONES3D ANIMATION EDITOR", 10, 10, 20, BLUE);
    DrawText("SPACE: Play/Pause | LEFT/RIGHT: Frame | DEL: Delete | CTRL+S: Export", 10, 35, 14, DARKGRAY);
    DrawText("1: Orbit | 2: FPS | 3-6: Load Anims | H/T: Billboards | F1: Toggle UI", 10, 52, 14, DARKGRAY);

    char frameText[128];
    snprintf(frameText, sizeof(frameText), "Animation: %s | Frame: %d/%d (%d existing) %s %s", 
             app->currentAnimation, 
             currentFrameNumber, 
             maxFrameNumber,
             existingFrames,
             app->editor.isPlaying ? "[PLAYING]" : "[PAUSED]",
             app->editor.needsSave ? "[*]" : "");
    DrawText(frameText, 10, 72, 16, app->editor.needsSave ? ORANGE : DARKGRAY);
}

static void App_Draw(AppState* app) {
    if (!app) return;

    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawAnimatedCharacter(app->character, app->character->renderer->camera);
    
    App_DrawUI(app);
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