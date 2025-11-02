#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include "bones_core.h"

extern TextureSetCollection* g_textureSets;

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

static const float HEAD_DEPTH_OFFSET = 0.04f;
static const float CHEST_OFFSET_Y = -0.06f;
static const float CHEST_OFFSET_Z = -0.005f;
static const float CHEST_FALLBACK_Y = -0.08f;
static const float HIP_OFFSET_Y = -0.02f;


void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads,
    int* headCount, int* headCapacity, BoneConfig* boneConfigs, int boneConfigCount) {
    *headCount = 0;
    if (!animation->isLoaded) return;

    int currentFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, currentFrame)) return;

    const AnimationFrame* frame = &animation->frames[currentFrame];

    if (*headCapacity < frame->personCount) {
        HeadRenderData* newArray = realloc(*heads, sizeof(HeadRenderData) * frame->personCount);
        if (!newArray) return;
        *heads = newArray;
        *headCapacity = frame->personCount;
    }

    static char processedHeads[100][20];
    int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!ShouldRenderHead(person)) continue;

        bool alreadyProcessed = false;
        for (int i = 0; i < processedCount; i++) {
            if (strcmp(processedHeads[i], person->personId) == 0) {
                alreadyProcessed = true;
                break;
            }
        }
        if (alreadyProcessed) continue;

        if (processedCount < 100) {
            strncpy(processedHeads[processedCount], person->personId, 19);
            processedHeads[processedCount][19] = '\0';
            processedCount++;
        }

        HeadRenderData* headData = &(*heads)[*headCount];
        memset(headData, 0, sizeof(HeadRenderData));

        headData->position = CalculateHeadPosition(person);
        headData->orientation = CalculateHeadOrientation(person);

        if (!headData->orientation.valid && Vector3Length(headData->position) < 1e-6f) continue;

        headData->valid = true;
        headData->visible = true;

        bool textureFound = false;
        if (g_textureSets && g_textureSets->loaded) {
            const char* activeTexture = BonesTextureSets_GetActiveTexture(g_textureSets, "Head");
            if (activeTexture) {
                strncpy(headData->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
                headData->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                textureFound = true;
                
                printf("HEAD_RENDER: Using texture %s\n", activeTexture);
            }
        }
        
        // Fallback al config normal si no hay texture set
        if (!textureFound) {
            BoneConfig* headConfig = FindBoneConfig(boneConfigs, boneConfigCount, "Head");
            if (headConfig) {
                strncpy(headData->texturePath, headConfig->texturePath, MAX_FILE_PATH_LENGTH - 1);
                headData->size = headConfig->size;
                headData->visible = headConfig->visible;
            }
            else {
                strncpy(headData->texturePath, "data/textures/hil/Head.png", MAX_FILE_PATH_LENGTH - 1);
                headData->size = 0.25f;
                headData->visible = true;
            }
            headData->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
        } else {
            // Usar size del config aunque la textura venga del texture set
            BoneConfig* headConfig = FindBoneConfig(boneConfigs, boneConfigCount, "Head");
            if (headConfig) {
                headData->size = headConfig->size;
            } else {
                headData->size = 0.25f;
            }
        }

        strncpy(headData->personId, person->personId, 15);
        headData->personId[15] = '\0';

        (*headCount)++;
    }
}

void CollectTorsosForRendering(const BonesAnimation* animation, TorsoRenderData** torsos,
    int* torsoCount, int* torsoCapacity, BoneConfig* boneConfigs, int boneConfigCount) {

    *torsoCount = 0;
    if (!animation->isLoaded) return;

    int currentFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, currentFrame)) return;

    const AnimationFrame* frame = &animation->frames[currentFrame];
    int estimatedTorsos = frame->personCount * 2;

    if (*torsoCapacity < estimatedTorsos) {
        TorsoRenderData* newArray = (TorsoRenderData*)realloc(*torsos, sizeof(TorsoRenderData) * estimatedTorsos);
        if (!newArray) return;
        *torsos = newArray;
        *torsoCapacity = estimatedTorsos;
    }

    static char processedTorsos[200][25];
    int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];

        if (ShouldRenderChest(person)) {
            char torsoKey[25];
            snprintf(torsoKey, sizeof(torsoKey), "%s_chest", person->personId);

            bool alreadyProcessed = false;
            for (int i = 0; i < processedCount; i++) {
                if (strcmp(processedTorsos[i], torsoKey) == 0) {
                    alreadyProcessed = true;
                    break;
                }
            }

            if (!alreadyProcessed) {
                if (processedCount < 200) {
                    strncpy(processedTorsos[processedCount], torsoKey, 24);
                    processedTorsos[processedCount][24] = '\0';
                    processedCount++;
                }

                TorsoRenderData* torsoData = &(*torsos)[*torsoCount];
                memset(torsoData, 0, sizeof(TorsoRenderData));

                torsoData->position = CalculateChestPosition(person);
                torsoData->orientation = CalculateChestOrientation(person);
                torsoData->type = TORSO_CHEST;
                torsoData->person = person;

                if (!torsoData->orientation.valid && Vector3Length(torsoData->position) < 1e-6f) {
                    continue;
                }

                torsoData->valid = true;
                torsoData->visible = true;

                BoneConfig* chestConfig = FindBoneConfig(boneConfigs, boneConfigCount, "Chest");
                if (chestConfig) {
                    strncpy(torsoData->texturePath, chestConfig->texturePath, MAX_FILE_PATH_LENGTH - 1);
                    torsoData->size = chestConfig->size;
                    torsoData->visible = chestConfig->visible;
                }
                else {
                    strncpy(torsoData->texturePath, "tex/Chest.png", MAX_FILE_PATH_LENGTH - 1);
                    torsoData->size = 0.4f;
                    torsoData->visible = true;
                }
                torsoData->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

                strncpy(torsoData->personId, person->personId, 15);
                torsoData->personId[15] = '\0';

                (*torsoCount)++;
            }
        }

        if (ShouldRenderHip(person)) {
            char torsoKey[25];
            snprintf(torsoKey, sizeof(torsoKey), "%s_hip", person->personId);

            bool alreadyProcessed = false;
            for (int i = 0; i < processedCount; i++) {
                if (strcmp(processedTorsos[i], torsoKey) == 0) {
                    alreadyProcessed = true;
                    break;
                }
            }

            if (!alreadyProcessed) {
                if (processedCount < 200) {
                    strncpy(processedTorsos[processedCount], torsoKey, 24);
                    processedTorsos[processedCount][24] = '\0';
                    processedCount++;
                }

                TorsoRenderData* torsoData = &(*torsos)[*torsoCount];
                memset(torsoData, 0, sizeof(TorsoRenderData));

                torsoData->position = CalculateHipPosition(person);
                torsoData->orientation = CalculateHipOrientation(person);
                torsoData->type = TORSO_HIP;
                torsoData->person = person;

                if (!torsoData->orientation.valid && Vector3Length(torsoData->position) < 1e-6f) {
                    continue;
                }

                torsoData->valid = true;
                torsoData->visible = true;

                BoneConfig* hipConfig = FindBoneConfig(boneConfigs, boneConfigCount, "Hip");
                if (hipConfig) {
                    strncpy(torsoData->texturePath, hipConfig->texturePath, MAX_FILE_PATH_LENGTH - 1);
                    torsoData->size = hipConfig->size;
                    torsoData->visible = hipConfig->visible;
                }
                else {
                    strncpy(torsoData->texturePath, "tex/Hip.png", MAX_FILE_PATH_LENGTH - 1);
                    torsoData->size = 0.35f;
                    torsoData->visible = true;
                }
                torsoData->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

                strncpy(torsoData->personId, person->personId, 15);
                torsoData->personId[15] = '\0';

                (*torsoCount)++;
            }
        }
    }
}


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

