/*
 * +=======================================================================+
 * |              BONES ANIMATION EVENTS & TEXTURE SETS SYSTEM             |
 * |                         PURE C SINGLE HEADER                          |
 * +=======================================================================+
 * | Sistema de eventos de animacion con texture sets automaticos.        |
 * | Permite definir cambios de textura en timeline de animacion.         |
 * |                                                                       |
 * | ARQUITECTURA:                                                         |
 * | [OpenPose JSON] -> [bones_core] -> [bones_animation] -> [renderer]   |
 * |       |                                  ^                            |
 * |   (poses puras)                  [Animation Metadata]                |
 * |                                  - texture_sets                       |
 * |                                  - timing/fps                         |
 * |                                  - eventos                            |
 * |                                                                       |
 * | ESTRUCTURA DE ARCHIVOS:                                               |
 * | project/                                                              |
 * | ├── data/                                                             |
 * | │   ├── poses/                                                        |
 * | │   │   └── idle.json              <- OpenPose JSON (sin tocar)      |
 * | │   ├── animations/                                                   |
 * | │   │   └── idle_anim.json         <- Metadata con eventos           |
 * | │   └── textures/                                                     |
 * | │       ├── bone_textures.txt      <- Config texturas base           |
 * | │       └── texture_sets.txt       <- Variantes de texturas          |
 * |                                                                       |
 * | FORMATO texture_sets.txt:                                             |
 * | # Bone Variant TexturePath                                            |
 * | Head idle tex/Head1.png                                               |
 * | Head blink tex/Head_Blink.png                                         |
 * | Head talk_open tex/Head_Talk1.png                                     |
 * | Head talk_closed tex/Head_Talk2.png                                   |
 * |                                                                       |
 * | FORMATO idle_anim.json:                                               |
 * | {                                                                     |
 * |   "name": "idle",                                                     |
 * |   "fps": 30,                                                          |
 * |   "start_frame": 0,                                                   |
 * |   "end_frame": 60,                                                    |
 * |   "loop": true,                                                       |
 * |   "events": [                                                         |
 * |     {"time": 0.5, "type": "texture", "bone": "Head",                 |
 * |      "person": "person_0", "variant": "blink"},                      |
 * |     {"time": 1.0, "type": "texture", "bone": "Head",                 |
 * |      "variant": "talk_open"},                                        |
 * |     {"time": 1.1, "type": "texture", "bone": "Head",                 |
 * |      "variant": "talk_closed"}                                       |
 * |   ]                                                                   |
 * | }                                                                     |
 * |                                                                       |
 * | USO:                                                                  |
 * | 1. #define BONES_ANIMATION_EVENTS_IMPLEMENTATION                      |
 * | 2. Cargar texture_sets.txt                                            |
 * | 3. Cargar idle_anim.json                                              |
 * | 4. Update(deltaTime) -> procesa eventos automaticamente               |
 * +=======================================================================+
 */

#ifndef BONES_CORE_H
#define BONES_CORE_H

#include <stdbool.h>


/* ===== CONFIGURATION ===== */
#ifndef BONES_MAX_TEXTURE_VARIANTS
#define BONES_MAX_TEXTURE_VARIANTS 16
#endif

#ifndef BONES_MAX_TEXTURE_SETS
#define BONES_MAX_TEXTURE_SETS 64
#endif

#ifndef BONES_MAX_ANIM_EVENTS
#define BONES_MAX_ANIM_EVENTS 256
#endif

#ifndef BONES_MAX_ANIM_CLIPS
#define BONES_MAX_ANIM_CLIPS 32
#endif

#ifndef BONES_AE_MAX_NAME
#define BONES_AE_MAX_NAME 64
#endif

#ifndef BONES_AE_MAX_PATH
#define BONES_AE_MAX_PATH 256
#endif

/* ===== TEXTURE SETS ===== */

typedef struct {
    char variantName[BONES_AE_MAX_NAME];
    char texturePath[BONES_AE_MAX_PATH];
    bool valid;
} BoneTextureVariant;

typedef struct {
    char boneName[BONES_AE_MAX_NAME];
    BoneTextureVariant variants[BONES_MAX_TEXTURE_VARIANTS];
    int variantCount;
    int activeVariantIndex;
    bool valid;
} BoneTextureSet;

typedef struct {
    BoneTextureSet sets[BONES_MAX_TEXTURE_SETS];
    int setCount;
    bool loaded;
} TextureSetCollection;

/* ===== ANIMATION EVENTS ===== */

typedef enum {
    ANIM_EVENT_TEXTURE,      /* Cambiar textura de un bone */
    ANIM_EVENT_SOUND,        /* Trigger de sonido */
    ANIM_EVENT_PARTICLE,     /* Spawn de particula */
    ANIM_EVENT_CUSTOM        /* Usuario define */
} AnimEventType;

