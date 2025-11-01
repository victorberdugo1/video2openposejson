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


#include "bones_anim_events.h"  // Añadir este include

// Declarar el extern para acceder a g_textureSets desde main.c
extern TextureSetCollection* g_textureSets;


/*
 * +--------------------------------------------------------------+
 * | Function: CalculateBoneMidpoint                              |
 * |                                                              |
 * | Compute an adjusted midpoint / projected position for a      |
 * | given bone in a Person. Handles several special cases:       |
 * |  - Neck: blend between calculated head and original neck.    |
 * |  - Wrists: apply a projection factor from forearm.           |
 * |  - Ankles: adjust to an anatomical foot-forward position.    |
 * |  - Default: simple midpoint with connected bone.             |
 * |                                                              |
 * | - Input: const char* boneName, const Person* person          |
 * | - Output: Vector3 position or (0,0,0) if invalid.            |
 * +--------------------------------------------------------------+
 */
static Vector3 CalculateBoneMidpoint(const char* boneName, const Person* person) {
    if (!person || !boneName) return (Vector3) { 0, 0, 0 };

    // Special case: NECK - make it follow the head more dynamically
    if (strcmp(boneName, "Neck") == 0) {
        Vector3 originalNeck = GetBonePositionByName(person, "Neck");
        if (originalNeck.x == 0 && originalNeck.y == 0 && originalNeck.z == 0) return originalNeck;

        Vector3 calculatedHead = CalculateHeadPosition(person);
        if (calculatedHead.x == 0 && calculatedHead.y == 0 && calculatedHead.z == 0) return originalNeck;

        // Calculate shoulder center as reference point
        Vector3 lShoulder = GetBonePositionByName(person, "LShoulder");
        Vector3 rShoulder = GetBonePositionByName(person, "RShoulder");
        Vector3 shoulderCenter = originalNeck; // fallback

        if ((lShoulder.x || lShoulder.y || lShoulder.z) && (rShoulder.x || rShoulder.y || rShoulder.z)) {
            shoulderCenter = (Vector3){
                (lShoulder.x + rShoulder.x) * 0.5f,
                (lShoulder.y + rShoulder.y) * 0.5f,
                (lShoulder.z + rShoulder.z) * 0.5f
            };
        }

        // Position neck as midpoint between shoulder center and head
        return (Vector3) {
            (shoulderCenter.x + calculatedHead.x) * 0.5f,
                (shoulderCenter.y + calculatedHead.y) * 0.5f,
                (shoulderCenter.z + calculatedHead.z) * 0.5f
        };
    }

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

/*
 * +--------------------------------------------------------------+
 * | Function: BonesGetErrorString                                |
 * |                                                              |
 * | Map a BonesError enum value to a human-readable message.     |
 * | Returns a static string describing the error.                |
 * |                                                              |
 * | - Input: BonesError error                                    |
 * | - Output: const char* description                            |
 * +--------------------------------------------------------------+
 */
const char* BonesGetErrorString(BonesError error) {
    static const char* errorStrings[] = {
        [BONES_SUCCESS] = "Operation successful",
        [BONES_ERROR_NULL_POINTER] = "Null pointer received",
        [BONES_ERROR_FILE_NOT_FOUND] = "File not found",
        [BONES_ERROR_INVALID_JSON] = "Invalid or malformed JSON",
        [BONES_ERROR_MEMORY_ALLOCATION] = "Memory allocation error",
        [BONES_ERROR_BONE_NOT_FOUND] = "Bone not found",
        [BONES_ERROR_FRAME_OUT_OF_RANGE] = "Frame out of valid range",
        [BONES_ERROR_PERSON_NOT_FOUND] = "Person not found",
        [BONES_ERROR_INVALID_COORDINATES] = "Invalid coordinates",
        [BONES_ERROR_BUFFER_OVERFLOW] = "Buffer overflow",
        [BONES_ERROR_EMPTY_DATA] = "Empty or no content data"
    };

    return (error < sizeof(errorStrings) / sizeof(errorStrings[0]) && errorStrings[error])
        ? errorStrings[error] : "Unknown error";
}

/*
 * +--------------------------------------------------------------+
 * | Function: BonesInit                                          |
 * |                                                              |
 * | Initialize a BonesAnimation structure: allocate frames array,|
 * | zero fields, set maxFrames and initial currentFrame.         |
 * |                                                              |
 * | - Input: BonesAnimation* animation, int maxFrames            |
 * | - Output: BonesError (BONES_SUCCESS on success)              |
 * +--------------------------------------------------------------+
 */
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

/*
 * +--------------------------------------------------------------+
 * | Function: BonesFree                                          |
 * |                                                              |
 * | Release dynamic memory owned by a BonesAnimation and clear   |
 * | the struct fields to a safe default.                         |
 * |                                                              |
 * | - Input: BonesAnimation* animation                           |
 * | - Effect: frees frames and resets struct.                    |
 * +--------------------------------------------------------------+
 */
void BonesFree(BonesAnimation* animation) {
    if (animation) {
        free(animation->frames);
        memset(animation, 0, sizeof(BonesAnimation));
        animation->currentFrame = -1;
    }
}

/*
 * +---------------------------------------------------------------+
 * | Function: ParseJSONFrame (static)                             |
 * |                                                               |
 * | Parse a single frame block from a JSON string into Person     |
 * | structures. Looks for "frame_N" and then "person_M" entries,  |
 * | extracting expected bone positions.                           |
 * |                                                               |
 * | - Input: jsonData, outFrameNum, outPersons[], outPersonCount  |
 * | - Output: BonesError (success or parse error).                |
 * +---------------------------------------------------------------+
 */
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

/*
 * +---------------------------------------------------------------+
 * | Function: BonesLoadFromJSON                                   |
 * |                                                               |
 * | Load JSON text from a file and pass it to BonesLoadFromString |
 * | to parse frames. Also stores the file path on success.        |
 * |                                                               |
 * | - Input: BonesAnimation* animation, const char* jsonFilePath  |
 * | - Output: BonesError                                          |
 * +---------------------------------------------------------------+
 */
BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath) {
    if (!animation || !jsonFilePath) return BONES_ERROR_NULL_POINTER;

    char* jsonData = LoadFileText(jsonFilePath);
    if (!jsonData) {
        TraceLog(LOG_ERROR, "BONES ERROR: File not found: %s", jsonFilePath);
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

/*
 * +--------------------------------------------------------------+
 * | Function: BonesLoadFromString                                |
 * |                                                              |
 * | Parse an entire JSON string for multiple frames. Repeatedly  |
 * | finds "frame_" blocks, calls ParseJSONFrame and fills the    |
 * | animation->frames array up to animation->maxFrames.          |
 * |                                                              |
 * | - Input: BonesAnimation* animation, const char* jsonString   |
 * | - Output: BonesError (BONES_SUCCESS if frames loaded)        |
 * +--------------------------------------------------------------+
 */
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

/*
 * +---------------------------------------------------------------+
 * | Function: BonesSetFrame                                       |
 * |                                                               |
 * | Set the current frame index on the animation after validating |
 * | that the animation is loaded and the index is within range.   |
 * |                                                               |
 * | - Input: BonesAnimation* animation, int frameNumber           |
 * | - Output: BonesError (success or out-of-range/null errors)    |
 * +---------------------------------------------------------------+
 */
BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber) {
    if (!animation) return BONES_ERROR_NULL_POINTER;
    if (!animation->isLoaded) return BONES_ERROR_EMPTY_DATA;
    if (frameNumber < 0 || frameNumber >= animation->frameCount) return BONES_ERROR_FRAME_OUT_OF_RANGE;

    animation->currentFrame = frameNumber;
    return BONES_SUCCESS;
}

/*
 * +--------------------------------------------------------------+
 * | Function: BonesGetCurrentFrame                               |
 * |                                                              |
 * | Return the current frame index from the animation or -1 if   |
 * | the pointer is NULL.                                         |
 * |                                                              |
 * | - Input: const BonesAnimation* animation                     |
 * | - Output: int current frame index                            |
 * +--------------------------------------------------------------+
 */
int BonesGetCurrentFrame(const BonesAnimation* animation) {
    return animation ? animation->currentFrame : -1;
}

/*
 * +--------------------------------------------------------------+
 * | Function: BonesGetFrameCount                                 |
 * |                                                              |
 * | Return the number of frames currently parsed in animation or |
 * | 0 if the pointer is NULL.                                    |
 * |                                                              |
 * | - Input: const BonesAnimation* animation                     |
 * | - Output: int frame count                                    |
 * +--------------------------------------------------------------+
 */
int BonesGetFrameCount(const BonesAnimation* animation) {
    return animation ? animation->frameCount : 0;
}

/*
 * +--------------------------------------------------------------+
 * | Function: BonesIsValidFrame                                  |
 * |                                                              |
 * | Check if a given frame index is valid: animation loaded,     |
 * | index in range and frame marked valid.                       |
 * |                                                              |
 * | - Input: const BonesAnimation* animation, int frameNumber    |
 * | - Output: bool (true if valid)                               |
 * +--------------------------------------------------------------+
 */
bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber) {
    return animation && animation->isLoaded && frameNumber >= 0 &&
        frameNumber < animation->frameCount && animation->frames[frameNumber].valid;
}

