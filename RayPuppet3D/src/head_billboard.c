#include "head_billboard.h"
#include "bonetile.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static const float HEAD_DEPTH_OFFSET = 0.04f;
static const float CHEST_OFFSET_Y = -0.06f;
static const float CHEST_OFFSET_Z = -0.005f;
static const float CHEST_FALLBACK_Y = -0.08f;
static const float HIP_OFFSET_Y = -0.02f;

/*
 * +------------------------------------------------------------------+
 * | Function: CacheBones (static)                                    |
 * |                                                                  |
 * | Extract and cache relevant bone positions from a Person for      |
 * | efficient access during head/torso calculations. Returns a       |
 * | CachedBones struct with position data and availability flags.    |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: CachedBones struct with cached positions               |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateChestPosition                                 |
 * |                                                                  |
 * | Calculate chest position from neck and shoulder bone positions.  |
 * | Uses cached bone data and applies anatomical offsets to place    |
 * | the chest at a realistic position between neck and shoulders.    |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: Vector3 chest position or (0,0,0) if invalid           |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateHipPosition                                   |
 * |                                                                  |
 * | Calculate hip center position from left/right hip bones and      |
 * | optional torso reference points. Applies anatomical offsets and  |
 * | maintains proper torso proportions relative to chest position.   |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: Vector3 hip position or (0,0,0) if invalid             |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateVirtualSpine                                  |
 * |                                                                  |
 * | Compute a virtual spine structure from shoulder and hip          |
 * | positions. Creates spine direction, forward, and right vectors   |
 * | for proper torso orientation calculations.                       |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: VirtualSpine struct with spine orientation data        |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CreateOrientation (static)                             |
 * |                                                                  |
 * | Helper function to create a TorsoOrientation struct from         |
 * | position and directional vectors. Computes yaw, pitch, and roll  |
 * | angles from the provided forward/up/right vectors.               |
 * |                                                                  |
 * | - Input: Vector3 pos, forward, up, right                         |
 * | - Output: TorsoOrientation with computed angles                  |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateChestOrientation                              |
 * |                                                                  |
 * | Compute chest orientation using virtual spine calculation.       |
 * | Returns a TorsoOrientation with forward/up/right vectors and     |
 * | euler angles for proper chest billboard rendering.               |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: TorsoOrientation for chest positioning                 |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateHipOrientation                                |
 * |                                                                  |
 * | Compute hip orientation using virtual spine calculation.         |
 * | Returns a TorsoOrientation with forward/up/right vectors and     |
 * | euler angles for proper hip billboard rendering.                 |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: TorsoOrientation for hip positioning                   |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateTorsoRenderData                               |
 * |                                                                  |
 * | Determine atlas index, rotation and mirroring for torso          |
 * | billboard rendering based on camera angle relative to torso      |
 * | orientation. Handles special rotations for chest-to-hip and      |
 * | hip-to-chest directional rendering.                              |
 * |                                                                  |
 * | - Input: TorsoRenderData*, Camera, out params                    |
 * | - Output: fills outChosenIndex, outRotation, outMirrored         |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: ShouldRenderChest                                      |
 * |                                                                  |
 * | Determine if a chest billboard should be rendered for this       |
 * | person based on availability of shoulder bones.                  |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: bool (true if chest should be rendered)                |
 * +------------------------------------------------------------------+
 */
bool ShouldRenderChest(const Person* person) {
    if (!person || !person->active) return false;
    return CacheBones(person).shoulderCount >= 1;
}

/*
 * +------------------------------------------------------------------+
 * | Function: ShouldRenderHip                                        |
 * |                                                                  |
 * | Determine if a hip billboard should be rendered for this         |
 * | person based on availability of hip bones.                       |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: bool (true if hip should be rendered)                  |
 * +------------------------------------------------------------------+
 */
bool ShouldRenderHip(const Person* person) {
    if (!person || !person->active) return false;
    return CacheBones(person).hipCount >= 1;
}

