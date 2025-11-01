#include "bonetile.h"
#include "bones3d.h" 
#include "head_billboard.h"
#include <string.h>
#include <math.h>

static const struct {
    const char* boneName;
    const char* connections[3];
    float priority[3];
} BONE_CONNECTIONS[] = {
    {"LShoulder", {"LShoulder", "LElbow", ""}, {1.0f, 0.8f, 0.0f}},
    {"LElbow", {"LElbow", "LWrist", ""}, {1.0f, 0.8f, 0.0f}},
    {"LWrist", {"LWrist", "LElbow", ""}, {1.0f, 0.0f, 0.0f}},
    {"RShoulder", {"RShoulder", "RElbow", ""}, {1.0f, 0.8f, 0.0f}},
    {"RElbow", {"RElbow", "RWrist", ""}, {1.0f, 0.8f, 0.0f}},
    {"RWrist", {"RElbow", "RWrist", ""}, {1.0f, 0.0f, 0.0f}},
    {"LHip", {"LHip", "LKnee", ""}, {1.0f, 0.8f, 0.0f}},
    {"LKnee", {"LKnee", "LAnkle", ""}, {1.0f, 0.8f, 0.0f}},
    {"LAnkle", {"LKnee", "LAnkle", ""}, {1.0f, 0.8f, 0.0f}},
    {"RHip", {"RHip", "RKnee", ""}, {1.0f, 0.8f, 0.0f}},
    {"RKnee", {"RKnee", "RAnkle", ""}, {1.0f, 0.8f, 0.0f}},
    {"RAnkle", {"RKnee", "RAnkle", ""}, {1.0f, 0.8f, 0.0f}},
    {"Neck", {"Neck", "Head", ""},  {0.8f, 1.0f, 0.0f}},
    {"", {"", "", ""}, {0.0f, 0.0f, 0.0f}}
};

static const struct {
    const char* boneName;
    const char* primaryConnection;
    const char* secondaryConnection;
    bool reverseForward;
    bool isLimb;
    bool useStableOrientation;
} BONE_ORIENTATIONS[] = {
    {"LShoulder", "LElbow", "Neck", false, true, true},
    {"LElbow", "Neck", "LWrist", true, true, true},
    {"LWrist", "LElbow", "", false, false, true},
    {"RShoulder", "RElbow", "Neck", true, true, true},
    {"RElbow", "Neck", "RWrist", false, true, true},
    {"RWrist", "RElbow", "", false, false, true},
    {"LHip", "LKnee", "Hip", true, true, true},
    {"LKnee", "LAnkle", "LHip", true, true, true},
    {"LAnkle", "LKnee", "", true, false, true},
    {"RHip", "RKnee", "Hip", false, true, true},
    {"RKnee", "RAnkle", "RHip", true, true, true},
    {"RAnkle", "RKnee", "", false, false, true},
    {"Neck", "HEAD_CALCULATED", "", true, false, true},
    {"", "", "", false, false, false}
};

static const struct {
    const char* boneName;
    float yawOffset;
    float pitchOffset;
    float rollOffset;
} BONE_ANGLE_OFFSETS[] = {
    {"LShoulder", 90.0f, 180.0f, -90.0f},
    {"LElbow", 90.0f, 225.0f, -90.0f},
    {"LWrist", 90.0f, 180.0f, 90.0f},
    {"RShoulder", -90.0f, 0.0f, 90.0f},
    {"RElbow", -90.0f, -90.0f, 90.0f},
    {"RWrist", 90.0f, 180.0f, 90.0f},
    {"LHip", 90.0f, -45.0f, 90.0f},
    {"LKnee", 90.0f, 90.0f, 90.0f},
    {"LAnkle",  90.0f, -90.0f, 90.0f},
    {"RHip", -90.0f, -45.0f, -90.0f},
    {"RKnee", -90.0f, 90.0f, 90.0f},
    {"RAnkle", 90.0f, 180.0f, 90.0f},
    {"Neck", -90.0f, 180.0f, -90.0f},
    {"", 0.0f, 0.0f, 0.0f}
};

/*
 * +------------------------------------------------------------------+
 * | Function: GetBoneConnectionsWithPriority                         |
 * |                                                                  |
 * | Look up connection names and priorities for a given bone name.   |
 * | Copies up to three connection names into the `connections` out   |
 * | array and fills `priorities`. Returns true if an entry was found.|
 * |                                                                  |
 * | - Input: boneName, out connections[3][32], out priorities[3]     |
 * | - Output: bool (found or not)                                    |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: GetBonePositionByName                                   |
 * |                                                                  |
 * | Returns a bone position for a Person by name. Handles special    |
 * | token "HEAD_CALCULATED" which returns a computed head position.  |
 * | If no bone found or invalid, returns (0,0,0).                    |
 * |                                                                  |
 * | - Input: Person*, boneName                                        |
 * | - Output: Vector3 position                                        |
 * +------------------------------------------------------------------+
 */
