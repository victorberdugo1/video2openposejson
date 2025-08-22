#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "raylib.h"
#include "rlgl.h"
#include "raymath.h"
#include "bonetile.h"
#include "bones3d.h"
#include <string.h>

// Tamaño base para diseño responsive
#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080

// Configuración simple por bone
typedef struct {
    char boneName[MAX_BONE_NAME_LENGTH];
    int textureIndex;
    bool visible;
    float size;
} BoneTextureConfig;

// Sistema de configuración simple
typedef struct {
    BoneTextureConfig* configs;
    int configCount;
    int configCapacity;
    bool loaded;
} SimpleTextureSystem;

// Estructura para configuración permanente de huesos
typedef struct {
    char boneName[MAX_BONE_NAME_LENGTH];
    int textureIndex;
    bool visible;
    float size;
    bool valid;
} BoneConfig;

// Estructura para datos de renderizado de un bone
typedef struct {
    Vector3 position;
    int atlasIndex;
    float rotation;
    bool mirrored;
    float distance;
    char boneName[MAX_BONE_NAME_LENGTH];
    char personId[16];
    int textureIndex;
    bool visible;
    float size;
    bool valid;
} BoneRenderData;

// Variables globales
static BoneRenderData* renderBones = NULL;
static int renderBonesCount = 0;
static int renderBonesCapacity = 0;
static SimpleTextureSystem textureSystem = { 0 };
static BoneConfig* boneConfigs = NULL;
static int boneConfigCount = 0;

// ============================================================================
// FUNCIONES DE CONFIGURACIÓN SIMPLE
// ============================================================================

// Función para verificar MAX_BONE_NAME_LENGTH
void CheckBoneNameLength() {
    #ifndef MAX_BONE_NAME_LENGTH
    #define MAX_BONE_NAME_LENGTH 32
    #endif
    
    if (MAX_BONE_NAME_LENGTH < 32) {
        TraceLog(LOG_ERROR, "MAX_BONE_NAME_LENGTH debe ser al menos 32 bytes");
    }
}

// Función para limpiar memoria de configuración
void CleanupTextureSystem() {
    if (textureSystem.configs) {
        free(textureSystem.configs);
        textureSystem.configs = NULL;
        textureSystem.configCount = 0;
        textureSystem.configCapacity = 0;
        textureSystem.loaded = false;
    }
    
    if (boneConfigs) {
        free(boneConfigs);
        boneConfigs = NULL;
        boneConfigCount = 0;
    }
}

// Cargar configuración simple desde JSON - CORREGIDO
bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        TraceLog(LOG_WARNING, "No se encontró archivo de configuración: %s. Usando valores por defecto.", filename);
        return false;
    }

    // Leer archivo línea por línea
    char line[512]; // Aumentado para mayor seguridad
    int lineCount = 0;

    // Contar líneas válidas primero
    while (fgets(line, sizeof(line), file)) {
        if (line[0] != '#' && line[0] != '\n' && strlen(line) > 5) {
            lineCount++;
        }
    }

    rewind(file);

    // Liberar configuración previa si existe
    if (system->configs) {
        free(system->configs);
        system->configs = NULL;
        system->configCount = 0;
    }

    // Asignar memoria
    system->configs = (BoneTextureConfig*)calloc(lineCount, sizeof(BoneTextureConfig));
    if (!system->configs) {
        TraceLog(LOG_ERROR, "Error allocating memory for texture config");
        fclose(file);
        return false;
    }
    
    system->configCapacity = lineCount;
    system->configCount = 0;

    // Leer configuraciones
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n' || strlen(line) < 5) continue;

        char boneName[MAX_BONE_NAME_LENGTH]; // Usar el tamaño correcto
        int textureIndex, visible;
        float size;

        // CORREGIDO: Usar MAX_BONE_NAME_LENGTH-1 en lugar de 63
        int fields = sscanf(line, "%31s %d %d %f", boneName, &textureIndex, &visible, &size);
        
        if (fields == 4) {
            if (system->configCount >= system->configCapacity) {
                TraceLog(LOG_WARNING, "Excedido límite de configuraciones, ignorando: %s", boneName);
                break;
            }
            
            BoneTextureConfig* config = &system->configs[system->configCount];
            strncpy(config->boneName, boneName, MAX_BONE_NAME_LENGTH - 1);
            config->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
            config->textureIndex = textureIndex;
            config->visible = (visible != 0);
            config->size = size;
            system->configCount++;
            
            TraceLog(LOG_DEBUG, "Config cargada: %s T%d V%d S%.2f", 
                    boneName, textureIndex, visible, size);
        } else {
            TraceLog(LOG_WARNING, "Línea inválida en configuración: %s", line);
        }
    }

    fclose(file);
    system->loaded = true;

    TraceLog(LOG_INFO, "Configuración simple cargada: %d bones configurados", system->configCount);
    return true;
}