/*
 * +------------------------------------------------------------------+
 * | Function: DrawTorsoBillboard                                     |
 * |                                                                  |
 * | Render a single torso billboard (chest or hip) using the         |
 * | calculated atlas position, rotation and mirroring. Handles       |
 * | camera-facing orientation and applies torso-specific rendering   |
 * | adjustments for front/back sector detection.                     |
 * |                                                                  |
 * | - Input: Texture2D, Camera, TorsoRenderData*, physCols/Rows      |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CollectTorsosForRendering                              |
 * |                                                                  |
 * | Collect all torso render data (chest/hip) from current frame     |
 * | into render arrays. Prevents duplicate processing, applies       |
 * | configuration settings, and prepares data for depth-sorted       |
 * | billboard rendering.                                             |
 * |                                                                  |
 * | - Input: animation, torsos array, configs                        |
 * | - Output: fills torsos array with render data                    |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateHeadPosition                                  |
 * |                                                                  |
 * | Calculate head position from facial features (eyes, nose, ears)  |
 * | and neck position. Uses weighted averaging of face center with   |
 * | depth offset for realistic head placement relative to neck.      |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: Vector3 head position or (0,0,0) if invalid            |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateHeadOrientation                               |
 * |                                                                  |
 * | Compute head orientation from facial landmark positions.         |
 * | Uses nose-to-ear/eye vectors to determine forward direction      |
 * | and eye-line for right vector, creating proper head billboard    |
 * | orientation for camera-relative rendering.                       |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: HeadOrientation with forward/up/right vectors          |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CalculateHeadRenderData                                |
 * |                                                                  |
 * | Determine atlas index, rotation and mirroring for head           |
 * | billboard based on camera angle relative to head orientation.    |
 * | Handles extreme pitch angles and uses world coordinates for      |
 * | top/bottom views to maintain visual consistency.                 |
 * |                                                                  |
 * | - Input: HeadRenderData*, Camera, out params                     |
 * | - Output: fills outChosenIndex, outRotation, outMirrored         |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: ShouldRenderHead                                       |
 * |                                                                  |
 * | Determine if a head billboard should be rendered for this        |
 * | person based on availability of facial feature bones (need       |
 * | at least 2 facial points: nose, eyes, or ears).                  |
 * |                                                                  |
 * | - Input: const Person* person                                    |
 * | - Output: bool (true if head should be rendered)                 |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: DrawHeadBillboard                                      |
 * |                                                                  |
 * | Render a single head billboard using calculated atlas position,  |
 * | rotation and mirroring. Applies camera-facing orientation and    |
 * | uses head-specific texture atlas coordinates for rendering.      |
 * |                                                                  |
 * | - Input: Texture2D, Camera, HeadRenderData*, physCols/Rows       |
 * +------------------------------------------------------------------+
 */
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

/*
 * +------------------------------------------------------------------+
 * | Function: CollectHeadsForRendering                               |
 * |                                                                  |
 * | Collect all head render data from current animation frame into   |
 * | render arrays. Prevents duplicate processing per person,         |
 * | applies head configuration settings, and prepares data for       |
 * | depth-sorted billboard rendering pipeline.                       |
 * |                                                                  |
 * | - Input: animation, heads array, configs                         |
 * | - Output: fills heads array with head render data                |
 * +------------------------------------------------------------------+
 */
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

        BoneConfig* headConfig = FindBoneConfig(boneConfigs, boneConfigCount, "Head");
        if (headConfig) {
            strncpy(headData->texturePath, headConfig->texturePath, MAX_FILE_PATH_LENGTH - 1);
            headData->size = headConfig->size;
            headData->visible = headConfig->visible;
        }
        else {
            strncpy(headData->texturePath, "tex/Head1.png", MAX_FILE_PATH_LENGTH - 1);
            headData->size = 0.25f;
            headData->visible = true;
        }
        headData->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

        strncpy(headData->personId, person->personId, 15);
        headData->personId[15] = '\0';

        (*headCount)++;
    }
}