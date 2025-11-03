#include "raylib.h"
#include "bones_core.h"


// ============================================================================
// CONSTANTES
// ============================================================================

#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080

static const float ORBIT_SENSITIVITY = 0.01f;
static const float FPS_SENSITIVITY = 0.003f;
static const float ZOOM_SENSITIVITY = 0.5f;
static const float MIN_ORBIT_RADIUS = 0.5f;
static const float MAX_ORBIT_RADIUS = 20.0f;
static const float MIN_PITCH = -85.0f * PI / 180.0f;
static const float MAX_PITCH = 81.0f * PI / 180.0f;
static const float BASE_SPEED = 5.0f;
static const float MOVEMENT_SPEED = 3.0f;

// ============================================================================
// ESTRUCTURA DE LA APLICACIÓN
// ============================================================================

typedef struct {
    AnimatedCharacter* character;
    
    // Control de cámara
    int camMode;
    float orbitYaw, orbitPitch, orbitRadius;
    
    // UI state
    bool showUI;
    
    // Animación actual
    char currentAnimation[64];
} AppState;

// ============================================================================
// INICIALIZACIÓN
// ============================================================================

static bool App_Init(AppState* app) {
    if (!app) return false;
    memset(app, 0, sizeof(*app));

    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - Character Viewer");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    
    #if defined(__linux__)
    for (int i = 0; i < 5; i++) PollInputEvents();
    #endif
    
    MaximizeWindow();
    SetTargetFPS(120);

    // Crear personaje animado
    app->character = CreateAnimatedCharacter("data/textures/bone_textures.txt", 
                                           "data/textures/texture_sets.txt");
    if (!app->character) {
        TraceLog(LOG_ERROR, "Failed to create animated character");
        CloseWindow();
        return false;
    }

    // Cargar animación por defecto
    if (LoadAnimation(app->character, "data/poses/idle.json", "data/animations/idle.anim")) {
        strcpy(app->currentAnimation, "idle");
        TraceLog(LOG_INFO, "Successfully loaded default idle animation");
    } else {
        TraceLog(LOG_ERROR, "Failed to load default animation");
        // Intentar cargar cualquier animación disponible
        if (LoadAnimation(app->character, "data/poses/talk.json", NULL)) {
            strcpy(app->currentAnimation, "talk");
            TraceLog(LOG_INFO, "Loaded talk animation as fallback");
        }
    }

    // Configuración inicial de cámara
    app->camMode = 1;
    app->orbitRadius = 2.5f;
    app->orbitPitch = -0.2f;
    app->showUI = true;

    TraceLog(LOG_INFO, "Application initialized successfully");
    return true;
}

static void App_Shutdown(AppState* app) {
    if (!app) return;

    DestroyAnimatedCharacter(app->character);
    CloseWindow();
}

// ============================================================================
// MANEJO DE ENTRADA - CON CAMBIO DE ANIMACIONES MEJORADO
// ============================================================================

static void App_HandleInput(AppState* app) {
    if (!app) return;

    // Play/Pause
    if (IsKeyPressed(KEY_P)) {
        SetCharacterAutoPlay(app->character, !app->character->autoPlay);
        TraceLog(LOG_INFO, "Animation %s", app->character->autoPlay ? "PLAYING" : "PAUSED");
    }

    // Navegación de frames manual
    if (IsKeyPressed(KEY_LEFT) && app->character->currentFrame > 0) {
        SetCharacterFrame(app->character, app->character->currentFrame - 1);
        TraceLog(LOG_INFO, "Manual frame: %d", app->character->currentFrame);
    }
    if (IsKeyPressed(KEY_RIGHT) && app->character->currentFrame < app->character->maxFrames - 1) {
        SetCharacterFrame(app->character, app->character->currentFrame + 1);
        TraceLog(LOG_INFO, "Manual frame: %d", app->character->currentFrame);
    }
    if (IsKeyPressed(KEY_HOME)) {
        SetCharacterFrame(app->character, 0);
        TraceLog(LOG_INFO, "Reset to frame 0");
    }
    if (IsKeyPressed(KEY_END) && app->character->maxFrames > 0) {
        SetCharacterFrame(app->character, app->character->maxFrames - 1);
        TraceLog(LOG_INFO, "Jump to last frame: %d", app->character->maxFrames - 1);
    }

    // Modos de cámara
    if (IsKeyPressed(KEY_ONE)) {
        app->camMode = 1; // Orbit camera
        EnableCursor();
        TraceLog(LOG_INFO, "Camera mode: ORBIT");
    }
    if (IsKeyPressed(KEY_TWO)) {
        app->camMode = 2; // FPS camera
        DisableCursor();
        TraceLog(LOG_INFO, "Camera mode: FPS");
    }

    // Cambio de animaciones con verificación
    if (IsKeyPressed(KEY_THREE)) {
        TraceLog(LOG_INFO, "Attempting to load IDLE animation...");
        if (LoadAnimation(app->character, "data/poses/idle.json", "data/animations/idle.anim")) {
            strcpy(app->currentAnimation, "idle");
            TraceLog(LOG_INFO, "Successfully loaded IDLE animation");
        } else {
            TraceLog(LOG_ERROR, "Failed to load IDLE animation");
        }
    }
    
    if (IsKeyPressed(KEY_FOUR)) {
        TraceLog(LOG_INFO, "Attempting to load TALK animation...");
        if (LoadAnimation(app->character, "data/poses/talk.json", "data/animations/talk.anim")) {
            strcpy(app->currentAnimation, "talk");
            TraceLog(LOG_INFO, "Successfully loaded TALK animation");
        } else {
            TraceLog(LOG_ERROR, "Failed to load TALK animation");
        }
    }
    
    if (IsKeyPressed(KEY_FIVE)) {
        TraceLog(LOG_INFO, "Attempting to load WALK animation...");
        if (LoadAnimation(app->character, "data/poses/walk.json", "data/animations/walk.anim")) {
            strcpy(app->currentAnimation, "walk");
            TraceLog(LOG_INFO, "Successfully loaded WALK animation");
        } else {
            TraceLog(LOG_ERROR, "Failed to load WALK animation");
        }
    }
    
    if (IsKeyPressed(KEY_SIX)) {
        TraceLog(LOG_INFO, "Attempting to load RUN animation...");
        if (LoadAnimation(app->character, "data/poses/run.json", "data/animations/run.anim")) {
            strcpy(app->currentAnimation, "run");
            TraceLog(LOG_INFO, "Successfully loaded RUN animation");
        } else {
            TraceLog(LOG_ERROR, "Failed to load RUN animation");
        }
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
    }
    
    // Debug info
    if (IsKeyPressed(KEY_F3)) {
        TraceLog(LOG_INFO, "Debug Info - Frames: %d/%d, AutoPlay: %s, HasController: %s",
                app->character->currentFrame + 1, app->character->maxFrames,
                app->character->autoPlay ? "YES" : "NO",
                app->character->animController ? "YES" : "NO");
    }
}