Vector3 SafeNormalize(Vector3 v) {
    float length = Vector3Length(v);
    if (length < 1e-6f) return (Vector3) { 0, 0, 1 };
    return Vector3Scale(v, 1.0f / length);
}

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
    if (logicalRow < 0) logicalRow = 0;

    if (physCols <= 0) physCols = ATLAS_COLS;
    if (physRows <= 0) physRows = ATLAS_ROWS;

    float cellW = (float)tex.width / (float)physCols;
    float cellH = (float)tex.height / (float)physRows;

    if (logicalCol >= physCols) logicalCol = physCols - 1;
    if (logicalRow >= physRows) logicalRow = physRows - 1;

    if (outMirrored) *outMirrored = mirrored;
    return (Rectangle) {
        logicalCol * cellW,
        logicalRow * cellH,
        cellW,
        cellH
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

bool IsWristBone(const char* boneName) {
    if (!boneName) return false;
    return (strcmp(boneName, "LWrist") == 0) || (strcmp(boneName, "RWrist") == 0);
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
            neighborValid = false;
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

BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int count, const char* name) {
    if (!bones || !name) return NULL;
    for (int i = 0; i < count; i++) {
        if (bones[i].valid && bones[i].visible && strcmp(bones[i].boneName, name) == 0) {
            return &bones[i];
        }
    }
    return NULL;
}

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
        *outRotation = 0.0f;
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

    if ((sector == 0 || sector == 4) &&
        localPitchDeg < 60.0f && localPitchDeg > -60.0f) {

        rotationCompensation *= 0.01f;

        if (boneData->boneName[0] != '\0') {
            if (strcmp(boneData->boneName, "LShoulder") == 0 ||
                strcmp(boneData->boneName, "RShoulder") == 0 ||
                strcmp(boneData->boneName, "LElbow") == 0 ||
                strcmp(boneData->boneName, "RElbow") == 0 ||
                strcmp(boneData->boneName, "LHip") == 0 ||
                strcmp(boneData->boneName, "RHip") == 0 ||
                strcmp(boneData->boneName, "LKnee") == 0 ||
                strcmp(boneData->boneName, "RKnee") == 0) {
                rotationCompensation = 0.0f;
            }
        }
    }

    if (((sector >= 7 || sector <= 1) || (sector >= 3 && sector <= 5)) &&
        localPitchDeg < 45.0f && localPitchDeg > -45.0f) {
        rotationCompensation *= 0.3f;
    }

    *outRotation += rotationCompensation;

    while (*outRotation >= 360.0f) *outRotation -= 360.0f;
    while (*outRotation < 0.0f) *outRotation += 360.0f;
}

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
    }

    if (pitchDeg >= 52.5f) {
        *outChosenIndex = 22;
        *outRotation = sector * 45.0f + 180.0f;
        *outMirrored = false;
    }
    else if (pitchDeg <= -51.0f) {
        *outChosenIndex = 3;
    }
}

static CachedBones CacheBones(const Person* person) {
    CachedBones cache = { 0 };

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        const char* name = bone->name;
        Vector3 pos = bone->position.position;

        if (strcmp(name, "Neck") == 0) {
            cache.neck = pos; cache.hasNeck = true;
        }
        else if (strcmp(name, "LShoulder") == 0) {
            cache.lShoulder = pos; cache.hasLShoulder = true; cache.shoulderCount++;
        }
        else if (strcmp(name, "RShoulder") == 0) {
            cache.rShoulder = pos; cache.hasRShoulder = true; cache.shoulderCount++;
        }
        else if (strcmp(name, "LHip") == 0) {
            cache.lHip = pos; cache.hasLHip = true; cache.hipCount++;
        }
        else if (strcmp(name, "RHip") == 0) {
            cache.rHip = pos; cache.hasRHip = true; cache.hipCount++;
        }
    }
    return cache;
}

Vector3 CalculateChestPosition(const Person* person) {
    if (!person || person->boneCount == 0) return (Vector3) { 0, 0, 0 };

    CachedBones cache = CacheBones(person);

    if (cache.hasNeck && cache.shoulderCount > 0) {
        Vector3 shoulderCenter = cache.hasLShoulder && cache.hasRShoulder ?
            Vector3Scale(Vector3Add(cache.lShoulder, cache.rShoulder), 0.5f) :
            (cache.hasLShoulder ? cache.lShoulder : cache.rShoulder);

        return (Vector3) {
            (cache.neck.x + shoulderCenter.x) * 0.5f,
            cache.neck.y + CHEST_OFFSET_Y,
            cache.neck.z + CHEST_OFFSET_Z
        };
    }

    if (cache.shoulderCount > 0 || cache.hasNeck) {
        Vector3 total = { 0,0,0 };
        int count = 0;

        if (cache.hasNeck) { total = Vector3Add(total, cache.neck); count++; }
        if (cache.hasLShoulder) { total = Vector3Add(total, cache.lShoulder); count++; }
        if (cache.hasRShoulder) { total = Vector3Add(total, cache.rShoulder); count++; }

        Vector3 result = Vector3Scale(total, 1.0f / count);
        result.y += CHEST_FALLBACK_Y;
        return result;
    }

    return (Vector3) { 0, 0, 0 };
}

