#include "bones3d.h"
#include "bonetile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Variables globales para configuración
static BonesRenderConfig g_renderConfig = {0};
static bool g_configInitialized = false;

// =============================================================================
// UTILIDADES INTERNAS
// =============================================================================

static void InitializeDefaultConfig(void) {
    if (!g_configInitialized) {
        g_renderConfig.defaultBoneSize = 0.35f;
        g_renderConfig.drawDebugSpheres = false;
        g_renderConfig.enableDepthSorting = true;
        g_renderConfig.debugColor = RED;
        g_renderConfig.debugSphereRadius = 0.035f;
        g_renderConfig.showInvalidBones = false;
        g_configInitialized = true;
    }
}

static bool IsValidPointer(const void* ptr) {
    return ptr != NULL;
}

static bool IsValidString(const char* str) {
    return str != NULL && str[0] != '\0';
}

static void SafeStrCopy(char* dest, const char* src, size_t destSize) {
    if (!IsValidPointer(dest) || !IsValidString(src) || destSize == 0) return;
    
    size_t len = strlen(src);
    if (len >= destSize) {
        len = destSize - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
}

// =============================================================================
// FUNCIONES DE MANEJO DE ERRORES
// =============================================================================

const char* BonesGetErrorString(BonesError error) {
    switch (error) {
        case BONES_SUCCESS: return "Operación exitosa";
        case BONES_ERROR_NULL_POINTER: return "Puntero nulo recibido";
        case BONES_ERROR_FILE_NOT_FOUND: return "Archivo no encontrado";
        case BONES_ERROR_INVALID_JSON: return "JSON inválido o mal formateado";
        case BONES_ERROR_MEMORY_ALLOCATION: return "Error de asignación de memoria";
        case BONES_ERROR_BONE_NOT_FOUND: return "Bone no encontrado";
        case BONES_ERROR_FRAME_OUT_OF_RANGE: return "Frame fuera del rango válido";
        case BONES_ERROR_PERSON_NOT_FOUND: return "Persona no encontrada";
        case BONES_ERROR_INVALID_COORDINATES: return "Coordenadas inválidas";
        case BONES_ERROR_BUFFER_OVERFLOW: return "Desbordamiento de buffer";
        case BONES_ERROR_EMPTY_DATA: return "Datos vacíos o sin contenido";
        default: return "Error desconocido";
    }
}

void BonesLogError(BonesError error, const char* context) {
    if (error != BONES_SUCCESS) {
        const char* errorStr = BonesGetErrorString(error);
        if (IsValidString(context)) {
            TraceLog(LOG_ERROR, "BONES ERROR [%s]: %s", context, errorStr);
        } else {
            TraceLog(LOG_ERROR, "BONES ERROR: %s", errorStr);
        }
    }
}

// =============================================================================
// INICIALIZACIÓN Y LIMPIEZA
// =============================================================================

BonesError BonesInit(BonesAnimation* animation, int maxFrames) {
    if (!IsValidPointer(animation)) {
        return BONES_ERROR_NULL_POINTER;
    }
    
    if (maxFrames <= 0 || maxFrames > MAX_FRAMES) {
        maxFrames = MAX_FRAMES;
    }
    
    // Limpiar estructura
    memset(animation, 0, sizeof(BonesAnimation));
    
    // Asignar memoria para frames
    animation->frames = (AnimationFrame*)calloc(maxFrames, sizeof(AnimationFrame));
    if (!IsValidPointer(animation->frames)) {
        return BONES_ERROR_MEMORY_ALLOCATION;
    }
    
    animation->maxFrames = maxFrames;
    animation->frameCount = 0;
    animation->currentFrame = -1;
    animation->isLoaded = false;
    
    InitializeDefaultConfig();
    
    TraceLog(LOG_INFO, "BONES: Sistema inicializado correctamente (%d frames max)", maxFrames);
    return BONES_SUCCESS;
}

void BonesFree(BonesAnimation* animation) {
    if (!IsValidPointer(animation)) {
        return;
    }
    
    if (IsValidPointer(animation->frames)) {
        free(animation->frames);
        animation->frames = NULL;
    }
    
    animation->frameCount = 0;
    animation->maxFrames = 0;
    animation->currentFrame = -1;
    animation->isLoaded = false;
    animation->filePath[0] = '\0';
    
    TraceLog(LOG_INFO, "BONES: Sistema liberado correctamente");
}

// =============================================================================
// CARGA DE DATOS - PARSER JSON SIMPLIFICADO
// =============================================================================

// Parser JSON básico para nuestros datos específicos
static BonesError ParseJSONFrame(const char* jsonData, int* outFrameNum, Person* outPersons, int* outPersonCount) {
    if (!IsValidString(jsonData) || !IsValidPointer(outFrameNum) || 
        !IsValidPointer(outPersons) || !IsValidPointer(outPersonCount)) {
        return BONES_ERROR_NULL_POINTER;
    }
    
    *outPersonCount = 0;
    
    // Buscar patrón "frame_XXXX"
    const char* frameStart = strstr(jsonData, "\"frame_");
    if (!frameStart) {
        return BONES_ERROR_INVALID_JSON;
    }
    
    // Extraer número de frame
    if (sscanf(frameStart, "\"frame_%d\"", outFrameNum) != 1) {
        return BONES_ERROR_INVALID_JSON;
    }
    
    // Buscar personas
    const char* personStart = strstr(frameStart, "\"person_");
    while (personStart && *outPersonCount < MAX_PERSONS) {
        Person* currentPerson = &outPersons[*outPersonCount];
        memset(currentPerson, 0, sizeof(Person));
        
        // Extraer ID de persona
        if (sscanf(personStart, "\"person_%15[^\"]\"", currentPerson->personId) != 1) {
            break;
        }
        
        currentPerson->active = true;
        currentPerson->boneCount = 0;
        
        // Buscar bones para esta persona
        const char* boneStart = personStart;
        const char* nextPerson = strstr(personStart + 1, "\"person_");
        
        // Lista de bones esperados de OpenPose
        const char* expectedBones[] = {
            "Nose", "LEye", "REye", "LEar", "REar",
            "LShoulder", "RShoulder", "LElbow", "RElbow", 
            "LWrist", "RWrist", "LHip", "RHip",
            "LKnee", "RKnee", "LAnkle", "RAnkle", "Neck"
        };
        int expectedBoneCount = sizeof(expectedBones) / sizeof(expectedBones[0]);
        
        for (int b = 0; b < expectedBoneCount && currentPerson->boneCount < MAX_BONES_PER_PERSON; b++) {
            char searchPattern[64];
            snprintf(searchPattern, sizeof(searchPattern), "\"%s\":", expectedBones[b]);
            
            const char* bonePos = strstr(boneStart, searchPattern);
            if (!bonePos || (nextPerson && bonePos > nextPerson)) {
                continue; // Este bone no está en esta persona
            }
            
            Bone* currentBone = &currentPerson->bones[currentPerson->boneCount];
            memset(currentBone, 0, sizeof(Bone));
            
            SafeStrCopy(currentBone->name, expectedBones[b], MAX_BONE_NAME_LENGTH);
            
            // Buscar coordenadas x, y, z
            float x = 0.0f, y = 0.0f, z = 0.0f;
            const char* xPos = strstr(bonePos, "\"x\":");
            const char* yPos = strstr(bonePos, "\"y\":");
            const char* zPos = strstr(bonePos, "\"z\":");
            
            if (xPos && yPos && zPos &&
                sscanf(xPos, "\"x\": %f", &x) == 1 &&
                sscanf(yPos, "\"y\": %f", &y) == 1 &&
                sscanf(zPos, "\"z\": %f", &z) == 1) {
                
                // Convertir coordenadas normalizadas (0-1) a mundo (-1 a 1, Y invertido)
                currentBone->position.position = (Vector3){
                    x * 2.0f - 1.0f,
                    1.0f - y,  // Invertir Y
                    z * 2.0f - 1.0f
                };
                currentBone->position.valid = BonesIsPositionValid(currentBone->position.position);
                currentBone->position.confidence = 1.0f; // OpenPose no provee confidence en este formato
                
                // Configuración por defecto
                currentBone->textureIndex = 0;
                currentBone->size = (Vector2){g_renderConfig.defaultBoneSize, g_renderConfig.defaultBoneSize};
                currentBone->rotation = 0.0f;
                currentBone->mirrored = false;
                currentBone->visible = true;
                
                currentPerson->boneCount++;
            }
        }
        
        if (currentPerson->boneCount > 0) {
            (*outPersonCount)++;
        }
        
        // Buscar siguiente persona
        personStart = nextPerson;
    }
    
    return (*outPersonCount > 0) ? BONES_SUCCESS : BONES_ERROR_EMPTY_DATA;
}

BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath) {
    if (!IsValidPointer(animation) || !IsValidString(jsonFilePath)) {
        return BONES_ERROR_NULL_POINTER;
    }
    
    // Cargar archivo
    char* jsonData = LoadFileText(jsonFilePath);
    if (!IsValidPointer(jsonData)) {
        BonesLogError(BONES_ERROR_FILE_NOT_FOUND, jsonFilePath);
        return BONES_ERROR_FILE_NOT_FOUND;
    }
    
    BonesError result = BonesLoadFromString(animation, jsonData);
    
    if (result == BONES_SUCCESS) {
        SafeStrCopy(animation->filePath, jsonFilePath, sizeof(animation->filePath));
    }
    
    UnloadFileText(jsonData);
    return result;
}

BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString) {
    if (!IsValidPointer(animation) || !IsValidString(jsonString)) {
        return BONES_ERROR_NULL_POINTER;
    }
    
    if (!IsValidPointer(animation->frames)) {
        BonesLogError(BONES_ERROR_NULL_POINTER, "Animation not initialized");
        return BONES_ERROR_NULL_POINTER;
    }
    
    animation->frameCount = 0;
    animation->currentFrame = -1;
    animation->isLoaded = false;
    
    // Parser simple: buscar todos los frames en el JSON
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
            TraceLog(LOG_INFO, "BONES: Frame %d cargado con %d personas", 
                    frame->frameNumber, frame->personCount);
        } else if (parseResult != BONES_ERROR_EMPTY_DATA) {
            BonesLogError(parseResult, "ParseJSONFrame");
        }
        
        searchPos = nextFrame + 1;
    }
    
    if (animation->frameCount > 0) {
        animation->currentFrame = 0;
        animation->isLoaded = true;
        TraceLog(LOG_INFO, "BONES: Animación cargada exitosamente (%d frames)", animation->frameCount);
        return BONES_SUCCESS;
    }
    
    return BONES_ERROR_EMPTY_DATA;
}

// =============================================================================
// NAVEGACIÓN DE FRAMES
// =============================================================================

BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber) {
    if (!IsValidPointer(animation)) {
        return BONES_ERROR_NULL_POINTER;
    }
    
    if (!animation->isLoaded) {
        return BONES_ERROR_EMPTY_DATA;
    }
    
    if (frameNumber < 0 || frameNumber >= animation->frameCount) {
        return BONES_ERROR_FRAME_OUT_OF_RANGE;
    }
    
    animation->currentFrame = frameNumber;
    return BONES_SUCCESS;
}

int BonesGetCurrentFrame(const BonesAnimation* animation) {
    return (IsValidPointer(animation)) ? animation->currentFrame : -1;
}

int BonesGetFrameCount(const BonesAnimation* animation) {
    return (IsValidPointer(animation)) ? animation->frameCount : 0;
}

bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber) {
    return IsValidPointer(animation) && 
           animation->isLoaded &&
           frameNumber >= 0 && 
           frameNumber < animation->frameCount &&
           animation->frames[frameNumber].valid;
}

// =============================================================================
// ACCESO A DATOS
// =============================================================================

BonesError BonesGetPerson(const BonesAnimation* animation, int frameNumber, 
                         const char* personId, Person** outPerson) {
    if (!IsValidPointer(animation) || !IsValidString(personId) || !IsValidPointer(outPerson)) {
        return BONES_ERROR_NULL_POINTER;
    }
    
    *outPerson = NULL;
    
    if (!BonesIsValidFrame(animation, frameNumber)) {
        return BONES_ERROR_FRAME_OUT_OF_RANGE;
    }
    
    const AnimationFrame* frame = &animation->frames[frameNumber];
    
    for (int i = 0; i < frame->personCount; i++) {
        if (strcmp(frame->persons[i].personId, personId) == 0) {
            *outPerson = (Person*)&frame->persons[i];
            return BONES_SUCCESS;
        }
    }
    
    return BONES_ERROR_PERSON_NOT_FOUND;
}

