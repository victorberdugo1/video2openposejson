#include "head_billboard.h"
#include "bonetile.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Constante para controlar la profundidad de la cabeza
static const float HEAD_DEPTH_OFFSET = 0.063f; // Negativo para atrás, positivo para adelante

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

    // Recopilar todos los puntos faciales
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

    // MÉTODO MEJORADO: Calcular la cabeza basándose en la geometría facial real
    if (hasNose && eyeCount > 0 && (hasLEar || hasREar || hasNeck)) {
        // Calcular el centro de la cara usando múltiples puntos
        Vector3 faceCenter = { 0, 0, 0 };
        int facePointCount = 0;

        // Añadir nariz (peso doble por ser punto frontal clave)
        faceCenter = Vector3Add(faceCenter, nosePos);
        faceCenter = Vector3Add(faceCenter, nosePos);
        facePointCount += 2;

        // Añadir ojos
        faceCenter = Vector3Add(faceCenter, eyeCenter);
        facePointCount++;

        // Calcular vector "hacia atrás" desde la nariz
        Vector3 backReference;
        bool hasBackReference = false;

        if (hasLEar && hasREar) {
            // Usar centro de orejas como referencia trasera
            backReference = Vector3Scale(Vector3Add(lEar, rEar), 0.5f);
            hasBackReference = true;
        }
        else if (hasLEar || hasREar) {
            // Usar una oreja como referencia
            backReference = hasLEar ? lEar : rEar;
            hasBackReference = true;
        }
        else if (hasNeck) {
            // Usar cuello como referencia trasera
            backReference = neckPos;
            hasBackReference = true;
        }

        if (hasBackReference) {
            // Añadir referencia trasera al cálculo del centro
            faceCenter = Vector3Add(faceCenter, backReference);
            facePointCount++;
        }

        // Calcular centro promedio
        faceCenter = Vector3Scale(faceCenter, 1.0f / facePointCount);

        // Calcular dirección frontal basada en geometría facial real
        Vector3 frontDirection = { 0, 0, 1 }; // Default
        if (hasBackReference) {
            Vector3 noseToBack = Vector3Subtract(backReference, nosePos);
            float backDistance = Vector3Length(noseToBack);
            if (backDistance > 1e-6f) {
                frontDirection = Vector3Scale(noseToBack, 1.0f / backDistance);
            }
        }

        // Posicionar la cabeza usando el offset configurable (puede ser hacia adelante o atrás)
        Vector3 headPos = Vector3Add(faceCenter, Vector3Scale(frontDirection, HEAD_DEPTH_OFFSET));

        // Ajuste dinámico hacia arriba basado en la inclinación de la cabeza
        Vector3 eyeToNose = Vector3Subtract(nosePos, eyeCenter);
        float verticalComponent = eyeToNose.y;

        // Si la cabeza está inclinada hacia abajo (agachándose), subir menos
        // Si está normal o hacia arriba, subir más
        float dynamicUpOffset = 0.03f;
        if (verticalComponent < -0.01f) {
            // Cabeza inclinada hacia abajo
            dynamicUpOffset = 0.015f;
        }
        else if (verticalComponent > 0.01f) {
            // Cabeza inclinada hacia arriba
            dynamicUpOffset = 0.045f;
        }

        headPos.y += dynamicUpOffset;

        return headPos;
    }

    // Fallback al método original si no hay suficientes puntos faciales
    // Este fallback mantiene la relación cuello-cara original
    if (eyeCount > 0 && hasNeck) {
        Vector3 headPos = {
            neckPos.x * 0.7f + eyeCenter.x * 0.3f,
            eyeCenter.y,
            hasNose ? neckPos.z * 0.8f + nosePos.z * 0.2f : neckPos.z * 0.9f + eyeCenter.z * 0.1f
        };

        // Mantener los offsets originales del fallback (no aplicar HEAD_VISUAL_OFFSET aquí)
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

    // Usar la posición calculada dinámicamente
    orientation.position = CalculateHeadPosition(person);

    // Calculate vectors
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
        CalculateBoneRenderData(headData->position, camera, outChosenIndex, outRotation, outMirrored, "Head");
        return;
    }

    static const int indices[3][8] = {
        {  0,  4,  5,  6,  7,  6,  5,  4 },
        {  2, 12, 13, 14, 15, 14, 13, 12 },
        {  1,  8,  9, 10, 11, 10,  9,  8 }
    };

    Vector3 camDir = Vector3Subtract(camera.position, headData->position);

    // Transform to head's local space
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

    // Determine sector
    float normalizedYaw = localYawDeg + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;
    int sector = (int)(normalizedYaw / 45.0f);

    // Handle special views
    if (localPitchDeg >= 70.0f) {
        *outChosenIndex = 3;  // topdown
        *outRotation = sector * 45.0f + 180.0f;
        *outMirrored = false;
    }
    else if (localPitchDeg <= -70.0f) {
        *outChosenIndex = 15; // bottom
        *outRotation = (8 - sector) * 45.0f + 180.0f;
        if (*outRotation >= 360.0f) *outRotation -= 360.0f;
        *outMirrored = true;
    }
    else {
        // Choose row based on pitch
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

        // Check for duplicates
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