// Cargar configuración de huesos una vez al inicio
void LoadBoneConfigurations() {
    // Liberar configuración previa si existe
    if (boneConfigs) {
        free(boneConfigs);
        boneConfigs = NULL;
        boneConfigCount = 0;
    }

    if (!textureSystem.loaded || textureSystem.configCount == 0) return;

    boneConfigCount = textureSystem.configCount;
    boneConfigs = (BoneConfig*)calloc(boneConfigCount, sizeof(BoneConfig));
    if (!boneConfigs) {
        TraceLog(LOG_ERROR, "Error allocating memory for bone configs");
        return;
    }

    for (int i = 0; i < boneConfigCount; i++) {
        strncpy(boneConfigs[i].boneName, textureSystem.configs[i].boneName, MAX_BONE_NAME_LENGTH - 1);
        boneConfigs[i].boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
        boneConfigs[i].textureIndex = textureSystem.configs[i].textureIndex;
        boneConfigs[i].visible = textureSystem.configs[i].visible;
        boneConfigs[i].size = textureSystem.configs[i].size;
        boneConfigs[i].valid = true;
    }

    TraceLog(LOG_INFO, "Configuración de huesos cargada: %d huesos", boneConfigCount);
}

// Buscar configuración por nombre de bone
BoneTextureConfig* FindBoneTextureConfig(const char* boneName) {
    for (int i = 0; i < textureSystem.configCount; i++) {
        if (strcmp(textureSystem.configs[i].boneName, boneName) == 0) {
            return &textureSystem.configs[i];
        }
    }
    return NULL;
}

// Buscar configuración de hueso por nombre
BoneConfig* FindBoneConfig(const char* boneName) {
    for (int i = 0; i < boneConfigCount; i++) {
        if (strcmp(boneConfigs[i].boneName, boneName) == 0) {
            return &boneConfigs[i];
        }
    }
    return NULL;
}

// Obtener índice de textura para un bone (con fallback inteligente)
int GetTextureIndexForBone(const char* boneName) {
    // Buscar configuración específica
    BoneConfig* config = FindBoneConfig(boneName);
    if (config) {
        return config->textureIndex;
    }

    // Fallback por patrón de nombre (mantener tu lógica original)
    if (strstr(boneName, "Nose") || strstr(boneName, "Eye") ||
        strstr(boneName, "Ear") || strstr(boneName, "Head")) {
        return 1; // textura de cabeza
    }

    if (strstr(boneName, "Neck") || strstr(boneName, "Shoulder") ||
        strstr(boneName, "Chest") || strstr(boneName, "Spine")) {
        return 0; // textura de torso
    }

    if (strstr(boneName, "Elbow") || strstr(boneName, "Wrist") ||
        strstr(boneName, "Knee") || strstr(boneName, "Ankle") ||
        strstr(boneName, "Hip")) {
        return 2; // textura de extremidades
    }

    return 0; // por defecto
}

// Verificar si un bone debe ser visible
bool IsBoneVisible(const char* boneName) {
    BoneConfig* config = FindBoneConfig(boneName);
    if (config) {
        return config->visible;
    }
    return true; // visible por defecto
}

// Obtener tamaño de un bone
float GetBoneSize(const char* boneName) {
    BoneConfig* config = FindBoneConfig(boneName);
    if (config) {
        return config->size;
    }
    return 0.35f; // tamaño por defecto
}

// Crear archivo de configuración de ejemplo - MEJORADO
void CreateExampleConfig(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) return;

    fprintf(file, "# Configuración simple de texturas por bone\n");
    fprintf(file, "# Formato: BoneName TextureIndex Visible Size\n");
    fprintf(file, "# TextureIndex: 0=torso, 1=cabeza, 2=extremidades, 3=custom...\n");
    fprintf(file, "# Visible: 1=si, 0=no\n");
    fprintf(file, "# Size: tamaño del billboard (0.1-1.0)\n");
    fprintf(file, "# IMPORTANTE: Nombres de bone deben ser exactos y únicos\n");
    fprintf(file, "\n");

    // Configuraciones basadas en tu JSON
    fprintf(file, "# Cabeza y cara\n");
    fprintf(file, "Nose 1 1 0.25\n");
    fprintf(file, "LEye 1 0 0.20\n");  // Ocultos según tu config
    fprintf(file, "REye 1 0 0.20\n");  
    fprintf(file, "LEar 1 0 0.15\n");  
    fprintf(file, "REar 1 0 0.15\n");  
    fprintf(file, "Head 1 0 0.40\n");  // Oculto si no existe en JSON
    fprintf(file, "\n");
    
    fprintf(file, "# Cuello y torso\n");
    fprintf(file, "Neck 1 1 0.30\n");    // Textura de cabeza para cuello
    fprintf(file, "LShoulder 0 1 0.35\n");
    fprintf(file, "RShoulder 0 1 0.35\n");
    fprintf(file, "\n");
    
    fprintf(file, "# Extremidades superiores\n");
    fprintf(file, "LElbow 2 1 0.30\n");
    fprintf(file, "RElbow 2 1 0.30\n");
    fprintf(file, "LWrist 2 1 0.25\n");
    fprintf(file, "RWrist 2 1 0.25\n");
    fprintf(file, "\n");
    
    fprintf(file, "# Extremidades inferiores\n");
    fprintf(file, "LHip 2 1 0.35\n");
    fprintf(file, "RHip 2 1 0.35\n");
    fprintf(file, "LKnee 2 1 0.30\n");
    fprintf(file, "RKnee 2 1 0.30\n");
    fprintf(file, "LAnkle 2 1 0.25\n");
    fprintf(file, "RAnkle 2 1 0.25\n");
    fprintf(file, "\n");
    
    fprintf(file, "# Notas:\n");
    fprintf(file, "# - Solo se renderizan bones que existen en el JSON\n");
    fprintf(file, "# - Bones con Visible=0 no se muestran\n");
    fprintf(file, "# - Size controla el tamaño del billboard\n");

    fclose(file);
    TraceLog(LOG_INFO, "Archivo de configuración de ejemplo mejorado creado: %s", filename);
}