Vector3 CalculateHipPosition(const Person* person) {
    if (!person || person->boneCount == 0) return (Vector3) { 0, 0, 0 };

    CachedBones cache = CacheBones(person);

    if (cache.hipCount == 0) return (Vector3) { 0, 0, 0 };

    Vector3 hipCenter = { 0, 0, 0 };
    int hipPointCount = 0;

    if (cache.hasLHip) {
        hipCenter = Vector3Add(hipCenter, cache.lHip);
        hipPointCount++;
    }
    if (cache.hasRHip) {
        hipCenter = Vector3Add(hipCenter, cache.rHip);
        hipPointCount++;
    }

    hipCenter = Vector3Scale(hipCenter, 1.0f / hipPointCount);

    Vector3 hipTorsoCenter = { 0, 0, 0 };
    int hipTorsoCenterCount = 0;

    hipTorsoCenter = Vector3Add(hipTorsoCenter, hipCenter);
    hipTorsoCenter = Vector3Add(hipTorsoCenter, hipCenter);
    hipTorsoCenterCount += 2;

    if (cache.shoulderCount > 0) {
        Vector3 shoulderCenter = { 0, 0, 0 };
        int shoulderPointCount = 0;

        if (cache.hasLShoulder) {
            shoulderCenter = Vector3Add(shoulderCenter, cache.lShoulder);
            shoulderPointCount++;
        }
        if (cache.hasRShoulder) {
            shoulderCenter = Vector3Add(shoulderCenter, cache.rShoulder);
            shoulderPointCount++;
        }

        if (shoulderPointCount > 0) {
            shoulderCenter = Vector3Scale(shoulderCenter, 1.0f / shoulderPointCount);
            hipTorsoCenter = Vector3Add(hipTorsoCenter, shoulderCenter);
            hipTorsoCenterCount++;
        }
    }

    hipTorsoCenter = Vector3Scale(hipTorsoCenter, 1.0f / hipTorsoCenterCount);

    Vector3 hipDirection = { 0, 0, 1 };
    if (cache.shoulderCount > 0) {
        Vector3 shoulderCenter = { 0, 0, 0 };
        int shoulderPointCount = 0;

        if (cache.hasLShoulder) {
            shoulderCenter = Vector3Add(shoulderCenter, cache.lShoulder);
            shoulderPointCount++;
        }
        if (cache.hasRShoulder) {
            shoulderCenter = Vector3Add(shoulderCenter, cache.rShoulder);
            shoulderPointCount++;
        }

        if (shoulderPointCount > 0) {
            shoulderCenter = Vector3Scale(shoulderCenter, 1.0f / shoulderPointCount);
            Vector3 shoulderToHip = Vector3Subtract(hipCenter, shoulderCenter);
            float torsoLength = Vector3Length(shoulderToHip);
            if (torsoLength > 1e-6f) {
                hipDirection = Vector3Scale(shoulderToHip, 1.0f / torsoLength);
            }
        }
    }

    Vector3 hipPos = Vector3Add(hipTorsoCenter, Vector3Scale(hipDirection, 0.01f));

    Vector3 chestPos = CalculateChestPosition(person);
    if (Vector3Length(chestPos) > 0.0f) {
        Vector3 chestToOriginalHip = Vector3Subtract(hipCenter, chestPos);
        float torsoDistance = Vector3Length(chestToOriginalHip);

        if (torsoDistance > 0.12f) {
            Vector3 torsoDirection = Vector3Scale(chestToOriginalHip, 1.0f / torsoDistance);
            hipPos = Vector3Add(chestPos, Vector3Scale(torsoDirection, 0.10f));
        }
    }

    hipPos.y += HIP_OFFSET_Y;

    return hipPos;
}

VirtualSpine CalculateVirtualSpine(const Person* person) {
    VirtualSpine spine = { 0 };
    if (!person || person->boneCount == 0) return spine;

    CachedBones cache = CacheBones(person);

    if (!cache.hasLShoulder || !cache.hasRShoulder || !cache.hasLHip || !cache.hasRHip) {
        return spine;
    }

    spine.chestPosition = CalculateChestPosition(person);
    spine.hipPosition = CalculateHipPosition(person);

    Vector3 spineVec = Vector3Subtract(spine.chestPosition, spine.hipPosition);
    float spineLength = Vector3Length(spineVec);
    if (spineLength < EPSILON) return spine;

    spine.spineDirection = Vector3Scale(spineVec, 1.0f / spineLength);

    Vector3 shoulderLine = Vector3Subtract(cache.rShoulder, cache.lShoulder);
    float shoulderLength = Vector3Length(shoulderLine);
    if (shoulderLength < EPSILON) return spine;

    spine.spineRight = Vector3Scale(shoulderLine, 1.0f / shoulderLength);
    spine.spineForward = Vector3CrossProduct(spine.spineRight, spine.spineDirection);

    float forwardLength = Vector3Length(spine.spineForward);
    if (forwardLength < EPSILON) return spine;

    spine.spineForward = Vector3Scale(spine.spineForward, 1.0f / forwardLength);
    spine.spineRight = Vector3CrossProduct(spine.spineDirection, spine.spineForward);

    float rightLength = Vector3Length(spine.spineRight);
    if (rightLength > EPSILON) {
        spine.spineRight = Vector3Scale(spine.spineRight, 1.0f / rightLength);
    }

    spine.valid = true;
    return spine;
}

static TorsoOrientation CreateOrientation(Vector3 pos, Vector3 forward, Vector3 up, Vector3 right) {
    TorsoOrientation orientation = { 0 };
    orientation.position = pos;
    orientation.forward = forward;
    orientation.up = up;
    orientation.right = right;

    orientation.yaw = atan2f(forward.x, forward.z);

    float horizDistance = sqrtf(forward.x * forward.x + forward.z * forward.z);
    orientation.pitch = atan2f(-forward.y, horizDistance);

    orientation.roll = atan2f(right.y, sqrtf(right.x * right.x + right.z * right.z));

    orientation.valid = true;
    return orientation;
}

