// bonetile.c
#include "bonetile.h"
#include <math.h>

// Helper: dibuja quad texturizado usando la textura pasada
static void DrawQuadTextured3D(Texture2D tex, Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3, float u0, float v0t, float u1, float v1t) {
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

void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size, float rotationDeg, bool mirrored) {
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    float a = rotationDeg * (PI / 180.0f);
    float ca = cosf(a);
    float sa = sinf(a);

    Vector3 newRight = Vector3Subtract(Vector3Scale(right, ca), Vector3Scale(up, sa));
    Vector3 newUp    = Vector3Add(Vector3Scale(right, sa), Vector3Scale(up, ca));

    Vector3 halfX = Vector3Scale(newRight, size.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp,    size.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(pos, halfX), halfY);
    Vector3 p1 = Vector3Add   (Vector3Subtract(pos, halfY), halfX);
    Vector3 p2 = Vector3Add   (Vector3Add(pos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(pos, halfY), halfX);

    float texW = (float)tex.width;
    float texH = (float)tex.height;
    float u_left  = src.x / texW;
    float u_right = (src.x + src.width) / texW;
    float v_top   = src.y / texH;
    float v_bottom= (src.y + src.height) / texH;

    if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
    if (src.height < 0){ float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

    float v0t = v_bottom;
    float v1t = v_top;

    if (mirrored) {
        float tmp = u_left; u_left = u_right; u_right = tmp;
    }

    DrawQuadTextured3D(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t);
}

Rectangle GetAtlasCellSrcPos(Texture2D tex, int col, int rowIndex, bool mirrored, bool *outMirrored) {
    // rowIndex: 0 = top
    int atlasRow = ATLAS_ROWS - 1 - rowIndex;
    float cellW = (float)tex.width / ATLAS_COLS;
    float cellH = (float)tex.height / ATLAS_ROWS;
    float srcX = col * cellW;
    float srcY = atlasRow * cellH;

    if (outMirrored) *outMirrored = mirrored;
    return (Rectangle){ srcX, srcY, cellW, cellH };
}

