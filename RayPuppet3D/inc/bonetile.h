// bonetile.h
#ifndef BONETILE_H
#define BONETILE_H

#include "raylib.h"

#define ATLAS_COLS 4
#define ATLAS_ROWS 4

// Estructura para datos de morphing - DEBE estar aquí para evitar dependencias circulares
typedef struct {
    int primaryIndex;      // Índice principal del atlas
    int secondaryIndex;    // Índice secundario para blend
    float blendFactor;     // 0.0 = solo primary, 1.0 = solo secondary
    int effectType;
    float rotation;        // Rotación en grados
    bool mirrored;         // Si está espejado
} BoneMorphData;

// Funciones existentes
void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos,
    Vector2 size, float rotationDeg, bool mirrored);

Rectangle GetAtlasCellSrcPos(Texture2D tex, int col, int rowIndex, bool mirrored, bool* outMirrored);

// Función original que faltaba la declaración
void CalculateBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored);

// Nuevas funciones para morphing
void CalculateBoneMorphData(Vector3 bonePos, Camera camera, BoneMorphData* outMorphData);

void DrawBonetileWithMorphing(Texture2D tex, Camera camera, BoneMorphData morphData,
    Vector3 pos, Vector2 size, int physCols, int physRows);

// ESTA es la función que falta - SrcFromLogical
Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow,
    int physCols, int physRows, bool mirrored, bool* outMirrored);

#endif // BONETILE_H