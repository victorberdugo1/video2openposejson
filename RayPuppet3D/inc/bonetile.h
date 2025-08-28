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

bool GetBoneConnections(const char* boneName, char connections[3][MAX_BONE_NAME_LENGTH]);
Vector3 GetConnectedBonePosition(const char* boneName, const struct Person* person);
BoneOrientation CalculateBoneOrientation(const char* boneName, const struct Person* person);

bool GetBoneConnectionsWithPriority(const char* boneName, char connections[3][MAX_BONE_NAME_LENGTH], float priorities[3]);
BoneOrientation CalculateEnhancedBoneOrientation(const char* boneName, const struct Person* person);

void CalculateEnhancedBoneRenderData(const BoneRenderData* boneData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);

BoneRenderData CreateBoneRenderData(const char* boneName, const Person* person,
    const BoneConfig* config);

void DrawBoneWithOrientation(Texture2D texture, Camera camera, const char* boneName,
    const Person* person, const BoneConfig* config,
    int physCols, int physRows);

void CalculateBoneRenderDataWithOrientation(const BoneRenderData* boneData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);

void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored);

Rectangle GetAtlasCellSrcPos(Texture2D tex, int col, int rowIndex, bool mirrored, bool* outMirrored);

void CalculateBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored);

Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows,
    bool mirrored, bool* outMirrored);

#endif /* BONETILE_H */