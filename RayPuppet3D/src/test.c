// test.c - Test con animaciones normalizadas (no saltan al cambiar)
#include "bones_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// OFFSET MANAGER - Sistema de normalización de animaciones
// ============================================================================

typedef struct {
    Vector3 centerPosition;  // Centro original de la animación
    bool calculated;
    char animationPath[256];
} AnimationOffset;

typedef struct {
    AnimationOffset offsets[32];
    int count;
} OffsetManager;

OffsetManager* CreateOffsetManager() {
    OffsetManager* manager = (OffsetManager*)calloc(1, sizeof(OffsetManager));
    if (!manager) return NULL;
    manager->count = 0;
    return manager;
}

void DestroyOffsetManager(OffsetManager* manager) {
    if (manager) free(manager);
}

Vector3 CalculateAnimationCenter(const AnimationFrame* frame) {
    if (!frame || !frame->valid || frame->personCount == 0) {
        return (Vector3){0, 0, 0};
    }
    Vector3 sum = {0, 0, 0};
    int validBones = 0;
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid) continue;
            sum = Vector3Add(sum, bone->position.position);
            validBones++;
        }
    }
    if (validBones > 0) {
        return Vector3Scale(sum, 1.0f / validBones);
    }
    return (Vector3){0, 0, 0};
}

AnimationOffset* GetOrCreateOffset(OffsetManager* manager, const char* animPath,
                                   const AnimationFrame* firstFrame) {
    if (!manager || !animPath) return NULL;
    
    // Buscar si ya existe
    for (int i = 0; i < manager->count; i++) {
        if (strcmp(manager->offsets[i].animationPath, animPath) == 0) {
            return &manager->offsets[i];
        }
    }
    
    // Crear nuevo offset
    if (manager->count >= 32) return NULL;
    AnimationOffset* newOffset = &manager->offsets[manager->count];
    strncpy(newOffset->animationPath, animPath, 255);
    newOffset->animationPath[255] = '\0';
    
    Vector3 animCenter = CalculateAnimationCenter(firstFrame);
    newOffset->centerPosition = animCenter;
    newOffset->calculated = true;
    manager->count++;
    
    printf("[OFFSET] Nueva animación '%s' - Centro: (%.3f, %.3f, %.3f)\n",
           animPath, animCenter.x, animCenter.y, animCenter.z);
    
    return newOffset;
}

void ApplyOffsetToFrame(AnimationFrame* frame, Vector3 offset) {
    if (!frame || !frame->valid) return;
    for (int p = 0; p < frame->personCount; p++) {
        Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            Bone* bone = &person->bones[b];
            if (!bone->position.valid) continue;
            bone->position.position = Vector3Add(bone->position.position, offset);
        }
    }
}

void ApplyOffsetToAnimation(BonesAnimation* animation, Vector3 offset) {
    if (!animation || !animation->isLoaded) return;
    for (int f = 0; f < animation->frameCount; f++) {
        ApplyOffsetToFrame(&animation->frames[f], offset);
    }
}

// ============================================================================
// ESTRUCTURAS DE JUEGO
// ============================================================================

typedef struct {
    AnimatedCharacter* character;
    Vector3 position;
    float rotation;
    int id;
    bool wasMoving;
    bool isPlayingAction;
    int actionStartFrame;
    int actionEndFrame;
    OffsetManager* offsetManager;
    Vector3 firstAnimationCenter;  // Centro de la primera animación (referencia)
    bool hasReferenceCenter;
} GameCharacter;

typedef struct {
    GameCharacter* characters;
    int count;
    int max;
    int controlled;
    Camera camera;
    float moveSpeed;
    float cameraSpeed;
    float camAngleH;
    float camAngleV;
    float camDistance;
    Vector3 camLateralOffset;
} GameWorld;

// ============================================================================
// FUNCIÓN AUXILIAR: Cargar animación con offset
// ============================================================================

