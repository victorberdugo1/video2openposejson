#include "bones3d.h"
#include "bonetile.h"
#include "head_billboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

static BonesRenderConfig g_renderConfig = { 0 };
static bool g_configInitialized = false;

// Mapa de conexiones para cálculo de puntos medios

// Mapa de conexiones actualizado para el nuevo sistema
static const struct {
    const char* boneName;
    const char* connectedBone;
    float projectionFactor; // Factor para proyección (1.0 = punto medio, >1.0 = proyección)
} MIDPOINT_CONNECTIONS[] = {
    // CUELLO - punto medio entre cabeza calculada y cuello original
    {"Neck", "HEAD_CALCULATED", 1.0f},  // Caso especial

    // BRAZOS
    // Shoulders: punto medio entre hombro original y codo
    {"LShoulder", "LElbow", 1.0f},
    {"RShoulder", "RElbow", 1.0f},

    // Elbows: punto medio entre codo original y muñeca
    {"LElbow", "LWrist", 1.0f},
    {"RElbow", "RWrist", 1.0f},

    // Wrists: proyección del antebrazo (codo -> muñeca extendido)
    {"LWrist", "LElbow", 1.3f}, // Proyección 30% más allá
    {"RWrist", "RElbow", 1.3f},

    // PIERNAS
    // Hips: punto medio entre cadera original y rodilla
    {"LHip", "LKnee", 1.0f},
    {"RHip", "RKnee", 1.0f},

    // Knees: punto medio entre rodilla original y tobillo
    {"LKnee", "LAnkle", 1.0f},
    {"RKnee", "RAnkle", 1.0f},

    {"LAnkle", "FOOT_FORWARD", 1.0f}, // Identificador especial
    {"RAnkle", "FOOT_FORWARD", 1.0f}, // Identificador especial

    {"", "", 0.0f}
};

static void InitializeDefaultConfig(void) {
    if (!g_configInitialized) {
        g_renderConfig.defaultBoneSize = 0.35f;
        g_renderConfig.drawDebugSpheres = false;
        g_renderConfig.enableDepthSorting = true;
        g_renderConfig.debugColor = RED;
        g_renderConfig.debugSphereRadius = 0.035f;
        g_renderConfig.showInvalidBones = false;
        g_configInitialized = true;
    }
}

static bool IsValidPointer(const void* ptr) {
    return ptr != NULL;
}

static bool IsValidString(const char* str) {
    return str != NULL && str[0] != '\0';
}

