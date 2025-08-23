#ifndef BONETILE_H
#define BONETILE_H

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

#define ATLAS_COLS 4
#define ATLAS_ROWS 4
#define AXIS_YAW 5
#define AXIS_PITCH 5

Rectangle GetAtlasCellSrcPos(Texture2D tex, int col, int rowIndex, bool mirrored, bool* outMirrored);
void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size, float rotationDeg, bool mirrored);
void CalculateBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored);
Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows, bool mirrored, bool* outMirrored);

#endif