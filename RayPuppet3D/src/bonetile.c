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
    const char* secondaryConnection; // Bone para calcular el up vector
    bool reverseForward;             // Si hay que invertir la dirección forward
    bool isLimb;                     // Si es una extremidad (brazo/pierna)
    bool useStableOrientation;       // Usar orientación estable para evitar oscilaciones
} BONE_ORIENTATIONS[] = {
    // Brazos 
    {"LShoulder", "LElbow", "Neck", false, true, true},
    {"LElbow", "Neck", "LWrist", true, true, true},
    {"LWrist", "LElbow", "", true, false, true},

    {"RShoulder", "RElbow", "Neck", false, true, true},
    {"RElbow", "Neck", "RWrist", true, true, true},
    {"RWrist", "RElbow", "", true, false, true},

    // Piernas 
    {"LHip", "LKnee", "Hip", true, true, true},
    {"LKnee", "LAnkle", "LHip", true, true, true},
    {"LAnkle", "LKnee", "", true, false, true},

    {"RHip", "RKnee", "Hip", false, true, true},
    {"RKnee", "RAnkle", "RHip", true, true, true},
    {"RAnkle", "RKnee", "", true, false, true},

    // Cuello 
    {"Neck", "HEAD_CALCULATED", "", true, false, true},

    {"", "", "", false, false, false}
};

// Offsets simplificados y más estables
static const struct {
    const char* boneName;
    float yawOffset;    // ROTACIÓN HORIZONTAL (grados)
    float pitchOffset;  // ROTACIÓN VERTICAL (grados) 
    float rollOffset;   // ROTACIÓN DE GIRO (grados)
} BONE_ANGLE_OFFSETS[] = {
    // BRAZOS
    {"LShoulder", 90.0f, 180.0f, -70.0f},
    {"LElbow", 90.0f, 180.0f, -70.0f},
    {"LWrist", 90.0f, 0.0f, 70.0f}, 

    {"RShoulder", -90.0f, 180.0f, 70.0f},
    {"RElbow", -90.0f, 180.0f, 70.0f},
    {"RWrist", 90.0f, 0.0f, 70.0f},

    // PIERNAS
    {"LHip", 90.0f, -45.0f, 90.0f},
    {"LKnee", 90.0f, 135.0f, 90.0f},  
    {"LAnkle",  90.0f, -90.0f, 110.0f},

    {"RHip", -90.0f, -90.0f, 90.0f},
    {"RKnee", 90.0f, 135.0f, 90.0f},
    {"RAnkle", 90.0f, 0.0f, 110.0f},
    
    {"Neck", -90.0f, 180.0f, -90.0f},
    {"", 0.0f, 0.0f, 0.0f}
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

// Función auxiliar mejorada para obtener posición de un bone por nombre
static Vector3 GetBonePositionByName(const Person* person, const char* boneName) {
    if (!person || !boneName) return (Vector3) { 0, 0, 0 };

    // Casos especiales para bones calculados - USAR FUNCIÓN EXISTENTE
    if (strcmp(boneName, "HEAD_CALCULATED") == 0) {
        return CalculateHeadPosition(person); // ¡Usar la función que ya existe!
    }

    // Buscar bone normal
    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (bone->position.valid && strcmp(bone->name, boneName) == 0) {
            return bone->position.position;
        }
    }
    return (Vector3) { 0, 0, 0 };
}

// Función auxiliar mejorada para normalizar un vector
static Vector3 SafeNormalize(Vector3 v) {
    float length = Vector3Length(v);
    if (length < 1e-6f) return (Vector3) { 0, 0, 1 }; // Vector forward por defecto
    return Vector3Scale(v, 1.0f / length);
}