bool LoadAnimationWithOffset(GameCharacter* gc, const char* animPath, const char* metaPath) {
    if (!gc || !gc->character || !gc->offsetManager) return false;
    
    // Cargar la animación normalmente
    if (!LoadAnimation(gc->character, animPath, metaPath)) {
        return false;
    }
    
    // Obtener el primer frame para calcular el centro
    if (gc->character->animation.isLoaded && gc->character->animation.frameCount > 0) {
        const AnimationFrame* firstFrame = &gc->character->animation.frames[0];
        
        // Obtener o crear el offset para esta animación
        AnimationOffset* animOffset = GetOrCreateOffset(gc->offsetManager, animPath, firstFrame);
        
        if (animOffset && animOffset->calculated) {
            // Si es la primera animación, guardar su centro como referencia
            if (!gc->hasReferenceCenter) {
                gc->firstAnimationCenter = animOffset->centerPosition;
                gc->hasReferenceCenter = true;
                printf("[OFFSET] Centro de referencia establecido: (%.3f, %.3f, %.3f)\n",
                       gc->firstAnimationCenter.x, gc->firstAnimationCenter.y, gc->firstAnimationCenter.z);
            }
            
            // Calcular el offset necesario para alinear con la primera animación
            Vector3 offset = Vector3Subtract(gc->firstAnimationCenter, animOffset->centerPosition);
            
            if (Vector3Length(offset) > 0.001f) {
                printf("[OFFSET] Aplicando corrección: (%.3f, %.3f, %.3f)\n",
                       offset.x, offset.y, offset.z);
                ApplyOffsetToAnimation(&gc->character->animation, offset);
            }
            
            gc->character->forceUpdate = true;
        }
    }
    
    return true;
}

// ============================================================================
// GESTIÓN DEL MUNDO
// ============================================================================

GameWorld* CreateWorld(int maxCharacters) {
    GameWorld* w = (GameWorld*)calloc(1, sizeof(GameWorld));
    w->max = maxCharacters;
    w->characters = (GameCharacter*)calloc(maxCharacters, sizeof(GameCharacter));
    w->count = 0;
    w->controlled = -1;

    w->camera.position = (Vector3){0.0f, 2.5f, 5.0f};
    w->camera.target = (Vector3){0.0f, 0.6f, 0.0f};
    w->camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    w->camera.fovy = 45.0f;
    w->camera.projection = CAMERA_PERSPECTIVE;

    w->moveSpeed = 1.8f;
    w->cameraSpeed = 3.0f;
    w->camAngleH = 0.0f;
    w->camAngleV = 20.0f;
    w->camDistance = 2.5f;
    w->camLateralOffset = (Vector3){0, 0, 0};

    return w;
}

void DestroyWorld(GameWorld* w) {
    if (!w) return;
    for (int i = 0; i < w->count; i++) {
        DestroyOffsetManager(w->characters[i].offsetManager);
        DestroyAnimatedCharacter(w->characters[i].character);
    }
    free(w->characters);
    free(w);
}

int AddCharacter(GameWorld* w, const char* texCfg, const char* texSets,
                 const char* animJson, const char* animMeta, Vector3 pos) {
    if (!w || w->count >= w->max) return -1;
    int idx = w->count;
    GameCharacter* gc = &w->characters[idx];

    gc->character = CreateAnimatedCharacter(texCfg, texSets);
    if (!gc->character) {
        printf("[ERROR] Failed to create character %d\n", idx);
        return -1;
    }

    // Crear el gestor de offsets para este personaje
    gc->offsetManager = CreateOffsetManager();
    if (!gc->offsetManager) {
        DestroyAnimatedCharacter(gc->character);
        printf("[ERROR] Failed to create offset manager for character %d\n", idx);
        return -1;
    }

    gc->hasReferenceCenter = false;
    gc->firstAnimationCenter = (Vector3){0, 0, 0};

    // Cargar la animación inicial con offset
    if (!LoadAnimationWithOffset(gc, animJson, animMeta)) {
        DestroyOffsetManager(gc->offsetManager);
        DestroyAnimatedCharacter(gc->character);
        printf("[ERROR] Failed to load animation for character %d\n", idx);
        return -1;
    }

    SetCharacterAutoPlay(gc->character, true);
    gc->position = pos;
    gc->rotation = 0.0f;
    gc->id = idx;
    gc->wasMoving = false;
    gc->isPlayingAction = false;
    gc->actionStartFrame = 0;
    gc->actionEndFrame = 0;

    w->count++;
    return idx;
}

// ============================================================================
// HELPER: Obtener info de la animación actual
// ============================================================================

void GetAnimationInfo(AnimatedCharacter* character, int* currentFrame, int* totalFrames) {
    if (!character || !character->animController) {
        *currentFrame = 0;
        *totalFrames = 0;
        return;
    }

    AnimationController* ctrl = character->animController;
    
    if (ctrl->currentClipIndex >= 0 && ctrl->currentClipIndex < ctrl->clipCount) {
        AnimationClipMetadata* clip = &ctrl->clips[ctrl->currentClipIndex];
        *currentFrame = ctrl->currentFrameInJSON;
        *totalFrames = clip->endFrame;
    } else {
        *currentFrame = character->currentFrame;
        *totalFrames = character->maxFrames;
    }
}

