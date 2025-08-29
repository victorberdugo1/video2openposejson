// torso_billboard.c - Sistema de torsos con orientación real basada en OpenPose
#include "torso_billboard.h"
#include "bonetile.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

Vector3 CalculateChestPosition(const Person* person) {
    if (!person || person->boneCount == 0) {
        return (Vector3) { 0, 0, 0 };
    }

    // Buscar cuello y hombros
    Vector3 neckPos = { 0, 0, 0 };
    Vector3 shoulderCenter = { 0, 0, 0 };
    bool hasNeck = false;
    int shoulderCount = 0;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        if (strcmp(bone->name, "Neck") == 0) {
            neckPos = bone->position.position;
            hasNeck = true;
        }
        else if (strcmp(bone->name, "LShoulder") == 0 || strcmp(bone->name, "RShoulder") == 0) {
            shoulderCenter = Vector3Add(shoulderCenter, bone->position.position);
            shoulderCount++;
        }
    }

    if (hasNeck && shoulderCount > 0) {
        // Promedio de hombros
        shoulderCenter = Vector3Scale(shoulderCenter, 1.0f / shoulderCount);

        // Pecho: desde el cuello hacia abajo, en dirección a los hombros
        // Aproximadamente 10-15cm hacia abajo del cuello
        Vector3 chestPos = neckPos;
        chestPos.y -= 0.06f; // 12cm hacia abajo del cuello

        // Ligeramente hacia adelante (pecho sale hacia adelante)
        chestPos.z += 0.015f; // 3cm hacia adelante

        // Centrar horizontalmente con los hombros
        chestPos.x = (neckPos.x + shoulderCenter.x) * 0.5f;

        return chestPos;
    }

    // Fallback al método original si no hay suficientes puntos
    Vector3 totalPos = { 0, 0, 0 };
    int pointCount = 0;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        if (strcmp(bone->name, "LShoulder") == 0 ||
            strcmp(bone->name, "RShoulder") == 0 ||
            strcmp(bone->name, "Neck") == 0) {

            totalPos = Vector3Add(totalPos, bone->position.position);
            pointCount++;
        }
    }

    if (pointCount > 0) {
        Vector3 chestPos = Vector3Scale(totalPos, 1.0f / pointCount);
        chestPos.y -= 0.08f; // Bajar un poco desde el promedio
        return chestPos;
    }

    return (Vector3) { 0, 0, 0 };
}

Vector3 CalculateHipPosition(const Person* person) {
    if (!person || person->boneCount == 0) {
        return (Vector3) { 0, 0, 0 };
    }

    Vector3 totalPos = { 0, 0, 0 };
    int pointCount = 0;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        if (strcmp(bone->name, "LHip") == 0 ||
            strcmp(bone->name, "RHip") == 0) {

            totalPos = Vector3Add(totalPos, bone->position.position);
            pointCount++;
        }
    }

    if (pointCount > 0) {
        return Vector3Scale(totalPos, 1.0f / pointCount);
    }

    return (Vector3) { 0, 0, 0 };
}

VirtualSpine CalculateVirtualSpine(const Person* person) {
    VirtualSpine spine = { 0 };
    spine.valid = false;

    if (!person || person->boneCount == 0) return spine;

    bool hasLShoulder = false, hasRShoulder = false, hasNeck = false;
    bool hasLHip = false, hasRHip = false;
    Vector3 lShoulder = { 0 }, rShoulder = { 0 }, neck = { 0 };
    Vector3 lHip = { 0 }, rHip = { 0 };

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        if (strcmp(bone->name, "LShoulder") == 0) {
            lShoulder = bone->position.position; hasLShoulder = true;
        }
        else if (strcmp(bone->name, "RShoulder") == 0) {
            rShoulder = bone->position.position; hasRShoulder = true;
        }
        else if (strcmp(bone->name, "Neck") == 0) {
            neck = bone->position.position; hasNeck = true;
        }
        else if (strcmp(bone->name, "LHip") == 0) {
            lHip = bone->position.position; hasLHip = true;
        }
        else if (strcmp(bone->name, "RHip") == 0) {
            rHip = bone->position.position; hasRHip = true;
        }
    }

    if ((!hasLShoulder || !hasRShoulder) || (!hasLHip || !hasRHip)) {
        return spine;
    }

    spine.chestPosition = Vector3Scale(Vector3Add(lShoulder, rShoulder), 0.5f);
    spine.hipPosition = Vector3Scale(Vector3Add(lHip, rHip), 0.5f);

    if (hasNeck) {
        spine.chestPosition = Vector3Scale(
            Vector3Add(Vector3Add(lShoulder, rShoulder), neck),
            1.0f / 3.0f
        );
    }

    Vector3 spineVec = Vector3Subtract(spine.chestPosition, spine.hipPosition);
    float spineLength = Vector3Length(spineVec);

    if (spineLength < 1e-4f) {
        return spine;
    }

    spine.spineDirection = Vector3Scale(spineVec, 1.0f / spineLength);

    Vector3 shoulderLine = Vector3Subtract(rShoulder, lShoulder);
    float shoulderLength = Vector3Length(shoulderLine);

    if (shoulderLength < 1e-4f) {
        return spine;
    }

    spine.spineRight = Vector3Scale(shoulderLine, 1.0f / shoulderLength);
    spine.spineForward = Vector3CrossProduct(spine.spineRight, spine.spineDirection);
    float forwardLength = Vector3Length(spine.spineForward);

    if (forwardLength < 1e-6f) {
        return spine;
    }

    spine.spineForward = Vector3Scale(spine.spineForward, 1.0f / forwardLength);
    spine.spineRight = Vector3CrossProduct(spine.spineDirection, spine.spineForward);
    float rightLength = Vector3Length(spine.spineRight);

    if (rightLength > 1e-6f) {
        spine.spineRight = Vector3Scale(spine.spineRight, 1.0f / rightLength);
    }

    spine.valid = true;
    return spine;
}

