// head_billboard.h
#ifndef HEAD_BILLBOARD_H
#define HEAD_BILLBOARD_H

#include "raylib.h"
#include "raymath.h"
#include "bones3d.h"


// Funciones principales
HeadOrientation CalculateHeadOrientation(const Person* person);
Vector3 CalculateHeadPosition(const Person* person);
bool ShouldRenderHead(const Person* person);
void DrawHeadBillboard(Texture2D texture, Camera camera, const HeadRenderData* headData, int physCols, int physRows);
void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads, int* headCount, int* headCapacity, BoneConfig* boneConfigs, int boneConfigCount);

// Funciones de posición
Vector3 CalculateChestPosition(const Person* person);
Vector3 CalculateHipPosition(const Person* person);

// Funciones de orientación
VirtualSpine CalculateVirtualSpine(const Person* person);
TorsoOrientation CalculateChestOrientation(const Person* person);
TorsoOrientation CalculateHipOrientation(const Person* person);

// Funciones de renderizado
void CalculateTorsoRenderData(const TorsoRenderData* torsoData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored);

// Funciones de visibilidad
bool ShouldRenderChest(const Person* person);
bool ShouldRenderHip(const Person* person);

// Funciones de dibujo
void DrawTorsoBillboard(Texture2D texture, Camera camera, const TorsoRenderData* torsoData, int physCols, int physRows);

// Funciones de recolección
void CollectTorsosForRendering(const BonesAnimation* animation, TorsoRenderData** torsos,
    int* torsoCount, int* torsoCapacity, BoneConfig* boneConfigs, int boneConfigCount);

#endif // HEAD_BILLBOARD_H