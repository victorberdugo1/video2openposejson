// head_billboard.c - Sistema de cabezas con orientación real basada en OpenPose
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

// --- NUEVA FUNCIÓN: Mapeo de atlas basado en orientación real de la cabeza ---
// --- FUNCIÓN CORREGIDA: Mapeo de atlas basado en orientación real de la cabeza ---
void CalculateHeadRenderData(const HeadRenderData* headData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!headData->orientation.valid) {
        // Fallback al método clásico si no hay orientación válida
        CalculateBoneRenderData(headData->position, camera, outChosenIndex, outRotation, outMirrored);
        return;
    }

    const int indices[3][8] = {
        {  0,  4,  5,  6,  7,  6,  5,  4 },  // fila principal (nivel medio)
        {  2, 12, 13, 14, 15, 14, 13, 12 },  // fila inferior (mirando hacia abajo)
        {  1,  8,  9, 10, 11, 10,  9,  8 }   // fila superior (mirando hacia arriba)
    };
    const int topdownIndex = 3;
    const int bottomIndex = 15;
    const float TOPDOWN_ANGLE = 70.0f;
    const float HIGH_THRESHOLD = 22.5f;
    const float MAIN_THRESHOLD = -22.5f;

    // Obtener la dirección desde la cabeza hacia la cámara (igual que en la función normal)
    Vector3 camDir = Vector3Subtract(camera.position, headData->position);

    // CLAVE: Transformar la dirección de la cámara al espacio local de la cabeza
    // Pero invertimos la interpretación para que coincida con lo que "ve" la cámara
    Vector3 localCamDir;
    localCamDir.x = Vector3DotProduct(camDir, headData->orientation.right);   // componente hacia la derecha de la cabeza
    localCamDir.y = Vector3DotProduct(camDir, headData->orientation.up);      // componente hacia arriba de la cabeza  
    localCamDir.z = Vector3DotProduct(camDir, headData->orientation.forward); // componente hacia adelante de la cabeza

    // CORRECCIÓN CLAVE: Invertir X para que la lógica coincida con la función normal
    // Cuando la cabeza mira a la derecha, la cámara ve su lado izquierdo
    localCamDir.x = -localCamDir.x;

    // Calcular yaw y pitch en el espacio local de la cabeza
    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localYawDeg = localYaw * RAD2DEG;

    float horizDistance = sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z);
    float localPitch = atan2f(localCamDir.y, horizDistance);
    float localPitchDeg = localPitch * RAD2DEG;

    // El resto de la función permanece igual...
    // Determinar la fila del atlas basada en el pitch local
    int chosenRow = -1;
    bool useTopdown = false;
    bool isTopView = false;

    if (localPitchDeg >= TOPDOWN_ANGLE) {
        useTopdown = true;
        isTopView = true;
    }
    else if (localPitchDeg >= HIGH_THRESHOLD) {
        chosenRow = 2; // fila superior
    }
    else if (localPitchDeg >= MAIN_THRESHOLD) {
        chosenRow = 0; // fila principal
    }
    else if (localPitchDeg >= -TOPDOWN_ANGLE) {
        chosenRow = 1; // fila inferior
    }
    else {
        useTopdown = true;
        isTopView = false;
    }

    // Calcular el sector basado en el yaw local
    int sector = 0;
    float normalizedYaw = localYawDeg + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    if (normalizedYaw < 45.0f) sector = 0;       // frente
    else if (normalizedYaw < 90.0f) sector = 1;  // frente-derecha
    else if (normalizedYaw < 135.0f) sector = 2; // derecha
    else if (normalizedYaw < 180.0f) sector = 3; // atrás-derecha
    else if (normalizedYaw < 225.0f) sector = 4; // atrás
    else if (normalizedYaw < 270.0f) sector = 5; // atrás-izquierda
    else if (normalizedYaw < 315.0f) sector = 6; // izquierda
    else sector = 7;                             // frente-izquierda

    // Asignar índice, rotación y espejado
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
        *outMirrored = !(sector >= 5 && sector <= 7); // espejear para los sectores izquierdos
    }
}

// --- Función de morphing para cabezas (opcional, por si quieres usar morphing también) ---
void CalculateHeadMorphData(const HeadRenderData* headData, Camera camera, BoneMorphData* outMorphData) {
    // Por ahora usar la versión sin morphing, pero podrías implementar morphing similar al de los bones
    int primaryIndex;
    float rotation;
    bool mirrored;

    CalculateHeadRenderData(headData, camera, &primaryIndex, &rotation, &mirrored);

    outMorphData->primaryIndex = primaryIndex;
    outMorphData->secondaryIndex = primaryIndex; // sin morphing por ahora
    outMorphData->blendFactor = 0.0f;
    outMorphData->rotation = rotation;
    outMorphData->mirrored = mirrored;
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

// --- DRAW ACTUALIZADO: usa la orientación real de la cabeza ---
void DrawHeadBillboard(Texture2D texture, Camera camera, const HeadRenderData* headData, int physCols, int physRows) {
    if (!headData || !headData->valid || !headData->visible) return;

    int chosenIndex;
    float rotation;
    bool mirrored;

    // USAR LA NUEVA FUNCIÓN que considera la orientación real de la cabeza
    CalculateHeadRenderData(headData, camera, &chosenIndex, &rotation, &mirrored);

    int logicalCol = chosenIndex % ATLAS_COLS;
    int logicalRow = chosenIndex / ATLAS_COLS;

    bool finalMirror = false;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows, mirrored, &finalMirror);

    // Dibujar con la rotación calculada basada en la orientación real de la cabeza
    Vector2 worldSize = (Vector2){ headData->size, headData->size };
    DrawBonetileCustom(texture, camera, src, headData->position, worldSize, rotation, finalMirror);
}

// --- Recopilar heads tal como antes, pero ahora con orientación correcta ---
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

        // Calcular posición y orientación reales
        headData->position = CalculateHeadPosition(person);
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