typedef struct {
    float time;                          /* Tiempo en segundos desde inicio */
    AnimEventType type;
    char boneName[BONES_AE_MAX_NAME];    /* "Head", "LWrist" */
    char personId[BONES_AE_MAX_NAME];    /* "person_0" (opcional) */
    char variantName[BONES_AE_MAX_NAME]; /* "blink", "talk_open" */
    char stringParam[BONES_AE_MAX_NAME]; /* Parametro extra (sound path, etc) */
    bool processed;                      /* Flag interno para evitar re-trigger */
    bool valid;
} AnimationEvent;

/* ===== ANIMATION CLIP METADATA ===== */

typedef struct {
    char name[BONES_AE_MAX_NAME];        /* "idle", "walk", "attack" */
    float fps;                           /* Frames por segundo */
    int startFrame;                      /* Frame inicial en el JSON */
    int endFrame;                        /* Frame final */
    bool loop;                           /* Loop automatico */
    
    AnimationEvent events[BONES_MAX_ANIM_EVENTS];
    int eventCount;
    
    bool valid;
} AnimationClipMetadata;

/* ===== ANIMATION CONTROLLER ===== */

typedef struct {
    TextureSetCollection* textureSets;   /* Puntero a texture sets */
    
    AnimationClipMetadata clips[BONES_MAX_ANIM_CLIPS];
    int clipCount;
    int currentClipIndex;
    
    float localTime;                     /* Tiempo dentro del clip actual */
    bool playing;
    
    /* Para integracion con tu BonesAnimation existente */
    void* bonesAnimation;                /* Puntero a tu BonesAnimation* */
    int currentFrameInJSON;              /* Frame actual en el JSON */
    
    bool valid;
} AnimationController;

/* ===== TEXTURE SETS API ===== */

TextureSetCollection* BonesTextureSets_Create(void);
void BonesTextureSets_Free(TextureSetCollection* collection);
bool BonesTextureSets_LoadFromFile(TextureSetCollection* collection, const char* filePath);
const char* BonesTextureSets_GetActiveTexture(const TextureSetCollection* collection, const char* boneName);
bool BonesTextureSets_SetVariant(TextureSetCollection* collection, const char* boneName, const char* variantName);
const char* BonesTextureSets_GetActiveVariantName(const TextureSetCollection* collection, const char* boneName);
void BonesTextureSets_ResetAll(TextureSetCollection* collection);
BoneTextureSet* BonesTextureSets_FindSet(TextureSetCollection* collection, const char* boneName);

/* ===== ANIMATION CONTROLLER API ===== */

/**
 * Crear controlador de animacion
 * 
 * @param bonesAnimation Puntero a tu estructura BonesAnimation* existente
 * @param textureSets Puntero a TextureSetCollection ya cargada
 * @return Nuevo controlador
 */
AnimationController* AnimController_Create(void* bonesAnimation, TextureSetCollection* textureSets);

/**
 * Cargar metadata de animacion desde JSON
 * 
 * Formato:
 * {
 *   "name": "idle",
 *   "fps": 30,
 *   "start_frame": 0,
 *   "end_frame": 60,
 *   "loop": true,
 *   "events": [
 *     {"time": 0.5, "type": "texture", "bone": "Head", "variant": "blink"},
 *     {"time": 1.0, "type": "texture", "bone": "Head", "variant": "talk_open"}
 *   ]
 * }
 * 
 * @param controller Controlador
 * @param jsonPath Ruta al archivo idle_anim.json
 * @return true si se cargo correctamente
 */
bool AnimController_LoadClipMetadata(AnimationController* controller, const char* jsonPath);

/**
 * Reproducir un clip por nombre
 * 
 * @param controller Controlador
 * @param clipName Nombre del clip ("idle", "walk", etc)
 * @return true si se encontro el clip
 */
bool AnimController_PlayClip(AnimationController* controller, const char* clipName);

/**
 * Pausar/reanudar
 */
void AnimController_Pause(AnimationController* controller);
void AnimController_Resume(AnimationController* controller);

/**
 * Update - procesa eventos automaticamente
 * 
 * Llama esto cada frame con deltaTime.
 * Automaticamente:
 * - Avanza el tiempo
 * - Calcula el frame actual del JSON
 * - Procesa eventos en el timeline
 * - Cambia texturas cuando toca
 * 
 * @param controller Controlador
 * @param deltaTime Tiempo transcurrido (GetFrameTime())
 */
void AnimController_Update(AnimationController* controller, float deltaTime);

/**
 * Obtener el frame actual que deberia mostrarse del JSON
 * 
 * @param controller Controlador
 * @return Frame index para pasarle a BonesSetFrame()
 */
int AnimController_GetCurrentFrame(const AnimationController* controller);

/**
 * Liberar memoria
 */
void AnimController_Free(AnimationController* controller);