BonesError BonesGetBone(const BonesAnimation* animation, int frameNumber, 
                       const char* personId, const char* boneName, Bone** outBone) {
    Person* person;
    BonesError result = BonesGetPerson(animation, frameNumber, personId, &person);
    if (result != BONES_SUCCESS) {
        return result;
    }
    
    if (!IsValidString(boneName) || !IsValidPointer(outBone)) {
        return BONES_ERROR_NULL_POINTER;
    }
    
    *outBone = NULL;
    
    for (int i = 0; i < person->boneCount; i++) {
        if (strcmp(person->bones[i].name, boneName) == 0) {
            *outBone = &person->bones[i];
            return BONES_SUCCESS;
        }
    }
    
    return BONES_ERROR_BONE_NOT_FOUND;
}

BonesError BonesGetBonePosition(const BonesAnimation* animation, int frameNumber, 
                               const char* personId, const char* boneName, 
                               Vector3* outPosition) {
    Bone* bone;
    BonesError result = BonesGetBone(animation, frameNumber, personId, boneName, &bone);
    if (result != BONES_SUCCESS) {
        return result;
    }
    
    if (!IsValidPointer(outPosition)) {
        return BONES_ERROR_NULL_POINTER;
    }
    
    if (!bone->position.valid) {
        return BONES_ERROR_INVALID_COORDINATES;
    }
    
    *outPosition = bone->position.position;
    return BONES_SUCCESS;
}

// =============================================================================
// UTILIDADES
// =============================================================================

Vector3 BonesNormalizedToWorld(Vector3 normalizedPos, Vector3 worldCenter, Vector3 worldScale) {
    return Vector3Add(worldCenter, Vector3Multiply(normalizedPos, worldScale));
}

Vector3 BonesWorldToNormalized(Vector3 worldPos, Vector3 worldCenter, Vector3 worldScale) {
    Vector3 centered = Vector3Subtract(worldPos, worldCenter);
    return (Vector3){
        centered.x / worldScale.x,
        centered.y / worldScale.y,  
        centered.z / worldScale.z
    };
}

bool BonesIsPositionValid(Vector3 position) {
    return !isnan(position.x) && !isnan(position.y) && !isnan(position.z) &&
           isfinite(position.x) && isfinite(position.y) && isfinite(position.z);
}

bool BonesIsBoneVisible(const Bone* bone, Camera camera, float maxDistance) {
    if (!IsValidPointer(bone) || !bone->visible || !bone->position.valid) {
        return false;
    }
    
    float distance = Vector3Distance(camera.position, bone->position.position);
    return distance <= maxDistance;
}

// =============================================================================
// CONFIGURACIÓN Y RENDERIZADO
// =============================================================================

