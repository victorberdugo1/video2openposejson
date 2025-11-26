#ifndef BONES_CORE_H
#define BONES_CORE_H

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================================
// CONSTANTES
// ============================================================================

#define ATLAS_COLS 4
#define ATLAS_ROWS 4
#define MAX_BONE_NAME_LENGTH 32
#define MAX_FILE_PATH_LENGTH 512
#define MAX_BONES_PER_PERSON 32
#define MAX_FRAMES 10000
#define MAX_PERSONS 10
#define MAX_TEXTURES 13
#define MAX_RENDER_ITEMS 512

// Constantes de renderizado
#define TORSO_BIAS 0.001f
#define BONE_BIAS 0.0f
#define HEAD_BIAS -0.001f
#define INDEX_BIAS -0.00001f
#define Z_FIGHTING_THRESHOLD 0.01f
#define MIN_DISTANCE_THRESHOLD 0.001f

// Texture Sets y Animation System
#define BONES_MAX_TEXTURE_VARIANTS 16
#define BONES_MAX_TEXTURE_SETS 64
#define BONES_MAX_ANIM_EVENTS 256
#define BONES_MAX_ANIM_CLIPS 32
#define BONES_AE_MAX_NAME 64
#define BONES_AE_MAX_PATH 256

// ============================================================================
// ENUMERACIONES
// ============================================================================

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

typedef enum {
    TORSO_CHEST = 0,
    TORSO_HIP = 1
} TorsoType;

typedef enum {
    ANIM_EVENT_TEXTURE,
    ANIM_EVENT_SOUND,
    ANIM_EVENT_PARTICLE,
    ANIM_EVENT_CUSTOM
} AnimEventType;

// ============================================================================
// ESTRUCTURAS BÁSICAS
// ============================================================================
typedef struct {
    Vector3 position;
    float confidence;
    bool valid;
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
    bool isOriginalKeyframe;  
} AnimationFrame;

typedef struct {
    AnimationFrame* frames;
    int frameCount;
    int maxFrames;
    int currentFrame;
    bool isLoaded;
    char filePath[256];
} BonesAnimation;

// ============================================================================
// ORIENTACIONES Y CONFIGURACIONES
// ============================================================================

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
    bool valid;
    Vector3 position;
    Vector3 forward;
    Vector3 up;
    Vector3 right;
    float yaw, pitch, roll;
} TorsoOrientation;

typedef struct {
    bool valid;
    Vector3 chestPosition;
    Vector3 hipPosition;
    Vector3 spineDirection;
    Vector3 spineRight;
    Vector3 spineForward;
} VirtualSpine;

typedef struct {
    Vector3 neck, lShoulder, rShoulder, lHip, rHip;
    bool hasNeck, hasLShoulder, hasRShoulder, hasLHip, hasRHip;
    int shoulderCount, hipCount;
} CachedBones;

typedef struct {
    Vector3 pos0;
    Vector3 pos1;
} BoneConnectionPositions;

typedef struct {
    char name[64];
    char texturesConfigPath[256];
    char textureSetsPath[256];
    char animationsPath[256];
} CharacterProfile;

// ============================================================================
// RENDERIZADO
// ============================================================================

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
    bool visible;
    Vector3 position;
    TorsoOrientation orientation;
    TorsoType type;
    float size;
    char texturePath[MAX_FILE_PATH_LENGTH];
    char personId[16];
    const Person* person;
    bool disableCompensation;
} TorsoRenderData;

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

typedef struct {
    float defaultBoneSize;
    bool drawDebugSpheres;
    bool enableDepthSorting;
    Color debugColor;
    float debugSphereRadius;
    bool showInvalidBones;
} BonesRenderConfig;

// ============================================================================
// SISTEMA DE TEXTURAS
// ============================================================================

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

typedef struct {
    char boneName[MAX_BONE_NAME_LENGTH];
    char texturePath[MAX_FILE_PATH_LENGTH];
    float size;
    bool visible;
    bool valid;
} BoneConfig;

// ============================================================================
// TEXTURE SETS
// ============================================================================

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

// ============================================================================
// ANIMATION SYSTEM
// ============================================================================

typedef struct {
    float time;
    AnimEventType type;
    char boneName[BONES_AE_MAX_NAME];
    char personId[BONES_AE_MAX_NAME];
    char variantName[BONES_AE_MAX_NAME];
    char stringParam[BONES_AE_MAX_NAME];
    bool processed;
    bool valid;
} AnimationEvent;

typedef struct {
    char name[BONES_AE_MAX_NAME];
    float fps;
    int startFrame;
    int endFrame;
    bool loop;
    
    AnimationEvent events[BONES_MAX_ANIM_EVENTS];
    int eventCount;
    
    bool valid;
} AnimationClipMetadata;

