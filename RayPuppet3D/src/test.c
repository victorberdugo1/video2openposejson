// test.c - Versión simplificada con control de cámara independiente
#include "bones_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// ESTRUCTURAS
// ============================================================================

typedef struct {
    AnimatedCharacter* character;
    Vector3 position;
    float rotation;
    int id;
} GameCharacter;

typedef struct {
    GameCharacter* characters;
    int count;
    int max;
    int controlled;
    Camera camera;
    float moveSpeed;
    float cameraSpeed;
} GameWorld;

// ============================================================================
// FUNCIONES DE TRANSFORMACIÓN
// ============================================================================

static Vector3 RotatePointAroundPivot(Vector3 point, Vector3 pivot, Vector3 worldPos, Matrix rotY) {
    Vector3 relative = Vector3Subtract(point, pivot);
    Vector3 rotated = Vector3Transform(relative, rotY);
    Vector3 pivotWorld = Vector3Add(worldPos, pivot);
    return Vector3Add(pivotWorld, rotated);
}

void DrawAnimatedCharacterTransformed(AnimatedCharacter* character, Camera camera,
                                      Vector3 worldPosition, float worldRotation) {
    if (!character || !character->animation.isLoaded) return;

    // Guardar cámara original
    Camera origCam = character->renderer->camera;
    character->renderer->camera = camera;

    Matrix rot = MatrixRotateY(worldRotation);
    Vector3 pivot = character->autoCenter;

    int bc = character->renderBonesCount;
    int hc = character->renderHeadsCount;
    int tc = character->renderTorsosCount;

    // Copiar datos de renderizado
    BoneRenderData* bonesCopy = NULL;
    HeadRenderData* headsCopy = NULL;
    TorsoRenderData* torsosCopy = NULL;

    if (bc > 0) {
        bonesCopy = (BoneRenderData*)malloc(sizeof(BoneRenderData) * bc);
        if (bonesCopy) memcpy(bonesCopy, character->renderBones, sizeof(BoneRenderData) * bc);
    }
    if (hc > 0) {
        headsCopy = (HeadRenderData*)malloc(sizeof(HeadRenderData) * hc);
        if (headsCopy) memcpy(headsCopy, character->renderHeads, sizeof(HeadRenderData) * hc);
    }
    if (tc > 0) {
        torsosCopy = (TorsoRenderData*)malloc(sizeof(TorsoRenderData) * tc);
        if (torsosCopy) memcpy(torsosCopy, character->renderTorsos, sizeof(TorsoRenderData) * tc);
    }

    // Transformar bones
    if (bonesCopy) {
        for (int i = 0; i < bc; i++) {
            BoneRenderData* b = &bonesCopy[i];
            if (!b->valid) continue;
            b->position = RotatePointAroundPivot(b->position, pivot, worldPosition, rot);
            if (b->orientation.valid) {
                b->orientation.forward = Vector3Transform(b->orientation.forward, rot);
                b->orientation.up = Vector3Transform(b->orientation.up, rot);
                b->orientation.right = Vector3Transform(b->orientation.right, rot);
                b->orientation.position = b->position;
                b->orientation.yaw += worldRotation;
            }
        }
    }

    // Transformar heads
    if (headsCopy) {
        for (int i = 0; i < hc; i++) {
            HeadRenderData* h = &headsCopy[i];
            if (!h->valid) continue;
            h->position = RotatePointAroundPivot(h->position, pivot, worldPosition, rot);
            if (h->orientation.valid) {
                h->orientation.forward = Vector3Transform(h->orientation.forward, rot);
                h->orientation.up = Vector3Transform(h->orientation.up, rot);
                h->orientation.right = Vector3Transform(h->orientation.right, rot);
                h->orientation.position = h->position;
                h->orientation.yaw += worldRotation;
            }
        }
    }

    // Transformar torsos
    if (torsosCopy) {
        for (int i = 0; i < tc; i++) {
            TorsoRenderData* t = &torsosCopy[i];
            t->disableCompensation = true;
            if (!t->valid) continue;
            t->position = RotatePointAroundPivot(t->position, pivot, worldPosition, rot);
            if (t->orientation.valid) {
                t->orientation.forward = Vector3Transform(t->orientation.forward, rot);
                t->orientation.up = Vector3Transform(t->orientation.up, rot);
                t->orientation.right = Vector3Transform(t->orientation.right, rot);
                t->orientation.position = t->position;
                t->orientation.yaw += worldRotation;
            }
        }
    }

    Vector3 transformedCenter = Vector3Add(worldPosition, pivot);

    // Renderizar
    BonesRenderer_RenderFrame(character->renderer,
                              bonesCopy ? bonesCopy : character->renderBones, bc,
                              headsCopy ? headsCopy : character->renderHeads, hc,
                              torsosCopy ? torsosCopy : character->renderTorsos, tc,
                              transformedCenter,
                              character->autoCenterCalculated);

    // Liberar copias
    if (bonesCopy) free(bonesCopy);
    if (headsCopy) free(headsCopy);
    if (torsosCopy) free(torsosCopy);

    // Restaurar cámara
    character->renderer->camera = origCam;
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

    // Configurar cámara
    w->camera.position = (Vector3){0.0f, 2.5f, 5.0f};
    w->camera.target = (Vector3){0.0f, 0.6f, 0.0f};
    w->camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    w->camera.fovy = 45.0f;
    w->camera.projection = CAMERA_PERSPECTIVE;

    w->moveSpeed = 1.8f;
    w->cameraSpeed = 3.0f;

    return w;
}

