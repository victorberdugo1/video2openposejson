#include "head_billboard.h"
#include "bonetile.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

Vector3 CalculateHeadPosition(const Person* person) {
    if (!person || person->boneCount == 0) {
        return (Vector3) { 0, 0, 0 };
    }

    Vector3 eyeCenter = { 0, 0, 0 };
    int eyeCount = 0;
    Vector3 neckPos = { 0, 0, 0 };
    bool hasNeck = false;
    Vector3 nosePos = { 0, 0, 0 };
    bool hasNose = false;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        // Solo ojos para referencia vertical
        if (strcmp(bone->name, "LEye") == 0 || strcmp(bone->name, "REye") == 0) {
            eyeCenter = Vector3Add(eyeCenter, bone->position.position);
            eyeCount++;
        }
        // Nariz para referencia frontal (profundidad)
        else if (strcmp(bone->name, "Nose") == 0) {
            nosePos = bone->position.position;
            hasNose = true;
        }
        // Cuello como base
        else if (strcmp(bone->name, "Neck") == 0) {
            neckPos = bone->position.position;
            hasNeck = true;
        }
    }

    // Cálculo anatómicamente correcto
    if (eyeCount > 0 && hasNeck) {
        eyeCenter = Vector3Scale(eyeCenter, 1.0f / eyeCount);

        Vector3 headCenter;

        // X: Promedio entre cuello y ojos, pero más cerca del cuello
        headCenter.x = neckPos.x * 0.7f + eyeCenter.x * 0.3f;

        // Y: Mantener altura de ojos (la cabeza está a esa altura)
        headCenter.y = eyeCenter.y;

        // Z: Atrás de la nariz, cerca del cuello (centro de masa del cráneo)
        if (hasNose) {
            // La cabeza está significativamente atrás de la cara
            headCenter.z = neckPos.z * 0.8f + nosePos.z * 0.2f;  // MÁS hacia atrás
        }
        else {
            // Sin nariz, usar solo referencia de cuello y ojos
            headCenter.z = neckPos.z * 0.9f + eyeCenter.z * 0.1f; // MÁS hacia atrás
        }

        return headCenter;
    }

    // Fallbacks progresivos
    if (eyeCount > 0) {
        eyeCenter = Vector3Scale(eyeCenter, 1.0f / eyeCount);

        // Sin cuello, estimar posición hacia atrás desde los ojos
        Vector3 estimatedHead = eyeCenter;
        if (hasNose) {
            // Mover hacia atrás desde la línea nariz-ojos
            Vector3 backOffset = Vector3Subtract(eyeCenter, nosePos);
            backOffset = Vector3Scale(backOffset, 0.5f); // 50% hacia atrás
            estimatedHead = Vector3Add(eyeCenter, backOffset);
        }
        return estimatedHead;
    }

    if (hasNeck) {
        // Solo cuello disponible, estimar cabeza arriba y adelante
        Vector3 estimatedHead = neckPos;
        estimatedHead.y += 0.15f; // Aproximadamente 15cm arriba del cuello
        return estimatedHead;
    }

    return (Vector3) { 0, 0, 0 };
}

