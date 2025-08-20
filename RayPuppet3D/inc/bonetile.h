// bonetile.h
#ifndef BONETILE_H
#define BONETILE_H

#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"

// Tamaño del atlas (defínelo aquí una vez)
#define ATLAS_COLS 4
#define ATLAS_ROWS 4

#define AXIS_YAW 5
#define AXIS_PITCH 5

// Prototipos públicos (las funciones reciben Texture2D en vez de depender de global)
Rectangle GetAtlasCellSrcPos(Texture2D tex, int col, int rowIndex, bool mirrored, bool *outMirrored);
void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size, float rotationDeg, bool mirrored);

#endif // BONETILE_H

