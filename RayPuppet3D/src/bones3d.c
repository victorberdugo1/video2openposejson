#include "bones3d.h"
#include "bonetile.h"
#include "head_billboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

static BonesRenderConfig g_renderConfig = {
    .defaultBoneSize = 0.35f,
    .drawDebugSpheres = false,
    .enableDepthSorting = true,
    .debugColor = RED,
    .debugSphereRadius = 0.035f,
    .showInvalidBones = false
};

static const struct {
    const char* boneName;
    const char* connectedBone;
    float projectionFactor;
} MIDPOINT_CONNECTIONS[] = {
    {"Neck", "HEAD_CALCULATED", 1.0f},
    {"LShoulder", "LElbow", 1.0f}, {"RShoulder", "RElbow", 1.0f},
    {"LElbow", "LWrist", 1.0f}, {"RElbow", "RWrist", 1.0f},
    {"LWrist", "LElbow", 1.3f}, {"RWrist", "RElbow", 1.3f},
    {"LHip", "LKnee", 1.0f}, {"RHip", "RKnee", 1.0f},
    {"LKnee", "LAnkle", 1.0f}, {"RKnee", "RAnkle", 1.0f},
    {"LAnkle", "FOOT_FORWARD", 1.0f}, {"RAnkle", "FOOT_FORWARD", 1.0f},
    {"", "", 0.0f}
};

static Vector3 CalculateBoneMidpoint(const char* boneName, const Person* person) {
    if (!person || !boneName) return (Vector3) { 0, 0, 0 };

    // Special case: NECK - midpoint between calculated head and original neck
    if (strcmp(boneName, "Neck") == 0) {
        Vector3 originalNeck = GetBonePositionByName(person, "Neck");
        if (originalNeck.x == 0 && originalNeck.y == 0 && originalNeck.z == 0) return originalNeck;

        Vector3 calculatedHead = CalculateHeadPosition(person);
        if (calculatedHead.x == 0 && calculatedHead.y == 0 && calculatedHead.z == 0) return originalNeck;

        return (Vector3) {
            calculatedHead.x * 0.33f + originalNeck.x * 0.67f,
                calculatedHead.y * 0.33f + originalNeck.y * 0.67f,
                calculatedHead.z * 0.33f + originalNeck.z * 0.67f
        };
    }

    // Find connection configuration
    const char* connectedBoneName = NULL;
    float projectionFactor = 1.0f;

    for (int i = 0; MIDPOINT_CONNECTIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(MIDPOINT_CONNECTIONS[i].boneName, boneName) == 0) {
            connectedBoneName = MIDPOINT_CONNECTIONS[i].connectedBone;
            projectionFactor = MIDPOINT_CONNECTIONS[i].projectionFactor;
            break;
        }
    }

    if (!connectedBoneName) return GetBonePositionByName(person, boneName);

    Vector3 bonePos = GetBonePositionByName(person, boneName);
    Vector3 connectedPos = GetBonePositionByName(person, connectedBoneName);

    // Special cases for wrists (projection)
    if (strstr(boneName, "Wrist") && projectionFactor != 1.0f) {
        if ((bonePos.x || bonePos.y || bonePos.z) && (connectedPos.x || connectedPos.y || connectedPos.z)) {
            Vector3 forearmVector = {
                bonePos.x - connectedPos.x,
                bonePos.y - connectedPos.y,
                bonePos.z - connectedPos.z
            };
            return (Vector3) {
                connectedPos.x + forearmVector.x * projectionFactor,
                    connectedPos.y + forearmVector.y * projectionFactor,
                    connectedPos.z + forearmVector.z * projectionFactor
            };
        }
        return bonePos;
    }

    // Special case for ankles (anatomical foot position)
    if (strstr(boneName, "Ankle") && strstr(connectedBoneName, "FOOT_FORWARD")) {
        if (bonePos.x || bonePos.y || bonePos.z) {
            Vector3 footPosition = bonePos;
            footPosition.z += 0.008f; // 15cm forward
            footPosition.y -= 0.025f; // 3cm down
            return footPosition;
        }
        return bonePos;
    }

    // Standard midpoint calculation
    if (!(bonePos.x || bonePos.y || bonePos.z)) return (Vector3) { 0, 0, 0 };
    if (!(connectedPos.x || connectedPos.y || connectedPos.z)) return bonePos;

    return (Vector3) {
        (bonePos.x + connectedPos.x) * 0.5f,
            (bonePos.y + connectedPos.y) * 0.5f,
            (bonePos.z + connectedPos.z) * 0.5f
    };
}