/* ===== IMPLEMENTATION ===== */
#ifdef BONES_ANIMATION_EVENTS_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ===== TEXTURE SETS IMPLEMENTATION ===== */

TextureSetCollection* BonesTextureSets_Create(void) {
    TextureSetCollection* collection;
    collection = (TextureSetCollection*)calloc(1, sizeof(TextureSetCollection));
    if (!collection) return NULL;
    collection->loaded = false;
    collection->setCount = 0;
    return collection;
}

void BonesTextureSets_Free(TextureSetCollection* collection) {
    if (collection) free(collection);
}

BoneTextureSet* BonesTextureSets_FindSet(TextureSetCollection* collection, const char* boneName) {
    int i;
    if (!collection || !boneName) return NULL;
    for (i = 0; i < collection->setCount; i++) {
        if (strcmp(collection->sets[i].boneName, boneName) == 0) {
            return &collection->sets[i];
        }
    }
    return NULL;
}

bool BonesTextureSets_LoadFromFile(TextureSetCollection* collection, const char* filePath) {
    FILE* file;
    char line[512];
    int lineNum;
    char boneName[BONES_AE_MAX_NAME];
    char variantName[BONES_AE_MAX_NAME];
    char texturePath[BONES_AE_MAX_PATH];
    BoneTextureSet* set;
    BoneTextureVariant* variant;
    
    if (!collection || !filePath) return false;
    
    file = fopen(filePath, "r");
    if (!file) {
        printf("TEXTURE_SETS: Failed to open %s\n", filePath);
        return false;
    }
    
    collection->setCount = 0;
    collection->loaded = false;
    lineNum = 0;
    
    while (fgets(line, sizeof(line), file) && collection->setCount < BONES_MAX_TEXTURE_SETS) {
        lineNum++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        if (sscanf(line, "%63s %63s %255s", boneName, variantName, texturePath) != 3) {
            printf("TEXTURE_SETS: Invalid line %d\n", lineNum);
            continue;
        }
        
        set = BonesTextureSets_FindSet(collection, boneName);
        
        if (!set) {
            if (collection->setCount >= BONES_MAX_TEXTURE_SETS) break;
            set = &collection->sets[collection->setCount];
            memset(set, 0, sizeof(BoneTextureSet));
            strncpy(set->boneName, boneName, BONES_AE_MAX_NAME - 1);
            set->boneName[BONES_AE_MAX_NAME - 1] = '\0';
            set->variantCount = 0;
            set->activeVariantIndex = 0;
            set->valid = true;
            collection->setCount++;
        }
        
        if (set->variantCount >= BONES_MAX_TEXTURE_VARIANTS) continue;
        
        variant = &set->variants[set->variantCount];
        strncpy(variant->variantName, variantName, BONES_AE_MAX_NAME - 1);
        variant->variantName[BONES_AE_MAX_NAME - 1] = '\0';
        strncpy(variant->texturePath, texturePath, BONES_AE_MAX_PATH - 1);
        variant->texturePath[BONES_AE_MAX_PATH - 1] = '\0';
        variant->valid = true;
        set->variantCount++;
        
        printf("TEXTURE_SETS: %s.%s -> %s\n", boneName, variantName, texturePath);
    }
    
    fclose(file);
    
    if (collection->setCount > 0) {
        collection->loaded = true;
        printf("TEXTURE_SETS: Loaded %d sets\n", collection->setCount);
        return true;
    }
    return false;
}

const char* BonesTextureSets_GetActiveTexture(const TextureSetCollection* collection, const char* boneName) {
    int i;
    const BoneTextureSet* set;
    if (!collection || !boneName || !collection->loaded) return NULL;
    for (i = 0; i < collection->setCount; i++) {
        set = &collection->sets[i];
        if (strcmp(set->boneName, boneName) == 0 && set->valid) {
            if (set->activeVariantIndex >= 0 && set->activeVariantIndex < set->variantCount) {
                return set->variants[set->activeVariantIndex].texturePath;
            }
        }
    }
    return NULL;
}

bool BonesTextureSets_SetVariant(TextureSetCollection* collection, const char* boneName, const char* variantName) {
    BoneTextureSet* set;
    int i;
    if (!collection || !boneName || !variantName) return false;
    set = BonesTextureSets_FindSet(collection, boneName);
    if (!set) return false;
    for (i = 0; i < set->variantCount; i++) {
        if (strcmp(set->variants[i].variantName, variantName) == 0) {
            set->activeVariantIndex = i;
            printf("TEXTURE_SETS: %s -> %s (%s)\n", boneName, variantName, set->variants[i].texturePath);
            return true;
        }
    }
    return false;
}