void DestroyWorld(GameWorld* w) {
    if (!w) return;
    for (int i = 0; i < w->count; i++) {
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

    // Crear personaje
    gc->character = CreateAnimatedCharacter(texCfg, texSets);
    if (!gc->character) {
        printf("[ERROR] Failed to create character %d\n", idx);
        return -1;
    }

    // Cargar animación
    if (!LoadAnimation(gc->character, animJson, animMeta)) {
        DestroyAnimatedCharacter(gc->character);
        printf("[ERROR] Failed to load animation for character %d\n", idx);
        return -1;
    }

    SetCharacterAutoPlay(gc->character, true);

    gc->position = pos;
    gc->rotation = 0.0f;
    gc->id = idx;

    w->count++;
    return idx;
}

void SetControlled(GameWorld* w, int idx) {
    if (!w || idx < 0 || idx >= w->count) return;
    w->controlled = idx;
}

// ============================================================================
// INPUT Y UPDATE
// ============================================================================

void ProcessInput(GameWorld* w, float dt) {
    if (!w) return;

    // ===== CÁMARA ORBITAL CON DESPLAZAMIENTO LATERAL =====
    if (w->controlled >= 0) {
        GameCharacter* c = &w->characters[w->controlled];
        
        // Ángulos y distancia persistentes
        static float angleH = 0.0f;  // Horizontal (yaw)
        static float angleV = 20.0f; // Vertical (pitch) - MÁS BAJO
        static float distance = 2.5f; // Distancia al personaje - MÁS CERCA
        static Vector3 lateralOffset = {0}; // Desplazamiento lateral
        static bool initialized = false;
        
        // Inicializar solo una vez con valores razonables
        if (!initialized) {
            angleH = 0.0f;
            angleV = 20.0f;
            distance = 2.5f;
            lateralOffset = (Vector3){0, 0, 0};
            initialized = true;
        }
        
        // Rotar horizontalmente con Q/E
        if (IsKeyDown(KEY_Q)) angleH += 90.0f * dt;
        if (IsKeyDown(KEY_E)) angleH -= 90.0f * dt;
        
        // Rotar verticalmente con R/F
        if (IsKeyDown(KEY_R)) angleV += 60.0f * dt;
        if (IsKeyDown(KEY_F)) angleV -= 60.0f * dt;
        
        // Limitar pitch vertical
        if (angleV > 60.0f) angleV = 60.0f;
        if (angleV < 5.0f) angleV = 5.0f;
        
        // Zoom con W/S
        if (IsKeyDown(KEY_W)) distance -= 3.0f * dt;
        if (IsKeyDown(KEY_S)) distance += 3.0f * dt;
        if (distance < 1.0f) distance = 1.0f;
        if (distance > 6.0f) distance = 6.0f;
        
        // Calcular vectores de la cámara
        float radH = angleH * DEG2RAD;
        float radV = angleV * DEG2RAD;
        
        // Vector hacia adelante de la cámara
        Vector3 forward = {
            cosf(radV) * sinf(radH),
            sinf(radV),
            cosf(radV) * cosf(radH)
        };
        
        // Vector derecha (perpendicular al forward)
        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, (Vector3){0, 1, 0}));
        
        // Desplazamiento lateral con A/D
        float lateralSpeed = 2.0f;
        if (IsKeyDown(KEY_A)) {
            lateralOffset = Vector3Add(lateralOffset, Vector3Scale(right, -lateralSpeed * dt));
        }
        if (IsKeyDown(KEY_D)) {
            lateralOffset = Vector3Add(lateralOffset, Vector3Scale(right, lateralSpeed * dt));
        }
        
        // Limitar desplazamiento lateral
        float lateralDist = Vector3Length(lateralOffset);
        if (lateralDist > 3.0f) {
            lateralOffset = Vector3Scale(Vector3Normalize(lateralOffset), 3.0f);
        }
        
        // Target = posición del personaje + offset lateral
        w->camera.target = Vector3Add(c->position, (Vector3){0, 0.6f, 0});
        w->camera.target = Vector3Add(w->camera.target, lateralOffset);
        
        // Posición de cámara = target + distancia orbital
        w->camera.position.x = w->camera.target.x + distance * cosf(radV) * sinf(radH);
        w->camera.position.y = w->camera.target.y + distance * sinf(radV);
        w->camera.position.z = w->camera.target.z + distance * cosf(radV) * cosf(radH);
    }

    // ===== CONTROL DE PERSONAJE (FLECHAS) =====
    if (w->controlled >= 0 && w->controlled < w->count) {
        GameCharacter* c = &w->characters[w->controlled];
        bool moving = false;

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
        if (IsKeyDown(KEY_LEFT)) {
            c->rotation += 2.5f * dt;
        }
        if (IsKeyDown(KEY_RIGHT)) {
            c->rotation -= 2.5f * dt;
        }

        // Normalizar rotación
        while (c->rotation > PI) c->rotation -= 2.0f * PI;
        while (c->rotation < -PI) c->rotation += 2.0f * PI;

        // Cambiar animación según movimiento
        static bool wasMoving = false;
        if (moving != wasMoving) {
            const char* animPath = moving ? "data/animations/walk.json" : "data/animations/idle.json";
            const char* metaPath = moving ? "data/animations/walk.anim" : "data/animations/idle.anim";
            LoadAnimation(c->character, animPath, metaPath);
            wasMoving = moving;
        }
    }
}