TorsoOrientation CalculateChestOrientation(const Person* person) {
    VirtualSpine spine = CalculateVirtualSpine(person);
    if (!spine.valid) {
        Vector3 chestPos = CalculateChestPosition(person);
        if (Vector3Length(chestPos) > 0.0f) {
            return CreateOrientation(chestPos, (Vector3) { 0, 0, 1 }, (Vector3) { 0, 1, 0 }, (Vector3) { 1, 0, 0 });
        }
        return (TorsoOrientation) { 0 };
    }

    return CreateOrientation(spine.chestPosition, spine.spineForward, spine.spineDirection, spine.spineRight);
}

TorsoOrientation CalculateHipOrientation(const Person* person) {
    VirtualSpine spine = CalculateVirtualSpine(person);
    if (!spine.valid) {
        Vector3 hipPos = CalculateHipPosition(person);
        if (Vector3Length(hipPos) > 0.0f) {
            return CreateOrientation(hipPos, (Vector3) { 0, 0, 1 }, (Vector3) { 0, 1, 0 }, (Vector3) { 1, 0, 0 });
        }
        return (TorsoOrientation) { 0 };
    }

    return CreateOrientation(spine.hipPosition, spine.spineForward, spine.spineDirection, spine.spineRight);
}

void CalculateTorsoRenderData(const TorsoRenderData* torsoData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!torsoData->orientation.valid) {
        CalculateHandBoneRenderData(torsoData->position, camera, outChosenIndex, outRotation, outMirrored, "");
        return;
    }

    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},
        {2,12,13,14,15,14,13,12},
        {1,8,9,10,11,10,9,8}
    };

    Vector3 camDir = Vector3Subtract(torsoData->position, camera.position);
    camDir = SafeNormalize(camDir);

    Vector3 localCamDir = {
        Vector3DotProduct(camDir, torsoData->orientation.right),
        Vector3DotProduct(camDir, torsoData->orientation.up),
        Vector3DotProduct(camDir, torsoData->orientation.forward)
    };

    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;

    float localPitchDeg = atan2f(localCamDir.y,
        sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z)) * RAD2DEG;

    localPitchDeg = -localPitchDeg;

    float normalizedYaw = localYaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    int sector = (int)(normalizedYaw / 45.0f) % 8;

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
        *outMirrored = (sector >= 1 && sector <= 4);
    }

    if (torsoData->type == TORSO_CHEST && torsoData->person &&
        localPitchDeg < 70.0f && localPitchDeg > -70.0f && outRotation) {

        Vector3 hipPosition = CalculateHipPosition(torsoData->person);

        Vector3 chestToHip = Vector3Subtract(hipPosition, torsoData->position);
        float distance = Vector3Length(chestToHip);
        if (distance > 0.001f) {
            chestToHip = Vector3Scale(chestToHip, 1.0f / distance);

            float pitchToHip = atan2f(chestToHip.y,
                sqrtf(chestToHip.x * chestToHip.x + chestToHip.z * chestToHip.z)) * RAD2DEG;

            Vector3 camToTorso = Vector3Subtract(torsoData->position, camera.position);
            camToTorso = SafeNormalize(camToTorso);
            Vector3 torsoRight = torsoData->orientation.right;
            float sideDot = Vector3DotProduct(camToTorso, torsoRight);
            bool viewingFromRight = (sideDot > 0.0f);

            if (viewingFromRight) {
                *outRotation = -pitchToHip - 80.0f;
            }
            else {
                *outRotation = pitchToHip + 80.0f;
            }

            while (*outRotation >= 360.0f) *outRotation -= 360.0f;
            while (*outRotation < 0.0f) *outRotation += 360.0f;
        }
    }

    if (torsoData->type == TORSO_HIP && torsoData->person &&
        localPitchDeg < 70.0f && localPitchDeg > -70.0f) {

        Vector3 chestPosition = CalculateChestPosition(torsoData->person);

        Vector3 hipToChest = Vector3Subtract(chestPosition, torsoData->position);

        float distance = Vector3Length(hipToChest);
        if (distance > 0.001f) {
            hipToChest = Vector3Scale(hipToChest, 1.0f / distance);

            float pitchToChest = atan2f(hipToChest.y,
                sqrtf(hipToChest.x * hipToChest.x + hipToChest.z * hipToChest.z)) * RAD2DEG;

            Vector3 camToTorso = Vector3Subtract(torsoData->position, camera.position);
            Vector3 torsoRight = torsoData->orientation.right;
            float sideDot = Vector3DotProduct(camToTorso, torsoRight);
            bool viewingFromRight = (sideDot > 0.0f);

            if (viewingFromRight) {
                *outRotation = pitchToChest - 80;
            }
            else {
                *outRotation = -pitchToChest + 80;
            }

            while (*outRotation >= 360.0f) *outRotation -= 360.0f;
            while (*outRotation < 0.0f) *outRotation += 360.0f;
        }
    }
}

bool ShouldRenderChest(const Person* person) {
    if (!person || !person->active) return false;
    return CacheBones(person).shoulderCount >= 1;
}

bool ShouldRenderHip(const Person* person) {
    if (!person || !person->active) return false;
    return CacheBones(person).hipCount >= 1;
}

void DrawTorsoBillboard(Texture2D texture, Camera camera, const TorsoRenderData* torsoData, int physCols, int physRows) {
    if (!torsoData || !torsoData->valid || !torsoData->visible) return;
    int chosenIndex;
    float rotation;
    bool mirrored;
    CalculateTorsoRenderData(torsoData, camera, &chosenIndex, &rotation, &mirrored);

    if (torsoData->orientation.valid) {
        Vector3 camDir = Vector3Subtract(torsoData->position, camera.position);
        camDir = SafeNormalize(camDir);
        Vector3 localCamDir = {
            Vector3DotProduct(camDir, torsoData->orientation.right),
            Vector3DotProduct(camDir, torsoData->orientation.up),
            Vector3DotProduct(camDir, torsoData->orientation.forward)
        };

        float localYaw = atan2f(localCamDir.x, localCamDir.z);
        if (localYaw < 0.0f) localYaw += 2.0f * PI;

        float localPitchDeg = atan2f(localCamDir.y,
            sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z)) * RAD2DEG;
        localPitchDeg = -localPitchDeg;

        float normalizedYaw = localYaw * RAD2DEG + 22.5f;
        if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;
        int sector = (int)(normalizedYaw / 45.0f) % 8;

        if ((sector == 0 || sector == 4) &&
            localPitchDeg < 70.0f && localPitchDeg > -70.0f) {
            rotation = 0.0f;
        }
    }

    int logicalCol = chosenIndex % ATLAS_COLS;
    int logicalRow = chosenIndex / ATLAS_COLS;
    bool finalMirror;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows, mirrored, &finalMirror);
    Vector2 worldSize = { torsoData->size, torsoData->size };
    DrawBonetileCustom(texture, camera, src, torsoData->position, worldSize, rotation, finalMirror, "");
}