Vector3 GetBonePositionByName(const Person* person, const char* boneName) {
    if (!person || !boneName) return (Vector3) { 0, 0, 0 };

    if (strcmp(boneName, "HEAD_CALCULATED") == 0) {
        return CalculateHeadPosition(person);
    }

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (bone->position.valid && strcmp(bone->name, boneName) == 0) {
            return bone->position.position;
        }
    }
    return (Vector3) { 0, 0, 0 };
}

/*
 * +------------------------------------------------------------------+
 * | Function: SafeNormalize                                          |
 * |                                                                  |
 * | Normalize a Vector3 safely: if vector length is nearly zero,     |
 * | return a reasonable default forward vector (0,0,1). Otherwise    |
 * | return normalized vector.                                        |
 * |                                                                  |
 * | - Input: Vector3 v                                               |
 * | - Output: Vector3 normalized or default                          |
 * +------------------------------------------------------------------+
 */
Vector3 SafeNormalize(Vector3 v) {
    float length = Vector3Length(v);
    if (length < 1e-6f) return (Vector3) { 0, 0, 1 };
    return Vector3Scale(v, 1.0f / length);
}

/*
 * +-------------------------------------------------------------------+
 * | Function: GetStablePerpendicularVector                            |
 * |                                                                   |
 * | Given a forward vector, pick a stable perpendicular axis from     |
 * | the world axis candidates (X,Y,Z) that has the smallest dot with  |
 * | forward, to avoid near-collinearity and produce a robust up/right |
 * | candidate.                                                        |
 * |                                                                   |
 * | - Input: forward Vector3                                          |
 * | - Output: Vector3 candidate perpendicular                         |
 * +-------------------------------------------------------------------+
 */
static Vector3 GetStablePerpendicularVector(Vector3 forward) {
    forward = SafeNormalize(forward);

    Vector3 candidates[3] = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1}
    };

    Vector3 bestCandidate = candidates[1];
    float minDot = fabs(Vector3DotProduct(forward, candidates[1]));

    for (int i = 0; i < 3; i++) {
        float dot = fabs(Vector3DotProduct(forward, candidates[i]));
        if (dot < minDot) {
            minDot = dot;
            bestCandidate = candidates[i];
        }
    }

    return bestCandidate;
}

/*
 * +-------------------------------------------------------------------+
 * | Function: RotateVectorAroundAxis                                  |
 * |                                                                   |
 * | Rotate a vector around an arbitrary axis using Rodrigues' formula |
 * | (constructed from cos/sin/oneMinusCos). If angle is nearly zero,  |
 * | returns the original vector.                                      |
 * |                                                                   |
 * | - Input: vector, axis, angle (radians)                            |
 * | - Output: rotated Vector3                                         |
 * +-------------------------------------------------------------------+
 */
static Vector3 RotateVectorAroundAxis(Vector3 vector, Vector3 axis, float angle) {
    if (fabs(angle) < 1e-6f) return vector;

    float cosAngle = cosf(angle);
    float sinAngle = sinf(angle);
    float oneMinusCos = 1.0f - cosAngle;

    axis = SafeNormalize(axis);

    Vector3 result;
    result.x = vector.x * (cosAngle + axis.x * axis.x * oneMinusCos) +
        vector.y * (axis.x * axis.y * oneMinusCos - axis.z * sinAngle) +
        vector.z * (axis.x * axis.z * oneMinusCos + axis.y * sinAngle);

    result.y = vector.x * (axis.y * axis.x * oneMinusCos + axis.z * sinAngle) +
        vector.y * (cosAngle + axis.y * axis.y * oneMinusCos) +
        vector.z * (axis.y * axis.z * oneMinusCos - axis.x * sinAngle);

    result.z = vector.x * (axis.z * axis.x * oneMinusCos - axis.y * sinAngle) +
        vector.y * (axis.z * axis.y * oneMinusCos + axis.x * sinAngle) +
        vector.z * (cosAngle + axis.z * axis.z * oneMinusCos);

    return result;
}

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateBoneOrientation                               |
 * |                                                                  |
 * | Compute orientation axes (forward/up/right) and Euler-like       |
 * | angles (yaw, pitch, roll) for a bone based on configured         |
 * | primary/secondary connections and angle offsets. Uses safe fall- |
 * | backs when data is missing.                                      |
 * |                                                                  |
 * | - Input: boneName, Person*, bonePosition                         |
 * | - Output: BoneOrientation struct (valid flag set when success)   |
 * +------------------------------------------------------------------+
 */