// ============================================================================
// ACTUALIZACIÓN DE CÁMARA
// ============================================================================

static void App_UpdateCamera(AppState* app, float dt) {
    if (!app || !app->character) return;

    Vector3 cameraTarget = app->character->autoCenterCalculated ? 
                          app->character->autoCenter : (Vector3){0, 0.6f, 0};

    if (app->camMode == 1) {
        // Cámara orbital
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mouseDelta = GetMouseDelta();
            app->orbitYaw += mouseDelta.x * ORBIT_SENSITIVITY;
            app->orbitPitch = Clamp(app->orbitPitch - mouseDelta.y * ORBIT_SENSITIVITY, 
                                   MIN_PITCH, MAX_PITCH);
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
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
        // Cámara FPS
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
        if (IsKeyDown(KEY_Q))
            app->character->renderer->camera.position = 
                Vector3Add(app->character->renderer->camera.position, 
                          Vector3Scale(app->character->renderer->camera.up, speed));
        if (IsKeyDown(KEY_LEFT_CONTROL))
            app->character->renderer->camera.position = 
                Vector3Subtract(app->character->renderer->camera.position, 
                              Vector3Scale(app->character->renderer->camera.up, speed));
        
        app->character->renderer->camera.target = 
            Vector3Add(app->character->renderer->camera.position, forward);
    }
}

// ============================================================================
// INTERFAZ DE USUARIO - CON MÁS INFORMACIÓN
// ============================================================================

static void App_DrawUI(AppState* app) {
    if (!app->showUI) return;

    DrawText("BONES3D CHARACTER VIEWER", 10, 10, 20, BLUE);
    DrawText("1: Orbit Cam | 2: FPS Cam | P: Play/Pause | H/T: Toggle Billboards", 10, 35, 16, DARKGRAY);
    DrawText("3: Idle | 4: Talk | 5: Walk | 6: Run | F1: Toggle UI | F3: Debug", 10, 55, 16, DARKGRAY);

    char frameText[128];
    const char* animStatus = app->character->autoPlay ? "(Playing)" : "(Paused)";
    const char* controllerStatus = app->character->animController ? "[Controller]" : "[No Controller]";
    
    snprintf(frameText, sizeof(frameText), "Animation: %s | Frame: %d/%d %s %s", 
             app->currentAnimation, app->character->currentFrame + 1, 
             app->character->maxFrames, animStatus, controllerStatus);
    DrawText(frameText, 10, 75, 16, DARKGRAY);

    char statsText[256];
    snprintf(statsText, sizeof(statsText), "Bones: %d | Heads: %d | Torsos: %d | Camera: %s",
        app->character->renderBonesCount, 
        app->character->renderHeadsCount,
        app->character->renderTorsosCount,
        app->camMode == 1 ? "ORBIT" : "FPS");
    DrawText(statsText, 10, 95, 16, DARKGRAY);

    // Información de ayuda adicional
    if (app->character->maxFrames == 0) {
        DrawText("WARNING: No animation loaded or 0 frames!", 10, 120, 16, RED);
    }
    
    if (!app->character->animController) {
        DrawText("WARNING: No animation controller - animation may not play", 10, 140, 16, ORANGE);
    }
}

// ============================================================================
// RENDERIZADO PRINCIPAL
// ============================================================================

static void App_Draw(AppState* app) {
    if (!app) return;

    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Dibujar el personaje
    DrawAnimatedCharacter(app->character, app->character->renderer->camera);

    // Dibujar UI
    App_DrawUI(app);

    EndDrawing();
}

// ============================================================================
// FUNCIÓN PRINCIPAL
// ============================================================================

int main(void) {
    AppState app;
    if (!App_Init(&app)) return -1;
    
    TraceLog(LOG_INFO, "Bones3D Character Viewer Initialized");
    TraceLog(LOG_INFO, "Use number keys 3-6 to change animations");
    TraceLog(LOG_INFO, "Press F3 for debug information");
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        
        App_HandleInput(&app);
        App_UpdateCamera(&app, dt);
        UpdateAnimatedCharacter(app.character, dt);
        App_Draw(&app);
    }
    
    TraceLog(LOG_INFO, "Shutting down...");
    App_Shutdown(&app);
    return 0;
}