Vector3 CalculateHeadPosition(const Person* person) {
    if (!person || person->boneCount == 0) return (Vector3) { 0, 0, 0 };

    Vector3 eyeCenter = { 0, 0, 0 };
    int eyeCount = 0;
    Vector3 neckPos = { 0, 0, 0 };
    bool hasNeck = false;
    Vector3 nosePos = { 0, 0, 0 };
    bool hasNose = false;
    Vector3 lEar = { 0, 0, 0 }, rEar = { 0, 0, 0 };
    bool hasLEar = false, hasREar = false;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        const char* name = bone->name;
        if (strcmp(name, "LEye") == 0 || strcmp(name, "REye") == 0) {
            eyeCenter = Vector3Add(eyeCenter, bone->position.position);
            eyeCount++;
        }
        else if (strcmp(name, "Nose") == 0) {
            nosePos = bone->position.position;
            hasNose = true;
        }
        else if (strcmp(name, "Neck") == 0) {
            neckPos = bone->position.position;
            hasNeck = true;
        }
        else if (strcmp(name, "LEar") == 0) {
            lEar = bone->position.position;
            hasLEar = true;
        }
        else if (strcmp(name, "REar") == 0) {
            rEar = bone->position.position;
            hasREar = true;
        }
    }

    if (eyeCount > 0) {
        eyeCenter = Vector3Scale(eyeCenter, 1.0f / eyeCount);
    }

    if (hasNose && eyeCount > 0 && (hasLEar || hasREar || hasNeck)) {
        Vector3 faceCenter = { 0, 0, 0 };
        int facePointCount = 0;

        faceCenter = Vector3Add(faceCenter, nosePos);
        faceCenter = Vector3Add(faceCenter, nosePos);
        facePointCount += 2;

        faceCenter = Vector3Add(faceCenter, eyeCenter);
        facePointCount++;

        Vector3 backReference;
        bool hasBackReference = false;

        if (hasLEar && hasREar) {
            backReference = Vector3Scale(Vector3Add(lEar, rEar), 0.5f);
            hasBackReference = true;
        }
        else if (hasLEar || hasREar) {
            backReference = hasLEar ? lEar : rEar;
            hasBackReference = true;
        }
        else if (hasNeck) {
            backReference = neckPos;
            hasBackReference = true;
        }

        if (hasBackReference) {
            faceCenter = Vector3Add(faceCenter, backReference);
            facePointCount++;
        }

        faceCenter = Vector3Scale(faceCenter, 1.0f / facePointCount);

        Vector3 frontDirection = { 0, 0, 1 };
        if (hasBackReference) {
            Vector3 noseToBack = Vector3Subtract(backReference, nosePos);
            float backDistance = Vector3Length(noseToBack);
            if (backDistance > 1e-6f) {
                frontDirection = Vector3Scale(noseToBack, 1.0f / backDistance);
            }
        }

        Vector3 headPos = Vector3Add(faceCenter, Vector3Scale(frontDirection, HEAD_DEPTH_OFFSET));

        Vector3 eyeToNose = Vector3Subtract(nosePos, eyeCenter);
        float verticalComponent = eyeToNose.y;

        float dynamicUpOffset = 0.03f;
        if (verticalComponent < -0.01f) {
            dynamicUpOffset = 0.015f;
        }
        else if (verticalComponent > 0.01f) {
            dynamicUpOffset = 0.045f;
        }

        headPos.y += dynamicUpOffset;

        return headPos;
    }

    if (eyeCount > 0 && hasNeck) {
        Vector3 headPos = {
            neckPos.x * 0.7f + eyeCenter.x * 0.3f,
            eyeCenter.y,
            hasNose ? neckPos.z * 0.8f + nosePos.z * 0.2f : neckPos.z * 0.9f + eyeCenter.z * 0.1f
        };

        headPos.z -= 0.01f;
        headPos.y += 0.03f;

        return headPos;
    }

    return (Vector3) { 0, 0, 0 };
}

HeadOrientation CalculateHeadOrientation(const Person* person) {
    HeadOrientation orientation = { .valid = false };
    if (!person || person->boneCount == 0) return orientation;

    Vector3 nose = { 0,0,0 }, lEye = { 0,0,0 }, rEye = { 0,0,0 }, lEar = { 0,0,0 }, rEar = { 0,0,0 };
    bool hasNose = false, hasLEye = false, hasREye = false, hasLEar = false, hasREar = false;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        const char* name = bone->name;
        if (strcmp(name, "Nose") == 0) {
            nose = bone->position.position; hasNose = true;
        }
        else if (strcmp(name, "LEye") == 0) {
            lEye = bone->position.position; hasLEye = true;
        }
        else if (strcmp(name, "REye") == 0) {
            rEye = bone->position.position; hasREye = true;
        }
        else if (strcmp(name, "LEar") == 0) {
            lEar = bone->position.position; hasLEar = true;
        }
        else if (strcmp(name, "REar") == 0) {
            rEar = bone->position.position; hasREar = true;
        }
    }

    if (!hasNose || !((hasLEye && hasREye) || (hasLEar && hasREar))) {
        Vector3 centerFallback = CalculateHeadPosition(person);
        if (Vector3Length(centerFallback) > 0.0f) {
            orientation.position = centerFallback;
            orientation.valid = true;
        }
        return orientation;
    }

    orientation.position = CalculateHeadPosition(person);

    Vector3 rightVec = { 1,0,0 };
    Vector3 backRef;

    if (hasLEar && hasREar) {
        rightVec = Vector3Normalize(Vector3Subtract(rEar, lEar));
        backRef = Vector3Scale(Vector3Add(lEar, rEar), 0.5f);
    }
    else if (hasLEye && hasREye) {
        rightVec = Vector3Normalize(Vector3Subtract(rEye, lEye));
        backRef = Vector3Scale(Vector3Add(lEye, rEye), 0.5f);
    }
    else {
        backRef = orientation.position;
    }

    Vector3 forward = Vector3Normalize(Vector3Subtract(nose, backRef));
    if (Vector3Length(forward) < 1e-6f) forward = (Vector3){ 0,0,1 };

    Vector3 up = Vector3Normalize(Vector3CrossProduct(rightVec, forward));
    if (Vector3Length(up) < 1e-6f) up = (Vector3){ 0,1,0 };

    rightVec = Vector3Normalize(Vector3CrossProduct(forward, up));

    orientation.forward = forward;
    orientation.up = up;
    orientation.right = rightVec;
    orientation.yaw = atan2f(forward.x, forward.z);
    orientation.pitch = atan2f(-forward.y, sqrtf(forward.x * forward.x + forward.z * forward.z));
    orientation.roll = atan2f(up.x, sqrtf(up.y * up.y + up.z * up.z));
    orientation.valid = true;

    return orientation;
}