BoneOrientation CalculateBoneOrientation(const char* boneName, const Person* person, Vector3 bonePosition) {
    BoneOrientation orientation = { 0 };
    orientation.position = bonePosition;
    orientation.valid = false;

    if (!boneName || !person) return orientation;

    const char* primaryConn = NULL;
    const char* secondaryConn = NULL;
    bool reverseForward = false;

    for (int i = 0; BONE_ORIENTATIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_ORIENTATIONS[i].boneName, boneName) == 0) {
            primaryConn = BONE_ORIENTATIONS[i].primaryConnection;
            secondaryConn = BONE_ORIENTATIONS[i].secondaryConnection;
            reverseForward = BONE_ORIENTATIONS[i].reverseForward;
            break;
        }
    }

    Vector3 forward = { 0, 0, 1 };
    Vector3 up = { 0, 1, 0 };
    Vector3 right = { 1, 0, 0 };

    if (primaryConn && strlen(primaryConn) > 0) {
        Vector3 primaryPos = GetBonePositionByName(person, primaryConn);
        float primaryDistance = Vector3Length(Vector3Subtract(primaryPos, bonePosition));

        if (primaryDistance > 1e-4f) {
            forward = Vector3Subtract(primaryPos, bonePosition);
            if (reverseForward) {
                forward = Vector3Scale(forward, -1.0f);
            }
            forward = SafeNormalize(forward);

            if (secondaryConn && strlen(secondaryConn) > 0) {
                Vector3 secondaryPos = GetBonePositionByName(person, secondaryConn);
                float secondaryDistance = Vector3Length(Vector3Subtract(secondaryPos, bonePosition));

                if (secondaryDistance > 1e-4f) {
                    Vector3 toSecondary = Vector3Subtract(secondaryPos, bonePosition);
                    toSecondary = SafeNormalize(toSecondary);

                    right = Vector3CrossProduct(forward, toSecondary);
                    float rightLength = Vector3Length(right);

                    if (rightLength > 1e-4f) {
                        right = Vector3Scale(right, 1.0f / rightLength);
                        up = Vector3CrossProduct(right, forward);
                        up = SafeNormalize(up);
                    }
                    else {
                        Vector3 tempUp = GetStablePerpendicularVector(forward);
                        right = Vector3CrossProduct(forward, tempUp);
                        right = SafeNormalize(right);
                        up = Vector3CrossProduct(right, forward);
                        up = SafeNormalize(up);
                    }
                }
                else {
                    Vector3 tempUp = GetStablePerpendicularVector(forward);
                    right = Vector3CrossProduct(forward, tempUp);
                    right = SafeNormalize(right);
                    up = Vector3CrossProduct(right, forward);
                    up = SafeNormalize(up);
                }
            }
            else {
                Vector3 tempUp = GetStablePerpendicularVector(forward);
                right = Vector3CrossProduct(forward, tempUp);
                right = SafeNormalize(right);
                up = Vector3CrossProduct(right, forward);
                up = SafeNormalize(up);
            }
        }
    }

    for (int i = 0; BONE_ANGLE_OFFSETS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_ANGLE_OFFSETS[i].boneName, boneName) == 0) {
            float yawRad = BONE_ANGLE_OFFSETS[i].yawOffset * (PI / 180.0f);
            float pitchRad = BONE_ANGLE_OFFSETS[i].pitchOffset * (PI / 180.0f);
            float rollRad = BONE_ANGLE_OFFSETS[i].rollOffset * (PI / 180.0f);

            if (fabs(yawRad) > 1e-6f) {
                forward = RotateVectorAroundAxis(forward, up, yawRad);
                right = RotateVectorAroundAxis(right, up, yawRad);
            }

            if (fabs(pitchRad) > 1e-6f) {
                forward = RotateVectorAroundAxis(forward, right, pitchRad);
                up = RotateVectorAroundAxis(up, right, pitchRad);
            }

            if (fabs(rollRad) > 1e-6f) {
                right = RotateVectorAroundAxis(right, forward, rollRad);
                up = RotateVectorAroundAxis(up, forward, rollRad);
            }
            break;
        }
    }

    forward = SafeNormalize(forward);
    right = SafeNormalize(right);
    up = SafeNormalize(up);

    right = Vector3CrossProduct(forward, up);
    right = SafeNormalize(right);
    up = Vector3CrossProduct(right, forward);
    up = SafeNormalize(up);

    orientation.forward = forward;
    orientation.up = up;
    orientation.right = right;

    orientation.yaw = atan2f(forward.x, forward.z);
    float horizDistance = sqrtf(forward.x * forward.x + forward.z * forward.z);
    orientation.pitch = atan2f(-forward.y, fmaxf(horizDistance, 1e-6f));

    Vector3 worldUp = { 0, 1, 0 };
    Vector3 projectedRight = Vector3Subtract(right, Vector3Scale(forward, Vector3DotProduct(right, forward)));
    projectedRight = SafeNormalize(projectedRight);
    orientation.roll = atan2f(Vector3DotProduct(projectedRight, worldUp),
        Vector3DotProduct(projectedRight, Vector3CrossProduct(forward, worldUp)));

    orientation.valid = true;
    return orientation;
}

