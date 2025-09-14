
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
    {"LWrist", "LElbow", "", false, false, true},

    {"RShoulder", "RElbow", "Neck", true, true, true},
    {"RElbow", "Neck", "RWrist", true, true, true},
    {"RWrist", "RElbow", "", false, false, true},

    // Piernas 
    {"LHip", "LKnee", "Hip", true, true, true},
    {"LKnee", "LAnkle", "LHip", true, true, true},
    {"LAnkle", "LKnee", "", false, false, true},

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
    /*{"LShoulder", 90.0f, 180.0f, -70.0f},
    {"LElbow", 90.0f, 180.0f, -70.0f},
    {"LWrist", 90.0f, 0.0f, 70.0f},

    {"RShoulder", -90.0f, 180.0f, -70.0f},
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
    {"", 0.0f, 0.0f, 0.0f}*/
    {"LShoulder", 85.0f, 175.0f, -75.0f},   // -5°, -5°, -5°
    {"LElbow", 85.0f, 175.0f, -65.0f},      // -5°, -5°, +5°
    {"LWrist", 90.0f, 180.0f, 90.0f},         // +5°, +5°, +5°

    {"RShoulder", -85.0f, 175.0f, -75.0f},  // +5°, -5°, -5°
    {"RElbow", -85.0f, 175.0f, 75.0f},      // +5°, -5°, +5°
    {"RWrist", 90.0f, 180.0f, 90.0f},         // +5°, +5°, +5°

    // PIERNAS - Ajustes menores
    {"LHip", 85.0f, -40.0f, 85.0f},         // -5°, +5°, -5°
    {"LKnee", 85.0f, 130.0f, 85.0f},        // -5°, -5°, -5°
    {"LAnkle", -85.0f, -85.0f, 105.0f},      // -5°, +5°, -5°

    {"RHip", -85.0f, -85.0f, 85.0f},        // +5°, +5°, -5°
    {"RKnee", 85.0f, 130.0f, 85.0f},        // -5°, -5°, -5°
    {"RAnkle", 85.0f, -5.0f, 105.0f},       // -5°, -5°, -5°

    {"Neck", -85.0f, 175.0f, -85.0f},       // +5°, -5°, +5°
    {"", 0.0f, 0.0f, 0.0f}
};