void CalculateHeadRenderData(const HeadRenderData* headData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {
    if (!headData->orientation.valid) {
        CalculateHandBoneRenderData(headData->position, camera, outChosenIndex, outRotation, outMirrored, "Head");
        return;
    }
    static const int indices[3][8] = {
        {  0,  4,  5,  6,  7,  6,  5,  4 },
        {  2, 12, 13, 14, 15, 14, 13, 12 },
        {  1,  8,  9, 10, 11, 10,  9,  8 }
    };
    Vector3 camDir = Vector3Subtract(camera.position, headData->position);
    Vector3 localCamDir = {
        -Vector3DotProduct(camDir, headData->orientation.right),
        Vector3DotProduct(camDir, headData->orientation.up),
        Vector3DotProduct(camDir, headData->orientation.forward)
    };
    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localYawDeg = localYaw * RAD2DEG;
    float horizDistance = sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z);
    float localPitchDeg = atan2f(localCamDir.y, horizDistance) * RAD2DEG;
    int sector;

    if (localPitchDeg >= 65.0f || localPitchDeg <= -65.0f) {
        Vector3 horizontalDiff = {
            camera.position.x - headData->position.x,
            0.0f,
            camera.position.z - headData->position.z
        };
        float horizontalDistance = Vector3Length(horizontalDiff);
        if (horizontalDistance > 0.05f) {
            float worldYaw = atan2f(horizontalDiff.x, horizontalDiff.z);
            if (worldYaw < 0.0f) worldYaw += 2.0f * PI;
            float normalizedWorldYaw = worldYaw * RAD2DEG + 22.5f;
            if (normalizedWorldYaw >= 360.0f) normalizedWorldYaw -= 360.0f;
            sector = (int)(normalizedWorldYaw / 45.0f) % 8;
        }
        else {
            sector = 0;
        }
    }
    else {
        float normalizedYaw = localYawDeg + 22.5f;
        if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;
        sector = (int)(normalizedYaw / 45.0f);
    }
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
        int chosenRow = (localPitchDeg >= 22.5f) ? 2 : (localPitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[chosenRow][sector];
        *outRotation = 0.0f;
        *outMirrored = !(sector >= 5 && sector <= 7);
    }
}

bool ShouldRenderHead(const Person* person) {
    if (!person || !person->active) return false;

    int facialPoints = 0;
    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        char firstChar = bone->name[0];
        if ((firstChar == 'N' && strcmp(bone->name, "Nose") == 0) ||
            ((firstChar == 'L' || firstChar == 'R') &&
                (strstr(bone->name, "Eye") || strstr(bone->name, "Ear")))) {
            facialPoints++;
        }
    }

    return facialPoints >= 2;
}

void DrawHeadBillboard(Texture2D texture, Camera camera, const HeadRenderData* headData,
    int physCols, int physRows) {
    if (!headData || !headData->valid || !headData->visible) return;

    int chosenIndex;
    float rotation;
    bool mirrored;

    CalculateHeadRenderData(headData, camera, &chosenIndex, &rotation, &mirrored);

    int logicalCol = chosenIndex % ATLAS_COLS;
    int logicalRow = chosenIndex / ATLAS_COLS;

    bool finalMirror;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows, mirrored, &finalMirror);
    Vector2 worldSize = { headData->size, headData->size };

    DrawBonetileCustom(texture, camera, src, headData->position, worldSize, rotation, finalMirror, "Head");
}


TextureSetCollection* BonesTextureSets_Create(void) {
    TextureSetCollection* collection;
    collection = (TextureSetCollection*)calloc(1, sizeof(TextureSetCollection));
    if (!collection) return NULL;
    collection->loaded = false;
    collection->setCount = 0;
    return collection;
}

void BonesTextureSets_Free(TextureSetCollection* collection) {
    if (collection) free(collection);
}

BoneTextureSet* BonesTextureSets_FindSet(TextureSetCollection* collection, const char* boneName) {
    int i;
    if (!collection || !boneName) return NULL;
    for (i = 0; i < collection->setCount; i++) {
        if (strcmp(collection->sets[i].boneName, boneName) == 0) {
            return &collection->sets[i];
        }
    }
    return NULL;
}