// ============================================================================
// FUNCIONES AUXILIARES (mantener las originales)
// ============================================================================

static Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow,
    int physCols, int physRows,
    bool mirrored, bool* outMirrored)
{
    if (logicalCol < 0) logicalCol = 0;
    if (logicalCol >= ATLAS_COLS) logicalCol = ATLAS_COLS - 1;
    if (logicalRow < 0) logicalRow = 0;
    if (logicalRow >= ATLAS_ROWS) logicalRow = ATLAS_ROWS - 1;

    float physCellW = (float)tex.width / (float)physCols;
    float physCellH = (float)tex.height / (float)physRows;

    int blockW = physCols / ATLAS_COLS;
    int blockH = physRows / ATLAS_ROWS;

    int physCol = logicalCol * blockW;
    int physRow = logicalRow * blockH;

    float srcX = physCol * physCellW;
    float srcY = physRow * physCellH;
    float srcW = physCellW * blockW;
    float srcH = physCellH * blockH;

    if (outMirrored) *outMirrored = mirrored;
    return (Rectangle) { srcX, srcY, srcW, srcH };
}

void CalculateBoneRenderData(Vector3 bonePos, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored)
{
    const int indices[3][8] = {
        {  0,  4,  5,  6,  7,  6,  5,  4 },
        {  2, 12, 13, 14, 15, 14, 13, 12 },
        {  1,  8,  9, 10, 11, 10,  9,  8 }
    };
    const int topdownIndex = 3;
    const float sectorAngles[8] = { 0,45,90,135,180,225,270,315 };
    const float TOPDOWN_ANGLE = 70.0f;
    const float HIGH_THRESHOLD = 22.5f;
    const float MAIN_THRESHOLD = -22.5f;

    Vector3 camDir = Vector3Subtract(camera.position, bonePos);
    float yaw = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;
    float yawDeg = yaw * RAD2DEG;

    float horiz = sqrtf(camDir.x * camDir.x + camDir.z * camDir.z);
    float pitch = atan2f(camDir.y, horiz);
    float pitchDeg = pitch * RAD2DEG;

    int chosenRow = -1;
    bool useTopdown = false;
    bool isTopView = false;

    if (pitchDeg >= TOPDOWN_ANGLE) {
        useTopdown = true;
        isTopView = true;
    }
    else if (pitchDeg >= HIGH_THRESHOLD) {
        chosenRow = 2;
    }
    else if (pitchDeg >= MAIN_THRESHOLD) {
        chosenRow = 0;
    }
    else if (pitchDeg >= -TOPDOWN_ANGLE) {
        chosenRow = 1;
    }
    else {
        useTopdown = true;
        isTopView = false;
    }

    int sector = 0;
    float minDiff = 360.0f;
    for (int i = 0; i < 8; i++) {
        float diff = fabsf(yawDeg - sectorAngles[i]);
        if (diff > 180.0f) diff = 360.0f - diff;
        if (diff < minDiff) { minDiff = diff; sector = i; }
    }

    if (useTopdown) {
        *outChosenIndex = topdownIndex;
        if (isTopView) {
            *outRotation = sectorAngles[sector] + 180.0f;
            *outMirrored = false;
        }
        else {
            *outRotation = 360.0f - sectorAngles[sector];
            *outMirrored = true;
        }
    }
    else {
        *outChosenIndex = indices[chosenRow][sector];
        *outRotation = 0.0f;
        *outMirrored = !(sector >= 5 && sector <= 7);
    }
}

float ScaleValue(float value, int screenDimension, int baseDimension) {
    return value * ((float)screenDimension / (float)baseDimension);
}

int ScaleFontSize(int baseSize, int screenHeight) {
    return (int)(baseSize * ((float)screenHeight / BASE_HEIGHT));
}

// Función mejorada para resize con verificación de límites
bool ResizeRenderBonesArray(int newCapacity) {
    if (newCapacity <= 0 || newCapacity > 10000) { // Límite de seguridad
        TraceLog(LOG_ERROR, "Capacidad solicitada inválida: %d", newCapacity);
        return false;
    }
    
    if (newCapacity <= renderBonesCapacity) return true;

    BoneRenderData* newArray = (BoneRenderData*)realloc(renderBones,
        sizeof(BoneRenderData) * newCapacity);
    if (!newArray) {
        TraceLog(LOG_ERROR, "Error reallocating memory for %d bones", newCapacity);
        return false;
    }

    // Inicializar nueva memoria a cero
    for (int i = renderBonesCapacity; i < newCapacity; i++) {
        memset(&newArray[i], 0, sizeof(BoneRenderData));
    }

    renderBones = newArray;
    renderBonesCapacity = newCapacity;
    return true;
}

int CompareBonesByDistance(const void* a, const void* b) {
    const BoneRenderData* boneA = (const BoneRenderData*)a;
    const BoneRenderData* boneB = (const BoneRenderData*)b;

    if (boneA->distance > boneB->distance) return -1;
    if (boneA->distance < boneB->distance) return 1;
    return 0;
}

// ============================================================================
// FUNCIÓN PRINCIPAL DE RECOPILACIÓN - CORREGIDA PARA EVITAR DUPLICADOS
// ============================================================================