const char* BonesGetErrorString(BonesError error) {
    static const char* errorStrings[] = {
        [BONES_SUCCESS] = "Operacion exitosa",
        [BONES_ERROR_NULL_POINTER] = "Puntero nulo recibido",
        [BONES_ERROR_FILE_NOT_FOUND] = "Archivo no encontrado",
        [BONES_ERROR_INVALID_JSON] = "JSON invalido o mal formateado",
        [BONES_ERROR_MEMORY_ALLOCATION] = "Error de asignacion de memoria",
        [BONES_ERROR_BONE_NOT_FOUND] = "Bone no encontrado",
        [BONES_ERROR_FRAME_OUT_OF_RANGE] = "Frame fuera del rango valido",
        [BONES_ERROR_PERSON_NOT_FOUND] = "Persona no encontrada",
        [BONES_ERROR_INVALID_COORDINATES] = "Coordenadas invalidas",
        [BONES_ERROR_BUFFER_OVERFLOW] = "Desbordamiento de buffer",
        [BONES_ERROR_EMPTY_DATA] = "Datos vacios o sin contenido"
    };

    return (error < sizeof(errorStrings) / sizeof(errorStrings[0]) && errorStrings[error])
        ? errorStrings[error] : "Error desconocido";
}

void BonesLogError(BonesError error, const char* context) {
    if (error != BONES_SUCCESS) {
        const char* errorStr = BonesGetErrorString(error);
        if (context && context[0]) {
            TraceLog(LOG_ERROR, "BONES ERROR [%s]: %s", context, errorStr);
        }
        else {
            TraceLog(LOG_ERROR, "BONES ERROR: %s", errorStr);
        }
    }
}

BonesError BonesInit(BonesAnimation* animation, int maxFrames) {
    if (!animation) return BONES_ERROR_NULL_POINTER;

    if (maxFrames <= 0 || maxFrames > MAX_FRAMES) maxFrames = MAX_FRAMES;

    memset(animation, 0, sizeof(BonesAnimation));
    animation->frames = calloc(maxFrames, sizeof(AnimationFrame));

    if (!animation->frames) return BONES_ERROR_MEMORY_ALLOCATION;

    animation->maxFrames = maxFrames;
    animation->currentFrame = -1;
    return BONES_SUCCESS;
}

void BonesFree(BonesAnimation* animation) {
    if (animation) {
        free(animation->frames);
        memset(animation, 0, sizeof(BonesAnimation));
        animation->currentFrame = -1;
    }
}

