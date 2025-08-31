#include "bonetile.h"
#include "bones3d.h" 
#include <string.h>
#include <math.h>

static const struct {
    const char* boneName;
    const char* connections[3];
    float priority[3];
} BONE_CONNECTIONS[] = {
    {"LShoulder", {"LShoulder", "LElbow", ""}, {1.0f, 0.8f, 0.0f}},
    {"LElbow", {"LElbow", "LWrist", ""}, {0.8f, 1.0f, 0.0f}},
    {"LWrist", {"LElbow", "", ""}, {1.0f, 0.0f, 0.0f}},
    {"RShoulder", {"RShoulder", "RElbow", ""}, {1.0f, 0.8f, 0.0f}},
    {"RElbow", {"RElbow", "RWrist", ""}, {0.8f, 1.0f, 0.0f}},
    {"RWrist", {"RElbow", "", ""}, {1.0f, 0.0f, 0.0f}},
    {"LHip", {"LHip", "LKnee", ""}, {1.0f, 0.8f, 0.0f}},
    {"LKnee", {"LKnee", "LAnkle", ""}, {0.8f, 1.0f, 0.0f}},
    {"LAnkle", {"LKnee", "", ""}, {1.0f, 0.0f, 0.0f}},
    {"RHip", {"RHip", "RKnee", ""}, {1.0f, 0.8f, 0.0f}},
    {"RKnee", {"RKnee", "RAnkle", ""}, {0.8f, 1.0f, 0.0f}},
    {"RAnkle", {"RKnee", "", ""}, {1.0f, 0.0f, 0.0f}},
    {"Neck", {"Head", "Neck", ""},  {0.8f, 1.0f, 0.0f}},
    {"", {"", "", ""}, {0.0f, 0.0f, 0.0f}}
};

bool GetBoneConnectionsWithPriority(const char* boneName, char connections[3][32], float priorities[3]) {
    for (int i = 0; BONE_CONNECTIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_CONNECTIONS[i].boneName, boneName) == 0) {
            for (int j = 0; j < 3; j++) {
                strncpy(connections[j], BONE_CONNECTIONS[i].connections[j], 31);
                connections[j][31] = '\0';
                priorities[j] = BONE_CONNECTIONS[i].priority[j];
            }
            return true;
        }
    }
    return false;
}

