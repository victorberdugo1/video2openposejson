#ifndef BONES3D_H
#define BONES3D_H

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stddef.h>

#include "bonetile.h" 

#define MAX_BONES_PER_PERSON 32
#define MAX_FRAMES 10000
#define MAX_PERSONS 10

#ifndef MAX_FILE_PATH_LENGTH
#define MAX_BONE_NAME_LENGTH 32
#define MAX_FILE_PATH_LENGTH 512
#endif

// Verification at compile time instead of runtime function
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

#endif