/*
 * +--------------------------------------------------------------+
 * | Function: BonesIsPositionValid                               |
 * |                                                              |
 * | Validate a Vector3: ensure no NaN and all components finite. |
 * |                                                              |
 * | - Input: Vector3 position                                    |
 * | - Output: bool (true if valid)                               |
 * +--------------------------------------------------------------+
 */
bool BonesIsPositionValid(Vector3 position) {
    return !isnan(position.x) && !isnan(position.y) && !isnan(position.z) &&
        isfinite(position.x) && isfinite(position.y) && isfinite(position.z);
}

/*
 * +---------------------------------------------------------------+
 * | Function: BonesGetDefaultRenderConfig                         |
 * |                                                               |
 * | Return the global default BonesRenderConfig value.            |
 * |                                                               |
 * | - Output: BonesRenderConfig (by value)                        |
 * +---------------------------------------------------------------+
 */
BonesRenderConfig BonesGetDefaultRenderConfig(void) {
    return g_renderConfig;
}

/*
 * +--------------------------------------------------------------+
 * | Function: BonesSetRenderConfig                               |
 * |                                                              |
 * | Replace the global render configuration with the provided    |
 * | config (if not NULL).                                        |
 * |                                                              |
 * | - Input: const BonesRenderConfig* config                     |
 * +--------------------------------------------------------------+
 */
