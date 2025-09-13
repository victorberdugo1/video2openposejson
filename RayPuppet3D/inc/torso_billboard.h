// torso_billboard.h - Sistema de torso con orientación basada en OpenPose
#ifndef TORSO_BILLBOARD_H
#define TORSO_BILLBOARD_H

#include "bones3d.h"
#include "raylib.h"
#include "raymath.h"
#include <stdio.h>

// Estructura para la columna vertebral virtual
typedef struct {
    bool valid;
    Vector3 chestPosition;
    Vector3 hipPosition;
    Vector3 spineDirection;    // Vector de la columna (hacia arriba)
    Vector3 spineRight;        // Vector derecha del cuerpo
    Vector3 spineForward;      // Vector hacia adelante del cuerpo (NUEVO)
} VirtualSpine;

// Orientación del torso (similar a HeadOrientation)
typedef struct {
    bool valid;
    Vector3 position;
    Vector3 forward;    // Hacia donde "mira" el torso
    Vector3 up;         // Hacia arriba del torso
    Vector3 right;      // Hacia la derecha del torso
    float yaw, pitch, roll;
} TorsoOrientation;

// Tipos de torso
typedef enum {
    TORSO_CHEST = 0,
    TORSO_HIP = 1
} TorsoType;

// Datos de renderizado del torso
typedef struct {
    bool valid;
    bool visible;
    Vector3 position;
    TorsoOrientation orientation;  // Nueva orientación real
    TorsoType type;
    float size;
    char texturePath[MAX_FILE_PATH_LENGTH];
    char personId[16];
    const Person* person;
} TorsoRenderData;

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

#endif // TORSO_BILLBOARD_H