void CollectBonesForRendering(const BonesAnimation* animation, Camera camera) {
    renderBonesCount = 0;

    if (!animation->isLoaded) return;

    int currentFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, currentFrame)) return;

    const AnimationFrame* frame = &animation->frames[currentFrame];

    // Estimar bones únicos necesarios
    int estimatedBones = 0;
    for (int p = 0; p < frame->personCount; p++) {
        if (frame->persons[p].active) {
            estimatedBones += frame->persons[p].boneCount;
        }
    }

    if (!ResizeRenderBonesArray(estimatedBones + 10)) {
        TraceLog(LOG_ERROR, "No se pudo redimensionar array de render bones");
        return;
    }

    // NUEVO: Array para controlar bones ya procesados (evitar duplicados)
    static char processedBones[1000][MAX_BONE_NAME_LENGTH]; // Ajustar tamaño si necesario
    int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;

        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid || !bone->visible) continue;

            if (!BonesIsPositionValid(bone->position.position)) continue;

            // Buscar configuración pre-cargada
            BoneConfig* config = FindBoneConfig(bone->name);
            if (!config) {
                // Si no hay configuración específica, usar valores por defecto
                // pero solo para bones conocidos
                if (strstr(bone->name, "Nose") || strstr(bone->name, "Eye") ||
                    strstr(bone->name, "Ear") || strstr(bone->name, "Head") ||
                    strstr(bone->name, "Neck") || strstr(bone->name, "Shoulder") ||
                    strstr(bone->name, "Chest") || strstr(bone->name, "Spine") ||
                    strstr(bone->name, "Elbow") || strstr(bone->name, "Wrist") ||
                    strstr(bone->name, "Knee") || strstr(bone->name, "Ankle") ||
                    strstr(bone->name, "Hip")) {
                    // Continuar con valores por defecto
                } else {
                    continue; // Saltar bones desconocidos
                }
            } else {
                // Verificar si el bone debe ser visible según configuración
                if (!config->visible) continue;
            }

            // NUEVO: Verificar si ya procesamos este bone para esta persona
            char boneKey[MAX_BONE_NAME_LENGTH + 20];
            snprintf(boneKey, sizeof(boneKey), "%s_%s", person->personId, bone->name);
            
            bool alreadyProcessed = false;
            for (int i = 0; i < processedCount; i++) {
                if (strcmp(processedBones[i], boneKey) == 0) {
                    alreadyProcessed = true;
                    break;
                }
            }
            
            if (alreadyProcessed) continue;
            
            // Marcar como procesado
            if (processedCount < 1000) {
                strncpy(processedBones[processedCount], boneKey, MAX_BONE_NAME_LENGTH - 1);
                processedBones[processedCount][MAX_BONE_NAME_LENGTH - 1] = '\0';
                processedCount++;
            }

            float distance = Vector3Distance(camera.position, bone->position.position);
            if (distance > 50.0f) continue;

            int atlasIndex;
            float rotation;
            bool mirrored;
            CalculateBoneRenderData(bone->position.position, camera,
                &atlasIndex, &rotation, &mirrored);

            BoneRenderData* renderBone = &renderBones[renderBonesCount];
            renderBone->position = bone->position.position;
            renderBone->atlasIndex = atlasIndex;
            renderBone->rotation = rotation;
            renderBone->mirrored = mirrored;
            renderBone->distance = distance;
            renderBone->valid = true;

            // Usar configuración si existe, sino valores por defecto
            if (config) {
                renderBone->textureIndex = config->textureIndex;
                renderBone->visible = config->visible;
                renderBone->size = config->size;
            } else {
                renderBone->textureIndex = GetTextureIndexForBone(bone->name);
                renderBone->visible = IsBoneVisible(bone->name);
                renderBone->size = GetBoneSize(bone->name);
            }

            strncpy(renderBone->boneName, bone->name, MAX_BONE_NAME_LENGTH - 1);
            renderBone->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';

            strncpy(renderBone->personId, person->personId, 15);
            renderBone->personId[15] = '\0';

            renderBonesCount++;
        }
    }

    if (renderBonesCount > 1) {
        qsort(renderBones, renderBonesCount, sizeof(BoneRenderData), CompareBonesByDistance);
    }
    
    TraceLog(LOG_DEBUG, "Bones recopilados: %d únicos de %d procesados", 
             renderBonesCount, processedCount);
}

// ============================================================================
// DEBUGGING Y VERIFICACIÓN
// ============================================================================

// Función para imprimir información de debug sobre configuración
void PrintConfigurationDebug() {
    TraceLog(LOG_INFO, "=== CONFIGURACIÓN DEBUG ===");
    TraceLog(LOG_INFO, "Configuraciones bone: %d", boneConfigCount);
    TraceLog(LOG_INFO, "Sistema configuración cargado: %s", textureSystem.loaded ? "SÍ" : "NO");
    
    for (int i = 0; i < boneConfigCount && i < 10; i++) { // Solo primeros 10
        BoneConfig* config = &boneConfigs[i];
        TraceLog(LOG_INFO, "  %s: T%d V%d S%.2f", 
                config->boneName, config->textureIndex, 
                config->visible, config->size);
    }
    
    if (boneConfigCount > 10) {
        TraceLog(LOG_INFO, "  ... y %d más", boneConfigCount - 10);
    }
}

