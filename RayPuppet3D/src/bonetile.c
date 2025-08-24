#include "bones3d.h"

/* ************************************************************************** */
/* DrawQuadTextured3D                                                         */
/* - Renders a textured quadrilateral in 3D space using raylib's rlgl API.    */
/* - Takes 4 vertices (v0–v3) and texture coordinates for proper mapping.     */
/* - Applies the given texture, draws the quad, and resets the texture state. */
/* ************************************************************************** */
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

/* ************************************************************************** */
/* DrawBonetileCustom                                                         */
/* - Draws a 2D sprite (bonetile) in 3D space relative to the camera.         */
/* - Applies size, rotation, and mirroring transformations to the quad.       */
/* - Computes correct orientation based on camera axes.                       */
/* ************************************************************************** */
void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size, float rotationDeg, bool mirrored) {
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    float a = rotationDeg * (PI / 180.0f);
    float ca = cosf(a);
    float sa = sinf(a);

    Vector3 newRight = Vector3Subtract(Vector3Scale(right, ca), Vector3Scale(up, sa));
    Vector3 newUp = Vector3Add(Vector3Scale(right, sa), Vector3Scale(up, ca));

    Vector3 halfX = Vector3Scale(newRight, size.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp, size.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(pos, halfX), halfY);
    Vector3 p1 = Vector3Add(Vector3Subtract(pos, halfY), halfX);
    Vector3 p2 = Vector3Add(Vector3Add(pos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(pos, halfY), halfX);

    float texW = (float)tex.width;
    float texH = (float)tex.height;
    float u_left = src.x / texW;
    float u_right = (src.x + src.width) / texW;
    float v_top = src.y / texH;
    float v_bottom = (src.y + src.height) / texW;

    if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
    if (src.height < 0) { float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

    float v0t = v_bottom;
    float v1t = v_top;

    if (mirrored) {
        float tmp = u_left; u_left = u_right; u_right = tmp;
    }

    DrawQuadTextured3D(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t);
}

/* ************************************************************************** */
/* GetAtlasCellSrcPos                                                         */
/* - Returns the source rectangle for a specific cell in a texture atlas.     */
/* - Computes cell position based on column and row indices.                  */
/* - Adjusts mirroring flag if provided.                                      */
/* ************************************************************************** */
Rectangle GetAtlasCellSrcPos(Texture2D tex, int col, int rowIndex, bool mirrored, bool* outMirrored) {
    int atlasRow;
    float cellW;
    float cellH;
    float srcX;
    float srcY;

    atlasRow = 5 - 1 - rowIndex;
    cellW = (float)tex.width / ATLAS_COLS;
    cellH = (float)tex.height / ATLAS_ROWS;
    srcX = col * cellW;
    srcY = atlasRow * cellH;
    if (outMirrored)
        *outMirrored = mirrored;
    return (Rectangle) { srcX, srcY, cellW, cellH };
}

/* ************************************************************************** */
/* CalculateBoneRenderData                                                    */
/* - Determines which sprite frame to use for a bone depending on camera view.*/
/* - Calculates yaw and pitch relative to the bone position.                  */
/* - Chooses correct row, index, rotation, and mirroring for rendering.       */
/* ************************************************************************** */
void CalculateBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored) {
    const int indices[3][8] = {
        {  0,  4,  5,  6,  7,  6,  5,  4 },
        {  2, 12, 13, 14, 15, 14, 13, 12 },
        {  1,  8,  9, 10, 11, 10,  9,  8 }
    };
    const int topdownIndex = 3;
    const float sectorAngles[8] = { 0,45,90,135,180,225,270,315 };
    const float TOPDOWN_ANGLE = 70.0f;
    const float HIGH_THRESHOLD = 22.5f;
    const float MAIN_THRESHOLD = -22.5f;

    Vector3 camDir = Vector3Subtract(camera.position, bonePos);
    float yaw = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;
    float yawDeg = yaw * RAD2DEG;

    float horiz = sqrtf(camDir.x * camDir.x + camDir.z * camDir.z);
    float pitch = atan2f(camDir.y, horiz);
    float pitchDeg = pitch * RAD2DEG;

    int chosenRow = -1;
    bool useTopdown = false;
    bool isTopView = false;

    if (pitchDeg >= TOPDOWN_ANGLE) {
        useTopdown = true;
        isTopView = true;
    }
    else if (pitchDeg >= HIGH_THRESHOLD) {
        chosenRow = 2;
    }
    else if (pitchDeg >= MAIN_THRESHOLD) {
        chosenRow = 0;
    }
    else if (pitchDeg >= -TOPDOWN_ANGLE) {
        chosenRow = 1;
    }
    else {
        useTopdown = true;
        isTopView = false;
    }

    int sector = 0;
    float minDiff = 360.0f;
    for (int i = 0; i < 8; i++) {
        float diff = fabsf(yawDeg - sectorAngles[i]);
        if (diff > 180.0f) diff = 360.0f - diff;
        if (diff < minDiff) { minDiff = diff; sector = i; }
    }

    if (useTopdown) {
        *outChosenIndex = topdownIndex;
        if (isTopView) {
            *outRotation = sectorAngles[sector] + 180.0f;
            *outMirrored = false;
        }
        else {
            *outRotation = 360.0f - sectorAngles[sector];
            *outMirrored = true;
        }
    }
    else {
        *outChosenIndex = indices[chosenRow][sector];
        *outRotation = 0.0f;
        *outMirrored = !(sector >= 5 && sector <= 7);
    }
}

/* ************************************************************************** */
/* SrcFromLogical                                                             */
/* - Converts logical atlas coordinates into a physical source rectangle.     */
/* - Handles boundary clamping and mirroring flags.                           */
/* - Useful when logical atlas layout differs from physical texture grid.     */
/* ************************************************************************** */
Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows, bool mirrored, bool* outMirrored) {
    if (logicalCol < 0) logicalCol = 0;
    if (logicalCol >= ATLAS_COLS) logicalCol = ATLAS_COLS - 1;
    if (logicalRow < 0) logicalRow = 0;
    if (logicalRow >= ATLAS_ROWS) logicalRow = ATLAS_ROWS - 1;

    float physCellW = (float)tex.width / (float)physCols;
    float physCellH = (float)tex.height / (float)physRows;

    int blockW = physCols / ATLAS_COLS;
    int blockH = physRows / ATLAS_ROWS;

    int physCol = logicalCol * blockW;
    int physRow = logicalRow * blockH;

    float srcX = physCol * physCellW;
    float srcY = physRow * physCellH;
    float srcW = physCellW * blockW;
    float srcH = physCellH * blockH;

    if (outMirrored) *outMirrored = mirrored;
    return (Rectangle) { srcX, srcY, srcW, srcH };
}