/*
 * +------------------------------------------------------------------+
 * | Function: DrawQuadTextured3D                                     |
 * |                                                                  |
 * | Low-level helper to draw a textured 3D quad with simple UVs.     |
 * | Binds texture, issues RL_QUADS with vertex positions and UVs,    |
 * | then unbinds texture.                                            |
 * |                                                                  |
 * | - Input: Texture2D tex, v0..v3 positions, u0/v0..u1/v1 UV coords |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: SrcFromLogical                                         |
 * |                                                                  |
 * | Convert logical atlas coordinates (col,row) into a source        |
 * | Rectangle in pixel space. Clamps indices and returns a Rectangle.|
 * | Optionally reports if mirroring was applied via outMirrored.     |
 * |                                                                  |
 * | - Input: tex, logicalCol,row, physCols,physRows, mirrored        |
 * | - Output: Rectangle src                                          |
 * +------------------------------------------------------------------+
 */
Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows,
    bool mirrored, bool* outMirrored) {
    if (logicalCol < 0) logicalCol = 0;
    if (logicalRow < 0) logicalRow = 0;

    if (physCols <= 0) physCols = ATLAS_COLS;
    if (physRows <= 0) physRows = ATLAS_ROWS;

    float cellW = (float)tex.width / (float)physCols;
    float cellH = (float)tex.height / (float)physRows;

    if (logicalCol >= physCols) logicalCol = physCols - 1;
    if (logicalRow >= physRows) logicalRow = physRows - 1;

    if (outMirrored) *outMirrored = mirrored;
    return (Rectangle) {
        logicalCol* cellW,
            logicalRow* cellH,
            cellW,
            cellH
    };
}

/*
 * +------------------------------------------------------------------+
 * | Function: DrawQuadTextured3D_UVs                                 |
 * |                                                                  |
 * | Variant of DrawQuadTextured3D that accepts full per-vertex UV    |
 * | coordinates (Vector2) for non-rectangular UV mapping.            |
 * |                                                                  |
 * | - Input: Texture2D, v0..v3, uv0..uv3                             |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: ShouldFlipBoneTexture                                  |
 * |                                                                  |
 * | Return true if the given bone name belongs to a set of bones     |
 * | that require vertical flip in the atlas sampling for correct     |
 * | appearance (left/right symmetry).                                |
 * |                                                                  |
 * | - Input: boneName                                                |
 * | - Output: bool (flip or not)                                     |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: IsWristBone                                            |
 * |                                                                  |
 * | Simple predicate: returns true if boneName is LWrist or RWrist.  |
 * | Used to apply wrist-specific tile/layout logic.                  |
 * +------------------------------------------------------------------+
 */
bool IsWristBone(const char* boneName) {
    if (!boneName) return false;
    return (strcmp(boneName, "LWrist") == 0) || (strcmp(boneName, "RWrist") == 0);
}

/*
 * +-------------------------------------------------------------------+
 * | Function: DrawBonetileCustom                                      |
 * |                                                                   |
 * | Draw a camera-facing textured bone quad (bonetile) at a 3D pos.   |
 * | Computes camera-facing axes (right/up), applies rotation degrees, |
 * | optional mirroring and vertex UVs, then delegates to quad drawer. |
 * |                                                                   |
 * | - Input: tex, camera, src rect, pos, size, rotationDeg, mirrored, |
 * |          boneName                                                 |
 * +-------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: ShouldUseVariableHeight                                |
 * |                                                                  |
 * | Returns true for bones that may change vertical size based on    |
 * | neighbor distance (limbs), enabling dynamic tile stretching.     |
 * +------------------------------------------------------------------+
 */
static bool ShouldUseVariableHeight(const char* boneName) {
    if (!boneName) return false;

    static const char* variableHeightBones[] = {
        "LShoulder", "LElbow", "RShoulder", "RElbow",
        "LHip", "LKnee", "RHip", "RKnee"
    };

    for (int i = 0; i < 8; i++) {
        if (strcmp(boneName, variableHeightBones[i]) == 0) return true;
    }
    return false;
}

/*
 * +------------------------------------------------------------------+
 * | Function: DrawBonetileCustomWithRoll                             |
 * |                                                                  |
 * | Draw a bonetile similar to DrawBonetileCustom but compute an     |
 * | additional roll based on neighbor direction and optionally scale |
 * | vertical size by neighbor distance. Uses UV variant drawer.      |
 * |                                                                  |
 * | - Input: tex, camera, src, pos, size, rotationDeg, mirrored,     |
 * |          neighborValid, neighborPos, boneName                    |
 * +------------------------------------------------------------------+
 */