// ============================================================================
// INPUT Y UPDATE
// ============================================================================

void ProcessInput(GameWorld* w, float dt) {
    if (!w) return;

    // ===== CÁMARA ORBITAL =====
    if (w->controlled >= 0 && w->controlled < w->count) {
        GameCharacter* c = &w->characters[w->controlled];

        if (IsKeyDown(KEY_Q)) w->camAngleH += 90.0f * dt;
        if (IsKeyDown(KEY_E)) w->camAngleH -= 90.0f * dt;
        if (IsKeyDown(KEY_R)) w->camAngleV += 60.0f * dt;
        if (IsKeyDown(KEY_F)) w->camAngleV -= 60.0f * dt;

        if (w->camAngleV > 35.0f) w->camAngleV = 35.0f;
        if (w->camAngleV < -35.0f) w->camAngleV = -35.0f;

        if (IsKeyDown(KEY_W)) w->camDistance -= 3.0f * dt;
        if (IsKeyDown(KEY_S)) w->camDistance += 3.0f * dt;
        if (w->camDistance < 1.0f) w->camDistance = 1.0f;
        if (w->camDistance > 6.0f) w->camDistance = 6.0f;

        float radH = w->camAngleH * DEG2RAD;
        float radV = w->camAngleV * DEG2RAD;

        Vector3 forward = {
            cosf(radV) * sinf(radH),
            sinf(radV),
            cosf(radV) * cosf(radH)
        };

        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, (Vector3){0, 1, 0}));

        float lateralSpeed = 2.0f;
        if (IsKeyDown(KEY_A)) w->camLateralOffset = Vector3Add(w->camLateralOffset, Vector3Scale(right, lateralSpeed * dt));
        if (IsKeyDown(KEY_D)) w->camLateralOffset = Vector3Add(w->camLateralOffset, Vector3Scale(right, -lateralSpeed * dt));

        float lateralDist = Vector3Length(w->camLateralOffset);
        if (lateralDist > 3.0f) w->camLateralOffset = Vector3Scale(Vector3Normalize(w->camLateralOffset), 3.0f);

        w->camera.target = Vector3Add(c->position, (Vector3){0, 0.6f, 0});
        w->camera.target = Vector3Add(w->camera.target, w->camLateralOffset);

        w->camera.position.x = w->camera.target.x + w->camDistance * cosf(radV) * sinf(radH);
        w->camera.position.y = w->camera.target.y + w->camDistance * sinf(radV);
        w->camera.position.z = w->camera.target.z + w->camDistance * cosf(radV) * cosf(radH);
    }

    // ===== ANIMACIONES DE ACCIÓN =====
    if (w->controlled >= 0 && w->controlled < w->count) {
        GameCharacter* c = &w->characters[w->controlled];
        
        // Solo permitir nuevas acciones si no está ejecutando otra
        if (!c->isPlayingAction) {
            // Jump - J
            if (IsKeyPressed(KEY_J)) {
                if (LoadAnimationWithOffset(c, "data/animations/jump.json", "data/animations/jump.anim")) {
                    printf("[INFO] Ejecutando animación: Jump\n");
                    c->isPlayingAction = true;
                    
                    if (c->character->animController && 
                        c->character->animController->currentClipIndex >= 0) {
                        AnimationClipMetadata* clip = 
                            &c->character->animController->clips[c->character->animController->currentClipIndex];
                        c->actionStartFrame = clip->startFrame;
                        c->actionEndFrame = clip->endFrame;
                        printf("[INFO] Jump: frames %d-%d (total: %d frames)\n", 
                               c->actionStartFrame, c->actionEndFrame, 
                               c->actionEndFrame - c->actionStartFrame + 1);
                    }
                }
            }
            // Kick - K
            else if (IsKeyPressed(KEY_K)) {
                if (LoadAnimationWithOffset(c, "data/animations/kick.json", "data/animations/kick.anim")) {
                    printf("[INFO] Ejecutando animación: Kick\n");
                    c->isPlayingAction = true;
                    
                    if (c->character->animController && 
                        c->character->animController->currentClipIndex >= 0) {
                        AnimationClipMetadata* clip = 
                            &c->character->animController->clips[c->character->animController->currentClipIndex];
                        c->actionStartFrame = clip->startFrame;
                        c->actionEndFrame = clip->endFrame;
                        printf("[INFO] Kick: frames %d-%d (total: %d frames)\n", 
                               c->actionStartFrame, c->actionEndFrame, 
                               c->actionEndFrame - c->actionStartFrame + 1);
                    }
                }
            }
            // Punch - P
            else if (IsKeyPressed(KEY_P)) {
                if (LoadAnimationWithOffset(c, "data/animations/punch.json", "data/animations/punch.anim")) {
                    printf("[INFO] Ejecutando animación: Punch\n");
                    c->isPlayingAction = true;
                    
                    if (c->character->animController && 
                        c->character->animController->currentClipIndex >= 0) {
                        AnimationClipMetadata* clip = 
                            &c->character->animController->clips[c->character->animController->currentClipIndex];
                        c->actionStartFrame = clip->startFrame;
                        c->actionEndFrame = clip->endFrame;
                        printf("[INFO] Punch: frames %d-%d (total: %d frames)\n", 
                               c->actionStartFrame, c->actionEndFrame, 
                               c->actionEndFrame - c->actionStartFrame + 1);
                    }
                }
            }
        }
    }

    // ===== CONTROL DE PERSONAJE =====
    if (w->controlled >= 0 && w->controlled < w->count) {
        GameCharacter* c = &w->characters[w->controlled];
        bool moving = false;
        
        // Solo permitir movimiento si no está ejecutando una acción
        if (!c->isPlayingAction) {
            if (IsKeyDown(KEY_UP)) {
                Vector3 dir = {sinf(c->rotation), 0, cosf(c->rotation)};
                c->position = Vector3Add(c->position, Vector3Scale(dir, w->moveSpeed * dt));
                moving = true;
            }
            if (IsKeyDown(KEY_DOWN)) {
                Vector3 dir = {sinf(c->rotation), 0, cosf(c->rotation)};
                c->position = Vector3Subtract(c->position, Vector3Scale(dir, w->moveSpeed * dt));
                moving = true;
            }
            if (IsKeyDown(KEY_LEFT)) c->rotation += 2.5f * dt;
            if (IsKeyDown(KEY_RIGHT)) c->rotation -= 2.5f * dt;

            while (c->rotation > PI) c->rotation -= 2.0f * PI;
            while (c->rotation < -PI) c->rotation += 2.0f * PI;

            // Cambiar animación según movimiento
            if (moving != c->wasMoving) {
                const char* animPath = moving ? "data/animations/walk.json" : "data/animations/idle.json";
                const char* metaPath = moving ? "data/animations/walk.anim" : "data/animations/idle.anim";
                LoadAnimationWithOffset(c, animPath, metaPath);
                c->wasMoving = moving;
            }
        }
    }
}