static void SafeStrCopy(char* dest, const char* src, size_t destSize) {
    if (!IsValidPointer(dest) || !IsValidString(src) || destSize == 0) return;

    size_t len = strlen(src);
    if (len >= destSize) len = destSize - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

// Función auxiliar para obtener posición original del hueso
// Función auxiliar para obtener posición original del hueso
static Vector3 GetOriginalBonePosition(const char* boneName, const Person* person) {
    if (!person || !boneName) return (Vector3) { 0, 0, 0 };

    for (int i = 0; i < person->boneCount; i++) {
        if (strcmp(person->bones[i].name, boneName) == 0 && person->bones[i].position.valid) {
            return person->bones[i].position.position;
        }
    }
    return (Vector3) { 0, 0, 0 };
}



// 2. Reemplazar completamente la función CalculateBoneMidpoint:

static Vector3 CalculateBoneMidpoint(const char* boneName, const Person* person) {
    if (!person || !boneName) return (Vector3) { 0, 0, 0 };

    // CASO ESPECIAL: CUELLO - punto medio entre cabeza calculada y cuello original
    if (strcmp(boneName, "Neck") == 0) {
        // Obtener posición original del cuello de OpenPose
        Vector3 originalNeck = GetOriginalBonePosition("Neck", person);

        // Si no hay cuello original, no podemos calcular
        if (originalNeck.x == 0 && originalNeck.y == 0 && originalNeck.z == 0) {
            return originalNeck;
        }

        // Calcular posición anatómica de la cabeza
        Vector3 calculatedHead = CalculateHeadPosition(person);

        // Si no se pudo calcular la cabeza, usar cuello original
        if (calculatedHead.x == 0 && calculatedHead.y == 0 && calculatedHead.z == 0) {
            return originalNeck;
        }

        // Cuello a 1/3 de distancia desde cuello original hacia cabeza
        // (33% cabeza, 67% cuello original = cuello más cerca del punto original)
        return (Vector3) {
            calculatedHead.x * 0.33f + originalNeck.x * 0.67f,
                calculatedHead.y * 0.33f + originalNeck.y * 0.67f,
                calculatedHead.z * 0.33f + originalNeck.z * 0.67f
        };

        // Otras opciones comentadas:
        // 1/4 distancia: calculatedHead * 0.25f + originalNeck * 0.75f
        // Punto medio:   calculatedHead * 0.5f  + originalNeck * 0.5f  
        // 3/4 distancia: calculatedHead * 0.75f + originalNeck * 0.25f
    }

    // Buscar la configuración de conexión para otros huesos
    const char* connectedBoneName = NULL;
    float projectionFactor = 1.0f;

    for (int i = 0; MIDPOINT_CONNECTIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(MIDPOINT_CONNECTIONS[i].boneName, boneName) == 0) {
            connectedBoneName = MIDPOINT_CONNECTIONS[i].connectedBone;
            projectionFactor = MIDPOINT_CONNECTIONS[i].projectionFactor;
            break;
        }
    }

    if (!connectedBoneName) {
        // Si no hay conexión definida, usar posición original
        return GetOriginalBonePosition(boneName, person);
    }

    // Obtener posiciones de los huesos involucrados
    Vector3 bonePos = { 0, 0, 0 };
    Vector3 connectedPos = { 0, 0, 0 };
    bool foundBone = false, foundConnected = false;

    // BRAZOS - casos especiales que requieren posiciones originales
    if (strstr(boneName, "Shoulder") != NULL) {
        // Shoulder: punto medio entre hombro original y codo original
        bonePos = GetOriginalBonePosition(boneName, person);
        connectedPos = GetOriginalBonePosition(connectedBoneName, person);
        foundBone = (bonePos.x != 0 || bonePos.y != 0 || bonePos.z != 0);
        foundConnected = (connectedPos.x != 0 || connectedPos.y != 0 || connectedPos.z != 0);
    }
    else if (strstr(boneName, "Elbow") != NULL) {
        // Elbow: punto medio entre codo original y muñeca original
        bonePos = GetOriginalBonePosition(boneName, person);
        connectedPos = GetOriginalBonePosition(connectedBoneName, person);
        foundBone = (bonePos.x != 0 || bonePos.y != 0 || bonePos.z != 0);
        foundConnected = (connectedPos.x != 0 || connectedPos.y != 0 || connectedPos.z != 0);
    }
    else if (strstr(boneName, "Wrist") != NULL) {
        // Wrist: proyección del vector codo -> muñeca original
        Vector3 originalWrist = GetOriginalBonePosition(boneName, person);
        Vector3 elbow = GetOriginalBonePosition(connectedBoneName, person);

        if ((originalWrist.x != 0 || originalWrist.y != 0 || originalWrist.z != 0) &&
            (elbow.x != 0 || elbow.y != 0 || elbow.z != 0)) {

            // Calcular vector del antebrazo (codo -> muñeca)
            Vector3 forearmVector = {
                originalWrist.x - elbow.x,
                originalWrist.y - elbow.y,
                originalWrist.z - elbow.z
            };

            // Proyectar el punto más allá de la muñeca original
            return (Vector3) {
                elbow.x + forearmVector.x * projectionFactor,
                    elbow.y + forearmVector.y * projectionFactor,
                    elbow.z + forearmVector.z * projectionFactor
            };
        }

        // Si no se puede calcular la proyección, usar posición original
        return originalWrist;
    }
    // PIERNAS - casos especiales que requieren posiciones originales
    else if (strstr(boneName, "Hip") != NULL) {
        // Hip: punto medio entre cadera original y rodilla original
        bonePos = GetOriginalBonePosition(boneName, person);
        connectedPos = GetOriginalBonePosition(connectedBoneName, person);
        foundBone = (bonePos.x != 0 || bonePos.y != 0 || bonePos.z != 0);
        foundConnected = (connectedPos.x != 0 || connectedPos.y != 0 || connectedPos.z != 0);
    }
    else if (strstr(boneName, "Knee") != NULL) {
        // Knee: punto medio entre rodilla original y tobillo original
        bonePos = GetOriginalBonePosition(boneName, person);
        connectedPos = GetOriginalBonePosition(connectedBoneName, person);
        foundBone = (bonePos.x != 0 || bonePos.y != 0 || bonePos.z != 0);
        foundConnected = (connectedPos.x != 0 || connectedPos.y != 0 || connectedPos.z != 0);
    }
    else if (strstr(boneName, "Ankle") != NULL) {
        // Ankle: posición anatómica del pie - hacia adelante desde el tobillo
        Vector3 originalAnkle = GetOriginalBonePosition(boneName, person);

        if (originalAnkle.x != 0 || originalAnkle.y != 0 || originalAnkle.z != 0) {
            // Los pies van hacia adelante (eje Z positivo) desde el tobillo
            // No seguimos la dirección de la pantorrilla, sino hacia adelante anatómicamente
            Vector3 footPosition = originalAnkle;

            // Proyectar hacia adelante (dirección Z positiva = adelante en la mayoría de sistemas)
            // Aproximadamente la longitud de un pie humano (20-25cm)
            footPosition.z += 0.03f; // 15cm hacia adelante

            // Ligeramente hacia abajo para tocar el suelo
            footPosition.y -= 0.01f; // 3cm hacia abajo

            return footPosition;
        }

        return originalAnkle;
    }
    else {
        // Para otros huesos, usar el sistema original
        bonePos = GetOriginalBonePosition(boneName, person);
        connectedPos = GetOriginalBonePosition(connectedBoneName, person);
        foundBone = (bonePos.x != 0 || bonePos.y != 0 || bonePos.z != 0);
        foundConnected = (connectedPos.x != 0 || connectedPos.y != 0 || connectedPos.z != 0);
    }

    if (!foundBone) return (Vector3) { 0, 0, 0 };
    if (!foundConnected) return bonePos;

    // Calcular punto medio o proyección según el factor
    if (projectionFactor == 1.0f) {
        // Punto medio estándar
        return (Vector3) {
            (bonePos.x + connectedPos.x) * 0.5f,
                (bonePos.y + connectedPos.y) * 0.5f,
                (bonePos.z + connectedPos.z) * 0.5f
        };
    }
    else {
        // Proyección (para casos futuros si necesitas otros huesos proyectados)
        Vector3 direction = {
            connectedPos.x - bonePos.x,
            connectedPos.y - bonePos.y,
            connectedPos.z - bonePos.z
        };

        return (Vector3) {
            bonePos.x + direction.x * projectionFactor,
                bonePos.y + direction.y * projectionFactor,
                bonePos.z + direction.z * projectionFactor
        };
    }
}