void CalculateEnhancedBoneRenderData(const BoneRenderData* boneData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!boneData->orientation.valid) {
        CalculateBoneRenderData(boneData->position, camera, outChosenIndex, outRotation, outMirrored, "");
        return;
    }

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

    Vector3 camDir = Vector3Subtract(camera.position, boneData->position);

    Vector3 localCamDir;
    localCamDir.x = Vector3DotProduct(camDir, boneData->orientation.right);
    localCamDir.y = Vector3DotProduct(camDir, boneData->orientation.up);
    localCamDir.z = Vector3DotProduct(camDir, boneData->orientation.forward);

    localCamDir.x = -localCamDir.x;

    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localYawDeg = localYaw * RAD2DEG;

    float horizDistance = sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z);
    float localPitch = atan2f(localCamDir.y, horizDistance);
    float localPitchDeg = localPitch * RAD2DEG;

    int chosenRow = -1;
    bool useTopdown = false;
    bool isTopView = false;

    if (localPitchDeg >= TOPDOWN_ANGLE) {
        useTopdown = true;
        isTopView = true;
    }
    else if (localPitchDeg >= HIGH_THRESHOLD) {
        chosenRow = 2;
    }
    else if (localPitchDeg >= MAIN_THRESHOLD) {
        chosenRow = 0;
    }
    else if (localPitchDeg >= -TOPDOWN_ANGLE) {
        chosenRow = 1;
    }
    else {
        useTopdown = true;
        isTopView = false;
    }

    int sector = 0;
    float normalizedYaw = localYawDeg + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    if (normalizedYaw < 45.0f) sector = 0;
    else if (normalizedYaw < 90.0f) sector = 1;
    else if (normalizedYaw < 135.0f) sector = 2;
    else if (normalizedYaw < 180.0f) sector = 3;
    else if (normalizedYaw < 225.0f) sector = 4;
    else if (normalizedYaw < 270.0f) sector = 5;
    else if (normalizedYaw < 315.0f) sector = 6;
    else sector = 7;

    if (useTopdown) {
        if (isTopView) {
            *outChosenIndex = topdownIndex;
            *outRotation = sector * 45.0f + 180.0f;
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

static void DrawQuadTextured3D(Texture2D tex, Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
    float u0, float v0t, float u1, float v1t) {
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



Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows,
    bool mirrored, bool* outMirrored) {
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




// Versión extendida de DrawQuadTextured3D que acepta UVs por vértice
static void DrawQuadTextured3D_UVs(Texture2D tex,
    Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
    Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3)
{
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);

    rlTexCoord2f(uv0.x, uv0.y); rlVertex3f(v0.x, v0.y, v0.z);
    rlTexCoord2f(uv1.x, uv1.y); rlVertex3f(v1.x, v1.y, v1.z);
    rlTexCoord2f(uv2.x, uv2.y); rlVertex3f(v2.x, v2.y, v2.z);
    rlTexCoord2f(uv3.x, uv3.y); rlVertex3f(v3.x, v3.y, v3.z);

    rlEnd();
    rlSetTexture(0);
}

// Add this function to check if a bone needs V-flip
static bool ShouldFlipBoneTexture(const char* boneName) {
    if (!boneName) return false;
    
    // List of bones that appear upside down and need V-coordinate flipping
    const char* flipBones[] = {
        "LShoulder", "LElbow", 
        "RShoulder", "RElbow",
        "LHip", "LKnee",
        "RHip", "RKnee"
    };
    
    int flipBonesCount = sizeof(flipBones) / sizeof(flipBones[0]);
    
    for (int i = 0; i < flipBonesCount; i++) {
        if (strcmp(boneName, flipBones[i]) == 0) {
            return true;
        }
    }
    
    return false;
}

// Modified CalculateBoneRenderData function in bonetile.c
void CalculateBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored, const char* boneName) {
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

    if (normalizedYaw < 45.0f) sector = 0;
    else if (normalizedYaw < 90.0f) sector = 1;
    else if (normalizedYaw < 135.0f) sector = 2;
    else if (normalizedYaw < 180.0f) sector = 3;
    else if (normalizedYaw < 225.0f) sector = 4;
    else if (normalizedYaw < 270.0f) sector = 5;
    else if (normalizedYaw < 315.0f) sector = 6;
    else sector = 7;

    // Check if this bone needs V-flip correction
    bool needsVFlip = boneName ? ShouldFlipBoneTexture(boneName) : false;

    // For V-flipped bones, we need to adjust the sector mapping to account for the vertical flip
    if (needsVFlip && !useTopdown) {
        // When we flip V-coordinates, the rotational mapping gets inverted
        // We need to mirror the sector horizontally to compensate
        sector = (8 - sector) % 8;
    }

    if (useTopdown) {
        if (isTopView) {
            *outChosenIndex = topdownIndex;
            *outRotation = sector * 45.0f + 180.0f;
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


// Modified DrawBonetileCustom function
void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, const char* boneName) {
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
    float v_bottom = (src.y + src.height) / texH;

    if (src.width < 0) {
        float tmp = u_left; u_left = u_right; u_right = tmp;
    }
    if (src.height < 0) {
        float tmp = v_top; v_top = v_bottom; v_bottom = tmp;
    }

    // Check if this bone needs V-flip to correct upside-down appearance
    bool needsVFlip = ShouldFlipBoneTexture(boneName);
    
    float v0t, v1t;
    if (needsVFlip) {
        // Flip V coordinates for bones that appear upside down
        v0t = v_top;
        v1t = v_bottom;
    } else {
        // Keep original V coordinates for other bones
        v0t = v_bottom;
        v1t = v_top;
    }

    if (mirrored) {
        float tmp = u_left; u_left = u_right; u_right = tmp;
    }

    DrawQuadTextured3D(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t);
}

// Modified DrawBonetileCustomWithRoll function
void DrawBonetileCustomWithRoll(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, bool adjustUV, bool neighborValid, Vector3 neighborPos, const char* boneName)
{
    // --- calculo del sistema de ejes del billboard mirando a cámara ---
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    // si hay roll hacia vecino, lo calculamos en radianes
    float rollExtraDeg = 0.0f;
    if (neighborValid) {
        Vector3 dir = Vector3Subtract(neighborPos, pos);
        if (Vector3Length(dir) > 0.0001f) {
            dir = Vector3Normalize(dir);

            // proyectamos 'dir' sobre las bases RIGHT/UP del billboard (sin rotación aún)
            // NOTA: Usamos el sistema sin aplicar rotationDeg todavía (más estable)
            float px = Vector3DotProduct(dir, right);
            float py = Vector3DotProduct(dir, up);

            // atan2(px, py) da el ángulo alrededor del eje de la vista (roll)
            float rollRad = atan2f(px, py); // radianes
            rollExtraDeg = rollRad * (180.0f / PI);
        }
    }

    // combinamos rotación de atlas (rotationDeg) + rollExtraDeg
    float totalRotationDeg = rotationDeg + rollExtraDeg;

    // aplicamos rotación total para obtener newRight/newUp (rotación en el plano billboard)
    float a = totalRotationDeg * (PI / 180.0f);
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
    float v_bottom = (src.y + src.height) / texH;

    if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
    if (src.height < 0) { float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

    // Check if this bone needs V-flip to correct upside-down appearance
    bool needsVFlip = ShouldFlipBoneTexture(boneName);
    
    float v0t, v1t;
    if (needsVFlip) {
        // Flip V coordinates for bones that appear upside down
        v0t = v_top;
        v1t = v_bottom;
    } else {
        // Keep original V coordinates for other bones
        v0t = v_bottom;
        v1t = v_top;
    }

    // aplicar mirror (intercambia u)
    if (mirrored) {
        float tmp = u_left; u_left = u_right; u_right = tmp;
    }

    if (!adjustUV || fabsf(rollExtraDeg) < 0.0001f) {
        // comportamiento original: UVs alineadas sin rotar textura
        Vector2 uv0 = { u_left,  v0t }; // p0
        Vector2 uv1 = { u_right, v0t }; // p1
        Vector2 uv2 = { u_right, v1t }; // p2
        Vector2 uv3 = { u_left,  v1t }; // p3

        DrawQuadTextured3D_UVs(tex, p0, p1, p2, p3, uv0, uv1, uv2, uv3);
    }
    else {
        // rotar UV alrededor del centro de la sub-rect del atlas
        Vector2 uv0 = { u_left,  v0t };
        Vector2 uv1 = { u_right, v0t };
        Vector2 uv2 = { u_right, v1t };
        Vector2 uv3 = { u_left,  v1t };

        DrawQuadTextured3D_UVs(tex, p0, p1, p2, p3, uv0, uv1, uv2, uv3);
    }
}

BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int count, const char* name) {
    if (!bones || !name) return NULL;
    for (int i = 0; i < count; i++) {
        // asumimos que BoneRenderData tiene campos 'name', 'valid' y 'visible' (como en tu código)
        if (bones[i].valid && bones[i].visible && strcmp(bones[i].boneName, name) == 0) {
            return &bones[i];
        }
    }
    return NULL;
}