// Helper con alpha personalizado
static void DrawQuadTextured3DWithAlpha(Texture2D tex, Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
    float u0, float v0t, float u1, float v1t, float alpha) {
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    unsigned char alphaValue = (unsigned char)(alpha * 255.0f);
    rlColor4ub(255, 255, 255, alphaValue);
    rlTexCoord2f(u0, v0t); rlVertex3f(v0.x, v0.y, v0.z);
    rlTexCoord2f(u1, v0t); rlVertex3f(v1.x, v1.y, v1.z);
    rlTexCoord2f(u1, v1t); rlVertex3f(v2.x, v2.y, v2.z);
    rlTexCoord2f(u0, v1t); rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
    rlSetTexture(0);
}

// Nueva función para calcular datos de morphing MÁS AGRESIVA
void CalculateBoneMorphData(Vector3 bonePos, Camera camera, BoneMorphData* outMorphData) {
    const int indices[3][8] = {
        {  0,  4,  5,  6,  7,  6,  5,  4 },
        {  2, 12, 13, 14, 15, 14, 13, 12 },
        {  1,  8,  9, 10, 11, 10,  9,  8 }
    };
    const int topdownIndex = 3;
    const float sectorAngles[8] = { 0,45,90,135,180,225,270,315 };
    const float TOPDOWN_ANGLE = 70.0f;
    const float HIGH_THRESHOLD = 22.5f;
    const float MAIN_THRESHOLD = -22.5f;

    Vector3 camDir = Vector3Subtract(camera.position, bonePos);
    float yaw = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;
    float yawDeg = yaw * RAD2DEG;

    float horiz = sqrtf(camDir.x * camDir.x + camDir.z * camDir.z);
    float pitch = atan2f(camDir.y, horiz);
    float pitchDeg = pitch * RAD2DEG;

    // Determinar fila
    int chosenRow = -1;
    bool useTopdown = false;
    bool isTopView = false;

    if (pitchDeg >= TOPDOWN_ANGLE) {
        useTopdown = true;
        isTopView = true;
    }
    else if (pitchDeg >= HIGH_THRESHOLD) {
        chosenRow = 2;
    }
    else if (pitchDeg >= MAIN_THRESHOLD) {
        chosenRow = 0;
    }
    else if (pitchDeg >= -TOPDOWN_ANGLE) {
        chosenRow = 1;
    }
    else {
        useTopdown = true;
        isTopView = false;
    }

    // Encontrar sector principal
    int primarySector = 0;
    float minDistance = 360.0f;
    for (int i = 0; i < 8; i++) {
        float angleDiff = fabsf(yawDeg - sectorAngles[i]);
        if (angleDiff > 180.0f) angleDiff = 360.0f - angleDiff;

        if (angleDiff < minDistance) {
            minDistance = angleDiff;
            primarySector = i;
        }
    }

    // Encontrar sector secundario (adyacente)
    int secondarySector = primarySector;
    float blendFactor = 0.0f;

    if (!useTopdown) {
        // Calcular qué tan cerca estamos del borde entre sectores
        float halfSector = 22.5f; // 45/2 grados

        if (minDistance > 5.0f) { // Solo hacer blend si no estamos muy centrados
            // Encontrar el sector adyacente más cercano
            int leftSector = (primarySector - 1 + 8) % 8;
            int rightSector = (primarySector + 1) % 8;

            float leftDiff = fabsf(yawDeg - sectorAngles[leftSector]);
            if (leftDiff > 180.0f) leftDiff = 360.0f - leftDiff;

            float rightDiff = fabsf(yawDeg - sectorAngles[rightSector]);
            if (rightDiff > 180.0f) rightDiff = 360.0f - rightDiff;

            // Elegir el sector adyacente más cercano
            if (leftDiff < rightDiff) {
                secondarySector = leftSector;
            }
            else {
                secondarySector = rightSector;
            }

            // Calcular factor de blend más agresivo
            if (minDistance < halfSector) {
                blendFactor = minDistance / halfSector;
                // Hacer el blend más visible
                blendFactor = powf(blendFactor, 0.7f); // Curva que da más blend
                blendFactor = Clamp(blendFactor, 0.0f, 0.85f); // Más blend máximo
            }
        }
    }

    if (useTopdown) {
        outMorphData->primaryIndex = topdownIndex;
        outMorphData->secondaryIndex = topdownIndex;
        outMorphData->blendFactor = 0.0f;

        if (isTopView) {
            outMorphData->rotation = sectorAngles[primarySector] + 180.0f;
            outMorphData->mirrored = false;
        }
        else {
            outMorphData->rotation = 360.0f - sectorAngles[primarySector];
            outMorphData->mirrored = true;
        }
    }
    else {
        outMorphData->primaryIndex = indices[chosenRow][primarySector];
        outMorphData->secondaryIndex = indices[chosenRow][secondarySector];
        outMorphData->blendFactor = blendFactor;
        outMorphData->rotation = 0.0f;
        outMorphData->mirrored = !(primarySector >= 5 && primarySector <= 7);

        // Validar que son diferentes
        if (outMorphData->primaryIndex == outMorphData->secondaryIndex) {
            outMorphData->blendFactor = 0.0f;
        }
    }
}