void UpdateWorld(GameWorld* w, float dt) {
    if (!w) return;
    for (int i = 0; i < w->count; i++) {
        UpdateAnimatedCharacter(w->characters[i].character, dt);
    }
}

// ============================================================================
// RENDER
// ============================================================================

void RenderWorld(GameWorld* w) {
    if (!w) return;

    BeginMode3D(w->camera);

    // Dibujar grid
    DrawGrid(20, 1.0f);

    // ===== ORDENAR PERSONAJES POR DISTANCIA (MÁS LEJOS PRIMERO) =====
    typedef struct {
        int index;
        float distance;
    } CharacterDistance;
    
    CharacterDistance distances[16]; // Máximo de personajes
    int validCount = 0;
    
    // Calcular distancias
    for (int i = 0; i < w->count && i < 16; i++) {
        GameCharacter* gc = &w->characters[i];
        float dist = Vector3Distance(w->camera.position, gc->position);
        distances[validCount].index = i;
        distances[validCount].distance = dist;
        validCount++;
    }
    
    // Ordenar por distancia (bubble sort - simple y suficiente para pocos personajes)
    for (int i = 0; i < validCount - 1; i++) {
        for (int j = 0; j < validCount - i - 1; j++) {
            if (distances[j].distance < distances[j + 1].distance) {
                CharacterDistance temp = distances[j];
                distances[j] = distances[j + 1];
                distances[j + 1] = temp;
            }
        }
    }
    
    // Renderizar en orden (más lejos primero)
    for (int i = 0; i < validCount; i++) {
        int idx = distances[i].index;
        GameCharacter* gc = &w->characters[idx];
        
        DrawAnimatedCharacterTransformed(gc->character, w->camera, gc->position, gc->rotation);

        // Indicador de dirección
        if (idx == w->controlled) {
            Vector3 start = gc->position;
            start.y += 0.05f;
            Vector3 end = Vector3Add(start, (Vector3){sinf(gc->rotation) * 0.5f, 0, cosf(gc->rotation) * 0.5f});
            DrawLine3D(start, end, GREEN);
            DrawSphere(end, 0.05f, GREEN);
        }

        // Indicador de posición
        DrawSphere(gc->position, 0.03f, idx == w->controlled ? BLUE : RED);
    }

    EndMode3D();
}

void DrawUI(GameWorld* w) {
    DrawText("Bones3D - Test Simplificado", 10, 10, 20, DARKGRAY);
    DrawFPS(10, 40);
    
    if (w->controlled >= 0) {
        DrawText(TextFormat("Controlando personaje: %d", w->controlled), 10, 70, 16, DARKGRAY);
    }
    
    DrawText("=== CONTROLES ===", 10, 100, 14, DARKGRAY);
    DrawText("Q/E: Rotar camara horizontal", 10, 120, 14, DARKGRAY);
    DrawText("R/F: Rotar camara vertical", 10, 140, 14, DARKGRAY);
    DrawText("W/S: Zoom acercar/alejar", 10, 160, 14, DARKGRAY);
    DrawText("A/D: Desplazar camara lateralmente", 10, 180, 14, DARKGRAY);
    DrawText("Flechas: Mover/rotar personaje", 10, 200, 14, DARKGRAY);
    DrawText("TAB: Cambiar personaje controlado", 10, 220, 14, DARKGRAY);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    InitWindow(1280, 720, "Bones3D - Test Simplificado");
    SetTargetFPS(60);

    GameWorld* world = CreateWorld(4);

    // Añadir personajes
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

    SetControlled(world, 0);

    // Loop principal
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Cambiar personaje con TAB
        if (IsKeyPressed(KEY_TAB) && world->count > 0) {
            int next = (world->controlled + 1) % world->count;
            SetControlled(world, next);
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