void UpdateWorld(GameWorld* w, float dt) {
    if (!w) return;
    
    for (int i = 0; i < w->count; i++) {
        GameCharacter* gc = &w->characters[i];
        UpdateAnimatedCharacter(gc->character, dt);
        
        // Verificar si la animación de acción ha terminado
        if (gc->isPlayingAction) {
            int currentFrame, totalFrames;
            GetAnimationInfo(gc->character, &currentFrame, &totalFrames);
            
            bool animationComplete = false;
            
            if (gc->character->animController) {
                AnimationController* ctrl = gc->character->animController;
                
                if (currentFrame >= gc->actionEndFrame) {
                    if (!ctrl->playing || 
                        (ctrl->currentClipIndex >= 0 && 
                         !ctrl->clips[ctrl->currentClipIndex].loop)) {
                        animationComplete = true;
                    }
                }
            } else {
                if (currentFrame >= totalFrames - 1) {
                    animationComplete = true;
                }
            }
            
            if (animationComplete) {
                
                gc->isPlayingAction = false;
                gc->actionStartFrame = 0;
                gc->actionEndFrame = 0;
                
                // Volver a idle o walk según el estado de movimiento
                const char* animPath = gc->wasMoving ? 
                    "data/animations/walk.json" : "data/animations/idle.json";
                const char* metaPath = gc->wasMoving ? 
                    "data/animations/walk.anim" : "data/animations/idle.anim";
                LoadAnimationWithOffset(gc, animPath, metaPath);
            }
        }
    }
}

// ============================================================================
// RENDER
// ============================================================================

void RenderWorld(GameWorld* w) {
    if (!w) return;
    BeginMode3D(w->camera);
    DrawGrid(20, 1.0f);

    int n = w->count;
    if (n > 0) {
        int idxs[64];
        float dists[64];
        if (n > 64) n = 64;
        for (int i = 0; i < n; i++) {
            idxs[i] = i;
            dists[i] = Vector3Distance(w->camera.position, w->characters[i].position);
        }
        for (int i = 0; i < n - 1; i++) {
            int sel = i;
            for (int j = i + 1; j < n; j++) if (dists[j] > dists[sel]) sel = j;
            if (sel != i) {
                float td = dists[i]; dists[i] = dists[sel]; dists[sel] = td;
                int ti = idxs[i]; idxs[i] = idxs[sel]; idxs[sel] = ti;
            }
        }
        for (int i = 0; i < n; i++) {
            GameCharacter* gc = &w->characters[idxs[i]];
            DrawAnimatedCharacterTransformed(gc->character, w->camera, gc->position, gc->rotation);
        }
    }

    EndMode3D();
}