const char* BonesGetErrorString(BonesError error) {
    switch (error) {
    case BONES_SUCCESS: return "Operacion exitosa";
    case BONES_ERROR_NULL_POINTER: return "Puntero nulo recibido";
    case BONES_ERROR_FILE_NOT_FOUND: return "Archivo no encontrado";
    case BONES_ERROR_INVALID_JSON: return "JSON invalido o mal formateado";
    case BONES_ERROR_MEMORY_ALLOCATION: return "Error de asignacion de memoria";
    case BONES_ERROR_BONE_NOT_FOUND: return "Bone no encontrado";
    case BONES_ERROR_FRAME_OUT_OF_RANGE: return "Frame fuera del rango valido";
    case BONES_ERROR_PERSON_NOT_FOUND: return "Persona no encontrada";
    case BONES_ERROR_INVALID_COORDINATES: return "Coordenadas invalidas";
    case BONES_ERROR_BUFFER_OVERFLOW: return "Desbordamiento de buffer";
    case BONES_ERROR_EMPTY_DATA: return "Datos vacios o sin contenido";
    default: return "Error desconocido";
    }
}

void BonesLogError(BonesError error, const char* context) {
    if (error != BONES_SUCCESS) {
        const char* errorStr = BonesGetErrorString(error);
        if (IsValidString(context)) {
            TraceLog(LOG_ERROR, "BONES ERROR [%s]: %s", context, errorStr);
        }
        else {
            TraceLog(LOG_ERROR, "BONES ERROR: %s", errorStr);
        }
    }
}

