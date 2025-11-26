// main.c
// Versión corregida: transiciones suaves y posicionamiento correcto de torsos

#include "bones_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
#else
    #include <unistd.h>
    #define MKDIR(path) mkdir(path, 0755)
#endif

// ------------------------------- Tipos ------------------------------------

typedef struct {
    AnimatedCharacter* character;
    Vector3 position;
    Vector3 direction;
    float rotation;
    bool isControlled;
    int id;
    char currentClip[64];
    char texCfgPath[512];
    char texSetsPath[512];
    Vector3 lastValidCenter;
    bool hasValidCenter;
} GameCharacter;

typedef struct {
    GameCharacter* characters;
    int characterCount;
    int maxCharacters;
    int controlledIndex;
    Camera camera;
    float moveSpeed;
    float rotationSpeed;
} GameWorld;

// ----------------------------- Prototipos ---------------------------------
void NormalizeCharacterOriginToFeet(AnimatedCharacter* ch);
static void EnsureUniqueRenderBuffers(GameWorld* world, AnimatedCharacter* ch);

// ----------------------------- Utilidades ---------------------------------

static float vec3len(Vector3 v) {
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

static float lerpAngleShort(float a, float b, float t) {
    float d = b - a;
    while (d > M_PI) d -= 2.0f * M_PI;
    while (d < -M_PI) d += 2.0f * M_PI;
    return a + d * t;
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static bool PlayClipIfExists(AnimationController* ac, const char* name) {
    if (!ac || !name || !name[0]) return false;
    if (AnimController_PlayClip(ac, name)) return true;

    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.anim", name);
    if (AnimController_PlayClip(ac, tmp)) return true;

    snprintf(tmp, sizeof(tmp), "data/animations/%s", name);
    if (AnimController_PlayClip(ac, tmp)) return true;

    snprintf(tmp, sizeof(tmp), "data/animations/%s.anim", name);
    if (AnimController_PlayClip(ac, tmp)) return true;

    return false;
}

static bool LoadAnimationForCharacter(GameCharacter* gc, const char* jsonPath, const char* animPath, const char* clipNameToStore) {
    if (!gc || !gc->character || !jsonPath || !animPath) return false;

    printf("[ANIM-FORCE] char %d: LoadAnimation('%s','%s')\n", gc->id, jsonPath, animPath);
    
    // Guardar el centro actual antes de cambiar la animación
    if (gc->character->autoCenterCalculated) {
        gc->lastValidCenter = gc->character->autoCenter;
        gc->hasValidCenter = true;
    }
    
    if (!LoadAnimation(gc->character, jsonPath, animPath)) {
        printf("[ANIM-FORCE] char %d: LoadAnimation FAILED for %s / %s\n", gc->id, jsonPath, animPath);
        return false;
    }

    // Normalizar primero
    NormalizeCharacterOriginToFeet(gc->character);

    // Si teníamos un centro válido previo, restaurarlo temporalmente
    // para evitar el "salto" visual durante la transición
    if (gc->hasValidCenter) {
        gc->character->autoCenter = gc->lastValidCenter;
        gc->character->autoCenterCalculated = true;
    }

    SetCharacterAutoPlay(gc->character, true);
    if (gc->character->animation.frameCount > 0) SetCharacterFrame(gc->character, 0);

    if (clipNameToStore) {
        strncpy(gc->currentClip, clipNameToStore, sizeof(gc->currentClip)-1);
        gc->currentClip[sizeof(gc->currentClip)-1] = '\0';
    }

    printf("[ANIM-FORCE] char %d: now playing '%s' (frames=%d)\n", gc->id, gc->currentClip, gc->character->animation.frameCount);
    return true;
}

// ----------------------- Normalización origen (pies = y=0) -----------------
void NormalizeCharacterOriginToFeet(AnimatedCharacter* ch) {
    if (!ch) return;
    int n = ch->renderBonesCount;
    if (n <= 0) return;

    float minY = 1e9f;
    bool found = false;
    for (int i = 0; i < n; ++i) {
        BoneRenderData* b = &ch->renderBones[i];
        if (!b->valid) continue;
        if (!found || b->position.y < minY) minY = b->position.y;
        found = true;
    }
    if (!found) return;

    Vector3 delta = (Vector3){0.0f, -minY, 0.0f};
    for (int i = 0; i < n; ++i) {
        BoneRenderData* b = &ch->renderBones[i];
        if (!b->valid) continue;
        b->position = Vector3Add(b->position, delta);
        if (b->orientation.valid) b->orientation.position = Vector3Add(b->orientation.position, delta);
    }
    for (int i = 0; i < ch->renderHeadsCount; ++i) {
        HeadRenderData* h = &ch->renderHeads[i];
        if (!h->valid) continue;
        h->position = Vector3Add(h->position, delta);
        if (h->orientation.valid) h->orientation.position = Vector3Add(h->orientation.position, delta);
    }
    for (int i = 0; i < ch->renderTorsosCount; ++i) {
        TorsoRenderData* t = &ch->renderTorsos[i];
        if (!t->valid) continue;
        t->position = Vector3Add(t->position, delta);
        if (t->orientation.valid) t->orientation.position = Vector3Add(t->orientation.position, delta);
    }

    // NO resetear autoCenter aquí, dejar que se calcule naturalmente
}

// ---------------------- Draw transformado (girar alrededor pivot) ---------

static Vector3 rotatePointAroundPivot(const Vector3 src, const Vector3 pivotLocal, const Vector3 worldPos, const Matrix rotY) {
    Vector3 rel = Vector3Subtract(src, pivotLocal);
    Vector3 rotated = Vector3Transform(rel, rotY);
    Vector3 pivotWorld = Vector3Add(worldPos, pivotLocal);
    return Vector3Add(pivotWorld, rotated);
}

void DrawAnimatedCharacterTransformed(AnimatedCharacter* character, Camera camera,
                                      Vector3 worldPosition, float worldRotation) {
    if (!character || !character->animation.isLoaded) return;
    Camera origCam = character->renderer->camera;
    character->renderer->camera = camera;

    Matrix rot = MatrixRotateY(worldRotation);
    Vector3 pivot = character->autoCenter;

    int bc = character->renderBonesCount;
    int hc = character->renderHeadsCount;
    int tc = character->renderTorsosCount;

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

    if (bonesCopy) {
        for (int i = 0; i < bc; ++i) {
            BoneRenderData* b = &bonesCopy[i];
            if (!b->valid) continue;
            b->position = rotatePointAroundPivot(b->position, pivot, worldPosition, rot);
            if (b->orientation.valid) {
                b->orientation.forward = Vector3Transform(b->orientation.forward, rot);
                b->orientation.up = Vector3Transform(b->orientation.up, rot);
                b->orientation.right = Vector3Transform(b->orientation.right, rot);
                b->orientation.position = b->position;
                b->orientation.yaw += worldRotation;
            }
        }
    }

    if (headsCopy) {
        for (int i = 0; i < hc; ++i) {
            HeadRenderData* h = &headsCopy[i];
            if (!h->valid) continue;
            h->position = rotatePointAroundPivot(h->position, pivot, worldPosition, rot);
            if (h->orientation.valid) {
                h->orientation.forward = Vector3Transform(h->orientation.forward, rot);
                h->orientation.up = Vector3Transform(h->orientation.up, rot);
                h->orientation.right = Vector3Transform(h->orientation.right, rot);
                h->orientation.position = h->position;
                h->orientation.yaw += worldRotation;
            }
        }
    }

    if (torsosCopy) {
        for (int i = 0; i < tc; ++i) {
            TorsoRenderData* t = &torsosCopy[i];
	    t->disableCompensation = true;//TODO: compensate *outRotation = pitchToChest ± 80;
            if (!t->valid) continue;
            t->position = rotatePointAroundPivot(t->position, pivot, worldPosition, rot);
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

    BonesRenderer_RenderFrame(character->renderer,
                              bonesCopy ? bonesCopy : character->renderBones, bc,
                              headsCopy ? headsCopy : character->renderHeads, hc,
                              torsosCopy ? torsosCopy : character->renderTorsos, tc,
                              transformedCenter,
                              character->autoCenterCalculated);

    if (bonesCopy) free(bonesCopy);
    if (headsCopy) free(headsCopy);
    if (torsosCopy) free(torsosCopy);

    character->renderer->camera = origCam;
}

// ---------------- Helper para duplicar buffers si están compartidos -------

static void EnsureUniqueRenderBuffers(GameWorld* world, AnimatedCharacter* ch) {
    if (!world || !ch) return;

    if (world->characterCount <= 0) return;

    for (int i = 0; i < world->characterCount; ++i) {
        AnimatedCharacter* other = world->characters[i].character;
        if (!other || other == ch) continue;

        // bones
        if (ch->renderBones && other->renderBones && ch->renderBones == other->renderBones && ch->renderBonesCount > 0) {
            size_t sz = sizeof(BoneRenderData) * (size_t)ch->renderBonesCount;
            BoneRenderData* nb = (BoneRenderData*)malloc(sz);
            if (nb) {
                memcpy(nb, ch->renderBones, sz);
                ch->renderBones = nb;
                printf("[DBG] Duplicated renderBones for new character (count=%d)\n", ch->renderBonesCount);
            }
        }
        // heads
        if (ch->renderHeads && other->renderHeads && ch->renderHeads == other->renderHeads && ch->renderHeadsCount > 0) {
            size_t sz = sizeof(HeadRenderData) * (size_t)ch->renderHeadsCount;
            HeadRenderData* nh = (HeadRenderData*)malloc(sz);
            if (nh) {
                memcpy(nh, ch->renderHeads, sz);
                ch->renderHeads = nh;
                printf("[DBG] Duplicated renderHeads for new character (count=%d)\n", ch->renderHeadsCount);
            }
        }
        // torsos
        if (ch->renderTorsos && other->renderTorsos && ch->renderTorsos == other->renderTorsos && ch->renderTorsosCount > 0) {
            size_t sz = sizeof(TorsoRenderData) * (size_t)ch->renderTorsosCount;
            TorsoRenderData* nt = (TorsoRenderData*)malloc(sz);
            if (nt) {
                memcpy(nt, ch->renderTorsos, sz);
                ch->renderTorsos = nt;
                printf("[DBG] Duplicated renderTorsos for new character (count=%d)\n", ch->renderTorsosCount);
            }
        }
    }
}

// -------------------------- Mundo y gestión -------------------------------

GameWorld* CreateGameWorld(int maxChars) {
    GameWorld* w = (GameWorld*)calloc(1, sizeof(GameWorld));
    w->maxCharacters = maxChars;
    w->characters = (GameCharacter*)calloc(maxChars, sizeof(GameCharacter));
    w->characterCount = 0;
    w->controlledIndex = -1;

    w->camera.position = (Vector3){0.0f, 2.5f, 5.0f};
    w->camera.target = (Vector3){0.0f, 0.6f, 0.0f};
    w->camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    w->camera.fovy = 45.0f;
    w->camera.projection = CAMERA_PERSPECTIVE;

    w->moveSpeed = 1.8f;
    w->rotationSpeed = 3.5f;

    return w;
}

int AddCharacterToWorld(GameWorld* w, const char* texCfg, const char* texSets,
                        const char* animJson, const char* animMeta, Vector3 startPos) {
    if (!w || w->characterCount >= w->maxCharacters) return -1;
    int idx = w->characterCount;
    GameCharacter* gc = &w->characters[idx];

    // Inicializar campos nuevos
    gc->lastValidCenter = (Vector3){0, 0, 0};
    gc->hasValidCenter = false;

    strncpy(gc->texCfgPath, texCfg, sizeof(gc->texCfgPath)-1);
    gc->texCfgPath[sizeof(gc->texCfgPath)-1] = '\0';
    strncpy(gc->texSetsPath, texSets, sizeof(gc->texSetsPath)-1);
    gc->texSetsPath[sizeof(gc->texSetsPath)-1] = '\0';

    if (!file_exists(gc->texCfgPath)) {
        printf("[LOAD] Missing texCfg for char %d: %s\n", idx, gc->texCfgPath);
    }
    if (!file_exists(gc->texSetsPath)) {
        printf("[LOAD] Missing texSets for char %d: %s\n", idx, gc->texSetsPath);
    }

    gc->character = CreateAnimatedCharacter(gc->texCfgPath, gc->texSetsPath);
    if (!gc->character) {
        printf("[LOAD] CreateAnimatedCharacter FAILED for char %d\n", idx);
        return -1;
    }

    if (!LoadAnimation(gc->character, animJson, animMeta)) {
        DestroyAnimatedCharacter(gc->character);
        printf("[LOAD] Failed to load anim for char %d: %s / %s\n", idx, animJson, animMeta);
        return -1;
    }

    NormalizeCharacterOriginToFeet(gc->character);

    EnsureUniqueRenderBuffers(w, gc->character);

    printf("[DBG] char %d created: renderBones=%p renderHeads=%p renderTorsos=%p renderer=%p textureSets=%p\n",
           idx, (void*)gc->character->renderBones, (void*)gc->character->renderHeads,
           (void*)gc->character->renderTorsos, (void*)gc->character->renderer,
           (void*)gc->character->textureSets);

    gc->position = startPos;
    gc->direction = (Vector3){0,0,1};
    gc->rotation = 0.0f;
    gc->isControlled = false;
    gc->id = idx;
    gc->currentClip[0] = '\0';

    SetCharacterAutoPlay(gc->character, true);

    if (gc->character->animController) {
        if (PlayClipIfExists(gc->character->animController, "idle")) {
            strncpy(gc->currentClip, "idle", sizeof(gc->currentClip)-1);
            gc->currentClip[sizeof(gc->currentClip)-1] = '\0';
        } else {
            gc->currentClip[0] = '\0';
        }
    }

    w->characterCount++;
    return idx;
}

void SetControlled(GameWorld* w, int idx) {
    if (!w || idx < 0 || idx >= w->characterCount) return;
    if (w->controlledIndex >= 0) w->characters[w->controlledIndex].isControlled = false;
    w->controlledIndex = idx;
    w->characters[idx].isControlled = true;
}

void MoveCharacter(GameCharacter* c, Vector3 delta) {
    c->position = Vector3Add(c->position, delta);
}

void RotateCharacter(GameCharacter* c, float d) {
    c->rotation += d;
    while (c->rotation > PI) c->rotation -= 2.0f * PI;
    while (c->rotation < -PI) c->rotation += 2.0f * PI;
    c->direction.x = sinf(c->rotation);
    c->direction.z = cosf(c->rotation);
    float l = sqrtf(c->direction.x*c->direction.x + c->direction.z*c->direction.z);
    if (l > 0) { c->direction.x /= l; c->direction.z /= l; }
}

// ------------------------- Entrada y animación ----------------------------

void ProcessPlayerInput(GameWorld* w, float dt) {
    if (!w || w->controlledIndex < 0) return;
    GameCharacter* c = &w->characters[w->controlledIndex];

    float ix = 0.0f, iz = 0.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) iz -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) iz += 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) ix += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) ix -= 1.0f;
    if (IsKeyDown(KEY_Q)) ix -= 1.0f;
    if (IsKeyDown(KEY_E)) ix += 1.0f;

    Vector3 mv = (Vector3){ix, 0.0f, iz};
    float ml = vec3len(mv);
    bool moving = ml > 1e-4f;

    if (moving) {
        mv.x /= ml; mv.z /= ml;
        MoveCharacter(c, Vector3Scale(mv, w->moveSpeed * dt));
        float targetYaw = atan2f(mv.x, mv.z);
        c->rotation = lerpAngleShort(c->rotation, targetYaw, fminf(12.0f * dt, 1.0f));
        c->direction.x = sinf(c->rotation); c->direction.z = cosf(c->rotation);
    } else {
        float rotIn = 0.0f;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) rotIn += 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) rotIn -= 1.0f;
        if (rotIn != 0.0f) RotateCharacter(c, rotIn * w->rotationSpeed * dt);
    }

    const char* targetClip = moving ? "walk" : "idle";

    if (strcmp(c->currentClip, targetClip) != 0) {
        if (!LoadAnimationForCharacter(c,
            moving ? "data/animations/walk.json" : "data/animations/idle.json",
            moving ? "data/animations/walk.anim" : "data/animations/idle.anim",
            targetClip)) {
            if (c->character && c->character->animController) {
                if (PlayClipIfExists(c->character->animController, targetClip)) {
                    strncpy(c->currentClip, targetClip, sizeof(c->currentClip)-1);
                    c->currentClip[sizeof(c->currentClip)-1] = '\0';
                }
            }
        }
    }
}

