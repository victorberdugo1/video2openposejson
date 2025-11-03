#ifndef BONES_CORE_H
#define BONES_CORE_H

#include "raymath.h"
#include "raylib.h"
#include "rlgl.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


#define ATLAS_COLS 4
#define ATLAS_ROWS 4


#ifndef MAX_BONE_NAME_LENGTH
#define MAX_BONE_NAME_LENGTH 32
#endif
#ifndef MAX_FILE_PATH_LENGTH
#define MAX_FILE_PATH_LENGTH 512
#endif

typedef struct Person Person;

typedef struct {
    Vector3 position;
    Vector3 forward;
    Vector3 up;
    Vector3 right;
    float yaw;
    float pitch;
    float roll;
    bool valid;
} BoneOrientation;

typedef struct {
    char personId[16];
    char boneName[MAX_BONE_NAME_LENGTH];
    Vector3 position;
    BoneOrientation orientation;
    int atlasIndex;
    float rotation;
    bool mirrored;
    float distance;
    char texturePath[MAX_FILE_PATH_LENGTH];
    float size;
    bool valid;
    bool visible;
} BoneRenderData;

typedef struct {
    char boneName[MAX_BONE_NAME_LENGTH];
    char texturePath[MAX_FILE_PATH_LENGTH];
    float size;
    bool visible;
    bool valid;
} BoneConfig;

typedef struct {
    Vector3 pos0;
    Vector3 pos1;
} BoneConnectionPositions;

typedef struct {
    Vector3 position;
    Vector3 forward;
    Vector3 up;
    Vector3 right;
    float yaw;
    float pitch;
    float roll;
    bool valid;
} HeadOrientation;

typedef struct {
    Vector3 position;
    HeadOrientation orientation;
    float size;
    bool visible;
    bool valid;
    char texturePath[MAX_FILE_PATH_LENGTH];
    char personId[16];
} HeadRenderData;

typedef struct {
    bool valid;
    Vector3 chestPosition;
    Vector3 hipPosition;
    Vector3 spineDirection;
    Vector3 spineRight;
    Vector3 spineForward;
} VirtualSpine;

typedef struct {
    bool valid;
    Vector3 position;
    Vector3 forward;
    Vector3 up;
    Vector3 right;
    float yaw, pitch, roll;
} TorsoOrientation;

typedef enum {
    TORSO_CHEST = 0,
    TORSO_HIP = 1
} TorsoType;

typedef struct {
    bool valid;
    bool visible;
    Vector3 position;
    TorsoOrientation orientation;
    TorsoType type;
    float size;
    char texturePath[MAX_FILE_PATH_LENGTH];
    char personId[16];
    const Person* person;
} TorsoRenderData;

typedef struct {
    Vector3 neck, lShoulder, rShoulder, lHip, rHip;
    bool hasNeck, hasLShoulder, hasRShoulder, hasLHip, hasRHip;
    int shoulderCount, hipCount;
} CachedBones;

#define MAX_BONES_PER_PERSON 32
#define MAX_FRAMES 10000
#define MAX_PERSONS 10

#ifndef MAX_FILE_PATH_LENGTH
#define MAX_BONE_NAME_LENGTH 32
#define MAX_FILE_PATH_LENGTH 512
#endif

#if MAX_BONE_NAME_LENGTH < 32
#error "MAX_BONE_NAME_LENGTH debe ser al menos 32 bytes"
#endif

typedef enum {
    BONES_SUCCESS = 0,
    BONES_ERROR_NULL_POINTER,
    BONES_ERROR_FILE_NOT_FOUND,
    BONES_ERROR_INVALID_JSON,
    BONES_ERROR_MEMORY_ALLOCATION,
    BONES_ERROR_BONE_NOT_FOUND,
    BONES_ERROR_FRAME_OUT_OF_RANGE,
    BONES_ERROR_PERSON_NOT_FOUND,
    BONES_ERROR_INVALID_COORDINATES,
    BONES_ERROR_BUFFER_OVERFLOW,
    BONES_ERROR_EMPTY_DATA
} BonesError;

typedef struct {
    Vector3 position;
    bool valid;
    float confidence;
} BonePosition;

typedef struct {
    char name[MAX_BONE_NAME_LENGTH];
    BonePosition position;
    int textureIndex;
    Vector2 size;
    float rotation;
    bool mirrored;
    bool visible;
} Bone;