BonesError BonesInit(BonesAnimation* animation, int maxFrames) {
    if (!IsValidPointer(animation)) return BONES_ERROR_NULL_POINTER;

    if (maxFrames <= 0 || maxFrames > MAX_FRAMES) maxFrames = MAX_FRAMES;

    memset(animation, 0, sizeof(BonesAnimation));

    animation->frames = (AnimationFrame*)calloc(maxFrames, sizeof(AnimationFrame));
    if (!IsValidPointer(animation->frames)) return BONES_ERROR_MEMORY_ALLOCATION;

    animation->maxFrames = maxFrames;
    animation->frameCount = 0;
    animation->currentFrame = -1;
    animation->isLoaded = false;

    InitializeDefaultConfig();
    return BONES_SUCCESS;
}

void BonesFree(BonesAnimation* animation) {
    if (!IsValidPointer(animation)) return;

    if (IsValidPointer(animation->frames)) {
        free(animation->frames);
        animation->frames = NULL;
    }

    animation->frameCount = 0;
    animation->maxFrames = 0;
    animation->currentFrame = -1;
    animation->isLoaded = false;
    animation->filePath[0] = '\0';
}

static BonesError ParseJSONFrame(const char* jsonData, int* outFrameNum, Person* outPersons, int* outPersonCount) {
    if (!IsValidString(jsonData) || !IsValidPointer(outFrameNum) ||
        !IsValidPointer(outPersons) || !IsValidPointer(outPersonCount)) {
        return BONES_ERROR_NULL_POINTER;
    }

    *outPersonCount = 0;

    const char* frameStart = strstr(jsonData, "\"frame_");
    if (!frameStart) return BONES_ERROR_INVALID_JSON;

    if (sscanf(frameStart, "\"frame_%d\"", outFrameNum) != 1) {
        return BONES_ERROR_INVALID_JSON;
    }

    const char* personStart = strstr(frameStart, "\"person_");
    while (personStart && *outPersonCount < MAX_PERSONS) {
        Person* currentPerson = &outPersons[*outPersonCount];
        memset(currentPerson, 0, sizeof(Person));

        if (sscanf(personStart, "\"person_%15[^\"]\"", currentPerson->personId) != 1) break;

        currentPerson->active = true;
        currentPerson->boneCount = 0;

        const char* nextPerson = strstr(personStart + 1, "\"person_");

        const char* expectedBones[] = {
            "Nose", "LEye", "REye", "LEar", "REar",
            "LShoulder", "RShoulder", "LElbow", "RElbow",
            "LWrist", "RWrist", "LHip", "RHip",
            "LKnee", "RKnee", "LAnkle", "RAnkle", "Neck"
        };
        int expectedBoneCount = sizeof(expectedBones) / sizeof(expectedBones[0]);

        for (int b = 0; b < expectedBoneCount && currentPerson->boneCount < MAX_BONES_PER_PERSON; b++) {
            char searchPattern[64];
            snprintf(searchPattern, sizeof(searchPattern), "\"%s\":", expectedBones[b]);

            const char* bonePos = strstr(personStart, searchPattern);
            if (!bonePos || (nextPerson && bonePos > nextPerson)) continue;

            Bone* currentBone = &currentPerson->bones[currentPerson->boneCount];
            memset(currentBone, 0, sizeof(Bone));

            SafeStrCopy(currentBone->name, expectedBones[b], MAX_BONE_NAME_LENGTH);

            float x = 0.0f, y = 0.0f, z = 0.0f;
            const char* xPos = strstr(bonePos, "\"x\":");
            const char* yPos = strstr(bonePos, "\"y\":");
            const char* zPos = strstr(bonePos, "\"z\":");

            if (xPos && yPos && zPos &&
                sscanf(xPos, "\"x\": %f", &x) == 1 &&
                sscanf(yPos, "\"y\": %f", &y) == 1 &&
                sscanf(zPos, "\"z\": %f", &z) == 1) {

                currentBone->position.position = (Vector3){
                    x * 2.0f - 1.0f,
                    1.0f - y,
                    z * 2.0f - 1.0f
                };
                currentBone->position.valid = BonesIsPositionValid(currentBone->position.position);
                currentBone->position.confidence = 1.0f;

                currentBone->textureIndex = 0;
                currentBone->size = (Vector2){ g_renderConfig.defaultBoneSize, g_renderConfig.defaultBoneSize };
                currentBone->rotation = 0.0f;
                currentBone->mirrored = false;
                currentBone->visible = true;

                currentPerson->boneCount++;
            }
        }

        if (currentPerson->boneCount > 0) (*outPersonCount)++;
        personStart = nextPerson;
    }

    return (*outPersonCount > 0) ? BONES_SUCCESS : BONES_ERROR_EMPTY_DATA;
}

BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath) {
    if (!IsValidPointer(animation) || !IsValidString(jsonFilePath)) {
        return BONES_ERROR_NULL_POINTER;
    }

    char* jsonData = LoadFileText(jsonFilePath);
    if (!IsValidPointer(jsonData)) {
        BonesLogError(BONES_ERROR_FILE_NOT_FOUND, jsonFilePath);
        return BONES_ERROR_FILE_NOT_FOUND;
    }

    BonesError result = BonesLoadFromString(animation, jsonData);

    if (result == BONES_SUCCESS) {
        SafeStrCopy(animation->filePath, jsonFilePath, sizeof(animation->filePath));
    }

    UnloadFileText(jsonData);
    return result;
}

BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString) {
    if (!IsValidPointer(animation) || !IsValidString(jsonString)) {
        return BONES_ERROR_NULL_POINTER;
    }

    if (!IsValidPointer(animation->frames)) {
        BonesLogError(BONES_ERROR_NULL_POINTER, "Animation not initialized");
        return BONES_ERROR_NULL_POINTER;
    }

    animation->frameCount = 0;
    animation->currentFrame = -1;
    animation->isLoaded = false;

    const char* searchPos = jsonString;

    while (animation->frameCount < animation->maxFrames) {
        const char* nextFrame = strstr(searchPos, "\"frame_");
        if (!nextFrame) break;

        AnimationFrame* frame = &animation->frames[animation->frameCount];
        memset(frame, 0, sizeof(AnimationFrame));

        BonesError parseResult = ParseJSONFrame(nextFrame, &frame->frameNumber,
            frame->persons, &frame->personCount);

        if (parseResult == BONES_SUCCESS) {
            frame->valid = true;
            animation->frameCount++;
        }
        else if (parseResult != BONES_ERROR_EMPTY_DATA) {
            BonesLogError(parseResult, "ParseJSONFrame");
        }

        searchPos = nextFrame + 1;
    }

    if (animation->frameCount > 0) {
        animation->currentFrame = 0;
        animation->isLoaded = true;
        return BONES_SUCCESS;
    }

    return BONES_ERROR_EMPTY_DATA;
}

BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber) {
    if (!IsValidPointer(animation)) return BONES_ERROR_NULL_POINTER;
    if (!animation->isLoaded) return BONES_ERROR_EMPTY_DATA;
    if (frameNumber < 0 || frameNumber >= animation->frameCount) {
        return BONES_ERROR_FRAME_OUT_OF_RANGE;
    }

    animation->currentFrame = frameNumber;
    return BONES_SUCCESS;
}

int BonesGetCurrentFrame(const BonesAnimation* animation) {
    return (IsValidPointer(animation)) ? animation->currentFrame : -1;
}

int BonesGetFrameCount(const BonesAnimation* animation) {
    return (IsValidPointer(animation)) ? animation->frameCount : 0;
}

bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber) {
    return IsValidPointer(animation) &&
        animation->isLoaded &&
        frameNumber >= 0 &&
        frameNumber < animation->frameCount &&
        animation->frames[frameNumber].valid;
}

BonesError BonesGetPerson(const BonesAnimation* animation, int frameNumber,
    const char* personId, Person** outPerson) {
    if (!IsValidPointer(animation) || !IsValidString(personId) || !IsValidPointer(outPerson)) {
        return BONES_ERROR_NULL_POINTER;
    }

    *outPerson = NULL;

    if (!BonesIsValidFrame(animation, frameNumber)) {
        return BONES_ERROR_FRAME_OUT_OF_RANGE;
    }

    const AnimationFrame* frame = &animation->frames[frameNumber];

    for (int i = 0; i < frame->personCount; i++) {
        if (strcmp(frame->persons[i].personId, personId) == 0) {
            *outPerson = (Person*)&frame->persons[i];
            return BONES_SUCCESS;
        }
    }

    return BONES_ERROR_PERSON_NOT_FOUND;
}