BonesRenderConfig BonesGetDefaultRenderConfig(void) {
    InitializeDefaultConfig();
    return g_renderConfig;
}

void BonesSetRenderConfig(const BonesRenderConfig* config) {
    if (IsValidPointer(config)) {
        g_renderConfig = *config;
        g_configInitialized = true;
    }
}

// Integración con tu sistema bonetile existente
BonesError BonesDrawBone(const Bone* bone, Camera camera, Texture2D texture,
                        const BonesRenderConfig* config) {
    if (!IsValidPointer(bone) || !IsValidPointer(config)) {
        return BONES_ERROR_NULL_POINTER;
    }
    
    if (!bone->visible || !bone->position.valid) {
        return BONES_SUCCESS; // No es error, simplemente no se dibuja
    }
    
    // Usar tu función existente DrawBonetileCustom
    // Para simplificar, usamos una celda por defecto del atlas
    Rectangle src = GetAtlasCellSrcPos(texture, 0, 0, bone->mirrored, NULL);
    
    DrawBonetileCustom(texture, camera, src, bone->position.position, 
                      bone->size, bone->rotation, bone->mirrored);
    
    // Debug sphere si está habilitado
    if (config->drawDebugSpheres) {
        DrawSphereWires(bone->position.position, config->debugSphereRadius, 8, 8, config->debugColor);
    }
    
    return BONES_SUCCESS;
}

// =============================================================================
// FUNCIONES DE DEBUG Y ESTADÍSTICAS
// =============================================================================

void BonesPrintFrameInfo(const BonesAnimation* animation, int frameNumber) {
    if (!BonesIsValidFrame(animation, frameNumber)) {
        TraceLog(LOG_WARNING, "BONES DEBUG: Frame inválido %d", frameNumber);
        return;
    }
    
    const AnimationFrame* frame = &animation->frames[frameNumber];
    TraceLog(LOG_INFO, "BONES DEBUG: Frame %d - %d personas", frame->frameNumber, frame->personCount);
    
    for (int i = 0; i < frame->personCount; i++) {
        BonesPrintPersonInfo(&frame->persons[i]);
    }
}

void BonesPrintPersonInfo(const Person* person) {
    if (!IsValidPointer(person)) return;
    
    int validBones = BonesCountValidBones(person);
    TraceLog(LOG_INFO, "BONES DEBUG: Persona %s - %d bones (%d válidos)", 
            person->personId, person->boneCount, validBones);
}

void BonesPrintBoneInfo(const Bone* bone) {
    if (!IsValidPointer(bone)) return;
    
    TraceLog(LOG_INFO, "BONES DEBUG: Bone %s - Pos(%.3f, %.3f, %.3f) Válido:%s", 
            bone->name, 
            bone->position.position.x, bone->position.position.y, bone->position.position.z,
            bone->position.valid ? "Sí" : "No");
}

int BonesCountValidBones(const Person* person) {
    if (!IsValidPointer(person)) return 0;
    
    int count = 0;
    for (int i = 0; i < person->boneCount; i++) {
        if (person->bones[i].position.valid) {
            count++;
        }
    }
    return count;
}

Vector3 BonesCalculatePersonCenter(const Person* person) {
    if (!IsValidPointer(person) || person->boneCount == 0) {
        return (Vector3){0, 0, 0};
    }
    
    Vector3 sum = {0, 0, 0};
    int validCount = 0;
    
    for (int i = 0; i < person->boneCount; i++) {
        if (person->bones[i].position.valid) {
            sum = Vector3Add(sum, person->bones[i].position.position);
            validCount++;
        }
    }
    
    if (validCount > 0) {
        return Vector3Scale(sum, 1.0f / validCount);
    }
    
    return (Vector3){0, 0, 0};
}

Vector3 BonesInterpolatePosition(Vector3 pos1, Vector3 pos2, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return Vector3Lerp(pos1, pos2, t);
}