// Función auxiliar mejorada para crear un vector perpendicular estable
static Vector3 GetStablePerpendicularVector(Vector3 forward) {
    forward = SafeNormalize(forward);

    // Encontrar el eje más perpendicular al forward
    Vector3 candidates[3] = {
        {1, 0, 0}, // X axis
        {0, 1, 0}, // Y axis (up)
        {0, 0, 1}  // Z axis
    };

    Vector3 bestCandidate = candidates[1]; // Default to Y up
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

// Función auxiliar para rotar un vector alrededor de un eje (mejorada)
static Vector3 RotateVectorAroundAxis(Vector3 vector, Vector3 axis, float angle) {
    if (fabs(angle) < 1e-6f) return vector; // No rotation needed

    float cosAngle = cosf(angle);
    float sinAngle = sinf(angle);
    float oneMinusCos = 1.0f - cosAngle;

    // Normalizar el eje
    axis = SafeNormalize(axis);

    // Fórmula de rotación de Rodrigues
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

// Función COMPLETAMENTE REESCRITA para calcular orientación de bones
BoneOrientation CalculateBoneOrientation(const char* boneName, const Person* person, Vector3 bonePosition) {
    BoneOrientation orientation = { 0 };
    orientation.position = bonePosition;
    orientation.valid = false;

    if (!boneName || !person) return orientation;

    // Buscar la configuración de orientación para este bone
    const char* primaryConn = NULL;
    const char* secondaryConn = NULL;
    bool reverseForward = false;

    for (int i = 0; BONE_ORIENTATIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_ORIENTATIONS[i].boneName, boneName) == 0) {
            primaryConn = BONE_ORIENTATIONS[i].primaryConnection;
            secondaryConn = BONE_ORIENTATIONS[i].secondaryConnection;
            reverseForward = BONE_ORIENTATIONS[i].reverseForward;
            // isLimb y useStable están definidos en la estructura pero no se usan en esta implementación
            break;
        }
    }

    Vector3 forward = { 0, 0, 1 }; // Forward por defecto
    Vector3 up = { 0, 1, 0 };      // Up por defecto
    Vector3 right = { 1, 0, 0 };   // Right por defecto

    if (primaryConn && strlen(primaryConn) > 0) {
        // Obtener posición del bone primario de conexión
        Vector3 primaryPos = GetBonePositionByName(person, primaryConn);
        float primaryDistance = Vector3Length(Vector3Subtract(primaryPos, bonePosition));

        if (primaryDistance > 1e-4f) { // Threshold más alto para estabilidad
            // Calcular vector forward
            forward = Vector3Subtract(primaryPos, bonePosition);
            if (reverseForward) {
                forward = Vector3Scale(forward, -1.0f);
            }
            forward = SafeNormalize(forward);

            // Calcular vector up basado en conexión secundaria o usando método estable
            if (secondaryConn && strlen(secondaryConn) > 0) {
                Vector3 secondaryPos = GetBonePositionByName(person, secondaryConn);
                float secondaryDistance = Vector3Length(Vector3Subtract(secondaryPos, bonePosition));

                if (secondaryDistance > 1e-4f) {
                    Vector3 toSecondary = Vector3Subtract(secondaryPos, bonePosition);
                    toSecondary = SafeNormalize(toSecondary);

                    // Crear un vector right perpendicular
                    right = Vector3CrossProduct(forward, toSecondary);
                    float rightLength = Vector3Length(right);

                    if (rightLength > 1e-4f) {
                        right = Vector3Scale(right, 1.0f / rightLength);
                        up = Vector3CrossProduct(right, forward);
                        up = SafeNormalize(up);
                    }
                    else {
                        // Si los vectores son paralelos, usar método alternativo
                        Vector3 tempUp = GetStablePerpendicularVector(forward);
                        right = Vector3CrossProduct(forward, tempUp);
                        right = SafeNormalize(right);
                        up = Vector3CrossProduct(right, forward);
                        up = SafeNormalize(up);
                    }
                }
                else {
                    // Usar método estable si no hay conexión secundaria válida
                    Vector3 tempUp = GetStablePerpendicularVector(forward);
                    right = Vector3CrossProduct(forward, tempUp);
                    right = SafeNormalize(right);
                    up = Vector3CrossProduct(right, forward);
                    up = SafeNormalize(up);
                }
            }
            else {
                // Sin conexión secundaria, usar método estable
                Vector3 tempUp = GetStablePerpendicularVector(forward);
                right = Vector3CrossProduct(forward, tempUp);
                right = SafeNormalize(right);
                up = Vector3CrossProduct(right, forward);
                up = SafeNormalize(up);
            }
        }
        else {
            // Distancia muy pequeña, usar orientación por defecto
            forward = (Vector3){ 0, 0, 1 };
            up = (Vector3){ 0, 1, 0 };
            right = (Vector3){ 1, 0, 0 };
        }
    }

    // Aplicar offsets de ángulos de manera más controlada
    for (int i = 0; BONE_ANGLE_OFFSETS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_ANGLE_OFFSETS[i].boneName, boneName) == 0) {
            // Convertir offsets de grados a radianes
            float yawRad = BONE_ANGLE_OFFSETS[i].yawOffset * (PI / 180.0f);
            float pitchRad = BONE_ANGLE_OFFSETS[i].pitchOffset * (PI / 180.0f);
            float rollRad = BONE_ANGLE_OFFSETS[i].rollOffset * (PI / 180.0f);

            // Aplicar rotaciones de manera más estable
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

    // Renormalizar para asegurar ortogonalidad perfecta
    forward = SafeNormalize(forward);
    right = SafeNormalize(right);
    up = SafeNormalize(up);

    // Re-ortogonalizar usando Gram-Schmidt
    right = Vector3CrossProduct(forward, up);
    right = SafeNormalize(right);
    up = Vector3CrossProduct(right, forward);
    up = SafeNormalize(up);

    // Asignar vectores finales
    orientation.forward = forward;
    orientation.up = up;
    orientation.right = right;

    // Calcular ángulos finales de manera más estable
    orientation.yaw = atan2f(forward.x, forward.z);
    float horizDistance = sqrtf(forward.x * forward.x + forward.z * forward.z);
    orientation.pitch = atan2f(-forward.y, fmaxf(horizDistance, 1e-6f)); // Evitar división por cero

    // Roll más estable
    Vector3 worldUp = { 0, 1, 0 };
    Vector3 projectedRight = Vector3Subtract(right, Vector3Scale(forward, Vector3DotProduct(right, forward)));
    projectedRight = SafeNormalize(projectedRight);
    orientation.roll = atan2f(Vector3DotProduct(projectedRight, worldUp),
        Vector3DotProduct(projectedRight, Vector3CrossProduct(forward, worldUp)));

    orientation.valid = true;
    return orientation;
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

// Nueva función para renderizar un bone con orientación completa
void DrawBoneWithOrientation(Texture2D texture, Camera camera, const BoneRenderData* boneData, int physCols, int physRows) {
    if (!boneData || !boneData->valid || !boneData->visible) return;

    int chosenIndex;
    float rotation;
    bool mirrored;

    if (boneData->orientation.valid) {
        CalculateEnhancedBoneRenderData(boneData, camera, &chosenIndex, &rotation, &mirrored);
    }
    else {
        CalculateBoneRenderData(boneData->position, camera, &chosenIndex, &rotation, &mirrored, boneData->boneName);
    }

    int logicalCol = chosenIndex % ATLAS_COLS;
    int logicalRow = chosenIndex / ATLAS_COLS;

    bool finalMirror;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows, mirrored, &finalMirror);
    Vector2 worldSize = { boneData->size, boneData->size };

    DrawBonetileCustom(texture, camera, src, boneData->position, worldSize, rotation, finalMirror, boneData->boneName);
}