typedef struct {
    TextureSetCollection* textureSets;
    
    AnimationClipMetadata clips[BONES_MAX_ANIM_CLIPS];
    int clipCount;
    int currentClipIndex;
    
    float localTime;
    bool playing;
    
    void* bonesAnimation;
    int currentFrameInJSON;
    
    bool valid;
} AnimationController;

// ============================================================================
// PERSONAJE ANIMADO
// ============================================================================

typedef struct AnimatedCharacter {
    BonesAnimation animation;
    BonesRenderer* renderer;
    
    SimpleTextureSystem textureSystem;
    BoneConfig* boneConfigs;
    int boneConfigCount;
    BonesRenderConfig renderConfig;
    
    BoneRenderData* renderBones;
    int renderBonesCount;
    int renderBonesCapacity;
    HeadRenderData* renderHeads;
    int renderHeadsCount;
    int renderHeadsCapacity;
    TorsoRenderData* renderTorsos;
    int renderTorsosCount;
    int renderTorsosCapacity;
    
    int currentFrame;
    int maxFrames;
    bool autoPlay;
    float autoPlaySpeed;
    
    Vector3 autoCenter;
    bool autoCenterCalculated;
    
    bool renderHeadBillboards;
    bool renderTorsoBillboards;
    int lastProcessedFrame;
    bool forceUpdate;
    
    AnimationController* animController;
    TextureSetCollection* textureSets;
} AnimatedCharacter;

// ============================================================================
// TABLAS DE CONEXIONES (CONSTANTES GLOBALES)
// ============================================================================

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

// ============================================================================
// FUNCIONES CORE - ANIMACIÓN
// ============================================================================

BonesError BonesInit(BonesAnimation* animation, int maxFrames);
void BonesFree(BonesAnimation* animation);
BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath);
BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString);
BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber);
int BonesGetCurrentFrame(const BonesAnimation* animation);
int BonesGetFrameCount(const BonesAnimation* animation);
bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber);
bool BonesIsPositionValid(Vector3 position);
const char* BonesGetErrorString(BonesError error);

// ============================================================================
// FUNCIONES DE EDICIÓN DE ANIMACIONES
// ============================================================================


bool BonesInterpolateFrames(BonesAnimation* animation, int frameA, int frameB, int framesToAdd);
bool BonesInsertEmptyFrame(BonesAnimation* animation, int position);
bool BonesCopyFrame(BonesAnimation* animation, int sourceFrame, int targetFrame);
bool BonesCreateMissingFrames(BonesAnimation* animation);

// ============================================================================
// FUNCIONES DE CONFIGURACIÓN
// ============================================================================

BonesRenderConfig BonesGetDefaultRenderConfig(void);
void BonesSetRenderConfig(const BonesRenderConfig* config);
void SetAnimationTransitionDuration(float duration);

// ============================================================================
// FUNCIONES DE CÁLCULO DE POSICIONES
// ============================================================================

Vector3 GetBonePositionByName(const Person* person, const char* boneName);
Vector3 CalculateHeadPosition(const Person* person);
Vector3 CalculateChestPosition(const Person* person);
Vector3 CalculateHipPosition(const Person* person);
VirtualSpine CalculateVirtualSpine(const Person* person);

// ============================================================================
// FUNCIONES DE ORIENTACIÓN
// ============================================================================

HeadOrientation CalculateHeadOrientation(const Person* person);
TorsoOrientation CalculateChestOrientation(const Person* person);
TorsoOrientation CalculateHipOrientation(const Person* person);
BoneOrientation CalculateBoneOrientation(const char* boneName, const Person* person, Vector3 bonePosition);

// ============================================================================
// FUNCIONES DE RENDERIZADO - CÁLCULOS
// ============================================================================

bool ShouldRenderHead(const Person* person);
bool ShouldRenderChest(const Person* person);
bool ShouldRenderHip(const Person* person);

void CalculateHeadRenderData(const HeadRenderData* headData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);
void CalculateTorsoRenderData(const TorsoRenderData* torsoData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);
void CalculateLimbBoneRenderData(const BoneRenderData* boneData, const Person* person, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);
void CalculateHandBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored, const char* boneName);

// ============================================================================
// FUNCIONES DE RECOLECCIÓN DE DATOS
// ============================================================================

void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads,
    int* headCount, int* headCapacity, BoneConfig* boneConfigs, int boneConfigCount,
    TextureSetCollection* textureSets);

void CollectTorsosForRendering(const BonesAnimation* animation, TorsoRenderData** torsos,
    int* torsoCount, int* torsoCapacity, BoneConfig* boneConfigs, int boneConfigCount,
    TextureSetCollection* textureSets);

void CollectBonesForRendering(const BonesAnimation* animation, Camera camera, BoneRenderData** renderBones,
    int* renderBonesCount, int* renderBonesCapacity, BoneConfig* boneConfigs, int boneConfigCount,
    TextureSetCollection* textureSets);


