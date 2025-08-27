// bonetile.c
#include "bonetile.h"
#include "bones3d.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef MAX_FILE_PATH_LENGTH
#define MAX_FILE_PATH_LENGTH 260
#endif
#ifndef ATLAS_COLS
#define ATLAS_COLS 8
#endif
#ifndef ATLAS_ROWS
#define ATLAS_ROWS 5
#endif



// Activa las correcciones por hueso
#define BONETILE_HAS_BONE_CORRECTIONS 1

// -----------------------
// Per-bone correction helper (MODIFICADO)
// -----------------------
typedef struct {
    const char* name;    // nombre del hueso (ej: "Neck")
    int index;           // índice del hueso (usa -1 si no lo usas)
    bool swapXZ;         // intercambiar X <-> Z antes de atan2
    bool invertX;        // invertir X (multiplicar por -1)
    bool invertZ;        // invertir Z
    float yawOffsetDeg;  // offset de yaw en grados (se suma a localYawDeg)
    bool overrideBind;   // si debe ignorar el bind yaw calculado

    // Nuevos campos para rotación y pivote de textura:
    float pivotX;        // pivote X normalizado [0..1], 0=izq, 0.5=centro, 1=dcha
    float pivotY;        // pivote Y normalizado [0..1], 0=arriba, 0.5=centro, 1=abajo
    float textureRotationDeg; // rotación adicional (grados) aplicada a la textura
} BoneCorrection;


// Ajusta aquí las correcciones por hueso.
static BoneCorrection g_boneCorrections[] = {
    // Nombre    index  swapXZ invertX invertZ yawOffsetDeg overrideBind pivotX pivotY textureRotationDeg
    // invertir frente/espalda: yawOffsetDeg += 180, forzar recálculo del bind
    // 270 + 180 = 450 -> 450 % 360 = 90
    // Intento 2: invertir X y Z y además invertir frente/espalda
    // Intento 3: swap X<->Z y flip front/back
    // Frontal (primera prueba)
    // Frontal, corregir verticalmente (si queda invertida)
    // Frontal con swapXZ (si el eje del modelo está rotado 90°)
    { "Neck", -1, true, false, false,  -90.0f, true, 0.5f, 0.32f, 90.0f },

    { NULL, -1, false, false, false, 0.0f, false, 0.5f, 0.5f, 0.0f } // terminador (pivote centro por defecto)
};


// -----------------------
// Tabla de bind yaw (calculados la primera vez que se ve cada texturePath)
// -----------------------
#define MAX_BIND_ENTRIES 512

typedef struct {
    char texturePath[MAX_FILE_PATH_LENGTH];
    float bindYawDeg;
    bool used;
} BindEntry;

static BindEntry g_bindTable[MAX_BIND_ENTRIES] = { 0 };

// Buscar entrada de bind por texturePath
static BindEntry* FindBindEntryByTexture(const char* texPath) {
    if (!texPath || texPath[0] == '\0') return NULL;
    for (int i = 0; i < MAX_BIND_ENTRIES; ++i) {
        if (g_bindTable[i].used && strcmp(g_bindTable[i].texturePath, texPath) == 0) {
            return &g_bindTable[i];
        }
    }
    return NULL;
}

// Crear nueva entrada de bind (devuelve NULL si la tabla está llena)
static BindEntry* CreateBindEntry(const char* texPath, float bindYawDeg) {
    if (!texPath) return NULL;
    for (int i = 0; i < MAX_BIND_ENTRIES; ++i) {
        if (!g_bindTable[i].used) {
            g_bindTable[i].used = true;
            strncpy(g_bindTable[i].texturePath, texPath, MAX_FILE_PATH_LENGTH - 1);
            g_bindTable[i].texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
            g_bindTable[i].bindYawDeg = bindYawDeg;
            return &g_bindTable[i];
        }
    }
    return NULL; // tabla llena
}

// Resetear bind yaw para una textura concreta (forzar recalculo la próxima vez que se dibuje)
void ResetBindYawForTexture(const char* texturePath) {
    if (!texturePath) return;

    // Buscar y eliminar entrada existente
    for (int i = 0; i < MAX_BIND_ENTRIES; ++i) {
        if (g_bindTable[i].used && strcmp(g_bindTable[i].texturePath, texturePath) == 0) {
            g_bindTable[i].used = false;
            break;
        }
    }
}