bool BonesTextureSets_LoadFromFile(TextureSetCollection* collection, const char* filePath) {
    FILE* file;
    char line[512];
    int lineNum;
    char boneName[BONES_AE_MAX_NAME];
    char variantName[BONES_AE_MAX_NAME];
    char texturePath[BONES_AE_MAX_PATH];
    BoneTextureSet* set;
    BoneTextureVariant* variant;
    
    if (!collection || !filePath) return false;
    
    file = fopen(filePath, "r");
    if (!file) {
        printf("TEXTURE_SETS: Failed to open %s\n", filePath);
        return false;
    }
    
    collection->setCount = 0;
    collection->loaded = false;
    lineNum = 0;
    
    while (fgets(line, sizeof(line), file) && collection->setCount < BONES_MAX_TEXTURE_SETS) {
        lineNum++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        if (sscanf(line, "%63s %63s %255s", boneName, variantName, texturePath) != 3) {
            printf("TEXTURE_SETS: Invalid line %d\n", lineNum);
            continue;
        }
        
        set = BonesTextureSets_FindSet(collection, boneName);
        
        if (!set) {
            if (collection->setCount >= BONES_MAX_TEXTURE_SETS) break;
            set = &collection->sets[collection->setCount];
            memset(set, 0, sizeof(BoneTextureSet));
            strncpy(set->boneName, boneName, BONES_AE_MAX_NAME - 1);
            set->boneName[BONES_AE_MAX_NAME - 1] = '\0';
            set->variantCount = 0;
            set->activeVariantIndex = 0;
            set->valid = true;
            collection->setCount++;
        }
        
        if (set->variantCount >= BONES_MAX_TEXTURE_VARIANTS) continue;
        
        variant = &set->variants[set->variantCount];
        strncpy(variant->variantName, variantName, BONES_AE_MAX_NAME - 1);
        variant->variantName[BONES_AE_MAX_NAME - 1] = '\0';
        strncpy(variant->texturePath, texturePath, BONES_AE_MAX_PATH - 1);
        variant->texturePath[BONES_AE_MAX_PATH - 1] = '\0';
        variant->valid = true;
        set->variantCount++;
        
        printf("TEXTURE_SETS: %s.%s -> %s\n", boneName, variantName, texturePath);
    }
    
    fclose(file);
    
    if (collection->setCount > 0) {
        collection->loaded = true;
        printf("TEXTURE_SETS: Loaded %d sets\n", collection->setCount);
        return true;
    }
    return false;
}

const char* BonesTextureSets_GetActiveTexture(const TextureSetCollection* collection, const char* boneName) {
    int i;
    const BoneTextureSet* set;
    if (!collection || !boneName || !collection->loaded) return NULL;
    for (i = 0; i < collection->setCount; i++) {
        set = &collection->sets[i];
        if (strcmp(set->boneName, boneName) == 0 && set->valid) {
            if (set->activeVariantIndex >= 0 && set->activeVariantIndex < set->variantCount) {
                return set->variants[set->activeVariantIndex].texturePath;
            }
        }
    }
    return NULL;
}

bool BonesTextureSets_SetVariant(TextureSetCollection* collection, const char* boneName, const char* variantName) {
    BoneTextureSet* set;
    int i;
    if (!collection || !boneName || !variantName) return false;
    set = BonesTextureSets_FindSet(collection, boneName);
    if (!set) return false;
    for (i = 0; i < set->variantCount; i++) {
        if (strcmp(set->variants[i].variantName, variantName) == 0) {
            set->activeVariantIndex = i;
            printf("TEXTURE_SETS: %s -> %s (%s)\n", boneName, variantName, set->variants[i].texturePath);
            return true;
        }
    }
    return false;
}

const char* BonesTextureSets_GetActiveVariantName(const TextureSetCollection* collection, const char* boneName) {
    int i;
    const BoneTextureSet* set;
    if (!collection || !boneName || !collection->loaded) return NULL;
    for (i = 0; i < collection->setCount; i++) {
        set = &collection->sets[i];
        if (strcmp(set->boneName, boneName) == 0 && set->valid) {
            if (set->activeVariantIndex >= 0 && set->activeVariantIndex < set->variantCount) {
                return set->variants[set->activeVariantIndex].variantName;
            }
        }
    }
    return NULL;
}

void BonesTextureSets_ResetAll(TextureSetCollection* collection) {
    int i;
    if (!collection) return;
    for (i = 0; i < collection->setCount; i++) {
        collection->sets[i].activeVariantIndex = 0;
    }
}

/* ===== ANIMATION CONTROLLER IMPLEMENTATION ===== */

AnimationController* AnimController_Create(void* bonesAnimation, TextureSetCollection* textureSets) {
    AnimationController* controller;
    controller = (AnimationController*)calloc(1, sizeof(AnimationController));
    if (!controller) return NULL;
    
    controller->bonesAnimation = bonesAnimation;
    controller->textureSets = textureSets;
    controller->clipCount = 0;
    controller->currentClipIndex = -1;
    controller->localTime = 0.0f;
    controller->playing = false;
    controller->currentFrameInJSON = 0;
    controller->valid = true;
    
    return controller;
}

void AnimController_Free(AnimationController* controller) {
    if (controller) free(controller);
}

/* Helper: Parse simple JSON value */
static bool ParseJSONFloat(const char* json, const char* key, float* outValue) {
    char searchKey[128];
    const char* pos;
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    pos = strstr(json, searchKey);
    if (pos && sscanf(pos + strlen(searchKey), "%f", outValue) == 1) {
        return true;
    }
    return false;
}

static bool ParseJSONInt(const char* json, const char* key, int* outValue) {
    char searchKey[128];
    const char* pos;
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    pos = strstr(json, searchKey);
    if (pos && sscanf(pos + strlen(searchKey), "%d", outValue) == 1) {
        return true;
    }
    return false;
}

static bool ParseJSONString(const char* json, const char* key, char* outValue, int maxLen) {
    char searchKey[128];
    const char* pos;
    const char* start;
    const char* end;
    int len;
    
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    pos = strstr(json, searchKey);
    if (!pos) return false;
    
    start = strchr(pos + strlen(searchKey), '"');
    if (!start) return false;
    start++;
    
    end = strchr(start, '"');
    if (!end) return false;
    
    len = (int)(end - start);
    if (len >= maxLen) len = maxLen - 1;
    
    strncpy(outValue, start, len);
    outValue[len] = '\0';
    return true;
}