void CalculateEnhancedBoneRenderData(const BoneRenderData* boneData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!boneData->orientation.valid) {
        CalculateBoneRenderData(boneData->position, camera, outChosenIndex, outRotation, outMirrored, boneData->boneName);
        return;
    }

    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},{2,12,13,14,15,14,13,12},{1,8,9,10,11,10,9,8}
    };

    // Calcular dirección INVERTIDA de la cámara en espacio local del bone
    Vector3 camDir = Vector3Subtract(boneData->position, camera.position);
    camDir = SafeNormalize(camDir);

    // Transformar a coordenadas locales del bone
    Vector3 localCamDir = {
        Vector3DotProduct(camDir, boneData->orientation.right),
        Vector3DotProduct(camDir, boneData->orientation.up),
        Vector3DotProduct(camDir, boneData->orientation.forward)
    };

    // Calcular ángulos en el espacio local
    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;

    float localPitchDeg = atan2f(localCamDir.y,
        sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z)) * RAD2DEG;

    // INVERSIÓN ESPECÍFICA DE PITCH para ciertos bones
    bool shouldInvertPitch = false;
    if (boneData->boneName[0] != '\0') {
        // Bones que necesitan inversión de pitch: todos EXCEPTO cuello, manos y pies
        if (strcmp(boneData->boneName, "Neck") != 0 &&
            strcmp(boneData->boneName, "LWrist") != 0 &&
            strcmp(boneData->boneName, "RWrist") != 0 &&
            strcmp(boneData->boneName, "LAnkle") != 0 &&
            strcmp(boneData->boneName, "RAnkle") != 0) {
            shouldInvertPitch = true;
        }
    }

    if (shouldInvertPitch) {
        localPitchDeg = -localPitchDeg;  // Invertir solo el pitch
    }

    // Normalizar yaw para el sistema de sectores
    float normalizedYaw = localYaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    int sector = (int)(normalizedYaw / 45.0f) % 8;

    // Seleccionar sprite basado en el ángulo de pitch
    if (localPitchDeg >= 70.0f) {
        *outChosenIndex = 3;  // Vista desde arriba
        *outRotation = sector * 45.0f;
        *outMirrored = false;
    }
    else if (localPitchDeg <= -70.0f) {
        *outChosenIndex = 15; // Vista desde abajo
        *outRotation = sector * 45.0f;
        *outMirrored = true;
    }
    else {
        // Vista lateral/frontal
        int row = (localPitchDeg >= 22.5f) ? 2 : (localPitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[row][sector];
        *outRotation = 0.0f;
        *outMirrored = (sector >= 1 && sector <= 4);
    }
}

void CalculateBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored, const char* boneName) {
    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},{2,12,13,14,15,14,13,12},{1,8,9,10,11,10,9,8}
    };

    // Dirección de la cámara (NORMAL)
    Vector3 camDir = Vector3Subtract(camera.position, bonePos);
    camDir = SafeNormalize(camDir);

    // Buscar offsets específicos para este bone
    float yawOffsetRad = 0.0f;
    float pitchOffsetRad = 0.0f;

    if (boneName) {
        for (int i = 0; BONE_ANGLE_OFFSETS[i].boneName[0] != '\0'; i++) {
            if (strcmp(BONE_ANGLE_OFFSETS[i].boneName, boneName) == 0) {
                yawOffsetRad = BONE_ANGLE_OFFSETS[i].yawOffset * (PI / 180.0f);
                pitchOffsetRad = BONE_ANGLE_OFFSETS[i].pitchOffset * (PI / 180.0f);
                break;
            }
        }
    }

    // Aplicar offset de yaw
    if (fabs(yawOffsetRad) > 1e-6f) {
        float cosYaw = cosf(yawOffsetRad);
        float sinYaw = sinf(yawOffsetRad);
        float newX = camDir.x * cosYaw - camDir.z * sinYaw;
        float newZ = camDir.x * sinYaw + camDir.z * cosYaw;
        camDir.x = newX;
        camDir.z = newZ;
    }

    // Aplicar offset de pitch
    if (fabs(pitchOffsetRad) > 1e-6f) {
        float horizDist = sqrtf(camDir.x * camDir.x + camDir.z * camDir.z);
        if (horizDist > 1e-6f) {
            float cosPitch = cosf(pitchOffsetRad);
            float sinPitch = sinf(pitchOffsetRad);
            float newY = camDir.y * cosPitch - horizDist * sinPitch;
            float newHorizDist = camDir.y * sinPitch + horizDist * cosPitch;

            float scale = newHorizDist / horizDist;
            camDir.x *= scale;
            camDir.z *= scale;
            camDir.y = newY;
        }
    }

    // SWAP de coordenadas X y Z para intercambiar los giros de cámara
    float tempX = camDir.x;
    camDir.x = camDir.z;  // X toma el valor de Z
    camDir.z = tempX;     // Z toma el valor de X
    // Y se mantiene igual (subir/bajar no se afecta)

    // Recalcular ángulos con el swap aplicado
    float yaw = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;

    float pitchDeg = atan2f(camDir.y, sqrtf(camDir.x * camDir.x + camDir.z * camDir.z)) * RAD2DEG;

    float normalizedYaw = yaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    int sector = (int)(normalizedYaw / 45.0f) % 8;

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
        *outMirrored = (sector >= 1 && sector <= 4);
    }
}