static BonesError ParseJSONFrame(const char* jsonData, int* outFrameNum, Person* outPersons, int* outPersonCount) {
    if (!jsonData || !outFrameNum || !outPersons || !outPersonCount) return BONES_ERROR_NULL_POINTER;

    *outPersonCount = 0;

    const char* frameStart = strstr(jsonData, "\"frame_");
    if (!frameStart || sscanf(frameStart, "\"frame_%d\"", outFrameNum) != 1) {
        return BONES_ERROR_INVALID_JSON;
    }

    static const char* expectedBones[] = {
        "Nose", "LEye", "REye", "LEar", "REar",
        "LShoulder", "RShoulder", "LElbow", "RElbow",
        "LWrist", "RWrist", "LHip", "RHip",
        "LKnee", "RKnee", "LAnkle", "RAnkle", "Neck"
    };

    const char* personStart = strstr(frameStart, "\"person_");
    while (personStart && *outPersonCount < MAX_PERSONS) {
        Person* currentPerson = &outPersons[*outPersonCount];
        memset(currentPerson, 0, sizeof(Person));

        if (sscanf(personStart, "\"person_%15[^\"]\"", currentPerson->personId) != 1) break;

        currentPerson->active = true;
        const char* nextPerson = strstr(personStart + 1, "\"person_");

        for (int b = 0; b < 18 && currentPerson->boneCount < MAX_BONES_PER_PERSON; b++) {
            char searchPattern[64];
            snprintf(searchPattern, sizeof(searchPattern), "\"%s\":", expectedBones[b]);

            const char* bonePos = strstr(personStart, searchPattern);
            if (!bonePos || (nextPerson && bonePos > nextPerson)) continue;

            float x, y, z;
            if (sscanf(strstr(bonePos, "\"x\":"), "\"x\": %f", &x) == 1 &&
                sscanf(strstr(bonePos, "\"y\":"), "\"y\": %f", &y) == 1 &&
                sscanf(strstr(bonePos, "\"z\":"), "\"z\": %f", &z) == 1) {

                Bone* currentBone = &currentPerson->bones[currentPerson->boneCount];
                strncpy(currentBone->name, expectedBones[b], MAX_BONE_NAME_LENGTH - 1);
                currentBone->name[MAX_BONE_NAME_LENGTH - 1] = '\0';

                currentBone->position.position = (Vector3){
                    x * 2.0f - 1.0f,
                    1.0f - y,
                    z * 2.0f - 1.0f
                };
                currentBone->position.valid = BonesIsPositionValid(currentBone->position.position);
                currentBone->position.confidence = 1.0f;
                currentBone->size = (Vector2){ g_renderConfig.defaultBoneSize, g_renderConfig.defaultBoneSize };
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
    if (!animation || !jsonFilePath) return BONES_ERROR_NULL_POINTER;

    char* jsonData = LoadFileText(jsonFilePath);
    if (!jsonData) {
        BonesLogError(BONES_ERROR_FILE_NOT_FOUND, jsonFilePath);
        return BONES_ERROR_FILE_NOT_FOUND;
    }

    BonesError result = BonesLoadFromString(animation, jsonData);
    if (result == BONES_SUCCESS) {
        strncpy(animation->filePath, jsonFilePath, sizeof(animation->filePath) - 1);
        animation->filePath[sizeof(animation->filePath) - 1] = '\0';
    }

    UnloadFileText(jsonData);
    return result;
}

BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString) {
    if (!animation || !jsonString || !animation->frames) return BONES_ERROR_NULL_POINTER;

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
    if (!animation) return BONES_ERROR_NULL_POINTER;
    if (!animation->isLoaded) return BONES_ERROR_EMPTY_DATA;
    if (frameNumber < 0 || frameNumber >= animation->frameCount) return BONES_ERROR_FRAME_OUT_OF_RANGE;

    animation->currentFrame = frameNumber;
    return BONES_SUCCESS;
}

int BonesGetCurrentFrame(const BonesAnimation* animation) {
    return animation ? animation->currentFrame : -1;
}

int BonesGetFrameCount(const BonesAnimation* animation) {
    return animation ? animation->frameCount : 0;
}

bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber) {
    return animation && animation->isLoaded && frameNumber >= 0 &&
        frameNumber < animation->frameCount && animation->frames[frameNumber].valid;
}

bool BonesIsPositionValid(Vector3 position) {
    return !isnan(position.x) && !isnan(position.y) && !isnan(position.z) &&
        isfinite(position.x) && isfinite(position.y) && isfinite(position.z);
}

BonesRenderConfig BonesGetDefaultRenderConfig(void) {
    return g_renderConfig;
}

void BonesSetRenderConfig(const BonesRenderConfig* config) {
    if (config) g_renderConfig = *config;
}

void CleanupTextureSystem(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount) {
    if (textureSystem->configs) {
        free(textureSystem->configs);
        memset(textureSystem, 0, sizeof(SimpleTextureSystem));
    }
    if (*boneConfigs) {
        free(*boneConfigs);
        *boneConfigs = NULL;
        *boneConfigCount = 0;
    }
}

time_t GetFileModificationTime(const char* filename) {
    struct stat fileStat;
    return (stat(filename, &fileStat) == 0) ? fileStat.st_mtime : 0;
}

bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename) {
    time_t currentModTime = GetFileModificationTime(filename);
    if (currentModTime == 0) return false;
    if (system->loaded && system->lastModified == currentModTime) return true;

    char* buffer = LoadFileText(filename);
    if (!buffer) return false;

    // Count valid lines
    int lineCount = 0;
    for (const char* ptr = buffer; *ptr; ptr++) {
        if (*ptr == '\n') {
            const char* lineStart = ptr;
            while (lineStart > buffer && *(lineStart - 1) != '\n') lineStart--;
            int lineLen = ptr - lineStart;
            if (lineLen > 5 && *lineStart != '#' && *lineStart != '\n') lineCount++;
        }
    }

    if (lineCount == 0) {
        UnloadFileText(buffer);
        return false;
    }

    free(system->configs);
    system->configs = calloc(lineCount, sizeof(BoneTextureConfig));
    if (!system->configs) {
        UnloadFileText(buffer);
        return false;
    }

    system->configCapacity = lineCount;
    system->configCount = 0;

    // Parse config
    char lineBuffer[512];
    const char* lineStart = buffer;

    for (const char* ptr = buffer; *ptr && system->configCount < system->configCapacity; ptr++) {
        if (*ptr == '\n') {
            int lineLen = ptr - lineStart;
            if (lineLen > 5 && lineLen < 511 && *lineStart != '#' && *lineStart != '\n') {
                memcpy(lineBuffer, lineStart, lineLen);
                lineBuffer[lineLen] = '\0';

                char boneName[MAX_BONE_NAME_LENGTH], texturePath[MAX_FILE_PATH_LENGTH];
                int visible;
                float size;

                if (sscanf(lineBuffer, "%31s %255s %d %f", boneName, texturePath, &visible, &size) == 4) {
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
    free(*boneConfigs);
    *boneConfigs = NULL;
    *boneConfigCount = 0;

    if (!textureSystem->loaded || textureSystem->configCount == 0) return;

    *boneConfigCount = textureSystem->configCount;
    *boneConfigs = calloc(*boneConfigCount, sizeof(BoneConfig));
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
    if (newCapacity <= 0 || newCapacity > 10000 || newCapacity <= *renderBonesCapacity) return true;

    BoneRenderData* newArray = realloc(*renderBones, sizeof(BoneRenderData) * newCapacity);
    if (!newArray) return false;

    memset(newArray + *renderBonesCapacity, 0, sizeof(BoneRenderData) * (newCapacity - *renderBonesCapacity));
    *renderBones = newArray;
    *renderBonesCapacity = newCapacity;
    return true;
}

int CompareBonesByDistance(const void* a, const void* b) {
    float diff = ((const BoneRenderData*)b)->distance - ((const BoneRenderData*)a)->distance;
    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}

void CollectBonesForRendering(const BonesAnimation* animation, Camera camera, BoneRenderData** renderBones,
    int* renderBonesCount, int* renderBonesCapacity, BoneConfig* boneConfigs, int boneConfigCount) {
    *renderBonesCount = 0;
    if (!animation->isLoaded) return;

    int currentFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, currentFrame)) return;

    const AnimationFrame* frame = &animation->frames[currentFrame];

    int estimatedBones = 0;
    for (int p = 0; p < frame->personCount; p++) {
        if (frame->persons[p].active) estimatedBones += frame->persons[p].boneCount;
    }

    if (!ResizeRenderBonesArray(renderBones, renderBonesCapacity, estimatedBones + 10)) return;

    static char processedBones[2000][MAX_BONE_NAME_LENGTH + 20];
    int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;

        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid || !bone->visible || !BonesIsPositionValid(bone->position.position)) continue;

            BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, bone->name);
            if (!config) {
                // Quick known bone check
                char firstChar = bone->name[0];
                bool isKnown = (firstChar == 'N' && strstr(bone->name, "Nose")) ||
                    ((firstChar == 'L' || firstChar == 'R') &&
                        (strstr(bone->name, "Eye") || strstr(bone->name, "Ear") ||
                            strstr(bone->name, "Shoulder") || strstr(bone->name, "Elbow") ||
                            strstr(bone->name, "Wrist") || strstr(bone->name, "Hip") ||
                            strstr(bone->name, "Knee") || strstr(bone->name, "Ankle"))) ||
                    (firstChar == 'H' && (strstr(bone->name, "Head") || strstr(bone->name, "Hip"))) ||
                    (firstChar == 'C' && strstr(bone->name, "Chest")) ||
                    (firstChar == 'S' && (strstr(bone->name, "Shoulder") || strstr(bone->name, "Spine")));

                if (!isKnown) continue;
            }
            else if (!config->visible) continue;

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

            // Calculate midpoint position
            Vector3 midpointPos = CalculateBoneMidpoint(bone->name, person);
            if (!BonesIsPositionValid(midpointPos)) continue;

            float distance = Vector3Distance(camera.position, midpointPos);
            if (distance > 50.0f) continue;

            BoneRenderData* renderBone = &(*renderBones)[*renderBonesCount];
            renderBone->position = midpointPos;
            renderBone->distance = distance;
            renderBone->valid = true;

            if (config) {
                strncpy(renderBone->texturePath, config->texturePath, MAX_FILE_PATH_LENGTH - 1);
                renderBone->visible = config->visible;
                renderBone->size = config->size;
            }
            else {
                strncpy(renderBone->texturePath, GetTexturePathForBone(boneConfigs, boneConfigCount, bone->name), MAX_FILE_PATH_LENGTH - 1);
                renderBone->visible = IsBoneVisible(boneConfigs, boneConfigCount, bone->name);
                renderBone->size = GetBoneSize(boneConfigs, boneConfigCount, bone->name);
            }
            renderBone->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

            strncpy(renderBone->boneName, bone->name, MAX_BONE_NAME_LENGTH - 1);
            renderBone->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
            strncpy(renderBone->personId, person->personId, 15);
            renderBone->personId[15] = '\0';
            EnrichBoneRenderDataWithOrientation(renderBone, person);
            (*renderBonesCount)++;
        }
    }

    if (*renderBonesCount > 1) {
        qsort(*renderBones, *renderBonesCount, sizeof(BoneRenderData), CompareBonesByDistance);
    }
}
// Nueva función para enriquecer BoneRenderData con orientación
void EnrichBoneRenderDataWithOrientation(BoneRenderData* renderBone, const Person* person) {
    if (!renderBone || !person) return;

    // Calcular orientación basada en conexiones
    BoneOrientation orientation = CalculateBoneOrientation(renderBone->boneName, person, renderBone->position);

    if (orientation.valid) {
        renderBone->orientation.position = orientation.position;
        renderBone->orientation.forward = orientation.forward;
        renderBone->orientation.up = orientation.up;
        renderBone->orientation.right = orientation.right;
        renderBone->orientation.yaw = orientation.yaw;
        renderBone->orientation.pitch = orientation.pitch;
        renderBone->orientation.roll = orientation.roll;
        renderBone->orientation.valid = true;
    }
    else {
        // Orientación por defecto si no se puede calcular
        renderBone->orientation.position = renderBone->position;
        renderBone->orientation.forward = (Vector3){ 0, 0, 1 };
        renderBone->orientation.up = (Vector3){ 0, 1, 0 };
        renderBone->orientation.right = (Vector3){ 1, 0, 0 };
        renderBone->orientation.yaw = 0.0f;
        renderBone->orientation.pitch = 0.0f;
        renderBone->orientation.roll = 0.0f;
        renderBone->orientation.valid = true;
    }
}