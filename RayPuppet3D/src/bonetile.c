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
    const int bottomIndex = 15;
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
    float normalizedYaw = yawDeg + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    if (normalizedYaw < 45.0f) {
        sector = 0;
    }
    else if (normalizedYaw < 90.0f) {
        sector = 1;
    }
    else if (normalizedYaw < 135.0f) {
        sector = 2;
    }
    else if (normalizedYaw < 180.0f) {
        sector = 3;
    }
    else if (normalizedYaw < 225.0f) {
        sector = 4;
    }
    else if (normalizedYaw < 270.0f) {
        sector = 5;
    }
    else if (normalizedYaw < 315.0f) {
        sector = 6;
    }
    else {
        sector = 7;
    }

    if (useTopdown) {
        if (isTopView) {
            *outChosenIndex = topdownIndex;
            *outRotation = sector * 45.0f + 180.f;
            *outMirrored = false;
        }
        else {
            *outChosenIndex = bottomIndex;
            *outRotation = (8 - sector) * 45.0f + 180.0f;
            if (*outRotation >= 360.0f) *outRotation -= 360.0f;
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

    // GARANTIZAR ALPHA MÍNIMO para evitar transparencia extrema
    alpha = fmaxf(alpha, 0.3f);  // Mínimo 30% de opacidad
    alpha = fminf(alpha, 1.0f);  // Máximo 100% de opacidad

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
    const int bottomIndex = 15;
    const float sectorAngles[8] = { 0,45,90,135,180,225,270,315 };
    const float TOPDOWN_ANGLE = 70.0f;
    const float HIGH_THRESHOLD = 22.5f;
    const float MAIN_THRESHOLD = -22.5f;

    // AJUSTADO: Rango óptimo para morphing visible pero controlado
    const float MORPH_RANGE = 8.0f;  // ±8 grados alrededor de cada transición
    const float transitionAngles[8] = { 22.5f, 67.5f, 112.5f, 157.5f, 202.5f, 247.5f, 292.5f, 337.5f };

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

    // Calcular sector usando la misma lógica que CalculateBoneRenderData
    int sector = 0;
    float normalizedYaw = yawDeg + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    if (normalizedYaw < 45.0f) sector = 0;
    else if (normalizedYaw < 90.0f) sector = 1;
    else if (normalizedYaw < 135.0f) sector = 2;
    else if (normalizedYaw < 180.0f) sector = 3;
    else if (normalizedYaw < 225.0f) sector = 4;
    else if (normalizedYaw < 270.0f) sector = 5;
    else if (normalizedYaw < 315.0f) sector = 6;
    else sector = 7;

    // MEJORADO: Verificar si estamos en un rango de transición con rangos más amplios
    int secondarySector = sector;
    float blendFactor = 0.0f;
    bool inTransitionRange = false;

    if (!useTopdown) {
        // Verificar si estamos cerca de alguna transición
        for (int i = 0; i < 8; i++) {
            float transitionAngle = transitionAngles[i];
            float angleDiff = fabsf(yawDeg - transitionAngle);

            // Manejar el wraparound en 360°/0°
            if (angleDiff > 180.0f) angleDiff = 360.0f - angleDiff;

            if (angleDiff <= MORPH_RANGE) {
                inTransitionRange = true;

                // Determinar sectores adyacentes para esta transición
                int sector1 = i;
                int sector2 = (i + 1) % 8;

                // Calcular cuál está más cerca
                float dist1 = fabsf(yawDeg - sectorAngles[sector1]);
                if (dist1 > 180.0f) dist1 = 360.0f - dist1;

                float dist2 = fabsf(yawDeg - sectorAngles[sector2]);
                if (dist2 > 180.0f) dist2 = 360.0f - dist2;

                if (dist1 < dist2) {
                    sector = sector1;
                    secondarySector = sector2;
                }
                else {
                    sector = sector2;
                    secondarySector = sector1;
                }

                // Calcular blend factor con transición más rápida
                float normalizedDiff = angleDiff / MORPH_RANGE;  // 0.0 = centro de transición, 1.0 = borde

                // Usar una curva más agresiva para transición más rápida
                blendFactor = 2.0f - (normalizedDiff * normalizedDiff);  // Curva cuadrática inversa
                blendFactor = Clamp(blendFactor, 0.95f, 1.0f);
                break;
            }
        }
    }

    if (useTopdown) {
        if (isTopView) {
            // VISTA DESDE ARRIBA: cámara por encima del hueso - igual que CalculateBoneRenderData
            outMorphData->primaryIndex = topdownIndex;  // índice 3
            outMorphData->secondaryIndex = topdownIndex;
            outMorphData->blendFactor = 0.0f;  // Sin morphing en topdown
            outMorphData->rotation = sector * 45.0f + 180.0f;  // Igual que la función original
            outMorphData->mirrored = false;
        }
        else {
            // VISTA DESDE ABAJO: cámara por debajo del hueso - igual que CalculateBoneRenderData
            outMorphData->primaryIndex = bottomIndex;  // índice 15
            outMorphData->secondaryIndex = bottomIndex;
            outMorphData->blendFactor = 0.0f;  // Sin morphing en topdown
            outMorphData->rotation = (8 - sector) * 45.0f + 180.0f;  // Igual que la función original
            if (outMorphData->rotation >= 360.0f) outMorphData->rotation -= 360.0f;
            outMorphData->mirrored = true;
        }
    }
    else {
        outMorphData->primaryIndex = indices[chosenRow][sector];
        outMorphData->secondaryIndex = indices[chosenRow][secondarySector];
        outMorphData->blendFactor = inTransitionRange ? blendFactor : 0.0f;  // Solo blend en rango de transición
        outMorphData->rotation = 0.0f;  // Sin rotación en vistas laterales, igual que la función original
        outMorphData->mirrored = !(sector >= 5 && sector <= 7);  // Misma lógica de mirroring

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
    if (morphData.blendFactor <= 0.05f) {  // Reducido de 0.1f a 0.05f para más morphing
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

    // Dibujar textura primaria con ALPHA MÁS ALTO
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

        // ALPHA AUMENTADO: de 0.1f a 0.4f para menos transparencia
        float primaryAlpha = 1.0f - (morphData.blendFactor * 0.7f);
        DrawQuadTextured3DWithAlpha(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t, primaryAlpha);
    }

    // Dibujar textura secundaria con ALPHA MÁS ALTO
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

        // ALPHA AUMENTADO: de 0.8f a 0.9f para textura secundaria más visible
        float secondaryAlpha = morphData.blendFactor * 0.95f;
        DrawQuadTextured3DWithAlpha(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t, secondaryAlpha);
    }
}