bool AnimController_LoadClipMetadata(AnimationController* controller, const char* jsonPath) {
    FILE* file;
    char* jsonData;
    long fileSize;
    AnimationClipMetadata* clip;
    const char* eventsStart;
    const char* eventPos;
    AnimationEvent* event;
    float eventTime;
    char eventType[32];
    char boneName[BONES_AE_MAX_NAME];
    char variantName[BONES_AE_MAX_NAME];
    char personId[BONES_AE_MAX_NAME];
    bool loopValue;
    
    if (!controller || !jsonPath) return false;
    
    file = fopen(jsonPath, "rb");
    if (!file) {
        printf("ANIM_METADATA: Failed to open %s\n", jsonPath);
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    jsonData = (char*)malloc(fileSize + 1);
    if (!jsonData) {
        fclose(file);
        return false;
    }
    
    fread(jsonData, 1, fileSize, file);
    jsonData[fileSize] = '\0';
    fclose(file);
    
    if (controller->clipCount >= BONES_MAX_ANIM_CLIPS) {
        free(jsonData);
        return false;
    }
    
    clip = &controller->clips[controller->clipCount];
    memset(clip, 0, sizeof(AnimationClipMetadata));
    
    /* Parse basic metadata */
    if (!ParseJSONString(jsonData, "name", clip->name, BONES_AE_MAX_NAME)) {
        strcpy(clip->name, "unnamed");
    }
    
    if (!ParseJSONFloat(jsonData, "fps", &clip->fps)) {
        clip->fps = 30.0f;
    }
    
    if (!ParseJSONInt(jsonData, "start_frame", &clip->startFrame)) {
        clip->startFrame = 0;
    }
    
    if (!ParseJSONInt(jsonData, "end_frame", &clip->endFrame)) {
        clip->endFrame = 60;
    }
    
    /* Parse loop (buscar true/false) */
    loopValue = false;
    if (strstr(jsonData, "\"loop\": true") || strstr(jsonData, "\"loop\":true")) {
        loopValue = true;
    }
    clip->loop = loopValue;
    
    /* Parse events array */
    clip->eventCount = 0;
    eventsStart = strstr(jsonData, "\"events\":");
    if (eventsStart) {
        eventsStart = strchr(eventsStart, '[');
        if (eventsStart) {
            eventPos = eventsStart;
            while (clip->eventCount < BONES_MAX_ANIM_EVENTS) {
                eventPos = strchr(eventPos + 1, '{');
                if (!eventPos || eventPos > strstr(eventsStart, "]")) break;
                
                event = &clip->events[clip->eventCount];
                memset(event, 0, sizeof(AnimationEvent));
                
                /* Parse event fields */
                if (!ParseJSONFloat(eventPos, "time", &eventTime)) continue;
                event->time = eventTime;
                
                if (!ParseJSONString(eventPos, "type", eventType, sizeof(eventType))) continue;
                
                if (strcmp(eventType, "texture") == 0) {
                    event->type = ANIM_EVENT_TEXTURE;
                } else if (strcmp(eventType, "sound") == 0) {
                    event->type = ANIM_EVENT_SOUND;
                } else {
                    event->type = ANIM_EVENT_CUSTOM;
                }
                
                ParseJSONString(eventPos, "bone", boneName, BONES_AE_MAX_NAME);
                strncpy(event->boneName, boneName, BONES_AE_MAX_NAME - 1);
                
                ParseJSONString(eventPos, "variant", variantName, BONES_AE_MAX_NAME);
                strncpy(event->variantName, variantName, BONES_AE_MAX_NAME - 1);
                
                if (ParseJSONString(eventPos, "person", personId, BONES_AE_MAX_NAME)) {
                    strncpy(event->personId, personId, BONES_AE_MAX_NAME - 1);
                }
                
                event->processed = false;
                event->valid = true;
                
                printf("ANIM_EVENT: t=%.2fs %s.%s -> %s\n", 
                       event->time, event->boneName, event->variantName, eventType);
                
                clip->eventCount++;
            }
        }
    }
    
    clip->valid = true;
    controller->clipCount++;
    
    printf("ANIM_METADATA: Loaded '%s' (fps=%.1f, frames=%d-%d, events=%d)\n",
           clip->name, clip->fps, clip->startFrame, clip->endFrame, clip->eventCount);
    
    free(jsonData);
    return true;
}

bool AnimController_PlayClip(AnimationController* controller, const char* clipName) {
    int i;
    if (!controller || !clipName) return false;
    
    for (i = 0; i < controller->clipCount; i++) {
        if (strcmp(controller->clips[i].name, clipName) == 0) {
            controller->currentClipIndex = i;
            controller->localTime = 0.0f;
            controller->playing = true;
            controller->currentFrameInJSON = controller->clips[i].startFrame;
            
            /* Reset event processed flags */
            {
                int j;
                for (j = 0; j < controller->clips[i].eventCount; j++) {
                    controller->clips[i].events[j].processed = false;
                }
            }
            
            printf("ANIM_CONTROLLER: Playing '%s'\n", clipName);
            return true;
        }
    }
    return false;
}

void AnimController_Pause(AnimationController* controller) {
    if (controller) controller->playing = false;
}

void AnimController_Resume(AnimationController* controller) {
    if (controller) controller->playing = true;
}

int AnimController_GetCurrentFrame(const AnimationController* controller) {
    if (!controller || controller->currentClipIndex < 0) return 0;
    return controller->currentFrameInJSON;
}

void AnimController_Update(AnimationController* controller, float deltaTime) {
    AnimationClipMetadata* clip;
    float clipDuration;
    int totalFrames;
    int i;
    AnimationEvent* event;
    
    if (!controller || !controller->playing || controller->currentClipIndex < 0) return;
    
    clip = &controller->clips[controller->currentClipIndex];
    if (!clip->valid) return;
    
    /* Advance time */
    controller->localTime += deltaTime;
    
    /* Calculate current frame */
    totalFrames = clip->endFrame - clip->startFrame + 1;
    clipDuration = (float)totalFrames / clip->fps;
    
    /* Loop handling */
    if (controller->localTime >= clipDuration) {
        if (clip->loop) {
            controller->localTime = fmodf(controller->localTime, clipDuration);
            /* Reset events for next loop */
            for (i = 0; i < clip->eventCount; i++) {
                clip->events[i].processed = false;
            }
        } else {
            controller->localTime = clipDuration;
            controller->playing = false;
        }
    }
    
    /* Calculate frame index */
    controller->currentFrameInJSON = clip->startFrame + 
        (int)(controller->localTime * clip->fps);
    
    if (controller->currentFrameInJSON > clip->endFrame) {
        controller->currentFrameInJSON = clip->endFrame;
    }
    
    /* Process events */
    for (i = 0; i < clip->eventCount; i++) {
        event = &clip->events[i];
        if (!event->valid || event->processed) continue;
        
        if (controller->localTime >= event->time) {
            /* Trigger event */
            if (event->type == ANIM_EVENT_TEXTURE && controller->textureSets) {
                BonesTextureSets_SetVariant(controller->textureSets, 
                                           event->boneName, 
                                           event->variantName);
            }
            
            event->processed = true;
        }
    }
}
