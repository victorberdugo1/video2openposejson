#ifndef BONETILE_H
#define BONETILE_H

#include "raylib.h"
#include "rlgl.h"
#include <stdbool.h>
#include <stdio.h>

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

#endif /* BONETILE_H */