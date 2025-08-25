// head_billboard.h
#ifndef HEAD_BILLBOARD_H
#define HEAD_BILLBOARD_H

#include "raylib.h"
#include "raymath.h"
#include "bones3d.h"

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

// Funciones principales
HeadOrientation CalculateHeadOrientation(const Person* person);
Vector3 CalculateHeadPosition(const Person* person);
bool ShouldRenderHead(const Person* person);
void DrawHeadBillboard(Texture2D texture, Camera camera, const HeadRenderData* headData, int physCols, int physRows);
void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads, int* headCount, int* headCapacity, BoneConfig* boneConfigs, int boneConfigCount);

#endif // HEAD_BILLBOARD_H