void BonesSetRenderConfig(const BonesRenderConfig* config) {
    if (config) g_renderConfig = *config;
}

/*
 * +---------------------------------------------------------------+
 * | Function: CleanupTextureSystem                                |
 * |                                                               |
 * | Free texture system resources and optional bone config array. |
 * |                                                               |
 * | - Input: SimpleTextureSystem* textureSystem, BoneConfig** cfg |
 * | - Effect: frees arrays and resets counts.                     |
 * +---------------------------------------------------------------+
 */
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

/*
 * +---------------------------------------------------------------+
 * | Function: LoadSimpleTextureConfig                             |
 * |                                                               |
 * | Load a simple bone texture config file: count non-comment     |
 * | lines, allocate config array, parse each valid line.          |
 * |                                                               |
 * | - Input: SimpleTextureSystem* system, const char* filename    |
 * | - Output: bool success/failure                                |
 * +---------------------------------------------------------------+
 */
bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename) {
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
        return true;
    }
    return false;
}

/*
 * +----------------------------------------------------------------+
 * | Function: LoadBoneConfigurations                               |
 * |                                                                |
 * | Convert parsed BoneTextureConfig entries into runtime          |
 * | BoneConfig structures used for rendering.                      |
 * |                                                                |
 * | - Input: SimpleTextureSystem* textureSystem, BoneConfig** out, |
 * |          int* outCount                                         |
 * +----------------------------------------------------------------+
 */
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

/*
 * +----------------------------------------------------------------+
 * | Function: FindBoneConfig                                       |
 * |                                                                |
 * | Linear search for a BoneConfig by boneName.                    |
 * |                                                                |
 * | - Input: BoneConfig* boneConfigs, int boneConfigCount,         |
 * |          const char* boneName                                  |
 * | - Output: BoneConfig* or NULL                                  |
 * +----------------------------------------------------------------+
 */