void DrawBonetileCustomWithRoll(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, bool neighborValid, Vector3 neighborPos, const BoneRenderData* boneData,
    const Person* person) {

    Vector3 camToPos = Vector3Subtract(pos, camera.position);
    float distance = Vector3Length(camToPos);
    if (distance > 0.0f) {
        camToPos = Vector3Scale(camToPos, 1.0f / distance);
        float cameraPitchDeg = atan2f(-camToPos.y, sqrtf(camToPos.x * camToPos.x + camToPos.z * camToPos.z)) * RAD2DEG;

        if (fabs(cameraPitchDeg) > 50.0f && strcmp(boneData->boneName, "Neck") != 0) {
            Vector3 personCenter = GetBonePositionByName(person, "Neck");
            Vector3 toCenter = Vector3Normalize(Vector3Subtract(personCenter, boneData->position));
            Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
            Vector3 camUp = Vector3Normalize(Vector3CrossProduct(camRight, camForward));
            Vector3 projectedToCenter = Vector3Subtract(toCenter,
                Vector3Scale(camForward, Vector3DotProduct(toCenter, camForward)));
            float projLength = Vector3Length(projectedToCenter);
            projectedToCenter = Vector3Scale(projectedToCenter, 1.0f / projLength);
            float rightComponent = Vector3DotProduct(projectedToCenter, camRight);
            float upComponent = Vector3DotProduct(projectedToCenter, camUp);

            rotationDeg = atan2f(rightComponent, upComponent) * RAD2DEG + 180.0f;
            neighborValid = false; // Desactivar roll adicional
        }
    }

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

    Vector2 actualSize = size;
    Vector3 actualPos = pos;

    if (ShouldUseVariableHeight(boneData->boneName) && neighborValid) {
        float neighborDistance = Vector3Distance(pos, neighborPos);
        float extensionFactor = 1.5f;
        actualSize.y = neighborDistance * extensionFactor;
    }

    Vector3 halfX = Vector3Scale(newRight, actualSize.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp, actualSize.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(actualPos, halfX), halfY);
    Vector3 p1 = Vector3Add(Vector3Subtract(actualPos, halfY), halfX);
    Vector3 p2 = Vector3Add(Vector3Add(actualPos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(actualPos, halfY), halfX);

    float texW = (float)tex.width, texH = (float)tex.height;
    float u_left = src.x / texW, u_right = (src.x + src.width) / texW;
    float v_top = src.y / texH, v_bottom = (src.y + src.height) / texH;

    if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
    if (src.height < 0) { float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

    bool needsVFlip = ShouldFlipBoneTexture(boneData->boneName);
    float v0t = needsVFlip ? v_top : v_bottom;
    float v1t = needsVFlip ? v_bottom : v_top;

    if (mirrored) { float tmp = u_left; u_left = u_right; u_right = tmp; }

    Vector2 uv0 = { u_left, v0t }, uv1 = { u_right, v0t }, uv2 = { u_right, v1t }, uv3 = { u_left, v1t };
    DrawQuadTextured3D_UVs(tex, p0, p1, p2, p3, uv0, uv1, uv2, uv3);
}

/*
 * +-------------------------------------------------------------------+
 * | Function: FindRenderBoneByName                                    |
 * |                                                                   |
 * | Search a BoneRenderData array for the first matching valid and    |
 * | visible bone by name and return a pointer to it or NULL if none.  |
 * |                                                                   |
 * | - Input: bones[], count, name                                     |
 * | - Output: BoneRenderData* or NULL                                 |
 * +-----------------------------------------------------------------*-+
 */
BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int count, const char* name) {
    if (!bones || !name) return NULL;
    for (int i = 0; i < count; i++) {
        if (bones[i].valid && bones[i].visible && strcmp(bones[i].boneName, name) == 0) {
            return &bones[i];
        }
    }
    return NULL;
}

/*
 * +-------------------------------------------------------------------+
 * | Function: GetBoneConnectionPositionsEx                            |
 * |                                                                   |
 * | For a given BoneRenderData, return two positions: the bone's own  |
 * | position (pos0) and a neighbor connection position (pos1) if any. |
 * | The neighbor search consults BONE_CONNECTIONS and person data.    |
 * |                                                                   |
 * | - Input: boneData, Person*                                        |
 * | - Output: BoneConnectionPositions { pos0, pos1 }                  |
 * +-------------------------------------------------------------------+
 */
BoneConnectionPositions GetBoneConnectionPositionsEx(const BoneRenderData* boneData, const Person* person) {
    BoneConnectionPositions result = { 0 };

    if (!boneData || !boneData->valid) return result;

    result.pos0 = boneData->position;
    result.pos1 = result.pos0;

    for (int i = 0; BONE_CONNECTIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_CONNECTIONS[i].boneName, boneData->boneName) != 0) continue;

        for (int k = 1; k < 3; k++) {
            const char* neighborName = BONE_CONNECTIONS[i].connections[k];
            if (!neighborName || neighborName[0] == '\0') continue;
            if (strcmp(neighborName, boneData->boneName) == 0) continue;

            if (person) {
                Vector3 p = GetBonePositionByName(person, neighborName);
                if (Vector3Length(p) > 1e-6f) {
                    result.pos1 = p;
                    return result;
                }
            }
        }
        return result;
    }
    return result;
}

/*
 * +-------------------------------------------------------------------+
 * | Function: CalculateLimbBoneRenderData                             |
 * |                                                                   |
 * | Decide which atlas index/row to use for a limb bone based on      |
 * | camera direction relative to bone orientation. Computes sector,   |
 * | pitch rows and optional rotation compensation for diagonal limbs. |
 * | Fills outChosenIndex, outRotation and outMirrored accordingly.    |
 * |                                                                   |
 * | - Input: boneData, person, camera                                 |
 * | - Output: outChosenIndex, outRotation, outMirrored                |
 * +-------------------------------------------------------------------+
 */