// ------------------------- Update / Render --------------------------------

void UpdateGameWorld(GameWorld* w, float dt) {
    if (!w) return;
    ProcessPlayerInput(w, dt);
    for (int i = 0; i < w->characterCount; ++i) {
        UpdateAnimatedCharacter(w->characters[i].character, dt);
        
        // Actualizar el centro guardado si es válido
        GameCharacter* gc = &w->characters[i];
        if (gc->character->autoCenterCalculated) {
            gc->lastValidCenter = gc->character->autoCenter;
            gc->hasValidCenter = true;
        }
    }
}

void RenderGameWorld(GameWorld* w) {
    if (!w) return;
    BeginMode3D(w->camera);

    for (int i = 0; i < w->characterCount; ++i) {
        GameCharacter* gc = &w->characters[i];
        DrawAnimatedCharacterTransformed(gc->character, w->camera, gc->position, gc->rotation);
        if (gc->isControlled) {
            Vector3 end = Vector3Add(gc->position, Vector3Scale(gc->direction, 0.5f));
            end.y = gc->position.y + 0.05f;
            Vector3 start = gc->position; start.y += 0.05f;
            DrawLine3D(start, end, GREEN);
            DrawSphere(end, 0.05f, GREEN);
        }
        DrawSphere(gc->position, 0.03f, gc->isControlled ? BLUE : RED);
    }
    EndMode3D();
}