// Función para dibujar con morphing MÁS VISIBLE
void DrawBonetileWithMorphing(Texture2D tex, Camera camera, BoneMorphData morphData,
    Vector3 pos, Vector2 size, int physCols, int physRows) {

    // Solo renderizar normal si realmente no hay blend
    if (morphData.blendFactor <= 0.1f) {
        int logicalCol = morphData.primaryIndex % ATLAS_COLS;
        int logicalRow = morphData.primaryIndex / ATLAS_COLS;

        Rectangle src = SrcFromLogical(tex, logicalCol, logicalRow, physCols, physRows,
            morphData.mirrored, NULL);
        DrawBonetileCustom(tex, camera, src, pos, size, morphData.rotation, morphData.mirrored);
        return;
    }

    // Calcular geometría común
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    float a = morphData.rotation * (PI / 180.0f);
    float ca = cosf(a);
    float sa = sinf(a);

    Vector3 newRight = Vector3Subtract(Vector3Scale(right, ca), Vector3Scale(up, sa));
    Vector3 newUp = Vector3Add(Vector3Scale(right, sa), Vector3Scale(up, ca));

    Vector3 halfX = Vector3Scale(newRight, size.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp, size.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(pos, halfX), halfY);
    Vector3 p1 = Vector3Add(Vector3Subtract(pos, halfY), halfX);
    Vector3 p2 = Vector3Add(Vector3Add(pos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(pos, halfY), halfX);

    float texW = (float)tex.width;
    float texH = (float)tex.height;

    // Habilitar blending
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableColorBlend();

    // Dibujar textura primaria
    {
        int logicalCol = morphData.primaryIndex % ATLAS_COLS;
        int logicalRow = morphData.primaryIndex / ATLAS_COLS;
        Rectangle src = SrcFromLogical(tex, logicalCol, logicalRow, physCols, physRows,
            morphData.mirrored, NULL);

        float u_left = src.x / texW;
        float u_right = (src.x + src.width) / texW;
        float v_top = src.y / texH;
        float v_bottom = (src.y + src.height) / texH;

        if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
        if (src.height < 0) { float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

        float v0t = v_bottom;
        float v1t = v_top;

        if (morphData.mirrored) {
            float tmp = u_left; u_left = u_right; u_right = tmp;
        }

        // Alpha más agresivo para la textura primaria
        float primaryAlpha = 1.0f - (morphData.blendFactor * 0.9f);
        DrawQuadTextured3DWithAlpha(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t, primaryAlpha);
    }

    // Dibujar textura secundaria con más visibilidad
    if (morphData.blendFactor > 0.01f && morphData.secondaryIndex != morphData.primaryIndex) {
        int logicalCol = morphData.secondaryIndex % ATLAS_COLS;
        int logicalRow = morphData.secondaryIndex / ATLAS_COLS;
        Rectangle src = SrcFromLogical(tex, logicalCol, logicalRow, physCols, physRows,
            morphData.mirrored, NULL);

        float u_left = src.x / texW;
        float u_right = (src.x + src.width) / texW;
        float v_top = src.y / texH;
        float v_bottom = (src.y + src.height) / texH;

        if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
        if (src.height < 0) { float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

        float v0t = v_bottom;
        float v1t = v_top;

        if (morphData.mirrored) {
            float tmp = u_left; u_left = u_right; u_right = tmp;
        }

        // Alpha más visible para la textura secundaria
        float secondaryAlpha = morphData.blendFactor * 0.8f;
        DrawQuadTextured3DWithAlpha(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t, secondaryAlpha);
    }
}