void CalculateLimbBoneRenderData(const BoneRenderData* boneData, const Person* person, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!boneData->orientation.valid) {
        CalculateHandBoneRenderData(boneData->position, camera, outChosenIndex, outRotation, outMirrored, boneData->boneName);
        return;
    }

    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},
        {2,12,13,14,15,14,13,12},
        {1,8,9,10,11,10,9,8}
    };

    Vector3 camDir = Vector3Subtract(boneData->position, camera.position);
    camDir = SafeNormalize(camDir);

    Vector3 localCamDir = {
        Vector3DotProduct(camDir, boneData->orientation.right),
        Vector3DotProduct(camDir, boneData->orientation.up),
        Vector3DotProduct(camDir, boneData->orientation.forward)
    };

    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;

    float localPitchDeg = atan2f(localCamDir.y,
        sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z)) * RAD2DEG;

    bool shouldInvertPitch = false;
    if (boneData->boneName[0] != '\0') {
        if (strcmp(boneData->boneName, "Neck") != 0 &&
            strcmp(boneData->boneName, "RShoulder") != 0 &&
            strcmp(boneData->boneName, "RHip") != 0 && 
            strcmp(boneData->boneName, "RElbow") != 0 &&
            strcmp(boneData->boneName, "RKnee") != 0 &&
            strcmp(boneData->boneName, "LAnkle") != 0) {
            shouldInvertPitch = true;
        }
    }
    if (shouldInvertPitch) {
        localPitchDeg = -localPitchDeg;
    }

    if (person) {
        BoneConnectionPositions p = GetBoneConnectionPositionsEx(boneData, person);

        Vector3 camF = Vector3Subtract(camera.target, camera.position);
        camF = SafeNormalize(camF);

        Vector3 camR = Vector3CrossProduct(camF, camera.up);
        camR = SafeNormalize(camR);

        Vector3 camU = Vector3CrossProduct(camR, camF);
        camU = SafeNormalize(camU);

        Vector3 boneVec = Vector3Subtract(p.pos1, p.pos0);
        float boneLen = Vector3Length(boneVec);
        if (boneLen > 1e-6f) {
            boneVec = Vector3Scale(boneVec, 1.0f / boneLen);

            Vector3 mid = {
                (p.pos0.x + p.pos1.x) * 0.5f,
                (p.pos0.y + p.pos1.y) * 0.5f,
                (p.pos0.z + p.pos1.z) * 0.5f
            };

            Vector3 toCam = Vector3Subtract(camera.position, mid);
            float toCamLen = Vector3Length(toCam);
            if (toCamLen > 1e-6f) {
                toCam = Vector3Scale(toCam, 1.0f / toCamLen);

                float d = Vector3DotProduct(boneVec, toCam);
                if (d > 1.0f) d = 1.0f;
                if (d < -1.0f) d = -1.0f;
                float angleBetweenDeg = acosf(d) * RAD2DEG;

                Vector3 boneInCam = {
                    Vector3DotProduct(boneVec, camR),
                    Vector3DotProduct(boneVec, camU),
                    Vector3DotProduct(boneVec, camF)
                };

                float horizLen = sqrtf(boneInCam.x * boneInCam.x + boneInCam.z * boneInCam.z);
                float bonePitchInCameraDeg = atan2f(boneInCam.y, horizLen) * RAD2DEG;

                float compensationFactor = 0.0f;

                if (boneData->boneName[0] != '\0') {
                    if (strstr(boneData->boneName, "Hip") != NULL) {
                        compensationFactor = 0.25f;
                    }
                    else if (strstr(boneData->boneName, "Knee") != NULL) {
                        compensationFactor = 0.25f;
                    }
                    else if (strstr(boneData->boneName, "Shoulder") != NULL) {
                        compensationFactor = 0.15f;
                    }
                    else if (strstr(boneData->boneName, "Elbow") != NULL) {
                        compensationFactor = 0.15f;
                    }
                }

                if (compensationFactor > 0.0f && angleBetweenDeg > 45.0f) {
                    float angleIntensity = (angleBetweenDeg - 45.0f) / 45.0f;
                    angleIntensity = fminf(angleIntensity, 1.0f);

                    float compensation = bonePitchInCameraDeg * compensationFactor * angleIntensity;
                    float maxCompensation = 12.0f;
                    compensation = fmaxf(fminf(compensation, maxCompensation), -maxCompensation);
                    localPitchDeg += compensation;
                }
            }
        }
    }

    float normalizedYaw = localYaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    int sector = (int)(normalizedYaw / 45.0f) % 8;

    float rotationCompensation = 0.0f;

    if (localPitchDeg >= 50.5f) {
        *outChosenIndex = 3;
        //*outRotation = sector * 45.0f + 180.0f; rotation on DrawBonetileCustomWithRoll
        *outMirrored = false;
        return;
    }
    else if (localPitchDeg <= -60.0f) {
        *outChosenIndex = 15;
        *outRotation = (8 - sector) * 45.0f + 180.0f;
        *outMirrored = true;
        return;
    }
    else {
        int row = (localPitchDeg >= 22.5f) ? 2 : (localPitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[row][sector];
        *outRotation = 0.0f;
        *outMirrored = (sector >= 1 && sector <= 4);

        // Ajuste espec fico para Right
        if (boneData->boneName[0] == 'R') {
            *outMirrored = !(*outMirrored);
        }

        if (person) {
            BoneConnectionPositions p = GetBoneConnectionPositionsEx(boneData, person);
            Vector3 boneVec = Vector3Subtract(p.pos1, p.pos0);
            float boneLen = Vector3Length(boneVec);

            if (boneLen > 1e-6f) {
                boneVec = SafeNormalize(boneVec);

                Vector3 verticalUp = { 0, 1, 0 };
                float dotWithVertical = Vector3DotProduct(boneVec, verticalUp);
                float angleFromVerticalDeg = acosf(fabs(dotWithVertical)) * RAD2DEG;

                float diagonalThreshold = 30.0f;

                if (angleFromVerticalDeg > diagonalThreshold) {
                    Vector3 camToPos = Vector3Subtract(boneData->position, camera.position);
                    camToPos = SafeNormalize(camToPos);

                    Vector3 projectedBone = Vector3Subtract(boneVec,
                        Vector3Scale(camToPos, Vector3DotProduct(boneVec, camToPos)));
                    float projLen = Vector3Length(projectedBone);

                    if (projLen > 1e-4f) {
                        projectedBone = SafeNormalize(projectedBone);

                        Vector3 camRight = Vector3Normalize(Vector3CrossProduct(
                            Vector3Subtract(camera.target, camera.position), camera.up));
                        Vector3 camUp = Vector3Normalize(Vector3CrossProduct(camRight,
                            Vector3Subtract(camera.target, camera.position)));

                        float rightDot = Vector3DotProduct(projectedBone, camRight);
                        float upDot = Vector3DotProduct(projectedBone, camUp);

                        rotationCompensation = atan2f(rightDot, upDot) * RAD2DEG;

                        float diagonalIntensity = (angleFromVerticalDeg - diagonalThreshold) / (90.0f - diagonalThreshold);
                        diagonalIntensity = fminf(diagonalIntensity, 1.0f);

                        float rotationFactor = 0.0f;
                        if (boneData->boneName[0] != '\0') {
                            if (strstr(boneData->boneName, "LHip") != NULL) {
                                rotationFactor = -0.6f;
                            }
                            else if (strstr(boneData->boneName, "RHip") != NULL) {
                                rotationFactor = -0.6f;
                            }
                            else if (strstr(boneData->boneName, "Knee") != NULL) {
                                rotationFactor = 0.5f;
                            }
                            else if (strstr(boneData->boneName, "Shoulder") != NULL) {
                                rotationFactor = 0.3f;
                            }
                            else if (strstr(boneData->boneName, "Elbow") != NULL) {
                                rotationFactor = 0.3f;
                            }
                        }

                        rotationCompensation *= rotationFactor * diagonalIntensity;

                        float maxRotationCompensation = 45.0f;
                        rotationCompensation = fmaxf(fminf(rotationCompensation, maxRotationCompensation),
                            -maxRotationCompensation);
                    }
                }
            }
        }
    }

    // Special neck rotation toward head 
    if (strcmp(boneData->boneName, "Neck") == 0 && person) {
        Vector3 headPos = CalculateHeadPosition(person);
        Vector3 neckToHead = Vector3Subtract(headPos, boneData->position);
        if (Vector3Length(neckToHead) > 0.001f) {
            Vector3 camRight = Vector3Normalize(Vector3CrossProduct(
                Vector3Subtract(camera.target, camera.position), camera.up));
            Vector3 camUp = Vector3Normalize(Vector3CrossProduct(camRight,
                Vector3Subtract(camera.target, camera.position)));

            neckToHead = SafeNormalize(neckToHead);
            rotationCompensation += atan2f(Vector3DotProduct(neckToHead, camRight),
                Vector3DotProduct(neckToHead, camUp)) * RAD2DEG;
        }
    }

        // Stabilization for front/back views (similar to torso stabilization)
    if ((sector == 0 || sector == 4) &&
        localPitchDeg < 60.0f && localPitchDeg > -60.0f) {

        // For front and back views, minimize rotation compensation to reduce jitter
        rotationCompensation *= 0.01f;  // Reduce compensation by 90%

        // For certain limbs, completely disable rotation in front/back view
        if (boneData->boneName[0] != '\0') {
            if (strcmp(boneData->boneName, "LShoulder") == 0 ||
                strcmp(boneData->boneName, "RShoulder") == 0 ||
                strcmp(boneData->boneName, "LElbow") == 0 ||
                strcmp(boneData->boneName, "RElbow") == 0 ||
                strcmp(boneData->boneName, "LHip") == 0 ||
                strcmp(boneData->boneName, "RHip") == 0 ||
                strcmp(boneData->boneName, "LKnee") == 0 ||
                strcmp(boneData->boneName, "RKnee") == 0) {
                rotationCompensation = 0.0f;  // No rotation for major joints in front/back
            }
        }
    }

    // Additional stabilization for near-frontal views (sectors 7, 0, 1 and 3, 4, 5)
    if (((sector >= 7 || sector <= 1) || (sector >= 3 && sector <= 5)) &&
        localPitchDeg < 45.0f && localPitchDeg > -45.0f) {

        // Reduce rotation jitter for near-frontal views
        rotationCompensation *= 0.3f;
    }

    *outRotation += rotationCompensation;

    while (*outRotation >= 360.0f) *outRotation -= 360.0f;
    while (*outRotation < 0.0f) *outRotation += 360.0f;
}

