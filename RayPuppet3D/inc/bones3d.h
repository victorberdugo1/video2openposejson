// bones3d.h
#ifndef BONES3D_H
#define BONES3D_H

#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"
#include "raymath.h"

/*
  Header compacto para manejar el esqueleto 3D.
  Contiene las estructuras de Bone3D y funciones relacionadas.
*/

#define MAX_CHILDREN3D    64
#define MAX_KEYFRAMES3D   4096

typedef struct {
    uint32_t time;
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
} Keyframe3D;

typedef struct Bone3D {
    char name[128];

    /* bind pose local */
    Vector3 bindPos;
    Quaternion bindRot;
    Vector3 bindScale;

    /* runtime local transform (result of animation interpolation) */
    Vector3 localPos;
    Quaternion localRot;
    Vector3 localScale;

    /* matrices */
    Matrix localMatrix;
    Matrix worldMatrix;
    Matrix invBindMatrix;

    /* hierarchy */
    uint8_t childCount;
    struct Bone3D *child[MAX_CHILDREN3D];
    struct Bone3D *parent;

    /* keyframes */
    uint32_t keyframeCount;
    Keyframe3D keyframes[MAX_KEYFRAMES3D];

    /* flags / id */
    uint8_t flags;
    uint32_t id;
} Bone3D;

/* --- Funciones públicas (prototipos) --- */

/* Crear / destruir */
Bone3D* bone3dCreate(const char *name, Vector3 bindPos, Quaternion bindRot, Vector3 bindScale);
void bone3dFreeTree(Bone3D *root);

/* Añadir hijo */
Bone3D* bone3dAddChild(Bone3D *parent, const char *name, Vector3 bindPos, Quaternion bindRot, Vector3 bindScale);

/* Actualizar matrices recursivamente (debe llamarse después de animar localPos/localRot/localScale) */
void bone3dUpdateMatricesRec(Bone3D *root);

/* Buscar por nombre (recursivo) */
Bone3D* bone3dFindByName(Bone3D *root, const char *name);

/* Interpolar keyframes (simple): actualiza localPos/localRot/localScale en cada bone para el tiempo 't' (float 0..1 o uint time) */
void bone3dApplyKeyframesSimple(Bone3D *root, float t);

/* Aux: calcular invBind matrices (llamar después de construir bind pose) */
void bone3dCalculateInvBind(Bone3D *root);

/* Debug draw: dibuja ejes y líneas parent->child */
void bone3dDrawDebug(Bone3D *root, Color color);

#endif // BONES3D_H