BonesError BonesGetBone(const BonesAnimation* animation, int frameNumber,
    const char* personId, const char* boneName, Bone** outBone) {
    Person* person;
    BonesError result = BonesGetPerson(animation, frameNumber, personId, &person);
    if (result != BONES_SUCCESS) return result;

    if (!IsValidString(boneName) || !IsValidPointer(outBone)) {
        return BONES_ERROR_NULL_POINTER;
    }

    *outBone = NULL;

    for (int i = 0; i < person->boneCount; i++) {
        if (strcmp(person->bones[i].name, boneName) == 0) {
            *outBone = &person->bones[i];
            return BONES_SUCCESS;
        }
    }

    return BONES_ERROR_BONE_NOT_FOUND;
}

BonesError BonesGetBonePosition(const BonesAnimation* animation, int frameNumber,
    const char* personId, const char* boneName,
    Vector3* outPosition) {
    Bone* bone;
    BonesError result = BonesGetBone(animation, frameNumber, personId, boneName, &bone);
    if (result != BONES_SUCCESS) return result;

    if (!IsValidPointer(outPosition)) return BONES_ERROR_NULL_POINTER;
    if (!bone->position.valid) return BONES_ERROR_INVALID_COORDINATES;

    *outPosition = bone->position.position;
    return BONES_SUCCESS;
}

bool BonesIsPositionValid(Vector3 position) {
    return !isnan(position.x) && !isnan(position.y) && !isnan(position.z) &&
        isfinite(position.x) && isfinite(position.y) && isfinite(position.z);
}

BonesRenderConfig BonesGetDefaultRenderConfig(void) {
    InitializeDefaultConfig();
    return g_renderConfig;
}

void BonesSetRenderConfig(const BonesRenderConfig* config) {
    if (IsValidPointer(config)) {
        g_renderConfig = *config;
        g_configInitialized = true;
    }
}

void CheckBoneNameLength(void) {
    if (MAX_BONE_NAME_LENGTH < 32) {
        TraceLog(LOG_ERROR, "MAX_BONE_NAME_LENGTH debe ser al menos 32 bytes");
    }
}

void CleanupTextureSystem(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount) {
    if (textureSystem->configs) {
        free(textureSystem->configs);
        textureSystem->configs = NULL;
        textureSystem->configCount = 0;
        textureSystem->configCapacity = 0;
        textureSystem->loaded = false;
        textureSystem->lastModified = 0;
    }

    if (*boneConfigs) {
        free(*boneConfigs);
        *boneConfigs = NULL;
        *boneConfigCount = 0;
    }
}

time_t GetFileModificationTime(const char* filename) {
    struct stat fileStat;
    if (stat(filename, &fileStat) == 0) {
        return fileStat.st_mtime;
    }
    return 0;
}

bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename) {
    time_t currentModTime = GetFileModificationTime(filename);
    if (currentModTime == 0) return false;

    if (system->loaded && system->lastModified == currentModTime) return true;

    char* buffer = LoadFileText(filename);
    if (!buffer) return false;

    // Count valid lines
    int lineCount = 0;
    const char* ptr = buffer;
    const char* lineStart = ptr;

    while (*ptr) {
        if (*ptr == '\n' || *ptr == '\0') {
            int lineLen = ptr - lineStart;
            if (lineLen > 5 && *lineStart != '#' && *lineStart != '\n') lineCount++;
            lineStart = ptr + 1;
        }
        ptr++;
    }

    if (lineCount == 0) {
        UnloadFileText(buffer);
        return false;
    }

    if (system->configs) {
        free(system->configs);
        system->configs = NULL;
        system->configCount = 0;
    }

    system->configs = (BoneTextureConfig*)calloc(lineCount, sizeof(BoneTextureConfig));
    if (!system->configs) {
        UnloadFileText(buffer);
        return false;
    }

    system->configCapacity = lineCount;
    system->configCount = 0;

    // Parse config
    ptr = buffer;
    lineStart = ptr;
    char lineBuffer[512];

    while (*ptr && system->configCount < system->configCapacity) {
        if (*ptr == '\n') {
            int lineLen = ptr - lineStart;

            if (lineLen > 5 && lineLen < 511 && *lineStart != '#' && *lineStart != '\n') {
                memcpy(lineBuffer, lineStart, lineLen);
                lineBuffer[lineLen] = '\0';

                char boneName[MAX_BONE_NAME_LENGTH];
                char texturePath[MAX_FILE_PATH_LENGTH];
                int visible;
                float size;

                int fields = sscanf(lineBuffer, "%31s %255s %d %f", boneName, texturePath, &visible, &size);

                if (fields == 4) {
                    BoneTextureConfig* config = &system->configs[system->configCount];
                    strncpy(config->boneName, boneName, MAX_BONE_NAME_LENGTH - 1);
                    config->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';

                    strncpy(config->texturePath, texturePath, MAX_FILE_PATH_LENGTH - 1);
                    config->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

                    config->visible = (visible != 0);
                    config->size = size;
                    system->configCount++;
                }
            }

            lineStart = ptr + 1;
        }
        ptr++;
    }

    UnloadFileText(buffer);

    if (system->configCount > 0) {
        system->loaded = true;
        system->lastModified = currentModTime;
        return true;
    }

    return false;
}

void LoadBoneConfigurations(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount) {
    if (*boneConfigs) {
        free(*boneConfigs);
        *boneConfigs = NULL;
        *boneConfigCount = 0;
    }

    if (!textureSystem->loaded || textureSystem->configCount == 0) return;

    *boneConfigCount = textureSystem->configCount;
    *boneConfigs = (BoneConfig*)calloc(*boneConfigCount, sizeof(BoneConfig));
    if (!*boneConfigs) return;

    for (int i = 0; i < *boneConfigCount; i++) {
        const BoneTextureConfig* src = &textureSystem->configs[i];
        BoneConfig* dst = &(*boneConfigs)[i];

        strncpy(dst->boneName, src->boneName, MAX_BONE_NAME_LENGTH - 1);
        dst->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';

        strncpy(dst->texturePath, src->texturePath, MAX_FILE_PATH_LENGTH - 1);
        dst->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

        dst->visible = src->visible;
        dst->size = src->size;
        dst->valid = true;
    }
}

BoneConfig* FindBoneConfig(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    for (int i = 0; i < boneConfigCount; i++) {
        if (strcmp(boneConfigs[i].boneName, boneName) == 0) {
            return &boneConfigs[i];
        }
    }
    return NULL;
}

const char* GetTexturePathForBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return config ? config->texturePath : "default.png";
}

bool IsBoneVisible(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return config ? config->visible : true;
}

float GetBoneSize(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return config ? config->size : 0.35f;
}

bool ResizeRenderBonesArray(BoneRenderData** renderBones, int* renderBonesCapacity, int newCapacity) {
    if (newCapacity <= 0 || newCapacity > 10000) return false;
    if (newCapacity <= *renderBonesCapacity) return true;

    BoneRenderData* newArray = (BoneRenderData*)realloc(*renderBones, sizeof(BoneRenderData) * newCapacity);
    if (!newArray) return false;

    for (int i = *renderBonesCapacity; i < newCapacity; i++) {
        memset(&newArray[i], 0, sizeof(BoneRenderData));
    }

    *renderBones = newArray;
    *renderBonesCapacity = newCapacity;
    return true;
}

int CompareBonesByDistance(const void* a, const void* b) {
    const BoneRenderData* boneA = (const BoneRenderData*)a;
    const BoneRenderData* boneB = (const BoneRenderData*)b;

    if (boneA->distance > boneB->distance) return -1;
    if (boneA->distance < boneB->distance) return 1;
    return 0;
}