BoneConfig* FindBoneConfig(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    for (int i = 0; i < boneConfigCount; i++) {
        if (strcmp(boneConfigs[i].boneName, boneName) == 0) {
            return &boneConfigs[i];
        }
    }
    return NULL;
}

/*
 * +----------------------------------------------------------------+
 * | Function: GetTexturePathForBone                                |
 * |                                                                |
 * | Return the texture path string for the requested bone name.    |
 * |                                                                |
 * | - Input: BoneConfig* boneConfigs, int boneConfigCount,         |
 * |          const char* boneName                                  |
 * | - Output: const char* texture path                             |
 * +----------------------------------------------------------------+
 */
const char* GetTexturePathForBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    // ====== NUEVO: Primero intentar obtener del texture set activo ======
    if (g_textureSets) {
        const char* activeTexture = BonesTextureSets_GetActiveTexture(g_textureSets, boneName);
        if (activeTexture) {
            return activeTexture;
        }
    }
    
    // ====== Fallback al config normal (tu código original) ======
    BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return config ? config->texturePath : "default.png";
}

/*
 * +---------------------------------------------------------------+
 * | Function: IsBoneVisible                                       |
 * |                                                               |
 * | Returns whether a bone should be rendered by checking its     |
 * | BoneConfig visibility flag. Defaults to true if no config.    |
 * +---------------------------------------------------------------+
 */
bool IsBoneVisible(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return config ? config->visible : true;
}

/*
 * +----------------------------------------------------------------+
 * | Function: GetBoneSize                                          |
 * |                                                                |
 * | Return the configured size for a bone or the default fallback. |
 * +----------------------------------------------------------------+
 */
float GetBoneSize(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return config ? config->size : 0.35f;
}

/*
 * +----------------------------------------------------------------+
 * | Function: ResizeRenderBonesArray                               |
 * |                                                                |
 * | Grow the render bones array to a larger capacity if requested. |
 * +----------------------------------------------------------------+
 */
bool ResizeRenderBonesArray(BoneRenderData** renderBones, int* renderBonesCapacity, int newCapacity) {
    if (newCapacity <= 0 || newCapacity > 10000 || newCapacity <= *renderBonesCapacity) return true;

    BoneRenderData* newArray = realloc(*renderBones, sizeof(BoneRenderData) * newCapacity);
    if (!newArray) return false;

    memset(newArray + *renderBonesCapacity, 0, sizeof(BoneRenderData) * (newCapacity - *renderBonesCapacity));
    *renderBones = newArray;
    *renderBonesCapacity = newCapacity;
    return true;
}

/*
 * +---------------------------------------------------------------+
 * | Function: CollectBonesForRendering                            |
 * |                                                               |
 * | Collect BoneRenderData entries from the current animation     |
 * | frame. Filters invalid/invisible bones, prevents duplicates,  |
 * | computes midpoints and sorts by distance for rendering.       |
 * +---------------------------------------------------------------+
 */
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

    // Sort by distance (farthest first for depth sorting)
    if (*renderBonesCount > 1) {
        for (int i = 0; i < *renderBonesCount - 1; i++) {
            for (int j = 0; j < *renderBonesCount - i - 1; j++) {
                if ((*renderBones)[j].distance < (*renderBones)[j + 1].distance) {
                    BoneRenderData temp = (*renderBones)[j];
                    (*renderBones)[j] = (*renderBones)[j + 1];
                    (*renderBones)[j + 1] = temp;
                }
            }
        }
    }
}

/*
 * +---------------------------------------------------------------+
 * | Function: EnrichBoneRenderDataWithOrientation                 |
 * |                                                               |
 * | Compute and store orientation data for a BoneRenderData using |
 * | the parent Person and fallback defaults if orientation cannot |
 * | be computed.                                                  |
 * +---------------------------------------------------------------+
 */
void EnrichBoneRenderDataWithOrientation(BoneRenderData* renderBone, const Person* person) {
    if (!renderBone || !person) return;

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