void EnrichBoneRenderDataWithOrientation(BoneRenderData* renderBone, const Person* person);
bool ResizeRenderBonesArray(BoneRenderData** renderBones, int* renderBonesCapacity, int newCapacity);

// ============================================================================
// FUNCIONES AUXILIARES
// ============================================================================

Vector3 SafeNormalize(Vector3 v);
bool IsWristBone(const char* boneName);
bool GetBoneConnectionsWithPriority(const char* boneName, char connections[3][32], float priorities[3]);
BoneConnectionPositions GetBoneConnectionPositionsEx(const BoneRenderData* boneData, const Person* person);
BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int count, const char* name);
const Person* FindPersonByBoneName(const AnimationFrame* frame, const char* boneName);

// ============================================================================
// SISTEMA DE TEXTURAS
// ============================================================================

void CleanupTextureSystem(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount);
bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename);
void LoadBoneConfigurations(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount);

BoneConfig* FindBoneConfig(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);

const char* GetTexturePathForBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName,
    TextureSetCollection* textureSets);

bool IsBoneVisible(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
float GetBoneSize(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);

// ============================================================================
// TEXTURE SETS API
// ============================================================================

TextureSetCollection* BonesTextureSets_Create(void);
void BonesTextureSets_Free(TextureSetCollection* collection);
bool BonesTextureSets_LoadFromFile(TextureSetCollection* collection, const char* filePath);
const char* BonesTextureSets_GetActiveTexture(const TextureSetCollection* collection, const char* boneName);
bool BonesTextureSets_SetVariant(TextureSetCollection* collection, const char* boneName, const char* variantName);
const char* BonesTextureSets_GetActiveVariantName(const TextureSetCollection* collection, const char* boneName);
void BonesTextureSets_ResetAll(TextureSetCollection* collection);
BoneTextureSet* BonesTextureSets_FindSet(TextureSetCollection* collection, const char* boneName);

// ============================================================================
// ANIMATION CONTROLLER API
// ============================================================================

AnimationController* AnimController_Create(void* bonesAnimation, TextureSetCollection* textureSets);
void AnimController_Free(AnimationController* controller);
bool AnimController_LoadClipMetadata(AnimationController* controller, const char* jsonPath);
bool AnimController_PlayClip(AnimationController* controller, const char* clipName);
void AnimController_Pause(AnimationController* controller);
void AnimController_Resume(AnimationController* controller);
void AnimController_Update(AnimationController* controller, float deltaTime);
int AnimController_GetCurrentFrame(const AnimationController* controller);

// ============================================================================
// RENDERER API
// ============================================================================

BonesRenderer* BonesRenderer_Create(void);
void BonesRenderer_Free(BonesRenderer* renderer);
bool BonesRenderer_Init(BonesRenderer* renderer);
int BonesRenderer_LoadTexture(BonesRenderer* renderer, const char* path);
void BonesRenderer_SetAtlasDimensions(BonesRenderer* renderer, int cols, int rows);
void BonesRenderer_RenderFrame(BonesRenderer* renderer, 
                              BoneRenderData* bones, int boneCount,
                              HeadRenderData* heads, int headCount, 
                              TorsoRenderData* torsos, int torsoCount,
                              Vector3 autoCenter, bool autoCenterCalculated);
void BonesRenderer_DrawGrid(BonesRenderer* renderer);
void BonesRenderer_DrawAutoCenter(BonesRenderer* renderer, Vector3 autoCenter);

// ============================================================================
// FUNCIONES DE DIBUJADO
// ============================================================================

Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows,
    bool mirrored, bool* outMirrored);
void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, const char* boneName);
void DrawBonetileCustomWithRoll(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, bool neighborValid, Vector3 neighborPos, 
    const BoneRenderData* boneData, const Person* person);
void DrawHeadBillboard(Texture2D texture, Camera camera, const HeadRenderData* headData, 
    int physCols, int physRows);
void DrawTorsoBillboard(Texture2D texture, Camera camera, const TorsoRenderData* torsoData, 
    int physCols, int physRows);

// ============================================================================
// PERSONAJE ANIMADO API
// ============================================================================

AnimatedCharacter* CreateAnimatedCharacter(const char* textureConfigPath, const char* textureSetsPath);
void DestroyAnimatedCharacter(AnimatedCharacter* character);
bool LoadAnimation(AnimatedCharacter* character, const char* animationPath, const char* metadataPath);
void UpdateAnimatedCharacter(AnimatedCharacter* character, float deltaTime);
void DrawAnimatedCharacter(AnimatedCharacter* character, Camera camera);
void SetCharacterFrame(AnimatedCharacter* character, int frame);
void SetCharacterAutoPlay(AnimatedCharacter* character, bool autoPlay);
void SetCharacterBillboards(AnimatedCharacter* character, bool heads, bool torsos);
void AnimController_UpdateFrameBounds(AnimationController* controller, int clipIndex,BonesAnimation* animation);

#endif // BONES_CORE_H