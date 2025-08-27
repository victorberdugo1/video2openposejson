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

// Declaraciones de funciones...
BonesError BonesInit(BonesAnimation* animation, int maxFrames);
void BonesFree(BonesAnimation* animation);
BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath);
BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString);
BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber);
int BonesGetCurrentFrame(const BonesAnimation* animation);
int BonesGetFrameCount(const BonesAnimation* animation);
bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber);
BonesError BonesGetPerson(const BonesAnimation* animation, int frameNumber, const char* personId, Person** outPerson);
BonesError BonesGetBone(const BonesAnimation* animation, int frameNumber, const char* personId, const char* boneName, Bone** outBone);
BonesError BonesGetBonePosition(const BonesAnimation* animation, int frameNumber, const char* personId, const char* boneName, Vector3* outPosition);
Vector3 BonesNormalizedToWorld(Vector3 normalizedPos, Vector3 worldCenter, Vector3 worldScale);
Vector3 BonesWorldToNormalized(Vector3 worldPos, Vector3 worldCenter, Vector3 worldScale);
bool BonesIsPositionValid(Vector3 position);
bool BonesIsBoneVisible(const Bone* bone, Camera camera, float maxDistance);
BonesRenderConfig BonesGetDefaultRenderConfig(void);
void BonesSetRenderConfig(const BonesRenderConfig* config);
BonesError BonesDrawFrame(const BonesAnimation* animation, int frameNumber, Camera camera, Texture2D* textures, int textureCount, const BonesRenderConfig* config);
BonesError BonesDrawPerson(const Person* person, Camera camera, Texture2D* textures, int textureCount, const BonesRenderConfig* config);
BonesError BonesDrawBone(const Bone* bone, Camera camera, Texture2D texture, const BonesRenderConfig* config);
const char* BonesGetErrorString(BonesError error);
void BonesLogError(BonesError error, const char* context);
void BonesPrintFrameInfo(const BonesAnimation* animation, int frameNumber);
void BonesPrintPersonInfo(const Person* person);
void BonesPrintBoneInfo(const Bone* bone);
int BonesCountValidBones(const Person* person);
float BonesCalculatePersonHeight(const Person* person);
Vector3 BonesCalculatePersonCenter(const Person* person);
Vector3 BonesInterpolatePosition(Vector3 pos1, Vector3 pos2, float t);
BonesError BonesInterpolateFrame(const BonesAnimation* animation, float frameTime, Person* outInterpolated);

void CheckBoneNameLength(void);
void CleanupTextureSystem(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount);
time_t GetFileModificationTime(const char* filename);
char* ReadEntireFile(const char* filename, long* fileSize);
int CountValidLines(const char* buffer);
bool ParseConfigFromBuffer(SimpleTextureSystem* system, const char* buffer);
bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename);
void LoadBoneConfigurations(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount);
BoneConfig* FindBoneConfig(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
const char* GetTexturePathForBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
bool IsBoneVisible(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
float GetBoneSize(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
bool ResizeRenderBonesArray(BoneRenderData** renderBones, int* renderBonesCapacity, int newCapacity);
int CompareBonesByDistance(const void* a, const void* b);
void CollectBonesForRendering(const BonesAnimation* animation, Camera camera, BoneRenderData** renderBones, int* renderBonesCount, int* renderBonesCapacity, BoneConfig* boneConfigs, int boneConfigCount);

#endif