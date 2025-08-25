// head_billboard.c - PIVOTE desde OpenPose, billboard sigue mirando a la cámara
#include "head_billboard.h"
#include "bonetile.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// --- Calcula la posición central de la cabeza usando los puntos faciales disponibles ---
Vector3 CalculateHeadPosition(const Person* person) {
    if (!person || person->boneCount == 0) {
        return (Vector3) { 0, 0, 0 };
    }

    Vector3 totalPos = { 0,0,0 };
    int pointCount = 0;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;

        // Incluir las marcas faciales de OpenPose si están presentes
        if (strcmp(bone->name, "Nose") == 0 ||
            strcmp(bone->name, "LEye") == 0 ||
            strcmp(bone->name, "REye") == 0 ||
            strcmp(bone->name, "LEar") == 0 ||
            strcmp(bone->name, "REar") == 0) {

            totalPos = Vector3Add(totalPos, bone->position.position);
            pointCount++;
        }
    }

    if (pointCount > 0) {
        return Vector3Scale(totalPos, 1.0f / pointCount);
    }

    return (Vector3) { 0, 0, 0 };
}

// --- Calcula una orientación "real" de la cabeza con Nose/ Eyes/ Ears ---
// Nota: esta orientación se calcula y se guarda en HeadRenderData->orientation,
// pero **no** se usa para rotar el billboard en DrawHeadBillboard (ese seguirá
// orientándose hacia la cámara). Aquí la dejamos por si la quieres para otros usos.
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

    // Necesitamos al menos nariz + (ojos o orejas) para una orientación fiable
    if (!hasNose || !((hasLEye && hasREye) || (hasLEar && hasREar))) {
        // Aún así calculamos la posición para el pivote
        Vector3 centerFallback = CalculateHeadPosition(person);
        if (Vector3Length(centerFallback) > 0.0f) {
            orientation.position = centerFallback;
            orientation.valid = true;
        }
        return orientation;
    }

    // posición central (pivote) basada en la media de los puntos disponibles
    Vector3 totalPos = nose;
    int pointCount = 1;
    if (hasLEye) { totalPos = Vector3Add(totalPos, lEye); pointCount++; }
    if (hasREye) { totalPos = Vector3Add(totalPos, rEye); pointCount++; }
    if (hasLEar) { totalPos = Vector3Add(totalPos, lEar); pointCount++; }
    if (hasREar) { totalPos = Vector3Add(totalPos, rEar); pointCount++; }
    Vector3 center = Vector3Scale(totalPos, 1.0f / pointCount);
    orientation.position = center;

    // right: preferir orejas, si no, ojos
    Vector3 rightVec = (Vector3){ 1,0,0 };
    if (hasLEar && hasREar) {
        rightVec = Vector3Normalize(Vector3Subtract(rEar, lEar));
    }
    else if (hasLEye && hasREye) {
        rightVec = Vector3Normalize(Vector3Subtract(rEye, lEye));
    }

    // back reference: media entre orejas/ojos para obtener un punto posterior
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

    // forward: nariz menos punto trasero
    Vector3 forward = Vector3Normalize(Vector3Subtract(nose, backRef));
    if (Vector3Length(forward) < 1e-6f) {
        forward = (Vector3){ 0,0,1 }; // fallback
    }

    // up: cross(right, forward)
    Vector3 up = Vector3Normalize(Vector3CrossProduct(rightVec, forward));
    if (Vector3Length(up) < 1e-6f) {
        up = (Vector3){ 0,1,0 };
    }

    // re-ortonormalizar right
    rightVec = Vector3Normalize(Vector3CrossProduct(forward, up));

    orientation.forward = forward;
    orientation.up = up;
    orientation.right = rightVec;

    // Euler angles (en radianes). Los dejamos por si quieres usarlos.
    orientation.yaw = atan2f(forward.x, forward.z);
    orientation.pitch = atan2f(-forward.y, sqrtf(forward.x * forward.x + forward.z * forward.z));
    orientation.roll = atan2f(up.x, sqrtf(up.y * up.y + up.z * up.z));

    orientation.valid = true;
    return orientation;
}

// --- Mantener la misma lógica de visibilidad que antes (mínimo 2 marcas faciales) ---
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

// --- DRAW: el billboard sigue orientándose hacia la cámara (como los bones) ---
// Importante: NO usamos headData->orientation para rotar el sprite — solo usamos
// headData->position como pivote.
void DrawHeadBillboard(Texture2D texture, Camera camera, const HeadRenderData* headData, int physCols, int physRows) {
    if (!headData || !headData->valid || !headData->visible) return;

    // Recalcular el frame/rotación/flip exactamente como los bones (basado en cámara)
    int chosenIndex;
    float rotation;
    bool mirrored;

    // Esta función debe existir en tu código (mismo comportamiento que para bones)
    CalculateBoneRenderData(headData->position, camera, &chosenIndex, &rotation, &mirrored);

    int logicalCol = chosenIndex % ATLAS_COLS;
    int logicalRow = chosenIndex / ATLAS_COLS;

    bool finalMirror = false;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows, mirrored, &finalMirror);

    // Dibujar con la rotación calculada a partir de la cámara (no de la cabeza)
    Vector2 worldSize = (Vector2){ headData->size, headData->size };
    DrawBonetileCustom(texture, camera, src, headData->position, worldSize, rotation, finalMirror);
}

// --- Recopilar heads tal como antes, pero posición = CalculateHeadPosition(person) ---
void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads, int* headCount, int* headCapacity, BoneConfig* boneConfigs, int boneConfigCount) {
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

        // PIVOTE actualizado con OpenPose
        headData->position = CalculateHeadPosition(person);

        // Guardar orientación real por si la quieres para lógica no visual
        headData->orientation = CalculateHeadOrientation(person);

        if (!headData->orientation.valid && Vector3Length(headData->position) < 1e-6f) {
            // No hay datos útiles -> saltar
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