const char* BonesTextureSets_GetActiveVariantName(const TextureSetCollection* collection, const char* boneName) {
    int i;
    const BoneTextureSet* set;
    if (!collection || !boneName || !collection->loaded) return NULL;
    for (i = 0; i < collection->setCount; i++) {
        set = &collection->sets[i];
        if (strcmp(set->boneName, boneName) == 0 && set->valid) {
            if (set->activeVariantIndex >= 0 && set->activeVariantIndex < set->variantCount) {
                return set->variants[set->activeVariantIndex].variantName;
            }
        }
    }
    return NULL;
}

void BonesTextureSets_ResetAll(TextureSetCollection* collection) {
    int i;
    if (!collection) return;
    for (i = 0; i < collection->setCount; i++) {
        collection->sets[i].activeVariantIndex = 0;
    }
}

/* ===== ANIMATION CONTROLLER IMPLEMENTATION ===== */

AnimationController* AnimController_Create(void* bonesAnimation, TextureSetCollection* textureSets) {
    AnimationController* controller;
    controller = (AnimationController*)calloc(1, sizeof(AnimationController));
    if (!controller) return NULL;
    
    controller->bonesAnimation = bonesAnimation;
    controller->textureSets = textureSets;
    controller->clipCount = 0;
    controller->currentClipIndex = -1;
    controller->localTime = 0.0f;
    controller->playing = false;
    controller->currentFrameInJSON = 0;
    controller->valid = true;
    
    return controller;
}

void AnimController_Free(AnimationController* controller) {
    if (controller) free(controller);
}

/* Helper: Parse simple JSON value */
static bool ParseJSONFloat(const char* json, const char* key, float* outValue) {
    char searchKey[128];
    const char* pos;
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    pos = strstr(json, searchKey);
    if (pos && sscanf(pos + strlen(searchKey), "%f", outValue) == 1) {
        return true;
    }
    return false;
}

static bool ParseJSONInt(const char* json, const char* key, int* outValue) {
    char searchKey[128];
    const char* pos;
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    pos = strstr(json, searchKey);
    if (pos && sscanf(pos + strlen(searchKey), "%d", outValue) == 1) {
        return true;
    }
    return false;
}

static bool ParseJSONString(const char* json, const char* key, char* outValue, int maxLen) {
    char searchKey[128];
    const char* pos;
    const char* start;
    const char* end;
    int len;
    
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    pos = strstr(json, searchKey);
    if (!pos) return false;
    
    start = strchr(pos + strlen(searchKey), '"');
    if (!start) return false;
    start++;
    
    end = strchr(start, '"');
    if (!end) return false;
    
    len = (int)(end - start);
    if (len >= maxLen) len = maxLen - 1;
    
    strncpy(outValue, start, len);
    outValue[len] = '\0';
    return true;
}