/*
BONE_ANGLE_OFFSETS - Corrección de orientación para texturas 2D de huesos

Problema: Las texturas circulares de huesos se ven mal orientadas sin corrección
Solución: 3 ángulos por hueso (Yaw, Pitch, Roll)

YAW (±85°): Rotación horizontal - hacia dónde "apunta" el hueso
- Izquierdo: +85° (gira izquierda), Derecho: -85° (gira derecha)

PITCH: Rotación vertical - hacia dónde "mira" el hueso
- 175°: huesos que miran "hacia abajo" (hombros, codos)
- 5°: huesos que miran "hacia adelante" (muñecas)
- 130°: valor intermedio (piernas)

ROLL (±65°-110°): Giro para simular curvatura 3D natural

Ejemplo: LShoulder (85°, 175°, -75°)
= Gira izquierda + mira hacia abajo + inclinación curva

Resultado: Texturas 2D que se ven como huesos 3D reales desde cualquier ángulo
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

// Nueva función para calcular orientación de bones
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





Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows,
    bool mirrored, bool* outMirrored) {

    // Clamp lógico
    if (logicalCol < 0) logicalCol = 0;
    if (logicalRow < 0) logicalRow = 0;

    // Evitar división por cero
    if (physCols <= 0) physCols = ATLAS_COLS;
    if (physRows <= 0) physRows = ATLAS_ROWS;

    // Usar physCols/physRows directamente como la grilla física
    float cellW = (float)tex.width / (float)physCols;
    float cellH = (float)tex.height / (float)physRows;

    // Si logicalCol/Row exceden, clamped al rango
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


static bool IsWristBone(const char* boneName) {
    if (!boneName) return false;
    return (strcmp(boneName, "LWrist") == 0) || (strcmp(boneName, "RWrist") == 0);
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

// Función auxiliar para determinar si es un bone que debe tener altura variable
static bool ShouldUseVariableHeight(const char* boneName) {
    if (!boneName) return false;

    // Solo brazos y piernas (sin manos/pies)
    static const char* variableHeightBones[] = {
        "LShoulder", "LElbow", "RShoulder", "RElbow",
        "LHip", "LKnee", "RHip", "RKnee"
    };

    for (int i = 0; i < 8; i++) {
        if (strcmp(boneName, variableHeightBones[i]) == 0) return true;
    }
    return false;
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

    // MODIFICACIÓN PARA ALTURA VARIABLE
    Vector2 actualSize = size;
    Vector3 actualPos = pos; // Mantener posición original del bone

    if (ShouldUseVariableHeight(boneName) && neighborValid) {
        // Calcular la distancia real al vecino
        float neighborDistance = Vector3Distance(pos, neighborPos);

        // Añadir un factor de extensión para que se superpongan los bones
        // Esto asegura que lleguen hasta los bordes y se toquen
        float extensionFactor = 1.5f; // 20% más largo para superposición
        actualSize.y = neighborDistance * extensionFactor;

        // NO centrar - mantener el bone en su posición original
        // El bone se alargará desde su posición hacia el vecino (y más allá)
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


void CalculateEnhancedBoneRenderData(const BoneRenderData* boneData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!boneData->orientation.valid) {
        CalculateBoneRenderData(boneData->position, camera, outChosenIndex, outRotation, outMirrored, boneData->boneName);
        return;
    }

    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},
        {2,12,13,14,15,14,13,12},
        {1,8,9,10,11,10,9,8}
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

    // MAPEO PARA MANOS 5x5 - TU ARRAY PERSONALIZADO CON 3 FILAS
    static const int handIndices[3][8] = {
        {23, 22, 2, 15, 16, 17, 18, 24},      // Fila 0: Vista inferior
        {9, 4, 0, 5, 6, 7, 8, 14}, // Fila 1: Vista frontal/lateral  
        {20, 19, 1, 10, 11, 12, 13, 21}     // Fila 2: Vista  superior
    };

    // Dirección de la cámara (desde el bone hacia la cámara)
    Vector3 camDir = Vector3Subtract(camera.position, bonePos);
    camDir = SafeNormalize(camDir);

    // Detectar muñeca
    bool isWrist = (boneName != NULL) && IsWristBone(boneName);

    // Aplicar offsets si existen
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

    // Aplicar offsets
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

    // Solo intercambiar X/Z para bones que NO son muñeca
    if (!isWrist) {
        float tmp = camDir.x; camDir.x = camDir.z; camDir.z = tmp;
    }

    // Calcular ángulos
    float yaw = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;
    float pitchDeg = atan2f(camDir.y, sqrtf(camDir.x * camDir.x + camDir.z * camDir.z)) * RAD2DEG;

    // Calcular sector (NECESARIO TAMBIÉN PARA VISTAS EXTREMAS)
    float normalizedYaw = yaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;
    int sector = (int)(normalizedYaw / 45.0f) % 8;

    if (isWrist) {
        // Sistema para muñecas: 3 rangos de pitch
        int pitchRow;

        if (pitchDeg >= 22.5f) {
            pitchRow = 0;        // Vista superior
        }
        else if (pitchDeg >= -22.5f) {
            pitchRow = 1;        // Vista frontal/lateral
        }
        else {
            pitchRow = 2;        // Vista inferior
        }

        // Validación de seguridad
        if (pitchRow < 0) pitchRow = 0;
        if (pitchRow > 2) pitchRow = 2;
        if (sector < 0) sector = 0;
        if (sector > 7) sector = 7;

        *outChosenIndex = handIndices[pitchRow][sector];
        *outRotation = 0.0f;
        *outMirrored = false;

        // Validar rango
        if (*outChosenIndex < 0) *outChosenIndex = 0;
        if (*outChosenIndex > 24) *outChosenIndex = *outChosenIndex % 25;

        return;
    }

    // Para otros bones: lógica estándar CON ROTACIÓN CADA 45 GRADOS
    else {
        const float TOP_THRESHOLD = 70.0f;
        const float BOTTOM_THRESHOLD = -70.0f;

        if (pitchDeg >= TOP_THRESHOLD) {
            // VISTA DESDE ARRIBA - IGUAL QUE CABEZA Y TORSO
            *outChosenIndex = 3;
            *outRotation = sector * 45.0f;  // ¡AHORA ROTA CADA 45 GRADOS!
            *outMirrored = false;
        }
        else if (pitchDeg <= BOTTOM_THRESHOLD) {
            // VISTA DESDE ABAJO - IGUAL QUE CABEZA Y TORSO  
            *outChosenIndex = 22;
            *outRotation = (8 - sector) * 45.0f;  // ¡AHORA ROTA CADA 45 GRADOS!
            if (*outRotation >= 360.0f) *outRotation -= 360.0f;
            *outMirrored = true;
        }
        else {
            // VISTA LATERAL/FRONTAL - Sin cambios
            static const int indices[3][8] = {
                {0,4,5,6,7,6,5,4},
                {2,12,13,14,15,14,13,12},
                {1,8,9,10,11,10,9,8}
            };

            int row = (pitchDeg >= 22.5f) ? 2 : (pitchDeg >= -22.5f) ? 0 : 1;
            *outChosenIndex = indices[row][sector];
            *outRotation = 0.0f;
            *outMirrored = (sector >= 1 && sector <= 4);
        }
    }
}
