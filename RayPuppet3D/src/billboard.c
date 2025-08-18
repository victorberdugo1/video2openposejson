#include "billboard.h"
#include <math.h>

// ---- Dibujar quad texturizado en 3D desde 4 vértices (usa rlgl) ----
static void DrawQuadTextured3D(Texture2D tex,
                               Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
                               float u0, float v0t, float u1, float v1t)
{
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        rlTexCoord2f(u0, v0t); rlVertex3f(v0.x, v0.y, v0.z);
        rlTexCoord2f(u1, v0t); rlVertex3f(v1.x, v1.y, v1.z);
        rlTexCoord2f(u1, v1t); rlVertex3f(v2.x, v2.y, v2.z);
        rlTexCoord2f(u0, v1t); rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
    rlSetTexture(0);
}

Billboard BillboardCreate(Texture2D atlas, Vector3 pos, float size) {
    Billboard bb = {0};
    bb.atlas = atlas;
    bb.position = pos;
    bb.size = size;
    bb.col = 0;
    bb.row = 0;
    bb.mirrored = false;
    bb.rotation = 0.0f;
    return bb;
}

Rectangle BillboardGetCellSrc(Billboard *bb) {
    float cellW = (float)bb->atlas.width / ATLAS_COLS;
    float cellH = (float)bb->atlas.height / ATLAS_ROWS;
    float srcX = bb->col * cellW;
    float srcY = (ATLAS_ROWS - 1 - bb->row) * cellH; // invertir vertical
    return (Rectangle){ srcX, srcY, cellW, cellH };
}

void BillboardDraw(Camera camera, Billboard *bb) {
    // Vectores básicos según cámara
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up    = Vector3Normalize(Vector3CrossProduct(right, camForward));

    // Rotación adicional (para top/bottom fijos)
    float a = bb->rotation * DEG2RAD;
    float ca = cosf(a);
    float sa = sinf(a);
    Vector3 newRight = Vector3Subtract(Vector3Scale(right, ca), Vector3Scale(up, sa));
    Vector3 newUp    = Vector3Add(Vector3Scale(right, sa), Vector3Scale(up, ca));

    // Tamaño en mundo según aspect ratio de la celda
    float cellW = (float)bb->atlas.width / ATLAS_COLS;
    float cellH = (float)bb->atlas.height / ATLAS_ROWS;
    float aspect = cellW / cellH;
    Vector2 worldSize = (Vector2){ bb->size * aspect, bb->size };

    Vector3 halfX = Vector3Scale(newRight, worldSize.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp,    worldSize.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(bb->position, halfX), halfY); // -x, -y
    Vector3 p1 = Vector3Add   (Vector3Subtract(bb->position, halfY), halfX);   // +x, -y
    Vector3 p2 = Vector3Add   (Vector3Add(bb->position, halfX), halfY);        // +x, +y
    Vector3 p3 = Vector3Subtract(Vector3Add(bb->position, halfY), halfX);     // -x, +y

    // UVs
    Rectangle src = BillboardGetCellSrc(bb);
    float texW = (float)bb->atlas.width;
    float texH = (float)bb->atlas.height;
    float u0 = src.x / texW;
    float u1 = (src.x + src.width) / texW;
    float v0 = src.y / texH;
    float v1 = (src.y + src.height) / texH;

    if (bb->mirrored) {
        float tmp = u0; u0 = u1; u1 = tmp;
    }

    // Raylib usa top-left = (0,0). Invertimos V para que encaje
    DrawQuadTextured3D(bb->atlas, p0, p1, p2, p3, u0, v1, u1, v0);
}