bool AnimController_LoadClipMetadata(AnimationController* controller, const char* jsonPath) {
    FILE* file;
    char* jsonData;
    long fileSize;
    AnimationClipMetadata* clip;
    const char* eventsStart;
    const char* eventPos;
    AnimationEvent* event;
    float eventTime;
    char eventType[32];
    char boneName[BONES_AE_MAX_NAME];
    char variantName[BONES_AE_MAX_NAME];
    char personId[BONES_AE_MAX_NAME];
    bool loopValue;
    
    if (!controller || !jsonPath) return false;
    
    file = fopen(jsonPath, "rb");
    if (!file) {
        printf("ANIM_METADATA: Failed to open %s\n", jsonPath);
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    jsonData = (char*)malloc(fileSize + 1);
    if (!jsonData) {
        fclose(file);
        return false;
    }
    
    fread(jsonData, 1, fileSize, file);
    jsonData[fileSize] = '\0';
    fclose(file);
    
    if (controller->clipCount >= BONES_MAX_ANIM_CLIPS) {
        free(jsonData);
        return false;
    }
    
    clip = &controller->clips[controller->clipCount];
    memset(clip, 0, sizeof(AnimationClipMetadata));
    
    /* Parse basic metadata */
    if (!ParseJSONString(jsonData, "name", clip->name, BONES_AE_MAX_NAME)) {
        strcpy(clip->name, "unnamed");
    }
    
    if (!ParseJSONFloat(jsonData, "fps", &clip->fps)) {
        clip->fps = 30.0f;
    }
    
    if (!ParseJSONInt(jsonData, "start_frame", &clip->startFrame)) {
        clip->startFrame = 0;
    }
    
    if (!ParseJSONInt(jsonData, "end_frame", &clip->endFrame)) {
        clip->endFrame = 60;
    }
    
    /* Parse loop (buscar true/false) */
    loopValue = false;
    if (strstr(jsonData, "\"loop\": true") || strstr(jsonData, "\"loop\":true")) {
        loopValue = true;
    }
    clip->loop = loopValue;
    
    /* Parse events array */
    clip->eventCount = 0;
    eventsStart = strstr(jsonData, "\"events\":");
    if (eventsStart) {
        eventsStart = strchr(eventsStart, '[');
        if (eventsStart) {
            eventPos = eventsStart;
            while (clip->eventCount < BONES_MAX_ANIM_EVENTS) {
                eventPos = strchr(eventPos + 1, '{');
                if (!eventPos || eventPos > strstr(eventsStart, "]")) break;
                
                event = &clip->events[clip->eventCount];
                memset(event, 0, sizeof(AnimationEvent));
                
                /* Parse event fields */
                if (!ParseJSONFloat(eventPos, "time", &eventTime)) continue;
                event->time = eventTime;
                
                if (!ParseJSONString(eventPos, "type", eventType, sizeof(eventType))) continue;
                
                if (strcmp(eventType, "texture") == 0) {
                    event->type = ANIM_EVENT_TEXTURE;
                } else if (strcmp(eventType, "sound") == 0) {
                    event->type = ANIM_EVENT_SOUND;
                } else {
                    event->type = ANIM_EVENT_CUSTOM;
                }
                
                ParseJSONString(eventPos, "bone", boneName, BONES_AE_MAX_NAME);
                strncpy(event->boneName, boneName, BONES_AE_MAX_NAME - 1);
                
                ParseJSONString(eventPos, "variant", variantName, BONES_AE_MAX_NAME);
                strncpy(event->variantName, variantName, BONES_AE_MAX_NAME - 1);
                
                if (ParseJSONString(eventPos, "person", personId, BONES_AE_MAX_NAME)) {
                    strncpy(event->personId, personId, BONES_AE_MAX_NAME - 1);
                }
                
                event->processed = false;
                event->valid = true;
                
                printf("ANIM_EVENT: t=%.2fs %s.%s -> %s\n", 
                       event->time, event->boneName, event->variantName, eventType);
                
                clip->eventCount++;
            }
        }
    }
    
    clip->valid = true;
    controller->clipCount++;
    
    printf("ANIM_METADATA: Loaded '%s' (fps=%.1f, frames=%d-%d, events=%d)\n",
           clip->name, clip->fps, clip->startFrame, clip->endFrame, clip->eventCount);
    
    free(jsonData);
    return true;
}

bool AnimController_PlayClip(AnimationController* controller, const char* clipName) {
    int i;
    if (!controller || !clipName) return false;
    
    for (i = 0; i < controller->clipCount; i++) {
        if (strcmp(controller->clips[i].name, clipName) == 0) {
            controller->currentClipIndex = i;
            controller->localTime = 0.0f;
            controller->playing = true;
            controller->currentFrameInJSON = controller->clips[i].startFrame;
            
            /* Reset event processed flags */
            {
                int j;
                for (j = 0; j < controller->clips[i].eventCount; j++) {
                    controller->clips[i].events[j].processed = false;
                }
            }
            
            printf("ANIM_CONTROLLER: Playing '%s'\n", clipName);
            return true;
        }
    }
    return false;
}

void AnimController_Pause(AnimationController* controller) {
    if (controller) controller->playing = false;
}

void AnimController_Resume(AnimationController* controller) {
    if (controller) controller->playing = true;
}

int AnimController_GetCurrentFrame(const AnimationController* controller) {
    if (!controller || controller->currentClipIndex < 0) return 0;
    return controller->currentFrameInJSON;
}

void AnimController_Update(AnimationController* controller, float deltaTime) {
    AnimationClipMetadata* clip;
    float clipDuration;
    int totalFrames;
    int i;
    AnimationEvent* event;
    
    if (!controller || !controller->playing || controller->currentClipIndex < 0) return;
    
    clip = &controller->clips[controller->currentClipIndex];
    if (!clip->valid) return;
    
    /* Advance time */
    controller->localTime += deltaTime;
    
    /* Calculate current frame */
    totalFrames = clip->endFrame - clip->startFrame + 1;
    clipDuration = (float)totalFrames / clip->fps;
    
    /* Loop handling */
    if (controller->localTime >= clipDuration) {
        if (clip->loop) {
            controller->localTime = fmodf(controller->localTime, clipDuration);
            /* Reset events for next loop */
            for (i = 0; i < clip->eventCount; i++) {
                clip->events[i].processed = false;
            }
        } else {
            controller->localTime = clipDuration;
            controller->playing = false;
        }
    }
    
    /* Calculate frame index */
    controller->currentFrameInJSON = clip->startFrame + 
        (int)(controller->localTime * clip->fps);
    
    if (controller->currentFrameInJSON > clip->endFrame) {
        controller->currentFrameInJSON = clip->endFrame;
    }
    
    /* Process events */
    for (i = 0; i < clip->eventCount; i++) {
        event = &clip->events[i];
        if (!event->valid || event->processed) continue;
        
        if (controller->localTime >= event->time) {
            /* Trigger event */
            if (event->type == ANIM_EVENT_TEXTURE && controller->textureSets) {
                BonesTextureSets_SetVariant(controller->textureSets, 
                                           event->boneName, 
                                           event->variantName);
            }
            
            event->processed = true;
        }
    }
}

#endif /* BONES_ANIMATION_EVENTS_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* BONES_ANIMATION_EVENTS_H */

/*
 * +=======================================================================+
 * |                    GUIA DE INTEGRACION COMPLETA                       |
 * +=======================================================================+
 *
 * PASO 1: PREPARAR ARCHIVOS
 * --------------------------
 * Crea estos archivos en tu proyecto:
 *
 * data/textures/texture_sets.txt:
 *   # Head expressions
 *   Head idle tex/Head1.png
 *   Head talk_open tex/Head_Talk1.png
 *   Head talk_closed tex/Head_Talk2.png
 *   Head blink tex/Head_Blink.png
 *
 * data/animations/idle_anim.json:
 *   {
 *     "name": "idle",
 *     "fps": 30,
 *     "start_frame": 0,
 *     "end_frame": 120,
 *     "loop": true,
 *     "events": [
 *       {"time": 0.5, "type": "texture", "bone": "Head", "variant": "blink"},
 *       {"time": 0.7, "type": "texture", "bone": "Head", "variant": "idle"},
 *       {"time": 2.0, "type": "texture", "bone": "Head", "variant": "talk_open"},
 *       {"time": 2.1, "type": "texture", "bone": "Head", "variant": "talk_closed"},
 *       {"time": 2.2, "type": "texture", "bone": "Head", "variant": "talk_open"},
 *       {"time": 2.3, "type": "texture", "bone": "Head", "variant": "talk_closed"},
 *       {"time": 2.4, "type": "texture", "bone": "Head", "variant": "idle"}
 *     ]
 *   }
 *
 *
 * PASO 2: EN main.c (al inicio)
 * ------------------------------
 * #define BONES_ANIMATION_EVENTS_IMPLEMENTATION
 * #include "bones_animation_events.h"
 *
 * static TextureSetCollection* g_textureSets = NULL;
 * static AnimationController* g_animController = NULL;
 *
 *
 * PASO 3: EN App_Init()
 * ---------------------
 * static bool App_Init(AppState* app) {
 *     ... tu codigo existente ...
 *
 *     // Cargar texture sets
 *     g_textureSets = BonesTextureSets_Create();
 *     if (!BonesTextureSets_LoadFromFile(g_textureSets, "texture_sets.txt")) {
 *         TraceLog(LOG_WARNING, "No texture sets loaded");
 *     }
 *
 *     // Crear controlador de animacion
 *     g_animController = AnimController_Create(&app->animation, g_textureSets);
 *
 *     // Cargar metadata de animacion
 *     if (AnimController_LoadClipMetadata(g_animController, "idle_anim.json")) {
 *         AnimController_PlayClip(g_animController, "idle");
 *     }
 *
 *     return true;
 * }
 *
 *
 * PASO 4: EN App_HandleInput() - Controles de animacion
 * ------------------------------------------------------
 * static void App_HandleInput(AppState* app, float dt) {
 *     ... tu codigo existente ...
 *
 *     // Pausar/reanudar animacion
 *     if (IsKeyPressed(KEY_P)) {
 *         if (g_animController->playing) {
 *             AnimController_Pause(g_animController);
 *         } else {
 *             AnimController_Resume(g_animController);
 *         }
 *     }
 *
 *     // Cambiar manual de variante (para testing)
 *     if (IsKeyPressed(KEY_ONE)) {
 *         BonesTextureSets_SetVariant(g_textureSets, "Head", "idle");
 *     }
 *     if (IsKeyPressed(KEY_TWO)) {
 *         BonesTextureSets_SetVariant(g_textureSets, "Head", "blink");
 *     }
 *     if (IsKeyPressed(KEY_THREE)) {
 *         BonesTextureSets_SetVariant(g_textureSets, "Head", "talk_open");
 *     }
 * }
 *
 *
 * PASO 5: EN main loop (antes de PrepareRenderData)
 * --------------------------------------------------
 * int main(void) {
 *     AppState app;
 *     if (!App_Init(&app)) return -1;
 *     
 *     while (!WindowShouldClose()) {
 *         float dt = GetFrameTime();
 *         
 *         // Update animation controller (procesa eventos automaticamente)
 *         AnimController_Update(g_animController, dt);
 *         
 *         // Sincronizar frame del JSON con el controlador
 *         int frameFromController = AnimController_GetCurrentFrame(g_animController);
 *         BonesSetFrame(&app.animation, frameFromController);
 *         app.currentFrame = frameFromController;
 *         
 *         App_HandleInput(&app, dt);
 *         App_UpdateCamera(&app, dt);
 *         App_UpdateAutoCenter(&app);
 *         App_PrepareRenderData(&app);
 *         App_Draw(&app);
 *     }
 *     
 *     AnimController_Free(g_animController);
 *     BonesTextureSets_Free(g_textureSets);
 *     App_Shutdown(&app);
 *     return 0;
 * }
 *
 *
 * PASO 6: MODIFICAR GetTexturePathForBone() en bones3d.c
 * -------------------------------------------------------
 * Reemplaza la funcion existente con esta version que usa texture sets:
 *
 * const char* GetTexturePathForBone(BoneConfig* boneConfigs, int boneConfigCount, 
 *                                    const char* boneName) {
 *     // Primero intentar obtener del texture set activo
 *     if (g_textureSets) {
 *         const char* activeTexture = BonesTextureSets_GetActiveTexture(g_textureSets, boneName);
 *         if (activeTexture) return activeTexture;
 *     }
 *     
 *     // Fallback al config normal
 *     BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
 *     return config ? config->texturePath : "default.png";
 * }
 *
 * IMPORTANTE: Declara g_textureSets como extern en bones3d.h:
 * extern TextureSetCollection* g_textureSets;
 *
 *
 * PASO 7: EN App_Shutdown()
 * --------------------------
 * static void App_Shutdown(AppState* app) {
 *     if (!app) return;
 *
 *     // Limpiar animation controller
 *     AnimController_Free(g_animController);
 *     BonesTextureSets_Free(g_textureSets);
 *
 *     ... resto del codigo de limpieza existente ...
 * }
 *
 *
 * PASO 8: MODIFICAR CollectBonesForRendering()
 * ---------------------------------------------
 * En bones3d.c, asegurate de que CollectBonesForRendering() use 
 * GetTexturePathForBone() para obtener la textura (ya lo hace).
 * El sistema automaticamente usara la variante activa del texture set.
 *
 * Similarmente para CollectHeadsForRendering():
 *     if (g_textureSets) {
 *         const char* activeTexture = BonesTextureSets_GetActiveTexture(g_textureSets, "Head");
 *         if (activeTexture) {
 *             strncpy(headData->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
 *         }
 *     } else if (headConfig) {
 *         strncpy(headData->texturePath, headConfig->texturePath, MAX_FILE_PATH_LENGTH - 1);
 *     }
 *
 *
 * +=======================================================================+
 * |                         EJEMPLOS DE USO                               |
 * +=======================================================================+
 *
 * EJEMPLO 1: Animacion de parpadeo
 * ---------------------------------
 * {
 *   "name": "blink",
 *   "fps": 30,
 *   "start_frame": 0,
 *   "end_frame": 10,
 *   "loop": false,
 *   "events": [
 *     {"time": 0.0, "type": "texture", "bone": "Head", "variant": "idle"},
 *     {"time": 0.1, "type": "texture", "bone": "Head", "variant": "blink"},
 *     {"time": 0.2, "type": "texture", "bone": "Head", "variant": "idle"}
 *   ]
 * }
 *
 *
 * EJEMPLO 2: Dialogo con boca animada
 * ------------------------------------
 * {
 *   "name": "talk",
 *   "fps": 30,
 *   "start_frame": 0,
 *   "end_frame": 90,
 *   "loop": true,
 *   "events": [
 *     {"time": 0.0, "type": "texture", "bone": "Head", "variant": "idle"},
 *     {"time": 0.5, "type": "texture", "bone": "Head", "variant": "talk_open"},
 *     {"time": 0.6, "type": "texture", "bone": "Head", "variant": "talk_closed"},
 *     {"time": 0.7, "type": "texture", "bone": "Head", "variant": "talk_open"},
 *     {"time": 0.8, "type": "texture", "bone": "Head", "variant": "talk_closed"},
 *     {"time": 0.9, "type": "texture", "bone": "Head", "variant": "talk_open"},
 *     {"time": 1.0, "type": "texture", "bone": "Head", "variant": "idle"},
 *     {"time": 1.5, "type": "texture", "bone": "Head", "variant": "talk_open"},
 *     {"time": 1.6, "type": "texture", "bone": "Head", "variant": "talk_closed"},
 *     {"time": 1.7, "type": "texture", "bone": "Head", "variant": "talk_open"},
 *     {"time": 1.8, "type": "texture", "bone": "Head", "variant": "idle"}
 *   ]
 * }
 *
 *
 * EJEMPLO 3: Emociones multiples
 * -------------------------------
 * texture_sets.txt:
 *   Head idle tex/Head_Neutral.png
 *   Head happy tex/Head_Happy.png
 *   Head sad tex/Head_Sad.png
 *   Head angry tex/Head_Angry.png
 *   Head surprised tex/Head_Surprised.png
 *
 * emotion_anim.json:
 * {
 *   "name": "emotions",
 *   "fps": 30,
 *   "start_frame": 0,
 *   "end_frame": 150,
 *   "loop": true,
 *   "events": [
 *     {"time": 0.0, "type": "texture", "bone": "Head", "variant": "idle"},
 *     {"time": 1.0, "type": "texture", "bone": "Head", "variant": "happy"},
 *     {"time": 2.0, "type": "texture", "bone": "Head", "variant": "sad"},
 *     {"time": 3.0, "type": "texture", "bone": "Head", "variant": "angry"},
 *     {"time": 4.0, "type": "texture", "bone": "Head", "variant": "surprised"},
 *     {"time": 5.0, "type": "texture", "bone": "Head", "variant": "idle"}
 *   ]
 * }
 *
 *
 * EJEMPLO 4: Multiples personas
 * ------------------------------
 * {
 *   "name": "conversation",
 *   "fps": 30,
 *   "start_frame": 0,
 *   "end_frame": 120,
 *   "loop": true,
 *   "events": [
 *     {"time": 0.5, "type": "texture", "bone": "Head", "person": "person_0", "variant": "talk_open"},
 *     {"time": 0.6, "type": "texture", "bone": "Head", "person": "person_0", "variant": "talk_closed"},
 *     {"time": 1.0, "type": "texture", "bone": "Head", "person": "person_0", "variant": "idle"},
 *     {"time": 1.5, "type": "texture", "bone": "Head", "person": "person_1", "variant": "talk_open"},
 *     {"time": 1.6, "type": "texture", "bone": "Head", "person": "person_1", "variant": "talk_closed"},
 *     {"time": 2.0, "type": "texture", "bone": "Head", "person": "person_1", "variant": "idle"}
 *   ]
 * }
 *
 *
 * +=======================================================================+
 * |                         TIPS Y TRUCOS                                 |
 * +=======================================================================+
 *
 * 1. SINCRONIZACION CON AUDIO:
 *    - Usa eventos de tipo "sound" para triggerar audio
 *    - Ajusta los tiempos de eventos texture para lip-sync
 *
 * 2. PERFORMANCE:
 *    - Los texture sets no cargan/descargan texturas, solo cambian referencias
 *    - Pre-carga todas las texturas al inicio para evitar hitches
 *
 * 3. DEBUGGING:
 *    - Los printf() muestran cuando se cambian variantes
 *    - Usa las teclas 1/2/3 para probar variantes manualmente
 *
 * 4. EXTENSION:
 *    - Puedes anadir mas tipos de eventos (ANIM_EVENT_SOUND, etc)
 *    - Implementa callbacks para eventos custom
 *
 * 5. MULTIPLES CLIPS:
 *    - Carga varios .json para diferentes animaciones
 *    - Cambia entre clips con AnimController_PlayClip()
 *
 *
 * +=======================================================================+
 * |                    RESOLUCION DE PROBLEMAS                            |
 * +=======================================================================+
 *
 * PROBLEMA: Las texturas no cambian
 * SOLUCION: Verifica que:
 *   - texture_sets.txt se carga correctamente
 *   - Los nombres de bone coinciden exactamente
 *   - GetTexturePathForBone() esta modificado correctamente
 *
 * PROBLEMA: La animacion no avanza
 * SOLUCION: Verifica que:
 *   - AnimController_Update() se llama cada frame
 *   - controller->playing es true
 *   - fps y frame range son correctos en el JSON
 *
 * PROBLEMA: Los eventos se disparan multiples veces
 * SOLUCION: 
 *   - El sistema ya previene esto con event->processed
 *   - Verifica que no resetees manualmente los eventos
 *
 * PROBLEMA: Loop no funciona
 * SOLUCION:
 *   - Asegurate de que "loop": true en el JSON
 *   - Verifica que clipDuration se calcula bien
 *
 *
 * +=======================================================================+
 * |                      ARQUITECTURA FINAL                               |
 * +=======================================================================+
 *
 * Tu proyecto ahora tiene esta arquitectura limpia:
 *
 * [OpenPose JSON] ──┐
 *                   │
 *                   ├──> [bones_core.c] ────> [bone positions]
 *                   │                                │
 * [texture_sets]    │                                ↓
 *       ↓           │                         [bones_animation.c]
 * [TextureSetCollection] ←──────────────┐           │
 *       ↓                                │           ↓
 * [AnimationController] ←────────────────┤    [render data]
 *       ↓                                │           ↓
 * [Animation Metadata JSON]              └─── [App_Draw()]
 *       ↓
 *  [eventos + timing]
 *
 * BENEFICIOS:
 * - JSON de poses SIN TOCAR
 * - Texturas dinamicas sin modificar archivos base
 * - Timeline de eventos visual y facil de editar
 * - Sistema extensible para sonidos, particulas, etc
 * - Sincronizacion perfecta entre frames y eventos
 *
 * +=======================================================================+
 */