typedef struct Person {
    char personId[16];
    Bone bones[MAX_BONES_PER_PERSON];
    int boneCount;
    bool active;
} Person;

typedef struct {
    int frameNumber;
    Person persons[MAX_PERSONS];
    int personCount;
    bool valid;
} AnimationFrame;

typedef struct {
    AnimationFrame* frames;
    int frameCount;
    int maxFrames;
    int currentFrame;
    bool isLoaded;
    char filePath[256];
} BonesAnimation;

typedef struct {
    float defaultBoneSize;
    bool drawDebugSpheres;
    bool enableDepthSorting;
    Color debugColor;
    float debugSphereRadius;
    bool showInvalidBones;
} BonesRenderConfig;

typedef struct {
    char boneName[MAX_BONE_NAME_LENGTH];
    char texturePath[MAX_FILE_PATH_LENGTH];
    bool visible;
    float size;
} BoneTextureConfig;

typedef struct {
    BoneTextureConfig* configs;
    int configCount;
    int configCapacity;
    bool loaded;
    time_t lastModified;
} SimpleTextureSystem;

static const struct {
    const char* boneName;
    const char* connectedBone;
    float projectionFactor;
} MIDPOINT_CONNECTIONS[] = {
    {"Neck", "HEAD_CALCULATED", 1.0f},
    {"LShoulder", "LElbow", 1.0f}, {"RShoulder", "RElbow", 1.0f},
    {"LElbow", "LWrist", 1.0f}, {"RElbow", "RWrist", 1.0f},
    {"LWrist", "LElbow", 1.3f}, {"RWrist", "RElbow", 1.3f},
    {"LHip", "LKnee", 1.0f}, {"RHip", "RKnee", 1.0f},
    {"LKnee", "LAnkle", 1.0f}, {"RKnee", "RAnkle", 1.0f},
    {"LAnkle", "FOOT_FORWARD", 1.0f}, {"RAnkle", "FOOT_FORWARD", 1.0f},
    {"", "", 0.0f}
};

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

void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads,
    int* headCount, int* headCapacity, BoneConfig* boneConfigs, int boneConfigCount);

void CollectTorsosForRendering(const BonesAnimation* animation, TorsoRenderData** torsos,
    int* torsoCount, int* torsoCapacity, BoneConfig* boneConfigs, int boneConfigCount);

// Bone connections and orientation functions
bool GetBoneConnectionsWithPriority(const char* boneName, char connections[3][MAX_BONE_NAME_LENGTH], float priorities[3]);
BoneOrientation CalculateBoneOrientation(const char* boneName, const Person* person, Vector3 bonePosition);
Vector3 GetBonePositionByName(const Person* person, const char* boneName);
BoneConnectionPositions GetBoneConnectionPositionsEx(const BoneRenderData* boneData, const Person* person);

// Utility functions
Vector3 SafeNormalize(Vector3 v);
bool IsWristBone(const char* boneName);

// Texture and rendering functions
Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows,
    bool mirrored, bool* outMirrored);

void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, const char* boneName);

void DrawBonetileCustomWithRoll(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, bool neighborValid, Vector3 neighborPos, const BoneRenderData* boneData, const Person* person);

// Bone render data calculation
BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int count, const char* name);

void CalculateLimbBoneRenderData(const BoneRenderData* boneData, const Person* person, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);

void CalculateHandBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored, const char* boneName);

// Head functions
Vector3 CalculateHeadPosition(const Person* person);
HeadOrientation CalculateHeadOrientation(const Person* person);
bool ShouldRenderHead(const Person* person);
void CalculateHeadRenderData(const HeadRenderData* headData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);
void DrawHeadBillboard(Texture2D texture, Camera camera, const HeadRenderData* headData, int physCols, int physRows);

// Torso functions
Vector3 CalculateChestPosition(const Person* person);
Vector3 CalculateHipPosition(const Person* person);
VirtualSpine CalculateVirtualSpine(const Person* person);
TorsoOrientation CalculateChestOrientation(const Person* person);
TorsoOrientation CalculateHipOrientation(const Person* person);
bool ShouldRenderChest(const Person* person);
bool ShouldRenderHip(const Person* person);
void CalculateTorsoRenderData(const TorsoRenderData* torsoData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);
void DrawTorsoBillboard(Texture2D texture, Camera camera, const TorsoRenderData* torsoData, int physCols, int physRows);

