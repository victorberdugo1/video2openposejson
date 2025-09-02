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
    for (int i = 0; BONE_CONNECTIONS[i].boneName[0]; i++) {
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

    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},{2,12,13,14,15,14,13,12},{1,8,9,10,11,10,9,8} };

    Vector3 camDir = Vector3Subtract(camera.position, boneData->position);
    Vector3 localCamDir = {
        -Vector3DotProduct(camDir, boneData->orientation.right),
        Vector3DotProduct(camDir, boneData->orientation.up),
        Vector3DotProduct(camDir, boneData->orientation.forward)
    };

    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;

    float localPitchDeg = atan2f(localCamDir.y, sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z)) * RAD2DEG;

    float normalizedYaw = localYaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    int sector = (int)(normalizedYaw / 45.0f);

    if (localPitchDeg >= 70.0f) {
        *outChosenIndex = 3;
        *outRotation = sector * 45.0f + 180.0f;
        *outMirrored = false;
    }
    else if (localPitchDeg <= -70.0f) {
        *outChosenIndex = 15;
        *outRotation = (8 - sector) * 45.0f + 180.0f;
        if (*outRotation >= 360.0f) *outRotation -= 360.0f;
        *outMirrored = true;
    }
    else {
        int row = (localPitchDeg >= 22.5f) ? 2 : (localPitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[row][sector];
        *outRotation = 0.0f;
        *outMirrored = sector < 5 || sector > 7;
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
    logicalCol = (logicalCol < 0) ? 0 : (logicalCol >= ATLAS_COLS) ? ATLAS_COLS - 1 : logicalCol;
    logicalRow = (logicalRow < 0) ? 0 : (logicalRow >= ATLAS_ROWS) ? ATLAS_ROWS - 1 : logicalRow;

    float physCellW = (float)tex.width / physCols;
    float physCellH = (float)tex.height / physRows;

    int blockW = physCols / ATLAS_COLS;
    int blockH = physRows / ATLAS_ROWS;

    if (outMirrored) *outMirrored = mirrored;
    return (Rectangle) {
        logicalCol* blockW* physCellW,
            logicalRow* blockH* physCellH,
            physCellW* blockW,
            physCellH* blockH
    };
}

static void DrawQuadTextured3D_UVs(Texture2D tex,
    Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
    Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3) {
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

static bool ShouldFlipBoneTexture(const char* boneName) {
    if (!boneName) return false;

    static const char* flipBones[] = {
        "LShoulder", "LElbow", "RShoulder", "RElbow",
        "LHip", "LKnee", "RHip", "RKnee"
    };

    for (int i = 0; i < 8; i++) {
        if (strcmp(boneName, flipBones[i]) == 0) return true;
    }
    return false;
}

void CalculateBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored, const char* boneName) {
    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},{2,12,13,14,15,14,13,12},{1,8,9,10,11,10,9,8} };

    Vector3 camDir = Vector3Subtract(camera.position, bonePos);
    float yaw = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;

    float pitchDeg = atan2f(camDir.y, sqrtf(camDir.x * camDir.x + camDir.z * camDir.z)) * RAD2DEG;

    float normalizedYaw = yaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    int sector = (int)(normalizedYaw / 45.0f);

    bool needsVFlip = boneName && ShouldFlipBoneTexture(boneName);

    if (pitchDeg >= 70.0f) {
        *outChosenIndex = 3;
        *outRotation = sector * 45.0f + 180.0f;
        *outMirrored = false;
    }
    else if (pitchDeg <= -70.0f) {
        *outChosenIndex = 15;
        *outRotation = (8 - sector) * 45.0f + 180.0f;
        if (*outRotation >= 360.0f) *outRotation -= 360.0f;
        *outMirrored = true;
    }
    else {
        if (needsVFlip) sector = (8 - sector) % 8;
        int row = (pitchDeg >= 22.5f) ? 2 : (pitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[row][sector];
        *outRotation = 0.0f;
        *outMirrored = sector < 5 || sector > 7;
    }
}

void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, const char* boneName) {
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    float a = rotationDeg * (PI / 180.0f);
    Vector3 newRight = Vector3Subtract(Vector3Scale(right, cosf(a)), Vector3Scale(up, sinf(a)));
    Vector3 newUp = Vector3Add(Vector3Scale(right, sinf(a)), Vector3Scale(up, cosf(a)));

    Vector3 halfX = Vector3Scale(newRight, size.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp, size.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(pos, halfX), halfY);
    Vector3 p1 = Vector3Add(Vector3Subtract(pos, halfY), halfX);
    Vector3 p2 = Vector3Add(Vector3Add(pos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(pos, halfY), halfX);

    float texW = (float)tex.width, texH = (float)tex.height;
    float u_left = src.x / texW, u_right = (src.x + src.width) / texW;
    float v_top = src.y / texH, v_bottom = (src.y + src.height) / texH;

    if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
    if (src.height < 0) { float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

    bool needsVFlip = ShouldFlipBoneTexture(boneName);
    float v0t = needsVFlip ? v_top : v_bottom;
    float v1t = needsVFlip ? v_bottom : v_top;

    if (mirrored) { float tmp = u_left; u_left = u_right; u_right = tmp; }

    DrawQuadTextured3D(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t);
}

void DrawBonetileCustomWithRoll(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, bool neighborValid, Vector3 neighborPos, const char* boneName) {
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    float rollExtraDeg = 0.0f;
    if (neighborValid) {
        Vector3 dir = Vector3Subtract(neighborPos, pos);
        if (Vector3Length(dir) > 0.0001f) {
            dir = Vector3Normalize(dir);
            rollExtraDeg = atan2f(Vector3DotProduct(dir, right), Vector3DotProduct(dir, up)) * (180.0f / PI);
        }
    }

    float a = (rotationDeg + rollExtraDeg) * (PI / 180.0f);
    Vector3 newRight = Vector3Subtract(Vector3Scale(right, cosf(a)), Vector3Scale(up, sinf(a)));
    Vector3 newUp = Vector3Add(Vector3Scale(right, sinf(a)), Vector3Scale(up, cosf(a)));

    Vector3 halfX = Vector3Scale(newRight, size.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp, size.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(pos, halfX), halfY);
    Vector3 p1 = Vector3Add(Vector3Subtract(pos, halfY), halfX);
    Vector3 p2 = Vector3Add(Vector3Add(pos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(pos, halfY), halfX);

    float texW = (float)tex.width, texH = (float)tex.height;
    float u_left = src.x / texW, u_right = (src.x + src.width) / texW;
    float v_top = src.y / texH, v_bottom = (src.y + src.height) / texH;

    if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
    if (src.height < 0) { float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

    bool needsVFlip = ShouldFlipBoneTexture(boneName);
    float v0t = needsVFlip ? v_top : v_bottom;
    float v1t = needsVFlip ? v_bottom : v_top;

    if (mirrored) { float tmp = u_left; u_left = u_right; u_right = tmp; }

    Vector2 uv0 = { u_left, v0t }, uv1 = { u_right, v0t }, uv2 = { u_right, v1t }, uv3 = { u_left, v1t };
    DrawQuadTextured3D_UVs(tex, p0, p1, p2, p3, uv0, uv1, uv2, uv3);
}

BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int count, const char* name) {
    if (!bones || !name) return NULL;
    for (int i = 0; i < count; i++) {
        if (bones[i].valid && bones[i].visible && strcmp(bones[i].boneName, name) == 0) {
            return &bones[i];
        }
    }
    return NULL;
}