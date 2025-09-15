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

bool GetBoneConnectionsWithPriority(const char* boneName, char connections[3][MAX_BONE_NAME_LENGTH], float priorities[3]);

void CalculateEnhancedBoneRenderData(const BoneRenderData* boneData, const Person* person, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);

void CalculateBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored, const char* boneName);


Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows,
    bool mirrored, bool* outMirrored);

void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, const char* boneName);

void DrawBonetileCustomWithRoll(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, bool neighborValid, Vector3 neighborPos, const char* boneName);

BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int count, const char* name);

void CalculateDirectionalBoneRenderData(Vector3 bonePos, Vector3 neighborPos, bool hasNeighbor,
    Camera camera, const char* boneName, int* outChosenIndex, float* outRotation, bool* outMirrored);

BoneOrientation CalculateBoneOrientation(const char* boneName, const Person* person, Vector3 bonePosition);

Vector3 SafeNormalize(Vector3 v);

#endif /* BONETILE_H */