// -----------------------
// Reemplaza la función GetBoneCorrection por esta versión compatible
// -----------------------
static BoneCorrection* GetBoneCorrection(const BoneRenderData* boneData) {
    if (!boneData) return NULL;

    // Primero intentar coincidir con el nombre del hueso directamente
    if (boneData->boneName[0] != '\0') {
        for (int i = 0; i < (int)(sizeof(g_boneCorrections) / sizeof(g_boneCorrections[0])); ++i) {
            BoneCorrection* c = &g_boneCorrections[i];
            if (!c->name) continue;
            if (c->name[0] == '\0') continue;
            if (strcmp(c->name, boneData->boneName) == 0) {
                return c;
            }
        }
    }

    // Si no hay coincidencia por nombre, intentar con la textura
    const char* path = boneData->texturePath;
    if (!path || path[0] == '\0') return NULL;

    // obtener basename (sin directorio)
    const char* base = strrchr(path, '/');
    if (base) base++; else base = path;

    // copiar a buffer y quitar extensión
    char namebuf[64];
    strncpy(namebuf, base, sizeof(namebuf) - 1);
    namebuf[sizeof(namebuf) - 1] = '\0';

    char* dot = strrchr(namebuf, '.');
    if (dot) *dot = '\0'; // elimina la extensión ".png", ".jpg", etc.

    // buscar coincidencia por nombre en la tabla de correcciones
    for (int i = 0; i < (int)(sizeof(g_boneCorrections) / sizeof(g_boneCorrections[0])); ++i) {
        BoneCorrection* c = &g_boneCorrections[i];
        if (!c->name) continue;
        if (c->name[0] == '\0') continue;
        if (strcmp(c->name, namebuf) == 0) return c;
    }

    // si no hay coincidencia por nombre, intentar con path completo
    for (int i = 0; i < (int)(sizeof(g_boneCorrections) / sizeof(g_boneCorrections[0])); ++i) {
        BoneCorrection* c = &g_boneCorrections[i];
        if (!c->name) continue;
        if (strcmp(c->name, path) == 0) return c;
    }

    return NULL;
}

static void ApplyBoneCorrection(Vector3* localCamDir, const BoneRenderData* boneData) {
    BoneCorrection* cfg = GetBoneCorrection(boneData);
    if (!cfg) return;

    if (cfg->swapXZ) {
        float t = localCamDir->x;
        localCamDir->x = localCamDir->z;
        localCamDir->z = t;
    }
    if (cfg->invertX) localCamDir->x = -localCamDir->x;
    if (cfg->invertZ) localCamDir->z = -localCamDir->z;
}