// Core animation functions
BonesError BonesInit(BonesAnimation* animation, int maxFrames);
void BonesFree(BonesAnimation* animation);
BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath);
BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString);
BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber);
int BonesGetCurrentFrame(const BonesAnimation* animation);
int BonesGetFrameCount(const BonesAnimation* animation);
bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber);

// Utility functions
bool BonesIsPositionValid(Vector3 position);
BonesRenderConfig BonesGetDefaultRenderConfig(void);
void BonesSetRenderConfig(const BonesRenderConfig* config);

// Error handling
const char* BonesGetErrorString(BonesError error);
void BonesLogError(BonesError error, const char* context);

// Texture system functions
void CleanupTextureSystem(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount);
time_t GetFileModificationTime(const char* filename);
bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename);
void LoadBoneConfigurations(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount);

// Bone configuration functions
BoneConfig* FindBoneConfig(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
const char* GetTexturePathForBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
bool IsBoneVisible(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
float GetBoneSize(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);

// Rendering functions
bool ResizeRenderBonesArray(BoneRenderData** renderBones, int* renderBonesCapacity, int newCapacity);
int CompareBonesByDistance(const void* a, const void* b);
void CollectBonesForRendering(const BonesAnimation* animation, Camera camera, BoneRenderData** renderBones,
    int* renderBonesCount, int* renderBonesCapacity, BoneConfig* boneConfigs, int boneConfigCount);

void EnrichBoneRenderDataWithOrientation(BoneRenderData* renderBone, const Person* person);

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
AnimationController* AnimController_Create(void* bonesAnimation, TextureSetCollection* textureSets);
bool AnimController_LoadClipMetadata(AnimationController* controller, const char* jsonPath);
bool AnimController_PlayClip(AnimationController* controller, const char* clipName);
void AnimController_Pause(AnimationController* controller);
void AnimController_Resume(AnimationController* controller);
void AnimController_Update(AnimationController* controller, float deltaTime);
int AnimController_GetCurrentFrame(const AnimationController* controller);
void AnimController_Free(AnimationController* controller);
const Person* FindPersonByBoneName(const AnimationFrame* frame, const char* boneName);


#define MAX_TEXTURES 13
#define MAX_RENDER_ITEMS 512

#define MAX_BONE_NAME_LENGTH 32

// Constantes de renderizado
static const float TORSO_BIAS = 0.001f;
static const float BONE_BIAS = 0.0f;
static const float HEAD_BIAS = -0.001f;
static const float INDEX_BIAS = -0.00001f;
static const float Z_FIGHTING_THRESHOLD = 0.01f;
static const float MIN_DISTANCE_THRESHOLD = 0.001f;

// ============================================================================
// ESTRUCTURAS DE RENDERIZADO
// ============================================================================

typedef struct {
    int type;
    int index;
    float distance;
    float depthBias;
    bool hasZFighting;
} RenderItem;

typedef struct {
    Texture2D textures[MAX_TEXTURES];
    char texturePaths[MAX_TEXTURES][MAX_FILE_PATH_LENGTH];
    int textureCount;
    int physCols, physRows;
    Camera camera;
} BonesRenderer;

// ============================================================================
// FUNCIONES PÚBLICAS DEL RENDERIZADOR
// ============================================================================

// Inicialización y limpieza
BonesRenderer* BonesRenderer_Create(void);
void BonesRenderer_Free(BonesRenderer* renderer);
bool BonesRenderer_Init(BonesRenderer* renderer);

// Gestión de texturas
int BonesRenderer_LoadTexture(BonesRenderer* renderer, const char* path);
void BonesRenderer_SetAtlasDimensions(BonesRenderer* renderer, int cols, int rows);

// Renderizado principal
void BonesRenderer_RenderFrame(BonesRenderer* renderer, 
                              BoneRenderData* bones, int boneCount,
                              HeadRenderData* heads, int headCount, 
                              TorsoRenderData* torsos, int torsoCount,
                              Vector3 autoCenter, bool autoCenterCalculated);

// Utilidades de renderizado
void BonesRenderer_DrawGrid(BonesRenderer* renderer);
void BonesRenderer_DrawAutoCenter(BonesRenderer* renderer, Vector3 autoCenter);

#endif