/*
 * +-------------------------------------------------------------------+
 * | Function: CalculateHandBoneRenderData                             |
 * |                                                                   |
 * | Specialized picker for hand/wrist sprites: compute camera vector  |
 * | with optional per-bone yaw/pitch offsets, select hand tile index  |
 * | from a 3x8 table based on yaw sector and pitch row. Applies some  |
 * | wrist-specific clamping and mirroring rules.                      |
 * |                                                                   |
 * | - Input: bonePos, camera, outChosenIndex, outRotation, outMirror, |
 * |          boneName                                                 |
 * +-------------------------------------------------------------------+
 */
void CalculateHandBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored, const char* boneName) {

    static const int handIndices[3][8] = {
        {23, 22, 2, 15, 16, 17, 18, 24},
        {9, 4, 0, 5, 6, 7, 8, 14},
        {20, 19, 1, 10, 11, 12, 13, 21}
    };

    Vector3 camDir = Vector3Subtract(camera.position, bonePos);
    camDir = SafeNormalize(camDir);

    bool isWrist = (boneName != NULL) && IsWristBone(boneName);

    float yawOffsetRad = 0.0f, pitchOffsetRad = 0.0f;
    if (boneName) {
        for (int i = 0; BONE_ANGLE_OFFSETS[i].boneName[0] != '\0'; i++) {
            if (strcmp(BONE_ANGLE_OFFSETS[i].boneName, boneName) == 0) {
                yawOffsetRad = BONE_ANGLE_OFFSETS[i].yawOffset * (PI / 180.0f);
                pitchOffsetRad = BONE_ANGLE_OFFSETS[i].pitchOffset * (PI / 180.0f);
                break;
            }
        }
    }

    if (fabs(yawOffsetRad) > 1e-6f) {
        float cosYaw = cosf(yawOffsetRad), sinYaw = sinf(yawOffsetRad);
        float newX = camDir.x * cosYaw - camDir.z * sinYaw;
        float newZ = camDir.x * sinYaw + camDir.z * cosYaw;
        camDir.x = newX; camDir.z = newZ;
    }

    if (fabs(pitchOffsetRad) > 1e-6f) {
        float horizDist = sqrtf(camDir.x * camDir.x + camDir.z * camDir.z);
        if (horizDist > 1e-6f) {
            float cosP = cosf(pitchOffsetRad), sinP = sinf(pitchOffsetRad);
            float newY = camDir.y * cosP - horizDist * sinP;
            float newHoriz = camDir.y * sinP + horizDist * cosP;
            float scale = newHoriz / horizDist;
            camDir.x *= scale; camDir.z *= scale; camDir.y = newY;
        }
    }

    if (!isWrist) {
        float tmp = camDir.x; camDir.x = camDir.z; camDir.z = tmp;
    }

    float yaw = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;
    float pitchDeg = atan2f(camDir.y, sqrtf(camDir.x * camDir.x + camDir.z * camDir.z)) * RAD2DEG;

    float normalizedYaw = yaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;
    int sector = (int)(normalizedYaw / 45.0f) % 8;

    if (isWrist) {
        int pitchRow;
        if (pitchDeg >= 22.5f) {
            pitchRow = 0;
        }
        else if (pitchDeg >= -22.5f) {
            pitchRow = 1;
        }
        else {
            pitchRow = 2;
        }

        if (pitchRow < 0) pitchRow = 0;
        if (pitchRow > 2) pitchRow = 2;
        if (sector < 0) sector = 0;
        if (sector > 7) sector = 7;

        *outChosenIndex = handIndices[pitchRow][sector];
        *outRotation = 0.0f;
        *outMirrored = false;

        if (*outChosenIndex < 0) *outChosenIndex = 0;
        if (*outChosenIndex > 24) *outChosenIndex = *outChosenIndex % 25;

        //return;
    }

    if (pitchDeg >= 52.5f) {
        *outChosenIndex = 22;
        *outRotation = sector * 45.0f + 180.0f;
        *outMirrored = false;
    }
    else if (pitchDeg <= -51.0f) {
        *outChosenIndex = 3;
        //*outRotation = (8 - sector) * 45.0f + 180.0f;
        //if (*outRotation >= 360.0f) *outRotation -= 360.0f;
        //*outMirrored = true;
    }
}