TorsoOrientation CalculateChestOrientation(const Person* person) {
    TorsoOrientation orientation = { 0 };
    orientation.valid = false;

    VirtualSpine spine = CalculateVirtualSpine(person);
    if (!spine.valid) {
        Vector3 chestPos = CalculateChestPosition(person);
        if (Vector3Length(chestPos) > 0.0f) {
            orientation.position = chestPos;
            orientation.forward = (Vector3){ 0, 0, 1 };
            orientation.up = (Vector3){ 0, 1, 0 };
            orientation.right = (Vector3){ 1, 0, 0 };
            orientation.valid = true;
        }
        return orientation;
    }

    orientation.position = spine.chestPosition;
    orientation.forward = spine.spineForward;
    orientation.up = spine.spineDirection;
    orientation.right = spine.spineRight;

    orientation.yaw = atan2f(orientation.forward.x, orientation.forward.z);

    float horizDistance = sqrtf(orientation.forward.x * orientation.forward.x +
        orientation.forward.z * orientation.forward.z);
    orientation.pitch = atan2f(-orientation.forward.y, horizDistance);

    orientation.roll = atan2f(orientation.right.y,
        sqrtf(orientation.right.x * orientation.right.x +
            orientation.right.z * orientation.right.z));

    orientation.valid = true;
    return orientation;
}

TorsoOrientation CalculateHipOrientation(const Person* person) {
    TorsoOrientation orientation = { 0 };
    orientation.valid = false;

    VirtualSpine spine = CalculateVirtualSpine(person);
    if (!spine.valid) {
        Vector3 hipPos = CalculateHipPosition(person);
        if (Vector3Length(hipPos) > 0.0f) {
            orientation.position = hipPos;
            orientation.forward = (Vector3){ 0, 0, 1 };
            orientation.up = (Vector3){ 0, 1, 0 };
            orientation.right = (Vector3){ 1, 0, 0 };
            orientation.valid = true;
        }
        return orientation;
    }

    orientation.position = spine.hipPosition;
    orientation.forward = spine.spineForward;
    orientation.up = spine.spineDirection;
    orientation.right = spine.spineRight;

    orientation.yaw = atan2f(orientation.forward.x, orientation.forward.z);

    float horizDistance = sqrtf(orientation.forward.x * orientation.forward.x +
        orientation.forward.z * orientation.forward.z);
    orientation.pitch = atan2f(-orientation.forward.y, horizDistance);

    orientation.roll = atan2f(orientation.right.y,
        sqrtf(orientation.right.x * orientation.right.x +
            orientation.right.z * orientation.right.z));

    orientation.valid = true;
    return orientation;
}

void CalculateTorsoRenderData(const TorsoRenderData* torsoData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!torsoData->orientation.valid) {
        CalculateBoneRenderData(torsoData->position, camera, outChosenIndex, outRotation, outMirrored);
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

    Vector3 camDir = Vector3Subtract(camera.position, torsoData->position);

    Vector3 localCamDir;
    localCamDir.x = Vector3DotProduct(camDir, torsoData->orientation.right);
    localCamDir.y = Vector3DotProduct(camDir, torsoData->orientation.up);
    localCamDir.z = Vector3DotProduct(camDir, torsoData->orientation.forward);

    localCamDir.x = -localCamDir.x;

    float localYaw = atan2f(-localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localYawDeg = localYaw * RAD2DEG;

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
    float normalizedYaw = localYawDeg + 22.5f + 180.0f;
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

bool ShouldRenderChest(const Person* person) {
    if (!person || !person->active) return false;

    int shoulderCount = 0;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        if (strcmp(bone->name, "LShoulder") == 0 || strcmp(bone->name, "RShoulder") == 0) {
            shoulderCount++;
        }
    }

    return shoulderCount >= 1;
}

bool ShouldRenderHip(const Person* person) {
    if (!person || !person->active) return false;

    int hipCount = 0;
    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        if (strcmp(bone->name, "LHip") == 0 || strcmp(bone->name, "RHip") == 0) {
            hipCount++;
        }
    }

    return hipCount >= 1;
}

void DrawTorsoBillboard(Texture2D texture, Camera camera, const TorsoRenderData* torsoData, int physCols, int physRows) {
    if (!torsoData || !torsoData->valid || !torsoData->visible) return;

    int chosenIndex;
    float rotation;
    bool mirrored;

    CalculateTorsoRenderData(torsoData, camera, &chosenIndex, &rotation, &mirrored);

    int logicalCol = chosenIndex % ATLAS_COLS;
    int logicalRow = chosenIndex / ATLAS_COLS;

    bool finalMirror = false;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows, mirrored, &finalMirror);

    Vector2 worldSize = (Vector2){ torsoData->size, torsoData->size };
    DrawBonetileCustom(texture, camera, src, torsoData->position, worldSize, rotation, finalMirror);
}

void CollectTorsosForRendering(const BonesAnimation* animation, TorsoRenderData** torsos,
    int* torsoCount, int* torsoCapacity, BoneConfig* boneConfigs,
    int boneConfigCount) {
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