HeadOrientation CalculateHeadOrientation(const Person* person) {
    HeadOrientation orientation = { 0 };
    orientation.valid = false;

    if (!person || person->boneCount == 0) return orientation;

    bool hasNose = false, hasLEye = false, hasREye = false, hasLEar = false, hasREar = false;
    Vector3 nose = { 0,0,0 }, lEye = { 0,0,0 }, rEye = { 0,0,0 }, lEar = { 0,0,0 }, rEar = { 0,0,0 };

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
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

    if (!hasNose || !((hasLEye && hasREye) || (hasLEar && hasREar))) {
        Vector3 centerFallback = CalculateHeadPosition(person);
        if (Vector3Length(centerFallback) > 0.0f) {
            orientation.position = centerFallback;
            orientation.valid = true;
        }
        return orientation;
    }

    // Calculate central position
    Vector3 totalPos = nose;
    int pointCount = 1;
    if (hasLEye) { totalPos = Vector3Add(totalPos, lEye); pointCount++; }
    if (hasREye) { totalPos = Vector3Add(totalPos, rEye); pointCount++; }
    if (hasLEar) { totalPos = Vector3Add(totalPos, lEar); pointCount++; }
    if (hasREar) { totalPos = Vector3Add(totalPos, rEar); pointCount++; }
    Vector3 center = Vector3Scale(totalPos, 1.0f / pointCount);
    orientation.position = center;

    // Calculate right vector
    Vector3 rightVec = (Vector3){ 1,0,0 };
    if (hasLEar && hasREar) {
        rightVec = Vector3Normalize(Vector3Subtract(rEar, lEar));
    }
    else if (hasLEye && hasREye) {
        rightVec = Vector3Normalize(Vector3Subtract(rEye, lEye));
    }

    // Calculate back reference
    Vector3 backRef;
    if (hasLEar && hasREar) {
        backRef = Vector3Scale(Vector3Add(lEar, rEar), 0.5f);
    }
    else if (hasLEye && hasREye) {
        backRef = Vector3Scale(Vector3Add(lEye, rEye), 0.5f);
    }
    else {
        backRef = center;
    }

    // Calculate forward vector
    Vector3 forward = Vector3Normalize(Vector3Subtract(nose, backRef));
    if (Vector3Length(forward) < 1e-6f) {
        forward = (Vector3){ 0,0,1 };
    }

    // Calculate up vector
    Vector3 up = Vector3Normalize(Vector3CrossProduct(rightVec, forward));
    if (Vector3Length(up) < 1e-6f) {
        up = (Vector3){ 0,1,0 };
    }

    // Re-orthonormalize right
    rightVec = Vector3Normalize(Vector3CrossProduct(forward, up));

    orientation.forward = forward;
    orientation.up = up;
    orientation.right = rightVec;

    // Calculate Euler angles
    orientation.yaw = atan2f(forward.x, forward.z);
    orientation.pitch = atan2f(-forward.y, sqrtf(forward.x * forward.x + forward.z * forward.z));
    orientation.roll = atan2f(up.x, sqrtf(up.y * up.y + up.z * up.z));

    orientation.valid = true;
    return orientation;
}

void CalculateHeadRenderData(const HeadRenderData* headData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!headData->orientation.valid) {
        CalculateBoneRenderData(headData->position, camera, outChosenIndex, outRotation, outMirrored);
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

    Vector3 camDir = Vector3Subtract(camera.position, headData->position);

    // Transform camera direction to head's local space
    Vector3 localCamDir;
    localCamDir.x = Vector3DotProduct(camDir, headData->orientation.right);
    localCamDir.y = Vector3DotProduct(camDir, headData->orientation.up);
    localCamDir.z = Vector3DotProduct(camDir, headData->orientation.forward);

    localCamDir.x = -localCamDir.x;

    float localYaw = atan2f(localCamDir.x, localCamDir.z);
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
    float normalizedYaw = localYawDeg + 22.5f;
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

bool ShouldRenderHead(const Person* person) {
    if (!person || !person->active) return false;

    int facialPoints = 0;
    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        if (strcmp(bone->name, "Nose") == 0 ||
            strcmp(bone->name, "LEye") == 0 ||
            strcmp(bone->name, "REye") == 0 ||
            strcmp(bone->name, "LEar") == 0 ||
            strcmp(bone->name, "REar") == 0) {
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

    bool finalMirror = false;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows, mirrored, &finalMirror);

    Vector2 worldSize = (Vector2){ headData->size, headData->size };
    DrawBonetileCustom(texture, camera, src, headData->position, worldSize, rotation, finalMirror);
}

void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads,
    int* headCount, int* headCapacity, BoneConfig* boneConfigs, int boneConfigCount) {
    *headCount = 0;

    if (!animation->isLoaded) return;

    int currentFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, currentFrame)) return;

    const AnimationFrame* frame = &animation->frames[currentFrame];

    if (*headCapacity < frame->personCount) {
        HeadRenderData* newArray = (HeadRenderData*)realloc(*heads, sizeof(HeadRenderData) * frame->personCount);
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

        if (!headData->orientation.valid && Vector3Length(headData->position) < 1e-6f) {
            continue;
        }

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