// ----------------------------- UI ----------------------------------------

void DrawUI(GameWorld* w) {
    DrawText("Bones3D - Multi characters (Fixed Transitions)", 10, 10, 20, DARKGRAY);
    DrawFPS(10, 40);
    if (w->controlledIndex >= 0) {
        GameCharacter* c = &w->characters[w->controlledIndex];
        DrawText(TextFormat("Control: %d  Clip: %s", c->id, c->currentClip[0] ? c->currentClip : "none"), 10, 70, 16, DARKGRAY);
    }
    DrawText("WASD/Arrows - move  A/D rotate", 10, 100, 14, DARKGRAY);
    DrawText("TAB - switch character", 10, 120, 14, DARKGRAY);
}

// ----------------------------- Cleanup ------------------------------------

void DestroyGameWorld(GameWorld* w) {
    if (!w) return;
    for (int i = 0; i < w->characterCount; ++i) {
        DestroyAnimatedCharacter(w->characters[i].character);
    }
    free(w->characters);
    free(w);
}

// -------------------------------- MAIN ------------------------------------

int main(void) {
    InitWindow(1280, 720, "Bones3D - Fixed Transitions & Torso Positioning");
    SetTargetFPS(60);

    GameWorld* world = CreateGameWorld(8);

    int c0 = AddCharacterToWorld(world,
        "data/textures/zeta/bone_textures.txt",
        "data/textures/zeta/texture_sets.txt",
        "data/animations/idle.json",
        "data/animations/idle.anim",
        (Vector3){ -1.0f, 1.2f, 2.0f });

    int c1 = AddCharacterToWorld(world,
        "data/textures/hil/bone_textures.txt",
        "data/textures/hil/texture_sets.txt",
        "data/animations/idle.json",
        "data/animations/idle.anim",
        (Vector3){ 1.0f, 1.2f, 0.0f });

    SetControlled(world, c0 >= 0 ? c0 : c1);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_TAB) && world->characterCount > 0) {
            int next = (world->controlledIndex + 1) % world->characterCount;
            SetControlled(world, next);
        }

        UpdateGameWorld(world, dt);

        BeginDrawing();
            ClearBackground(RAYWHITE);
            RenderGameWorld(world);
            DrawUI(world);
        EndDrawing();
    }

    DestroyGameWorld(world);
    CloseWindow();
    return 0;
}