// -----------------------
// Cálculo del bind yaw inicial (para que el sprite "mire" a la cámara en el instante bind)
// -----------------------
static float ComputeBindYawDeg(const BoneRenderData* boneData, Camera camera) {
    if (!boneData || !boneData->orientation.valid) return 0.0f;

    Vector3 camDir = Vector3Subtract(camera.position, boneData->position);

    Vector3 localCamDir;
    localCamDir.x = Vector3DotProduct(camDir, boneData->orientation.right);
    localCamDir.y = Vector3DotProduct(camDir, boneData->orientation.up);
    localCamDir.z = Vector3DotProduct(camDir, boneData->orientation.forward);

#ifdef BONETILE_HAS_BONE_CORRECTIONS
    // aplicar las correcciones por hueso (swap/invert) también para el cálculo inicial
    ApplyBoneCorrection(&localCamDir, boneData);
#endif

    /* usar la misma convención que torso: atan2f(-localCamDir.x, localCamDir.z) */
    float localYaw = atan2f(-localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localYawDeg = localYaw * RAD2DEG;

    // Queremos que la localYaw sea cero en el instante "bind", por tanto
    // bindYaw = -localYawDeg (se suma más tarde).
    return -localYawDeg;
}

// -----------------------
// GetBoneYawOffsetDeg ahora combina la corrección manual + el bind guardado (si existe)
// -----------------------
static float GetBoneYawOffsetDeg(const BoneRenderData* boneData) {
    float cfgOffset = 0.0f;
    bool overrideBind = false;

    BoneCorrection* cfg = GetBoneCorrection(boneData);
    if (cfg) {
        cfgOffset = cfg->yawOffsetDeg;
        overrideBind = cfg->overrideBind;
    }

    // Si overrideBind está activado, ignoramos el bind yaw calculado
    if (overrideBind) {
        return cfgOffset;
    }

    // luego añadir bind si existe (busca por texturePath)
    BindEntry* be = FindBindEntryByTexture(boneData->texturePath);
    if (be) {
        return cfgOffset + be->bindYawDeg;
    }

    return cfgOffset;
}

// -----------------------
// Bone connections data
// -----------------------
static const struct {
    const char* boneName;
    const char* connections[3];
    float priority[3];
} BONE_CONNECTIONS[] = {
    {"LShoulder", {"Neck", "LElbow", ""}, {1.0f, 0.8f, 0.0f}},
    {"LElbow", {"LShoulder", "LWrist", ""}, {0.8f, 1.0f, 0.0f}},
    {"LWrist", {"LElbow", "", ""}, {1.0f, 0.0f, 0.0f}},
    {"RShoulder", {"Neck", "RElbow", ""}, {1.0f, 0.8f, 0.0f}},
    {"RElbow", {"RShoulder", "RWrist", ""}, {0.8f, 1.0f, 0.0f}},
    {"RWrist", {"RElbow", "", ""}, {1.0f, 0.0f, 0.0f}},
    {"LHip", {"Neck", "LKnee", ""}, {1.0f, 0.8f, 0.0f}},
    {"LKnee", {"LHip", "LAnkle", ""}, {0.8f, 1.0f, 0.0f}},
    {"LAnkle", {"LKnee", "", ""}, {1.0f, 0.0f, 0.0f}},
    {"RHip", {"Neck", "RKnee", ""}, {1.0f, 0.8f, 0.0f}},
    {"RKnee", {"RHip", "RAnkle", ""}, {0.8f, 1.0f, 0.0f}},
    {"RAnkle", {"RKnee", "", ""}, {1.0f, 0.0f, 0.0f}},
    {"Neck", {"LShoulder", "RShoulder", "LHip"}, {0.9f, 0.9f, 0.6f}},
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

bool GetBoneConnections(const char* boneName, char connections[3][32]) {
    float priorities[3];
    return GetBoneConnectionsWithPriority(boneName, connections, priorities);
}

Vector3 GetConnectedBonePosition(const char* boneName, const struct Person* person) {
    const Person* p = (const Person*)person;

    if (!p || !boneName || strlen(boneName) == 0) {
        return (Vector3) { 0, 0, 0 };
    }

    for (int i = 0; i < p->boneCount; i++) {
        const Bone* bone = &p->bones[i];
        if (strcmp(bone->name, boneName) == 0 && bone->position.valid) {
            return bone->position.position;
        }
    }

    return (Vector3) { 0, 0, 0 };
}

// Modificación para CalculateEnhancedBoneOrientation en bonetile.c
// Añadir esta lógica especial para el Neck

BoneOrientation CalculateEnhancedBoneOrientation(const char* boneName, const struct Person* person) {
    const Person* p = (const Person*)person;
    BoneOrientation orientation = { 0 };
    orientation.valid = false;

    if (!p || !boneName) return orientation;

    Vector3 bonePos = GetConnectedBonePosition(boneName, person);
    if (Vector3Length(bonePos) < 1e-6f) return orientation;

    orientation.position = bonePos;

    // CASO ESPECIAL: Si es "Neck", orientarlo hacia la cabeza
    if (strcmp(boneName, "Neck") == 0) {
        // Buscar puntos de la cabeza para calcular la orientación
        Vector3 nose = { 0 }, lEye = { 0 }, rEye = { 0 }, lEar = { 0 }, rEar = { 0 };
        bool hasNose = false, hasLEye = false, hasREye = false, hasLEar = false, hasREar = false;

        for (int i = 0; i < p->boneCount; i++) {
            const Bone* bone = &p->bones[i];
            if (!bone->position.valid) continue;

            if (strcmp(bone->name, "Nose") == 0) {
                nose = bone->position.position; hasNose = true;
            }
            else if (strcmp(bone->name, "LEye") == 0) {
                lEye = bone->position.position; hasLEye = true;
            }
            else if (strcmp(bone->name, "REye") == 0) {
                rEye = bone->position.position; hasREye = true;
            }
            else if (strcmp(bone->name, "LEar") == 0) {
                lEar = bone->position.position; hasLEar = true;
            }
            else if (strcmp(bone->name, "REar") == 0) {
                rEar = bone->position.position; hasREar = true;
            }
        }

        if (hasNose) {
            // Calcular forward vector hacia la nariz
            Vector3 forward = Vector3Subtract(nose, bonePos);
            if (Vector3Length(forward) > 1e-6f) {
                forward = Vector3Normalize(forward);

                // Calcular right vector usando los ojos o las orejas (CORREGIDO)
                Vector3 right = { 1, 0, 0 };
                if (hasLEye && hasREye) {
                    // Cambio principal: invertimos el cálculo del vector right
                    right = Vector3Normalize(Vector3Subtract(lEye, rEye));
                }
                else if (hasLEar && hasREar) {
                    // También invertimos para las orejas
                    right = Vector3Normalize(Vector3Subtract(lEar, rEar));
                }

                // Calcular up vector
                Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
                if (Vector3Length(up) < 1e-6f) {
                    up = (Vector3){ 0, 1, 0 };
                }

                // Re-ortogonalizar right
                right = Vector3Normalize(Vector3CrossProduct(forward, up));

                orientation.forward = forward;
                orientation.up = up;
                orientation.right = right;

                // Calcular ángulos de Euler
                orientation.yaw = atan2f(forward.x, forward.z);
                orientation.pitch = atan2f(-forward.y, sqrtf(forward.x * forward.x + forward.z * forward.z));
                orientation.roll = atan2f(up.x, sqrtf(up.y * up.y + up.z * up.z));

                orientation.valid = true;
                return orientation;
            }
        }

        // Si no hay datos de cabeza suficientes, usar orientación por defecto hacia arriba
        orientation.forward = (Vector3){ 0, 1, 0 };  // Apuntar hacia arriba
        orientation.up = (Vector3){ 0, 0, -1 };      // "arriba" del neck hacia atrás
        orientation.right = (Vector3){ 1, 0, 0 };    // derecha normal
        orientation.valid = true;
        return orientation;
    }

    // Para otros huesos, usar la lógica original de conexiones
    char connections[3][32];
    float priorities[3];
    if (!GetBoneConnectionsWithPriority(boneName, connections, priorities)) {
        orientation.forward = (Vector3){ 0, 0, 1 };
        orientation.up = (Vector3){ 0, 1, 0 };
        orientation.right = (Vector3){ 1, 0, 0 };
        orientation.valid = true;
        return orientation;
    }

    Vector3 connectedPositions[3];
    float connectionWeights[3];
    int validConnections = 0;

    for (int i = 0; i < 3; i++) {
        if (strlen(connections[i]) > 0 && priorities[i] > 0.0f) {
            Vector3 connPos = GetConnectedBonePosition(connections[i], person);
            if (Vector3Length(connPos) > 1e-6f) {
                connectedPositions[validConnections] = connPos;
                connectionWeights[validConnections] = priorities[i];
                validConnections++;
            }
        }
    }

    if (validConnections == 0) {
        orientation.forward = (Vector3){ 0, 0, 1 };
        orientation.up = (Vector3){ 0, 1, 0 };
        orientation.right = (Vector3){ 1, 0, 0 };
        orientation.valid = true;
        return orientation;
    }

    Vector3 forward = { 0, 0, 1 };
    Vector3 up = { 0, 1, 0 };
    Vector3 right = { 1, 0, 0 };

    if (validConnections >= 1) {
        Vector3 dir = Vector3Subtract(connectedPositions[0], bonePos);
        if (Vector3Length(dir) > 1e-6f) {
            forward = Vector3Normalize(dir);
        }
    }

    if (validConnections >= 2) {
        Vector3 dir2 = Vector3Subtract(connectedPositions[1], bonePos);
        if (Vector3Length(dir2) > 1e-6f) {
            dir2 = Vector3Normalize(dir2);
            Vector3 cross = Vector3CrossProduct(forward, dir2);
            if (Vector3Length(cross) > 1e-6f) {
                right = Vector3Normalize(cross);
            }
        }
    }

    if (validConnections >= 3) {
        Vector3 dir3 = Vector3Subtract(connectedPositions[2], bonePos);
        if (Vector3Length(dir3) > 1e-6f) {
            dir3 = Vector3Normalize(dir3);
            Vector3 avgForward = Vector3Add(
                Vector3Scale(forward, connectionWeights[0]),
                Vector3Scale(dir3, connectionWeights[2] * 0.3f)
            );
            if (Vector3Length(avgForward) > 1e-6f) {
                forward = Vector3Normalize(avgForward);
            }
        }
    }

    Vector3 upCalc = Vector3CrossProduct(right, forward);
    if (Vector3Length(upCalc) > 1e-6f) {
        up = Vector3Normalize(upCalc);
    }

    right = Vector3Normalize(Vector3CrossProduct(forward, up));
    up = Vector3Normalize(Vector3CrossProduct(right, forward));

    orientation.forward = forward;
    orientation.up = up;
    orientation.right = right;

    orientation.yaw = atan2f(forward.x, forward.z);
    orientation.pitch = atan2f(-forward.y, sqrtf(forward.x * forward.x + forward.z * forward.z));
    orientation.roll = atan2f(up.x, sqrtf(up.y * up.y + up.z * up.z));

    orientation.valid = true;
    return orientation;
}

BoneOrientation CalculateBoneOrientation(const char* boneName, const struct Person* person) {
    return CalculateEnhancedBoneOrientation(boneName, person);
}

// Reemplaza la versión actual en bonetile.c
void CalculateEnhancedBoneRenderData(const BoneRenderData* boneData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!boneData->orientation.valid) {
        CalculateBoneRenderData(boneData->position, camera, outChosenIndex, outRotation, outMirrored);
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

    /* Transform camera direction to bone's local space */
    Vector3 localCamDir;
    localCamDir.x = Vector3DotProduct(camDir, boneData->orientation.right);
    localCamDir.y = Vector3DotProduct(camDir, boneData->orientation.up);
    localCamDir.z = Vector3DotProduct(camDir, boneData->orientation.forward);

#ifdef BONETILE_HAS_BONE_CORRECTIONS
    ApplyBoneCorrection(&localCamDir, boneData);
#endif

    /* usar la misma convención que torso: atan2f(-localCamDir.x, localCamDir.z) */
    float localYaw = atan2f(-localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localYawDeg = localYaw * RAD2DEG;

#ifdef BONETILE_HAS_BONE_CORRECTIONS
    float yawOffsetDeg = GetBoneYawOffsetDeg(boneData);
#else
    float yawOffsetDeg = 0.0f;
#endif

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
    /* añadir +22.5 (centrado en sector) +180 para alinear 'frente' con forward + posibles offsets por hueso */
    float normalizedYaw = localYawDeg + 22.5f + 180.0f + yawOffsetDeg;
    /* asegurar rango [0,360) */
    while (normalizedYaw < 0.0f) normalizedYaw += 360.0f;
    while (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

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

void CalculateBoneRenderDataWithOrientation(const BoneRenderData* boneData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {
    CalculateEnhancedBoneRenderData(boneData, camera, outChosenIndex, outRotation, outMirrored);
}

BoneRenderData CreateBoneRenderData(const char* boneName, const Person* person,
    const BoneConfig* config) {
    BoneRenderData boneData = { 0 };

    boneData.position = GetConnectedBonePosition(boneName, person);
    boneData.orientation = CalculateEnhancedBoneOrientation(boneName, person);

    boneData.valid = (Vector3Length(boneData.position) > 1e-6f) || boneData.orientation.valid;

    if (config) {
        strncpy(boneData.texturePath, config->texturePath, MAX_FILE_PATH_LENGTH - 1);
        boneData.size = config->size;
        boneData.visible = config->visible;
    }
    else {
        snprintf(boneData.texturePath, MAX_FILE_PATH_LENGTH, "tex/%s.png", boneName);
        boneData.size = 0.2f;
        boneData.visible = true;
    }
    boneData.texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

    return boneData;
}

void DrawBoneWithOrientation(Texture2D texture, Camera camera, const char* boneName,
    const Person* person, const BoneConfig* config,
    int physCols, int physRows) {

    BoneRenderData boneData = CreateBoneRenderData(boneName, person, config);

    if (!boneData.valid || !boneData.visible) return;

    // Buscar corrección para este hueso
    BoneCorrection* correction = GetBoneCorrection(&boneData);

    // Registrar/recalcular bind yaw (igual que antes)
    if (boneData.texturePath[0] != '\0') {
        BindEntry* be = FindBindEntryByTexture(boneData.texturePath);

        if (correction && correction->overrideBind && be) {
            ResetBindYawForTexture(boneData.texturePath);
            be = NULL;
        }

        if (!be) {
            float bindYaw = ComputeBindYawDeg(&boneData, camera);

            if (correction) {
                bindYaw += correction->yawOffsetDeg;
            }

            CreateBindEntry(boneData.texturePath, bindYaw);
        }
    }

    int chosenIndex;
    float rotation;
    bool mirrored;

    CalculateEnhancedBoneRenderData(&boneData, camera, &chosenIndex, &rotation, &mirrored);

    int logicalCol = chosenIndex % ATLAS_COLS;
    int logicalRow = chosenIndex / ATLAS_COLS;

    bool finalMirror = false;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows,
        mirrored, &finalMirror);

    Vector2 worldSize = (Vector2){ boneData.size, boneData.size };

    // --- APLICAR ROTACION EXTRA Y PIVOTE SI HAY CORRECCION ---
    float finalRotation = rotation;
    float pivotX = 0.5f, pivotY = 0.5f;

    if (correction) {
        finalRotation += correction->textureRotationDeg;
        pivotX = correction->pivotX;
        pivotY = correction->pivotY;
    }

    // Si el sprite se va a espejar horizontalmente, invertir pivotX para que el pivot visual coincida
    if (finalMirror) pivotX = 1.0f - pivotX;

    // calcular vectores de cámara (misma lógica que en DrawBonetileCustom)
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    // rotación aplicada al sistema de ejes del sprite
    float a = finalRotation * (PI / 180.0f);
    float ca = cosf(a);
    float sa = sinf(a);

    Vector3 newRight = Vector3Subtract(Vector3Scale(right, ca), Vector3Scale(up, sa));
    Vector3 newUp = Vector3Add(Vector3Scale(right, sa), Vector3Scale(up, ca));

    // Para que el punto (pivotX,pivotY) del sprite quede en 'boneData.position'
    // necesitamos desplazar el centro del sprite en (0.5 - pivot) * size, transformado a world space
    Vector3 pivotOffset = Vector3Add(
        Vector3Scale(newRight, (0.5f - pivotX) * worldSize.x),
        Vector3Scale(newUp, (0.5f - pivotY) * worldSize.y)
    );

    Vector3 drawPos = Vector3Add(boneData.position, pivotOffset);

    // finalmente dibujar usando la posición ajustada y la rotación final
    DrawBonetileCustom(texture, camera, src, drawPos, worldSize, finalRotation, finalMirror);
}


void CalculateEnhancedBoneMorphData(const BoneRenderData* boneData, Camera camera,
    BoneMorphData* outMorphData) {
    if (!boneData->orientation.valid) {
        CalculateBoneMorphData(boneData->position, camera, outMorphData);
        return;
    }

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

    const float MORPH_RANGE = 8.0f;
    const float transitionAngles[8] = { 22.5f, 67.5f, 112.5f, 157.5f, 202.5f, 247.5f, 292.5f, 337.5f };

    Vector3 camDir = Vector3Subtract(camera.position, boneData->position);

    Vector3 localCamDir;
    localCamDir.x = Vector3DotProduct(camDir, boneData->orientation.right);
    localCamDir.y = Vector3DotProduct(camDir, boneData->orientation.up);
    localCamDir.z = Vector3DotProduct(camDir, boneData->orientation.forward);

#ifdef BONETILE_HAS_BONE_CORRECTIONS
    ApplyBoneCorrection(&localCamDir, boneData);
#endif

    /* usar la misma convención que torso: atan2f(-localCamDir.x, localCamDir.z) */
    float localYaw = atan2f(-localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localYawDeg = localYaw * RAD2DEG;

#ifdef BONETILE_HAS_BONE_CORRECTIONS
    float yawOffsetDeg = GetBoneYawOffsetDeg(boneData);
#else
    float yawOffsetDeg = 0.0f;
#endif

    // aplicar offset al yaw para todas las comparaciones
    localYawDeg += yawOffsetDeg;
    while (localYawDeg < 0.0f) localYawDeg += 360.0f;
    while (localYawDeg >= 360.0f) localYawDeg -= 360.0f;

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
    float normalizedYaw = localYawDeg + 22.5f + 180.0f; // +180 igual que render
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    if (normalizedYaw < 45.0f) sector = 0;
    else if (normalizedYaw < 90.0f) sector = 1;
    else if (normalizedYaw < 135.0f) sector = 2;
    else if (normalizedYaw < 180.0f) sector = 3;
    else if (normalizedYaw < 225.0f) sector = 4;
    else if (normalizedYaw < 270.0f) sector = 5;
    else if (normalizedYaw < 315.0f) sector = 6;
    else sector = 7;

    int secondarySector = sector;
    float blendFactor = 0.0f;
    bool inTransitionRange = false;

    if (!useTopdown) {
        for (int i = 0; i < 8; i++) {
            float transitionAngle = transitionAngles[i];
            float angleDiff = fabsf(localYawDeg - transitionAngle);

            if (angleDiff > 180.0f) angleDiff = 360.0f - angleDiff;

            if (angleDiff <= MORPH_RANGE) {
                inTransitionRange = true;

                int sector1 = i;
                int sector2 = (i + 1) % 8;

                float dist1 = fabsf(localYawDeg - sectorAngles[sector1]);
                if (dist1 > 180.0f) dist1 = 360.0f - dist1;

                float dist2 = fabsf(localYawDeg - sectorAngles[sector2]);
                if (dist2 > 180.0f) dist2 = 360.0f - dist2;

                if (dist1 < dist2) {
                    sector = sector1;
                    secondarySector = sector2;
                }
                else {
                    sector = sector2;
                    secondarySector = sector1;
                }

                float normalizedDiff = angleDiff / MORPH_RANGE;
                blendFactor = 2.0f - (normalizedDiff * normalizedDiff);
                blendFactor = Clamp(blendFactor, 0.95f, 1.0f);
                break;
            }
        }
    }

    if (useTopdown) {
        if (isTopView) {
            outMorphData->primaryIndex = topdownIndex;
            outMorphData->secondaryIndex = topdownIndex;
            outMorphData->blendFactor = 0.0f;
            outMorphData->rotation = sector * 45.0f + 180.0f;
            outMorphData->mirrored = false;
        }
        else {
            outMorphData->primaryIndex = bottomIndex;
            outMorphData->secondaryIndex = bottomIndex;
            outMorphData->blendFactor = 0.0f;
            outMorphData->rotation = (8 - sector) * 45.0f + 180.0f;
            if (outMorphData->rotation >= 360.0f) outMorphData->rotation -= 360.0f;
            outMorphData->mirrored = true;
        }
    }
    else {
        outMorphData->primaryIndex = indices[chosenRow][sector];
        outMorphData->secondaryIndex = indices[chosenRow][secondarySector];
        outMorphData->blendFactor = inTransitionRange ? blendFactor : 0.0f;
        outMorphData->rotation = 0.0f;
        outMorphData->mirrored = !(sector >= 5 && sector <= 7);

        if (outMorphData->primaryIndex == outMorphData->secondaryIndex) {
            outMorphData->blendFactor = 0.0f;
        }
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

void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored) {
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

    float v0t = v_bottom;
    float v1t = v_top;

    if (mirrored) {
        float tmp = u_left; u_left = u_right; u_right = tmp;
    }

    DrawQuadTextured3D(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t);
}

Rectangle GetAtlasCellSrcPos(Texture2D tex, int col, int rowIndex, bool mirrored, bool* outMirrored) {
    int atlasRow = 5 - 1 - rowIndex;
    float cellW = (float)tex.width / ATLAS_COLS;
    float cellH = (float)tex.height / ATLAS_ROWS;
    float srcX = col * cellW;
    float srcY = atlasRow * cellH;

    if (outMirrored) *outMirrored = mirrored;

    return (Rectangle) { srcX, srcY, cellW, cellH };
}

void CalculateBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored) {
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

static void DrawQuadTextured3DWithAlpha(Texture2D tex, Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
    float u0, float v0t, float u1, float v1t, float alpha) {
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);

    alpha = fmaxf(alpha, 0.3f);
    alpha = fminf(alpha, 1.0f);

    unsigned char alphaValue = (unsigned char)(alpha * 255.0f);
    rlColor4ub(255, 255, 255, alphaValue);
    rlTexCoord2f(u0, v0t); rlVertex3f(v0.x, v0.y, v0.z);
    rlTexCoord2f(u1, v0t); rlVertex3f(v1.x, v1.y, v1.z);
    rlTexCoord2f(u1, v1t); rlVertex3f(v2.x, v2.y, v2.z);
    rlTexCoord2f(u0, v1t); rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
    rlSetTexture(0);
}

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

    const float MORPH_RANGE = 8.0f;
    const float transitionAngles[8] = { 22.5f, 67.5f, 112.5f, 157.5f, 202.5f, 247.5f, 292.5f, 337.5f };

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

    int secondarySector = sector;
    float blendFactor = 0.0f;
    bool inTransitionRange = false;

    if (!useTopdown) {
        for (int i = 0; i < 8; i++) {
            float transitionAngle = transitionAngles[i];
            float angleDiff = fabsf(yawDeg - transitionAngle);

            if (angleDiff > 180.0f) angleDiff = 360.0f - angleDiff;

            if (angleDiff <= MORPH_RANGE) {
                inTransitionRange = true;

                int sector1 = i;
                int sector2 = (i + 1) % 8;

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

                float normalizedDiff = angleDiff / MORPH_RANGE;
                blendFactor = 2.0f - (normalizedDiff * normalizedDiff);
                blendFactor = Clamp(blendFactor, 0.95f, 1.0f);
                break;
            }
        }
    }

    if (useTopdown) {
        if (isTopView) {
            outMorphData->primaryIndex = topdownIndex;
            outMorphData->secondaryIndex = topdownIndex;
            outMorphData->blendFactor = 0.0f;
            outMorphData->rotation = sector * 45.0f + 180.0f;
            outMorphData->mirrored = false;
        }
        else {
            outMorphData->primaryIndex = bottomIndex;
            outMorphData->secondaryIndex = bottomIndex;
            outMorphData->blendFactor = 0.0f;
            outMorphData->rotation = (8 - sector) * 45.0f + 180.0f;
            if (outMorphData->rotation >= 360.0f) outMorphData->rotation -= 360.0f;
            outMorphData->mirrored = true;
        }
    }
    else {
        outMorphData->primaryIndex = indices[chosenRow][sector];
        outMorphData->secondaryIndex = indices[chosenRow][secondarySector];
        outMorphData->blendFactor = inTransitionRange ? blendFactor : 0.0f;
        outMorphData->rotation = 0.0f;
        outMorphData->mirrored = !(sector >= 5 && sector <= 7);

        if (outMorphData->primaryIndex == outMorphData->secondaryIndex) {
            outMorphData->blendFactor = 0.0f;
        }
    }
}

void DrawBonetileWithMorphing(Texture2D tex, Camera camera, BoneMorphData morphData,
    Vector3 pos, Vector2 size, int physCols, int physRows) {

    if (morphData.blendFactor <= 0.05f) {
        int logicalCol = morphData.primaryIndex % ATLAS_COLS;
        int logicalRow = morphData.primaryIndex / ATLAS_COLS;

        Rectangle src = SrcFromLogical(tex, logicalCol, logicalRow, physCols, physRows,
            morphData.mirrored, NULL);
        DrawBonetileCustom(tex, camera, src, pos, size, morphData.rotation, morphData.mirrored);
        return;
    }

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

    rlSetBlendMode(BLEND_ALPHA);
    rlEnableColorBlend();

    {
        int logicalCol = morphData.primaryIndex % ATLAS_COLS;
        int logicalRow = morphData.primaryIndex / ATLAS_COLS;
        Rectangle src = SrcFromLogical(tex, logicalCol, logicalRow, physCols, physRows,
            morphData.mirrored, NULL);

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

        float v0t = v_bottom;
        float v1t = v_top;

        if (morphData.mirrored) {
            float tmp = u_left; u_left = u_right; u_right = tmp;
        }

        float primaryAlpha = 1.0f - (morphData.blendFactor * 0.7f);
        DrawQuadTextured3DWithAlpha(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t, primaryAlpha);
    }

    if (morphData.blendFactor > 0.01f && morphData.secondaryIndex != morphData.primaryIndex) {
        int logicalCol = morphData.secondaryIndex % ATLAS_COLS;
        int logicalRow = morphData.secondaryIndex / ATLAS_COLS;
        Rectangle src = SrcFromLogical(tex, logicalCol, logicalRow, physCols, physRows,
            morphData.mirrored, NULL);

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

        float v0t = v_bottom;
        float v1t = v_top;

        if (morphData.mirrored) {
            float tmp = u_left; u_left = u_right; u_right = tmp;
        }

        float secondaryAlpha = morphData.blendFactor * 0.95f;
        DrawQuadTextured3DWithAlpha(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t, secondaryAlpha);
    }
}