// Función modificada para usar puntos medios
void CollectBonesForRendering(const BonesAnimation* animation, Camera camera, BoneRenderData** renderBones,
    int* renderBonesCount, int* renderBonesCapacity, BoneConfig* boneConfigs, int boneConfigCount) {
    *renderBonesCount = 0;

    if (!animation->isLoaded) return;

    int currentFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, currentFrame)) return;

    const AnimationFrame* frame = &animation->frames[currentFrame];

    int estimatedBones = 0;
    for (int p = 0; p < frame->personCount; p++) {
        if (frame->persons[p].active) {
            estimatedBones += frame->persons[p].boneCount;
        }
    }

    if (!ResizeRenderBonesArray(renderBones, renderBonesCapacity, estimatedBones + 10)) return;

    static char processedBones[2000][MAX_BONE_NAME_LENGTH + 20];
    int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;

        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid || !bone->visible) continue;
            if (!BonesIsPositionValid(bone->position.position)) continue;

            BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, bone->name);
            if (!config) {
                // Check if it's a known bone
                char firstChar = bone->name[0];
                bool isKnownBone = false;

                switch (firstChar) {
                case 'N': isKnownBone = (strstr(bone->name, "Nose") != NULL); break;
                case 'L': case 'R':
                    isKnownBone = (strstr(bone->name, "Eye") != NULL ||
                        strstr(bone->name, "Ear") != NULL ||
                        strstr(bone->name, "Shoulder") != NULL ||
                        strstr(bone->name, "Elbow") != NULL ||
                        strstr(bone->name, "Wrist") != NULL ||
                        strstr(bone->name, "Hip") != NULL ||
                        strstr(bone->name, "Knee") != NULL ||
                        strstr(bone->name, "Ankle") != NULL);
                    break;
                case 'H': isKnownBone = (strstr(bone->name, "Head") != NULL ||
                    strstr(bone->name, "Hip") != NULL); break;
                case 'C': isKnownBone = (strstr(bone->name, "Chest") != NULL); break;
                case 'S': isKnownBone = (strstr(bone->name, "Shoulder") != NULL ||
                    strstr(bone->name, "Spine") != NULL); break;
                }

                if (!isKnownBone) continue;
            }
            else {
                if (!config->visible) continue;
            }

            // Check for duplicates
            char boneKey[MAX_BONE_NAME_LENGTH + 20];
            int keyLen = snprintf(boneKey, sizeof(boneKey), "%s_%s", person->personId, bone->name);

            bool alreadyProcessed = false;
            for (int i = 0; i < processedCount; i++) {
                if (memcmp(processedBones[i], boneKey, keyLen + 1) == 0) {
                    alreadyProcessed = true;
                    break;
                }
            }

            if (alreadyProcessed) continue;

            if (processedCount < 2000) {
                memcpy(processedBones[processedCount], boneKey, keyLen + 1);
                processedCount++;
            }

            // CLAVE: Calcular punto medio en lugar de usar la posición del bone directamente
            Vector3 midpointPos = CalculateBoneMidpoint(bone->name, person);
            if (!BonesIsPositionValid(midpointPos)) continue;

            float distance = Vector3Distance(camera.position, midpointPos);
            if (distance > 50.0f) continue;

            BoneRenderData* renderBone = &(*renderBones)[*renderBonesCount];
            renderBone->position = midpointPos; // Usar punto medio en lugar de posición del bone
            renderBone->distance = distance;
            renderBone->valid = true;

            if (config) {
                strncpy(renderBone->texturePath, config->texturePath, MAX_FILE_PATH_LENGTH - 1);
                renderBone->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                renderBone->visible = config->visible;
                renderBone->size = config->size;
            }
            else {
                const char* defaultPath = GetTexturePathForBone(boneConfigs, boneConfigCount, bone->name);
                strncpy(renderBone->texturePath, defaultPath, MAX_FILE_PATH_LENGTH - 1);
                renderBone->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                renderBone->visible = IsBoneVisible(boneConfigs, boneConfigCount, bone->name);
                renderBone->size = GetBoneSize(boneConfigs, boneConfigCount, bone->name);
            }

            strncpy(renderBone->boneName, bone->name, MAX_BONE_NAME_LENGTH - 1);
            renderBone->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';

            strncpy(renderBone->personId, person->personId, 15);
            renderBone->personId[15] = '\0';

            (*renderBonesCount)++;
        }
    }

    if (*renderBonesCount > 1) {
        qsort(*renderBones, *renderBonesCount, sizeof(BoneRenderData), CompareBonesByDistance);
    }
}