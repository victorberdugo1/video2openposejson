// bones3d.h
#ifndef BONES3D_H
#define BONES3D_H

#include "raylib.h"
#include "raymath.h"
#include <stdbool.h>

// =============================================================================
// DEFINICIONES Y CONSTANTES
// =============================================================================

#define MAX_BONE_NAME_LENGTH 32
#define MAX_BONES_PER_PERSON 32
#define MAX_FRAMES 10000
#define MAX_PERSONS 10

// Códigos de error
typedef enum {
    BONES_SUCCESS = 0,
    BONES_ERROR_NULL_POINTER,
    BONES_ERROR_FILE_NOT_FOUND,
    BONES_ERROR_INVALID_JSON,
    BONES_ERROR_MEMORY_ALLOCATION,
    BONES_ERROR_BONE_NOT_FOUND,
    BONES_ERROR_FRAME_OUT_OF_RANGE,
    BONES_ERROR_PERSON_NOT_FOUND,
    BONES_ERROR_INVALID_COORDINATES,
    BONES_ERROR_BUFFER_OVERFLOW,
    BONES_ERROR_EMPTY_DATA
} BonesError;

// Estructura para posición de un bone
typedef struct {
    Vector3 position;
    bool valid;          // Si la posición es válida/detectada
    float confidence;    // Confianza de la detección (0.0 - 1.0)
} BonePosition;

// Estructura para un bone individual
typedef struct {
    char name[MAX_BONE_NAME_LENGTH];
    BonePosition position;
    int textureIndex;    // Índice de textura para este bone
    Vector2 size;        // Tamaño del billboard
    float rotation;      // Rotación adicional
    bool mirrored;       // Si está espejado
    bool visible;        // Si debe dibujarse
} Bone;

// Estructura para una persona (conjunto de bones)
typedef struct {
    char personId[16];   // "person_0", "person_1", etc.
    Bone bones[MAX_BONES_PER_PERSON];
    int boneCount;
    bool active;         // Si esta persona está activa
} Person;

// Estructura para un frame completo
typedef struct {
    int frameNumber;
    Person persons[MAX_PERSONS];
    int personCount;
    bool valid;          // Si el frame tiene datos válidos
} AnimationFrame;

// Estructura principal de animación
typedef struct {
    AnimationFrame* frames;
    int frameCount;
    int maxFrames;
    int currentFrame;
    bool isLoaded;
    char filePath[256];  // Path del archivo cargado
} BonesAnimation;

// Configuración de renderizado
typedef struct {
    float defaultBoneSize;
    bool drawDebugSpheres;
    bool enableDepthSorting;
    Color debugColor;
    float debugSphereRadius;
    bool showInvalidBones;
} BonesRenderConfig;

// =============================================================================
// FUNCIONES PRINCIPALES
// =============================================================================

// Inicialización y limpieza
BonesError BonesInit(BonesAnimation* animation, int maxFrames);
void BonesFree(BonesAnimation* animation);

// Carga de datos
BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath);
BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString);

// Navegación de frames
BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber);
int BonesGetCurrentFrame(const BonesAnimation* animation);
int BonesGetFrameCount(const BonesAnimation* animation);
bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber);

// Acceso a datos
BonesError BonesGetPerson(const BonesAnimation* animation, int frameNumber,
                         const char* personId, Person** outPerson);
BonesError BonesGetBone(const BonesAnimation* animation, int frameNumber,
                       const char* personId, const char* boneName, Bone** outBone);
BonesError BonesGetBonePosition(const BonesAnimation* animation, int frameNumber,
                               const char* personId, const char* boneName,
                               Vector3* outPosition);

// Utilidades de conversión
Vector3 BonesNormalizedToWorld(Vector3 normalizedPos, Vector3 worldCenter, Vector3 worldScale);
Vector3 BonesWorldToNormalized(Vector3 worldPos, Vector3 worldCenter, Vector3 worldScale);

// Validación
bool BonesIsPositionValid(Vector3 position);
bool BonesIsBoneVisible(const Bone* bone, Camera camera, float maxDistance);

// Configuración de renderizado
BonesRenderConfig BonesGetDefaultRenderConfig(void);
void BonesSetRenderConfig(const BonesRenderConfig* config);

// Renderizado (integración con tu sistema bonetile)
BonesError BonesDrawFrame(const BonesAnimation* animation, int frameNumber,
                         Camera camera, Texture2D* textures, int textureCount,
                         const BonesRenderConfig* config);
BonesError BonesDrawPerson(const Person* person, Camera camera,
                          Texture2D* textures, int textureCount,
                          const BonesRenderConfig* config);
BonesError BonesDrawBone(const Bone* bone, Camera camera, Texture2D texture,
                        const BonesRenderConfig* config);

// Manejo de errores
const char* BonesGetErrorString(BonesError error);
void BonesLogError(BonesError error, const char* context);

// Utilidades de debugging
void BonesPrintFrameInfo(const BonesAnimation* animation, int frameNumber);
void BonesPrintPersonInfo(const Person* person);
void BonesPrintBoneInfo(const Bone* bone);

// Análisis y estadísticas
int BonesCountValidBones(const Person* person);
float BonesCalculatePersonHeight(const Person* person);
Vector3 BonesCalculatePersonCenter(const Person* person);

// Funciones de interpolación para animación suave
Vector3 BonesInterpolatePosition(Vector3 pos1, Vector3 pos2, float t);
BonesError BonesInterpolateFrame(const BonesAnimation* animation,
                                float frameTime, Person* outInterpolated);

#endif // BONES_H