void DrawUI(GameWorld* w) {
    DrawText("Bones3D - Animaciones Normalizadas", 10, 10, 20, DARKGRAY);
    DrawFPS(10, 40);
    
    if (w->controlled >= 0) {
        GameCharacter* c = &w->characters[w->controlled];
        DrawText(TextFormat("Controlando: Personaje %d", w->controlled), 10, 70, 16, DARKGRAY);
        DrawText(TextFormat("Animaciones cargadas: %d", c->offsetManager->count), 10, 90, 14, DARKGRAY);
        
        if (c->hasReferenceCenter) {
            DrawText(TextFormat("Centro ref: (%.2f, %.2f, %.2f)", 
                     c->firstAnimationCenter.x, c->firstAnimationCenter.y, c->firstAnimationCenter.z), 
                     10, 110, 12, DARKGRAY);
        }
        
        if (c->isPlayingAction) {
            int currentFrame, totalFrames;
            GetAnimationInfo(c->character, &currentFrame, &totalFrames);
            DrawText("[EJECUTANDO ACCION]", 10, 130, 16, RED);
            DrawText(TextFormat("Frame: %d/%d", currentFrame, totalFrames), 10, 150, 14, RED);
            
            // Barra de progreso
            float progress = (float)currentFrame / (float)totalFrames;
            DrawRectangle(10, 170, 200, 10, LIGHTGRAY);
            DrawRectangle(10, 170, (int)(200 * progress), 10, RED);
        }
    }
    
    DrawText("=== CONTROLES ===", 10, 190, 14, DARKGRAY);
    DrawText("Q/E: Rotar camara horizontal", 10, 210, 14, DARKGRAY);
    DrawText("R/F: Rotar camara vertical", 10, 230, 14, DARKGRAY);
    DrawText("W/S: Zoom acercar/alejar", 10, 250, 14, DARKGRAY);
    DrawText("A/D: Desplazar camara lateralmente", 10, 270, 14, DARKGRAY);
    DrawText("Flechas: Mover/rotar personaje", 10, 290, 14, DARKGRAY);
    DrawText("TAB: Cambiar personaje controlado", 10, 310, 14, DARKGRAY);
    DrawText("", 10, 330, 14, DARKGRAY);
    DrawText("=== ACCIONES (normalizadas) ===", 10, 350, 14, DARKGREEN);
    DrawText("J: Saltar (Jump)", 10, 370, 14, DARKGREEN);
    DrawText("K: Patada (Kick)", 10, 390, 14, DARKGREEN);
    DrawText("P: Punhetazo (Punch)", 10, 410, 14, DARKGREEN);
    DrawText("* Todas se reproducen alineadas", 10, 430, 12, ORANGE);
    DrawText("  con la primera animacion", 10, 445, 12, ORANGE);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    InitWindow(1280, 720, "Bones3D - Animaciones Normalizadas");
    SetTargetFPS(60);

    GameWorld* world = CreateWorld(4);

    AddCharacter(world,
        "data/textures/zeta/bone_textures.txt",
        "data/textures/zeta/texture_sets.txt",
        "data/animations/idle.json",
        "data/animations/idle.anim",
        (Vector3){0.0f, 0.0f, 0.0f});

    AddCharacter(world,
        "data/textures/hil/bone_textures.txt",
        "data/textures/hil/texture_sets.txt",
        "data/animations/idle.json",
        "data/animations/idle.anim",
        (Vector3){1.0f, 0.0f, 0.0f});

    AddCharacter(world,
        "data/textures/eld/bone_textures.txt",
        "data/textures/eld/texture_sets.txt",
        "data/animations/idle.json",
        "data/animations/idle.anim",
        (Vector3){-1.0f, 0.0f, 0.0f});

    world->controlled = 0;
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_TAB) && world->count > 0) {
            int next = (world->controlled + 1) % world->count;
            world->controlled = next;
        }

        ProcessInput(world, dt);
        UpdateWorld(world, dt);

        BeginDrawing();
            ClearBackground(RAYWHITE);
            RenderWorld(world);
            DrawUI(world);
        EndDrawing();
    }

    DestroyWorld(world);
    CloseWindow();
    return 0;
}