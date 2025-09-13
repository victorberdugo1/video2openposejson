// torso_billboard.c - Sistema de torsos con orientación real basada en OpenPose (OPTIMIZADO CORREGIDO)
#include "torso_billboard.h"
#include "bonetile.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// Constantes para evitar recálculos
static const float CHEST_OFFSET_Y = -0.06f;
static const float CHEST_OFFSET_Z = 0.015f;
static const float CHEST_FALLBACK_Y = -0.08f;


// Estructura para cachear búsquedas de bones
typedef struct {
    Vector3 neck, lShoulder, rShoulder, lHip, rHip;
    bool hasNeck, hasLShoulder, hasRShoulder, hasLHip, hasRHip;
    int shoulderCount, hipCount;
} CachedBones;

// Función auxiliar para buscar y cachear bones
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

    // Fallback
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

    if (cache.hasLHip && cache.hasRHip) {
        return Vector3Scale(Vector3Add(cache.lHip, cache.rHip), 0.5f);
    }

    return cache.hasLHip ? cache.lHip : cache.rHip;
}

VirtualSpine CalculateVirtualSpine(const Person* person) {
    VirtualSpine spine = { 0 };
    if (!person || person->boneCount == 0) return spine;

    CachedBones cache = CacheBones(person);

    if (!cache.hasLShoulder || !cache.hasRShoulder || !cache.hasLHip || !cache.hasRHip) {
        return spine;
    }

    spine.chestPosition = Vector3Scale(Vector3Add(cache.lShoulder, cache.rShoulder), 0.5f);
    spine.hipPosition = Vector3Scale(Vector3Add(cache.lHip, cache.rHip), 0.5f);

    if (cache.hasNeck) {
        spine.chestPosition = Vector3Scale(
            Vector3Add(Vector3Add(cache.lShoulder, cache.rShoulder), cache.neck), 1.0f / 3.0f);
    }

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

// Función auxiliar para calcular orientación
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

// Función auxiliar para normalizar un vector de forma segura (igual que en bonetile.c)
static Vector3 SafeNormalizeTorso(Vector3 v) {
    float length = Vector3Length(v);
    if (length < 1e-6f) return (Vector3) { 0, 0, 1 }; // Vector forward por defecto
    return Vector3Scale(v, 1.0f / length);
}

void CalculateTorsoRenderData(const TorsoRenderData* torsoData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!torsoData->orientation.valid) {
        CalculateBoneRenderData(torsoData->position, camera, outChosenIndex, outRotation, outMirrored, "");
        return;
    }

    // Usar el mismo sistema que bonetile.c para orientación válida
    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},
        {2,12,13,14,15,14,13,12},
        {1,8,9,10,11,10,9,8}
    };

    // Calcular dirección INVERTIDA de la cámara en espacio local del torso (igual que bonetile.c)
    Vector3 camDir = Vector3Subtract(torsoData->position, camera.position);
    camDir = SafeNormalizeTorso(camDir);

    // Transformar a coordenadas locales del torso
    Vector3 localCamDir = {
        Vector3DotProduct(camDir, torsoData->orientation.right),
        Vector3DotProduct(camDir, torsoData->orientation.up),
        Vector3DotProduct(camDir, torsoData->orientation.forward)
    };

    // Calcular ángulos en el espacio local
    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;

    float localPitchDeg = atan2f(localCamDir.y,
        sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z)) * RAD2DEG;

    // INVERSIÓN ESPECÍFICA DE PITCH para torsos (igual que en bonetile.c para bones que no son cuello/manos/pies)
    localPitchDeg = -localPitchDeg;  // Invertir el pitch para torsos

    // Normalizar yaw para el sistema de sectores
    float normalizedYaw = localYaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    int sector = (int)(normalizedYaw / 45.0f) % 8;

    // Seleccionar sprite basado en el ángulo de pitch (igual que bonetile.c)
    if (localPitchDeg >= 70.0f) {
        *outChosenIndex = 3;  // Vista desde arriba
        *outRotation = sector * 45.0f;
        *outMirrored = false;
    }
    else if (localPitchDeg <= -70.0f) {
        *outChosenIndex = 15; // Vista desde abajo
        *outRotation = sector * 45.0f;
        *outMirrored = true;
    }
    else {
        // Vista lateral/frontal
        int row = (localPitchDeg >= 22.5f) ? 2 : (localPitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[row][sector];
        *outRotation = 0.0f;
        *outMirrored = (sector >= 1 && sector <= 4);
    }

    /*/ AGREGAR ROTACIÓN CONTINUA SOLO PARA HIP
    if (torsoData->type == TORSO_HIP) {
        float currentTime = GetTime();
        float spinSpeed = 90.0f; // Grados por segundo
        float spinRotation = fmodf(currentTime * spinSpeed, 360.0f);
        *outRotation += spinRotation;
    }*/
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

    int logicalCol = chosenIndex % ATLAS_COLS;
    int logicalRow = chosenIndex / ATLAS_COLS;

    bool finalMirror;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows, mirrored, &finalMirror);
    Vector2 worldSize = { torsoData->size, torsoData->size };

    DrawBonetileCustom(texture, camera, src, torsoData->position, worldSize, rotation, finalMirror, "");
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

    // Volver al sistema original de detección de duplicados para compatibilidad exacta
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