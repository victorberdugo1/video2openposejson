#ifndef BILLBOARD_H
#define BILLBOARD_H

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

// Cantidad de columnas/filas en el atlas de sprites
#define ATLAS_COLS 5
#define ATLAS_ROWS 5

typedef struct Billboard {
    Texture2D atlas;      // Atlas con sprites
    Vector3   position;   // Posición en mundo
    float     size;       // Altura en mundo (ancho se ajusta por aspect ratio)
    int       col;        // Índice de columna en atlas
    int       row;        // Índice de fila en atlas
    bool      mirrored;   // Si se dibuja espejado
    float     rotation;   // Rotación en grados (para top/bottom especiales)
} Billboard;

// Inicializa un billboard con un atlas
Billboard BillboardCreate(Texture2D atlas, Vector3 pos, float size);

Rectangle GetAtlasCellSrcPos(Texture2D texture, int col, int row, bool mirrored, bool *finalMirrored);
void DrawBillboardCustom(Camera3D camera, Rectangle src, Vector3 pos, Vector2 size, float rotation, bool mirrored);

// Dibuja el billboard con la cámara activa
void BillboardDraw(Camera camera, Billboard *bb);

// Devuelve un rectángulo fuente de una celda en el atlas
Rectangle BillboardGetCellSrc(Billboard *bb);

#endif // BILLBOARD_H