// Función para verificar que no hay bones duplicados en renderizado
void VerifyNoDuplicateRendering() {
    for (int i = 0; i < renderBonesCount; i++) {
        for (int j = i + 1; j < renderBonesCount; j++) {
            if (strcmp(renderBones[i].boneName, renderBones[j].boneName) == 0 &&
                strcmp(renderBones[i].personId, renderBones[j].personId) == 0) {
                TraceLog(LOG_WARNING, "DUPLICADO ENCONTRADO: %s en persona %s", 
                        renderBones[i].boneName, renderBones[i].personId);
            }
        }
    }
}

// ============================================================================
// FUNCIÓN MAIN COMPLETA CORREGIDA
// ============================================================================

int main(void) {
    // Verificar constantes al inicio
    CheckBoneNameLength();
    
    InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - Simple Texture Configuration");

    SetWindowState(FLAG_WINDOW_RESIZABLE);

#if defined(__linux__)
    for (int i = 0; i < 5; i++) {
        PollInputEvents();
    }
#endif

    MaximizeWindow();
    SetTargetFPS(60);

    // Configurar cámara
    Camera camera = { 0 };
    camera.position = (Vector3){ 0.0f, 0.6f, 2.5f };
    camera.target = (Vector3){ 0.0f, 0.6f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int camMode = 1;
    float orbitYaw = 0.0f, orbitPitch = -0.2f, orbitRadius = 2.5f;
    bool cameraMouseControl = false;

    // Cargar configuración simple
    if (!LoadSimpleTextureConfig(&textureSystem, "bone_textures.txt")) {
        CreateExampleConfig("bone_textures.txt");
        LoadSimpleTextureConfig(&textureSystem, "bone_textures.txt");
    }

    // Cargar configuración de huesos
    LoadBoneConfigurations();

    // Imprimir debug de configuración
    PrintConfigurationDebug();

    // Inicializar Bones3D
    BonesAnimation animation;
    BonesError result = BonesInit(&animation, 1000);

    if (result != BONES_SUCCESS) {
        TraceLog(LOG_ERROR, "Error inicializando Bones3D: %s", BonesGetErrorString(result));
        CloseWindow();
        return -1;
    }

    result = BonesLoadFromJSON(&animation, "test.json");
    if (result != BONES_SUCCESS) {
        TraceLog(LOG_WARNING, "No se pudo cargar test.json: %s", BonesGetErrorString(result));
        TraceLog(LOG_INFO, "Sistema funcionará sin datos de bones");
    }
    else {
        TraceLog(LOG_INFO, "Bones3D cargado exitosamente: %d frames", BonesGetFrameCount(&animation));
        BonesSetFrame(&animation, 0);
        BonesPrintFrameInfo(&animation, 0);
    }

    BonesRenderConfig config = BonesGetDefaultRenderConfig();
    config.drawDebugSpheres = true;
    config.debugColor = GREEN;
    config.debugSphereRadius = 0.035f;
    config.enableDepthSorting = true;
    BonesSetRenderConfig(&config);

    // Cargar texturas
    const int MAX_TEXTURES = 4;
    Texture2D textures[MAX_TEXTURES];
    int textureCount = 0;

    // Textura A (torso)
    Image imgA = LoadImage("texA.png");
    if (imgA.data == NULL) {
        imgA = GenImageColor(1024, 1024, CLITERAL(Color){60, 120, 220, 255});
        ImageDrawText(&imgA, "TORSO", 8, 8, 128, WHITE);
    }
    textures[0] = LoadTextureFromImage(imgA);
    UnloadImage(imgA);
    SetTextureFilter(textures[0], TEXTURE_FILTER_POINT);
    textureCount++;

    // Textura B (cabeza)
    Image imgB = LoadImage("texB.png");
    if (imgB.data == NULL) {
        imgB = GenImageColor(1024, 1024, CLITERAL(Color){120, 200, 80, 255});
        ImageDrawText(&imgB, "HEAD", 8, 8, 128, BLACK);
    }
    textures[1] = LoadTextureFromImage(imgB);
    UnloadImage(imgB);
    SetTextureFilter(textures[1], TEXTURE_FILTER_POINT);
    textureCount++;

    // Textura C (extremidades)
    Image imgC = LoadImage("texC.png");
    if (imgC.data == NULL) {
        imgC = GenImageColor(1024, 1024, CLITERAL(Color){220, 80, 120, 255});
        ImageDrawText(&imgC, "LIMBS", 8, 8, 128, WHITE);
    }
    textures[2] = LoadTextureFromImage(imgC);
    UnloadImage(imgC);
    SetTextureFilter(textures[2], TEXTURE_FILTER_POINT);
    textureCount++;

    const int physCols = 8, physRows = 8;

    // Variables para navegación
    int maxFrames = BonesGetFrameCount(&animation);
    int currentFrame = 0;
    bool autoPlay = false;
    float autoPlayTimer = 0.0f;
    float autoPlaySpeed = 0.1f;

    Vector3 autoCenter = { 0 };
    bool autoCenterCalculated = false;

    TraceLog(LOG_INFO, "Texturas cargadas: %d", textureCount);

    // ========================================================================
    // LOOP PRINCIPAL
    // ========================================================================
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int currentScreenWidth = GetScreenWidth();
        int currentScreenHeight = GetScreenHeight();

        // Controles de navegación de frames
        if (animation.isLoaded && maxFrames > 0) {
            if (IsKeyPressed(KEY_LEFT) && currentFrame > 0) {
                currentFrame--;
                BonesSetFrame(&animation, currentFrame);
                autoCenterCalculated = false;
            }
            if (IsKeyPressed(KEY_RIGHT) && currentFrame < maxFrames - 1) {
                currentFrame++;
                BonesSetFrame(&animation, currentFrame);
                autoCenterCalculated = false;
            }
            if (IsKeyPressed(KEY_HOME)) {
                currentFrame = 0;
                BonesSetFrame(&animation, currentFrame);
                autoCenterCalculated = false;
            }
            if (IsKeyPressed(KEY_END) && maxFrames > 0) {
                currentFrame = maxFrames - 1;
                BonesSetFrame(&animation, currentFrame);
                autoCenterCalculated = false;
            }
            if (IsKeyPressed(KEY_SPACE)) {
                autoPlay = !autoPlay;
            }

            // Auto-reproducir
            if (autoPlay && maxFrames > 1) {
                autoPlayTimer += dt;
                if (autoPlayTimer >= autoPlaySpeed) {
                    autoPlayTimer = 0.0f;
                    currentFrame = (currentFrame + 1) % maxFrames;
                    BonesSetFrame(&animation, currentFrame);
                    autoCenterCalculated = false;
                }
            }
        }

        // Recarga de configuración con F5
        if (IsKeyPressed(KEY_F5)) {
            TraceLog(LOG_INFO, "Recargando configuración de texturas...");
            CleanupTextureSystem();
            LoadSimpleTextureConfig(&textureSystem, "bone_textures.txt");
            LoadBoneConfigurations();
            PrintConfigurationDebug();
        }

        // Debug de duplicados con F6
        if (IsKeyPressed(KEY_F6)) {
            VerifyNoDuplicateRendering();
        }

        // Calcular centro automático
        if (animation.isLoaded && !autoCenterCalculated) {
            if (BonesIsValidFrame(&animation, currentFrame)) {
                const AnimationFrame* frame = &animation.frames[currentFrame];
                Vector3 totalPos = { 0 };
                int validBoneCount = 0;

                for (int p = 0; p < frame->personCount; p++) {
                    const Person* person = &frame->persons[p];
                    if (!person->active) continue;

                    for (int b = 0; b < person->boneCount; b++) {
                        const Bone* bone = &person->bones[b];
                        if (bone->position.valid && BonesIsPositionValid(bone->position.position)) {
                            totalPos = Vector3Add(totalPos, bone->position.position);
                            validBoneCount++;
                        }
                    }
                }

                if (validBoneCount > 0) {
                    autoCenter = Vector3Scale(totalPos, 1.0f / validBoneCount);
                    autoCenterCalculated = true;
                }
            }
        }

        // Control de cámara
        if (IsKeyPressed(KEY_ONE)) {
            camMode = 1;
            cameraMouseControl = false;
            EnableCursor();
        }
        if (IsKeyPressed(KEY_TWO)) {
            camMode = 2;
            cameraMouseControl = true;
            DisableCursor();
            Vector3 target = autoCenterCalculated ? autoCenter : (Vector3) { 0, 0.6f, 0 };
            camera.position = (Vector3){ target.x, target.y + 0.5f, target.z + 2.0f };
            camera.target = target;
            Vector3 direction = Vector3Subtract(target, camera.position);
            orbitYaw = atan2f(direction.x, direction.z);
            orbitPitch = atan2f(direction.y, sqrtf(direction.x * direction.x + direction.z * direction.z));
        }

        if (IsKeyPressed(KEY_M)) {
            cameraMouseControl = !cameraMouseControl;
            if (cameraMouseControl) {
                DisableCursor();
            }
            else {
                EnableCursor();
            }
        }

        Vector3 cameraTarget = autoCenterCalculated ? autoCenter : (Vector3) { 0, 0.6f, 0 };

        if (camMode == 1) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                Vector2 mouseDelta = GetMouseDelta();
                orbitYaw += mouseDelta.x * 0.01f;
                orbitPitch += -mouseDelta.y * 0.01f;
                orbitPitch = Clamp(orbitPitch, -1.4f, 1.4f);
            }

            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                orbitRadius -= wheel * 0.5f;
                orbitRadius = Clamp(orbitRadius, 0.5f, 20.0f);
            }

            float x = orbitRadius * cosf(orbitPitch) * sinf(orbitYaw);
            float y = orbitRadius * sinf(orbitPitch);
            float z = orbitRadius * cosf(orbitPitch) * cosf(orbitYaw);

            camera.position = (Vector3){ cameraTarget.x + x, cameraTarget.y + y, cameraTarget.z + z };
            camera.target = cameraTarget;
        }
        else {
            if (cameraMouseControl) {
                Vector2 mouseDelta = GetMouseDelta();
                orbitYaw -= mouseDelta.x * 0.003f;
                orbitPitch -= mouseDelta.y * 0.003f;
                orbitPitch = Clamp(orbitPitch, -1.49f, 1.49f);
            }

            Vector3 forward = {
                sinf(orbitYaw) * cosf(orbitPitch),
                sinf(orbitPitch),
                cosf(orbitYaw) * cosf(orbitPitch)
            };
            forward = Vector3Normalize(forward);

            Vector3 right = {
                sinf(orbitYaw - PI / 2),
                0,
                cosf(orbitYaw - PI / 2)
            };
            right = Vector3Normalize(right);

            float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 8.0f : 3.0f;
            if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(forward, speed * dt));
            if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(forward, speed * dt));
            if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(right, speed * dt));
            if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(right, speed * dt));

            if (IsKeyDown(KEY_SPACE)) camera.position.y += speed * dt;
            if (IsKeyDown(KEY_LEFT_CONTROL)) camera.position.y -= speed * dt;

            camera.target = Vector3Add(camera.position, forward);
        }

        // Recopilar bones para renderizado
        CollectBonesForRendering(&animation, camera);

        // ====================================================================
        // RENDERIZADO
        // ====================================================================
        BeginDrawing();
        ClearBackground(RAYWHITE);

        BeginMode3D(camera);
        DrawGrid(24, 0.5f);

        if (autoCenterCalculated) {
            DrawSphereWires(autoCenter, 0.05f, 8, 8, ORANGE);
        }

        // Renderizar bones con configuración simple
        if (renderBonesCount > 0) {
            rlDisableDepthTest();
            BeginBlendMode(BLEND_ALPHA);

            for (int i = 0; i < renderBonesCount; i++) {
                const BoneRenderData* bone = &renderBones[i];
                if (!bone->valid || !bone->visible) continue;

                // Seleccionar textura
                int texIndex = bone->textureIndex;
                if (texIndex >= textureCount) texIndex = 0;
                Texture2D currentTex = textures[texIndex];

                // Calcular tamaño del mundo
                float physCellW = (float)currentTex.width / (float)physCols;
                float physCellH = (float)currentTex.height / (float)physRows;
                float logicalCellW = physCellW * (physCols / ATLAS_COLS);
                float logicalCellH = physCellH * (physRows / ATLAS_ROWS);
                float aspect = logicalCellW / logicalCellH;
                Vector2 worldSize = (Vector2){ bone->size * aspect, bone->size };

                // Calcular rectángulo fuente
                int logicalCol = bone->atlasIndex % ATLAS_COLS;
                int logicalRow = bone->atlasIndex / ATLAS_COLS;
                bool finalMirror = false;
                Rectangle src = SrcFromLogical(currentTex, logicalCol, logicalRow,
                    physCols, physRows, bone->mirrored, &finalMirror);

                // Dibujar bone
                DrawBonetileCustom(currentTex, camera, src, bone->position,
                    worldSize, bone->rotation, finalMirror);

                // Debug sphere con color por textura
                if (config.drawDebugSpheres) {
                    Color debugCol = (texIndex == 0) ? RED :
                        (texIndex == 1) ? BLUE :
                        (texIndex == 2) ? PURPLE : GREEN;
                    DrawSphereWires(bone->position, config.debugSphereRadius, 8, 8, debugCol);
                }
            }

            EndBlendMode();
            rlEnableDepthMask();
        }

        EndMode3D();

        // ================================================================
        // UI OVERLAY SIMPLIFICADA
        // ================================================================
        int baseFontSize = ScaleFontSize(16, currentScreenHeight);
        int titleSize = ScaleFontSize(20, currentScreenHeight);
        int smallFontSize = ScaleFontSize(12, currentScreenHeight);

        // Información principal
        DrawText("BONES3D - SIMPLE TEXTURE CONFIG",
            ScaleValue(20, currentScreenWidth, BASE_WIDTH),
            ScaleValue(20, currentScreenHeight, BASE_HEIGHT),
            titleSize, DARKGREEN);

        // Estado del sistema
        if (animation.isLoaded) {
            DrawText(TextFormat("JSON: OK | Frames: %d | Current: %d | Config: %s",
                maxFrames, currentFrame + 1, textureSystem.loaded ? "LOADED" : "DEFAULT"),
                ScaleValue(20, currentScreenWidth, BASE_WIDTH),
                ScaleValue(50, currentScreenHeight, BASE_HEIGHT),
                baseFontSize, DARKGREEN);

            DrawText(TextFormat("Bones Rendered: %d | Configured: %d | Textures: %d",
                renderBonesCount, boneConfigCount, textureCount),
                ScaleValue(20, currentScreenWidth, BASE_WIDTH),
                ScaleValue(70, currentScreenHeight, BASE_HEIGHT),
                baseFontSize, DARKBLUE);
        }
        else {
            DrawText("NO ANIMATION DATA LOADED",
                ScaleValue(20, currentScreenWidth, BASE_WIDTH),
                ScaleValue(50, currentScreenHeight, BASE_HEIGHT),
                baseFontSize, ORANGE);
        }

        // Lista de bones activos con su configuración
        int yOffset = ScaleValue(100, currentScreenHeight, BASE_HEIGHT);
        int maxBonesToShow = 8;
        for (int i = 0; i < renderBonesCount && i < maxBonesToShow; i++) {
            const BoneRenderData* bone = &renderBones[i];
            BoneConfig* config = FindBoneConfig(bone->boneName);

            Color boneColor = (bone->textureIndex == 0) ? RED :
                (bone->textureIndex == 1) ? BLUE :
                (bone->textureIndex == 2) ? PURPLE : GREEN;

            const char* configStr = config ? "CFG" : "DEF";
            DrawText(TextFormat("%s[%s]: T%d S%.2f %s",
                bone->boneName, bone->personId, bone->textureIndex, bone->size, configStr),
                ScaleValue(20, currentScreenWidth, BASE_WIDTH),
                yOffset + i * ScaleValue(18, currentScreenHeight, BASE_HEIGHT),
                smallFontSize, boneColor);
        }

        if (renderBonesCount > maxBonesToShow) {
            DrawText(TextFormat("... and %d more bones", renderBonesCount - maxBonesToShow),
                ScaleValue(20, currentScreenWidth, BASE_WIDTH),
                yOffset + maxBonesToShow * ScaleValue(18, currentScreenHeight, BASE_HEIGHT),
                smallFontSize, GRAY);
        }

        // Panel de configuración simplificado
        int panelX = currentScreenWidth - ScaleValue(400, currentScreenWidth, BASE_WIDTH);
        int panelY = ScaleValue(100, currentScreenHeight, BASE_HEIGHT);
        int panelW = ScaleValue(380, currentScreenWidth, BASE_WIDTH);
        int panelH = ScaleValue(300, currentScreenHeight, BASE_HEIGHT);

        DrawRectangle(panelX, panelY, panelW, panelH, Fade(BLACK, 0.7f));
        DrawRectangleLines(panelX, panelY, panelW, panelH, WHITE);

        DrawText("TEXTURE CONFIG", panelX + 10, panelY + 10,
            ScaleFontSize(16, currentScreenHeight), WHITE);

        DrawText("bone_textures.txt:", panelX + 10, panelY + 35,
            ScaleFontSize(14, currentScreenHeight), LIGHTGRAY);

        // Mostrar algunas configuraciones
        int configY = panelY + 60;
        int maxConfigsToShow = 12;
        for (int i = 0; i < boneConfigCount && i < maxConfigsToShow; i++) {
            BoneConfig* config = &boneConfigs[i];
            Color textColor = config->visible ? WHITE : GRAY;

            DrawText(TextFormat("%s T%d %s %.2f",
                config->boneName, config->textureIndex,
                config->visible ? "V" : "H", config->size),
                panelX + 10, configY + i * 16,
                ScaleFontSize(12, currentScreenHeight), textColor);
        }

        if (boneConfigCount > maxConfigsToShow) {
            DrawText(TextFormat("... and %d more", boneConfigCount - maxConfigsToShow),
                panelX + 10, configY + maxConfigsToShow * 16,
                ScaleFontSize(12, currentScreenHeight), DARKGRAY);
        }

        // ================================================================
        // CONTROLES SIMPLIFICADOS
        // ================================================================
        int controlsY = currentScreenHeight - ScaleValue(120, currentScreenHeight, BASE_HEIGHT);

        DrawText("SIMPLE CONTROLS:",
            ScaleValue(20, currentScreenWidth, BASE_WIDTH),
            controlsY,
            ScaleFontSize(16, currentScreenHeight), DARKGREEN);

        DrawText("Camera: 1=Orbit | 2=FPS | M=Mouse | WASD=Move",
            ScaleValue(20, currentScreenWidth, BASE_WIDTH),
            controlsY + ScaleValue(20, currentScreenHeight, BASE_HEIGHT),
            ScaleFontSize(14, currentScreenHeight), DARKGRAY);

        DrawText("Animation: ←→=Frame | Space=Auto | F5=Reload Config",
            ScaleValue(20, currentScreenWidth, BASE_WIDTH),
            controlsY + ScaleValue(40, currentScreenHeight, BASE_HEIGHT),
            ScaleFontSize(14, currentScreenHeight), DARKGRAY);

        DrawText("Debug: F6=Check Duplicates",
            ScaleValue(20, currentScreenWidth, BASE_WIDTH),
            controlsY + ScaleValue(60, currentScreenHeight, BASE_HEIGHT),
            ScaleFontSize(14, currentScreenHeight), DARKGRAY);

        // Status bar
        DrawRectangle(0, currentScreenHeight - ScaleValue(25, currentScreenHeight, BASE_HEIGHT),
            currentScreenWidth, ScaleValue(25, currentScreenHeight, BASE_HEIGHT),
            Fade(BLACK, 0.8f));

        DrawText(TextFormat("Frame: %d/%d | Bones: %d | Config: %s | FPS: %d",
            currentFrame + 1, maxFrames, renderBonesCount,
            textureSystem.loaded ? "CUSTOM" : "DEFAULT", GetFPS()),
            ScaleValue(10, currentScreenWidth, BASE_WIDTH),
            currentScreenHeight - ScaleValue(20, currentScreenHeight, BASE_HEIGHT),
            ScaleFontSize(14, currentScreenHeight), WHITE);

        // Indicador de auto-play
        if (autoPlay) {
            const char* playText = "AUTO-PLAY";
            int playTextWidth = MeasureText(playText, ScaleFontSize(14, currentScreenHeight));
            DrawText(playText,
                currentScreenWidth - playTextWidth - ScaleValue(10, currentScreenWidth, BASE_WIDTH),
                currentScreenHeight - ScaleValue(20, currentScreenHeight, BASE_HEIGHT),
                ScaleFontSize(14, currentScreenHeight), LIME);
        }

        EndDrawing();
    }

    // ========================================================================
    // LIMPIEZA
    // ========================================================================

    // Liberar array dinámico
    if (renderBones) {
        free(renderBones);
        renderBones = NULL;
        renderBonesCount = 0;
        renderBonesCapacity = 0;
    }

    // Usar función de limpieza mejorada
    CleanupTextureSystem();

    // Liberar texturas
    for (int i = 0; i < textureCount; i++) {
        UnloadTexture(textures[i]);
    }

    // Liberar sistema bones3d
    BonesFree(&animation);
    CloseWindow();

    return 0;
}
