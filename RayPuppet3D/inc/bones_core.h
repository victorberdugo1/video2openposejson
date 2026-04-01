#ifndef BONES_CORE_H
#define BONES_CORE_H

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define ATLAS_COLS                  4
#define ATLAS_ROWS                  4
#define ASYM_ATLAS_COLS             5
#define ASYM_ATLAS_ROWS             5
#define MAX_BONE_NAME_LENGTH        64
#define MAX_FILE_PATH_LENGTH        512
#define MAX_BONES_PER_PERSON        32
#define MAX_FRAMES                  10000
#define MAX_PERSONS                 10
#define MAX_TEXTURES                64
#define MAX_RENDER_ITEMS            512
#define MAX_ORNAMENTS               32

#define TORSO_BIAS                  0.001f
#define BONE_BIAS                   0.0f
#define HEAD_BIAS                   -0.001f
#define INDEX_BIAS                  -0.00001f
#define Z_FIGHTING_THRESHOLD        0.01f
#define MIN_DISTANCE_THRESHOLD      0.001f

#define BONES_MAX_TEXTURE_VARIANTS  16
#define BONES_MAX_TEXTURE_SETS      64
#define BONES_MAX_ANIM_EVENTS       256
#define BONES_MAX_ANIM_CLIPS        32
#define BONES_AE_MAX_NAME           64
#define BONES_AE_MAX_PATH           256

#define HEAD_DEPTH_OFFSET           0.04f
#define CHEST_OFFSET_Y              -0.06f
#define CHEST_OFFSET_Z              -0.005f
#define CHEST_FALLBACK_Y            -0.08f
#define HIP_OFFSET_Y                -0.02f

#define SLASH_TRAIL_MAX_SEGMENTS    64
#define SLASH_TRAIL_MAX_ACTIVE      8

#define BONES_MAX_MODEL3D_EVENTS    8
#define BONES_MODEL3D_MAX_PATH      256
#define BONES_MODEL3D_MAX_BONE      64

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

typedef enum {
    TORSO_CHEST = 0,
    TORSO_HIP   = 1
} TorsoType;

typedef enum {
    ANIM_EVENT_TEXTURE,
    ANIM_EVENT_SOUND,
    ANIM_EVENT_PARTICLE,
    ANIM_EVENT_SLASH,
    ANIM_EVENT_MODEL3D,
    ANIM_EVENT_CUSTOM
} AnimEventType;

typedef enum {
    PHYSICS_STIFF,
    PHYSICS_MEDIUM,
    PHYSICS_SOFT,
    PHYSICS_VERYSOFT
} OrnamentPhysicsPreset;

typedef struct {
    Vector3 position;
    float   confidence;
    bool    valid;
} BonePosition;

typedef struct {
    char          name[MAX_BONE_NAME_LENGTH];
    BonePosition  position;
    int           textureIndex;
    Vector2       size;
    float         rotation;
    bool          mirrored;
    bool          visible;
} Bone;

typedef struct Person {
    char personId[16];
    Bone bones[MAX_BONES_PER_PERSON];
    int  boneCount;
    bool active;
} Person;

typedef struct {
    int    frameNumber;
    Person persons[MAX_PERSONS];
    int    personCount;
    bool   valid;
    bool   isOriginalKeyframe;
} AnimationFrame;

typedef struct {
    AnimationFrame* frames;
    int             frameCount;
    int             maxFrames;
    int             currentFrame;
    bool            isLoaded;
    char            filePath[256];
} BonesAnimation;

typedef struct {
    Vector3 position;
    Vector3 forward;
    Vector3 up;
    Vector3 right;
    float   yaw, pitch, roll;
    bool    valid;
} BoneOrientation;

typedef struct {
    Vector3 position;
    Vector3 forward;
    Vector3 up;
    Vector3 right;
    float   yaw, pitch, roll;
    bool    valid;
} HeadOrientation;

typedef struct {
    Vector3 position;
    Vector3 forward;
    Vector3 up;
    Vector3 right;
    float   yaw, pitch, roll;
    bool    valid;
} TorsoOrientation;

typedef struct {
    bool    valid;
    Vector3 chestPosition;
    Vector3 hipPosition;
    Vector3 spineDirection;
    Vector3 spineRight;
    Vector3 spineForward;
} VirtualSpine;

typedef struct {
    Vector3 neck, lShoulder, rShoulder, lHip, rHip;
    bool    hasNeck, hasLShoulder, hasRShoulder, hasLHip, hasRHip;
    int     shoulderCount, hipCount;
} CachedBones;

typedef struct {
    Vector3 pos0;
    Vector3 pos1;
} BoneConnectionPositions;

typedef struct {
    char          personId[16];
    char          boneName[MAX_BONE_NAME_LENGTH];
    Vector3       position;
    BoneOrientation orientation;
    int           atlasIndex;
    float         rotation;
    bool          mirrored;
    float         distance;
    char          texturePath[MAX_FILE_PATH_LENGTH];
    float         size;
    bool          valid;
    bool          visible;
    bool          isAsymmetric;
} BoneRenderData;

typedef struct {
    Vector3         position;
    HeadOrientation orientation;
    float           size;
    bool            visible;
    bool            valid;
    char            texturePath[MAX_FILE_PATH_LENGTH];
    char            personId[16];
} HeadRenderData;

typedef struct {
    bool            valid;
    bool            visible;
    Vector3         position;
    TorsoOrientation orientation;
    TorsoType       type;
    float           size;
    char            texturePath[MAX_FILE_PATH_LENGTH];
    char            personId[16];
    const Person*   person;
    bool            disableCompensation;
    bool            isAsymmetric;
} TorsoRenderData;

typedef struct {
    int   type;
    int   index;
    float distance;
    float depthBias;
    bool  hasZFighting;
} RenderItem;

typedef struct {
    Texture2D textures[MAX_TEXTURES];
    char      texturePaths[MAX_TEXTURES][MAX_FILE_PATH_LENGTH];
    int       textureCount;
    int       physCols, physRows;
    Camera    camera;
} BonesRenderer;

typedef struct {
    float defaultBoneSize;
    bool  drawDebugSpheres;
    bool  enableDepthSorting;
    Color debugColor;
    float debugSphereRadius;
    bool  showInvalidBones;
} BonesRenderConfig;

typedef struct {
    char  boneName[MAX_BONE_NAME_LENGTH];
    char  texturePath[MAX_FILE_PATH_LENGTH];
    int   visibleMode;
    float size;
} BoneTextureConfig;

typedef struct {
    BoneTextureConfig* configs;
    int                configCount;
    int                configCapacity;
    bool               loaded;
    time_t             lastModified;
} SimpleTextureSystem;

typedef struct {
    char  boneName[MAX_BONE_NAME_LENGTH];
    char  texturePath[MAX_FILE_PATH_LENGTH];
    float size;
    int   visibleMode;
    bool  visible;
    bool  valid;
} BoneConfig;

typedef struct {
    char variantName[BONES_AE_MAX_NAME];
    char texturePath[BONES_AE_MAX_PATH];
    bool valid;
} BoneTextureVariant;

typedef struct {
    char             boneName[BONES_AE_MAX_NAME];
    BoneTextureVariant variants[BONES_MAX_TEXTURE_VARIANTS];
    int              variantCount;
    int              activeVariantIndex;
    bool             valid;
} BoneTextureSet;

typedef struct {
    BoneTextureSet sets[BONES_MAX_TEXTURE_SETS];
    int            setCount;
    bool           loaded;
} TextureSetCollection;

typedef struct {
    int   frameStart;
    int   frameEnd;
    char  boneName[BONES_AE_MAX_NAME];
    char  texturePath[BONES_AE_MAX_PATH];
    float emitOffsetX, emitOffsetY, emitOffsetZ;
    bool  trailEnabled;
    float widthStart;
    float widthEnd;
    Color colorStart;
    Color colorEnd;
    float lifetime;
    int   segments;
} SlashEventConfig;

typedef struct {
    Vector3 position;
    float   age;
    float   lifetime;
    bool    active;
} SlashTrailPoint;

typedef struct {
    bool             active;
    bool             alive;
    int              slashIndex;
    SlashEventConfig config;
    SlashTrailPoint  points[SLASH_TRAIL_MAX_SEGMENTS];
    int              pointCount;
    float            spawnTimer;
    float            spawnRate;
    Texture2D        texture;
    bool             hasTexture;
} SlashTrail;

typedef struct {
    SlashTrail trails[SLASH_TRAIL_MAX_ACTIVE];
} SlashTrailSystem;

typedef struct {
    int     frameStart;
    int     frameEnd;
    char    boneName[BONES_MODEL3D_MAX_BONE];
    char    modelPath[BONES_MODEL3D_MAX_PATH];
    float   scale;
    Vector3 offset;
    Vector3 rotationEuler;
    bool    valid;
} Model3DEventConfig;

typedef struct {
    Model3DEventConfig config;
    Model              model;
    bool               loaded;
    bool               visible;
} Model3DInstance;

typedef struct {
    Model3DInstance instances[BONES_MAX_MODEL3D_EVENTS];
    int             instanceCount;
} Model3DAttachmentSystem;

typedef struct {
    float         time;
    AnimEventType type;
    char          boneName[BONES_AE_MAX_NAME];
    char          personId[BONES_AE_MAX_NAME];
    char          variantName[BONES_AE_MAX_NAME];
    char          stringParam[BONES_AE_MAX_NAME];
    bool          processed;
    bool          valid;
} AnimationEvent;

typedef struct {
    char               name[BONES_AE_MAX_NAME];
    float              fps;
    int                startFrame;
    int                endFrame;
    bool               loop;
    AnimationEvent     events[BONES_MAX_ANIM_EVENTS];
    int                eventCount;
    SlashEventConfig   slashEvents[16];
    int                slashEventCount;
    Model3DEventConfig model3dEvents[BONES_MAX_MODEL3D_EVENTS];
    int                model3dEventCount;
    bool               valid;
} AnimationClipMetadata;

typedef struct {
    TextureSetCollection*  textureSets;
    AnimationClipMetadata* clips;
    int                    clipCount;
    int                    clipCapacity;
    int                    currentClipIndex;
    float                  localTime;
    bool                   playing;
    void*                  bonesAnimation;
    int                    currentFrameInJSON;
    bool                   valid;
} AnimationController;

typedef struct {
    char name[64];
    char texturesConfigPath[256];
    char textureSetsPath[256];
    char animationsPath[256];
} CharacterProfile;

typedef struct {
    char                  name[MAX_BONE_NAME_LENGTH];
    char                  anchorBoneName[MAX_BONE_NAME_LENGTH];
    Vector3               offsetFromAnchor;
    char                  texturePath[MAX_FILE_PATH_LENGTH];
    bool                  visible;
    float                 size;

    char                  chainParentName[MAX_BONE_NAME_LENGTH];
    bool                  isChained;
    int                   chainParentIndex;
    float                 chainRestLength;

    OrnamentPhysicsPreset physicsPreset;

    Vector3               currentPosition;
    Vector3               previousPosition;
    Vector3               velocity;
    bool                  initialized;

    float                 stiffness;
    float                 damping;
    float                 gravityScale;

    bool                  valid;
} BoneOrnament;

typedef struct {
    BoneOrnament ornaments[MAX_ORNAMENTS];
    int          ornamentCount;
    bool         loaded;
} OrnamentSystem;

typedef struct AnimatedCharacter {
    BonesAnimation          animation;
    BonesRenderer*          renderer;
    SimpleTextureSystem     textureSystem;
    BoneConfig*             boneConfigs;
    int                     boneConfigCount;
    BonesRenderConfig       renderConfig;
    BoneRenderData*         renderBones;
    int                     renderBonesCount;
    int                     renderBonesCapacity;
    HeadRenderData*         renderHeads;
    int                     renderHeadsCount;
    int                     renderHeadsCapacity;
    TorsoRenderData*        renderTorsos;
    int                     renderTorsosCount;
    int                     renderTorsosCapacity;
    int                     currentFrame;
    int                     maxFrames;
    bool                    autoPlay;
    float                   autoPlaySpeed;
    Vector3                 autoCenter;
    bool                    autoCenterCalculated;
    bool                    renderHeadBillboards;
    bool                    renderTorsoBillboards;
    int                     lastProcessedFrame;
    bool                    forceUpdate;
    AnimationController*    animController;
    TextureSetCollection*   textureSets;
    OrnamentSystem*         ornaments;
    SlashTrailSystem        slashTrails;
    Model3DAttachmentSystem model3dAttachments;
    Vector3                 worldPosition;
    float                   worldRotation;
    Vector3                 worldPivot;
    bool                    hasWorldTransform;
    bool                    lockRootXZ;
} AnimatedCharacter;

typedef struct {
    bool    active;
    char    boneName[BONES_AE_MAX_NAME];
    Vector3 bonePosition;
    int     frameStart;
    int     frameEnd;
    int     slashIndex;
} SlashHitboxInfo;

static const struct {
    const char* boneName;
    const char* connectedBone;
    float       projectionFactor;
} MIDPOINT_CONNECTIONS[] = {
    {"Neck",     "HEAD_CALCULATED", 1.0f},
    {"LShoulder","LElbow",          1.0f}, {"RShoulder","RElbow",  1.0f},
    {"LElbow",   "LWrist",          1.0f}, {"RElbow",   "RWrist",  1.0f},
    {"LWrist",   "LElbow",          1.3f}, {"RWrist",   "RElbow",  1.3f},
    {"LHip",     "LKnee",           1.0f}, {"RHip",     "RKnee",   1.0f},
    {"LKnee",    "LAnkle",          1.0f}, {"RKnee",    "RAnkle",  1.0f},
    {"LAnkle",   "FOOT_FORWARD",    1.0f}, {"RAnkle",   "FOOT_FORWARD", 1.0f},
    {"", "", 0.0f}
};

static const struct {
    const char* boneName;
    const char* connections[3];
    float       priority[3];
} BONE_CONNECTIONS[] = {
    {"LShoulder", {"LShoulder","LElbow",  ""}, {1.0f, 0.8f, 0.0f}},
    {"LElbow",    {"LElbow",   "LWrist",  ""}, {1.0f, 0.8f, 0.0f}},
    {"LWrist",    {"LWrist",   "LElbow",  ""}, {1.0f, 0.0f, 0.0f}},
    {"RShoulder", {"RShoulder","RElbow",  ""}, {1.0f, 0.8f, 0.0f}},
    {"RElbow",    {"RElbow",   "RWrist",  ""}, {1.0f, 0.8f, 0.0f}},
    {"RWrist",    {"RElbow",   "RWrist",  ""}, {1.0f, 0.0f, 0.0f}},
    {"LHip",      {"LHip",     "LKnee",   ""}, {1.0f, 0.8f, 0.0f}},
    {"LKnee",     {"LKnee",    "LAnkle",  ""}, {1.0f, 0.8f, 0.0f}},
    {"LAnkle",    {"LKnee",    "LAnkle",  ""}, {1.0f, 0.8f, 0.0f}},
    {"RHip",      {"RHip",     "RKnee",   ""}, {1.0f, 0.8f, 0.0f}},
    {"RKnee",     {"RKnee",    "RAnkle",  ""}, {1.0f, 0.8f, 0.0f}},
    {"RAnkle",    {"RKnee",    "RAnkle",  ""}, {1.0f, 0.8f, 0.0f}},
    {"Neck",      {"Neck",     "Head",    ""}, {0.8f, 1.0f, 0.0f}},
    {"", {"","",""}, {0.0f, 0.0f, 0.0f}}
};

static const struct {
    const char* boneName;
    const char* primaryConnection;
    const char* secondaryConnection;
    bool        reverseForward;
    bool        isLimb;
    bool        useStableOrientation;
} BONE_ORIENTATIONS[] = {
    {"LShoulder", "LElbow",           "Neck",             false, true,  true},
    {"LElbow",    "Neck",             "LWrist",           true,  true,  true},
    {"LWrist",    "LElbow",           "",                 false, false, true},
    {"RShoulder", "RElbow",           "Neck",             true,  true,  true},
    {"RElbow",    "Neck",             "RWrist",           false, true,  true},
    {"RWrist",    "RElbow",           "",                 false, false, true},
    {"LHip",      "LKnee",           "FRONT_CALCULATED", true,  true,  true},
    {"LKnee",     "LAnkle",          "LHip",             true,  true,  true},
    {"LAnkle",    "LKnee",           "FRONT_CALCULATED", true,  false, true},
    {"RHip",      "RKnee",           "FRONT_CALCULATED", false, true,  true},
    {"RKnee",     "RAnkle",          "RHip",             true,  true,  true},
    {"RAnkle",    "RKnee",           "REAR_CALCULATED",  false, false, true},
    {"Neck",      "HEAD_CALCULATED", "",                 true,  false, true},
    {"", "", "", false, false, false}
};

static const struct {
    const char* boneName;
    float       yawOffset;
    float       pitchOffset;
    float       rollOffset;
} BONE_ANGLE_OFFSETS[] = {
    {"LShoulder",  90.0f, 180.0f,  -90.0f},
    {"LElbow",     90.0f, 225.0f,  -90.0f},
    {"LWrist",     90.0f, 180.0f,   90.0f},
    {"RShoulder", -90.0f,   0.0f,   90.0f},
    {"RElbow",    -90.0f, -90.0f,   90.0f},
    {"RWrist",     90.0f, 180.0f,   90.0f},
    {"LHip",       90.0f, -45.0f,   90.0f},
    {"LKnee",      90.0f,  90.0f,   90.0f},
    {"LAnkle",     90.0f, -90.0f,   90.0f},
    {"RHip",      -90.0f, -45.0f,  -90.0f},
    {"RKnee",     -90.0f,  90.0f,   90.0f},
    {"RAnkle",     90.0f, 180.0f,   90.0f},
    {"Neck",      -90.0f, 180.0f,  -90.0f},
    {"", 0.0f, 0.0f, 0.0f}
};

static const struct {
    const char* boneName;
    float       scaleFactorUp;
    float       scaleFactorDown;
} BONE_SCALE_FACTORS[] = {
    {"LShoulder", 0.3f, 1.2f},
    {"RShoulder", 0.3f, 1.2f},
    {"LElbow",    0.0f, 0.9f},
    {"RElbow",    0.0f, 0.9f},
    {"LHip",      0.1f, 1.0f},
    {"RHip",      0.1f, 1.0f},
    {"LKnee",     0.1f, 1.0f},
    {"RKnee",     0.1f, 1.0f},
    {"", 0.5f, 0.5f}
};

static AnimationFrame g_transitionFromFrame;
static AnimationFrame g_transitionToFrame;
static bool           g_isTransitioning    = false;
static float          g_transitionTime     = 0.0f;
static float          g_transitionDuration = 0.85f;
static bool           g_hasValidFromFrame  = false;

static BonesRenderConfig g_renderConfig = {
    .defaultBoneSize    = 0.35f,
    .drawDebugSpheres   = false,
    .enableDepthSorting = true,
    .debugColor         = RED,
    .debugSphereRadius  = 0.035f,
    .showInvalidBones   = false
};

static inline bool BonesIsPositionValid(Vector3 position);
static inline int BonesGetCurrentFrame(const BonesAnimation* animation);
static inline bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber);
static inline BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber);
static inline int BonesGetFrameCount(const BonesAnimation* animation);
static inline bool BonesCreateMissingFrames(BonesAnimation* animation);
static inline BonesError BonesInit(BonesAnimation* animation, int maxFrames);
static inline void BonesFree(BonesAnimation* animation);
static inline BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath);
static inline BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString);

static inline Vector3 SafeNormalize(Vector3 v);
static inline bool IsWristBone(const char* boneName);
static inline bool IsAsymmetricBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
static inline CachedBones CacheBones(const Person* person);
static inline Vector3 GetBonePositionByName(const Person* person, const char* boneName);
static inline Vector3 CalculateHeadPosition(const Person* person);
static inline Vector3 CalculateChestPosition(const Person* person);
static inline Vector3 CalculateHipPosition(const Person* person);
static inline VirtualSpine CalculateVirtualSpine(const Person* person);
static inline TorsoOrientation CreateOrientation(Vector3 pos, Vector3 forward, Vector3 up, Vector3 right);
static inline TorsoOrientation CalculateChestOrientation(const Person* person);
static inline TorsoOrientation CalculateHipOrientation(const Person* person);
static inline HeadOrientation CalculateHeadOrientation(const Person* person);
static inline BoneOrientation CalculateBoneOrientation(const char* boneName, const Person* person, Vector3 bonePosition);
static inline Vector3 CalculateBoneMidpoint(const char* boneName, const Person* person);

static inline bool ShouldRenderHead(const Person* person);
static inline bool ShouldRenderChest(const Person* person);
static inline bool ShouldRenderHip(const Person* person);
static inline bool ShouldUseVariableHeight(const char* boneName);
static inline bool ShouldFlipBoneTexture(const char* boneName);
static inline bool GetBoneConnectionsWithPriority(const char* boneName, char connections[3][32], float priorities[3]);
static inline BonesRenderConfig BonesGetDefaultRenderConfig(void);
static inline void BonesSetRenderConfig(const BonesRenderConfig* config);

static inline bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename);
static inline void LoadBoneConfigurations(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount);
static inline BoneConfig* FindBoneConfig(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
static inline const char* GetTexturePathForBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName, TextureSetCollection* textureSets);
static inline bool IsBoneVisible(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
static inline int GetBoneVisibleMode(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
static inline float GetBoneSize(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName);
static inline void CleanupTextureSystem(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount);

static inline TextureSetCollection* BonesTextureSets_Create(void);
static inline void BonesTextureSets_Free(TextureSetCollection* collection);
static inline bool BonesTextureSets_LoadFromFile(TextureSetCollection* collection, const char* filePath);
static inline const char* BonesTextureSets_GetActiveTexture(const TextureSetCollection* collection, const char* boneName);
static inline bool BonesTextureSets_SetVariant(TextureSetCollection* collection, const char* boneName, const char* variantName);
static inline BoneTextureSet* BonesTextureSets_FindSet(TextureSetCollection* collection, const char* boneName);

static inline OrnamentSystem* Ornaments_Create(void);
static inline void Ornaments_Free(OrnamentSystem* system);
static inline bool Ornaments_LoadFromConfig(OrnamentSystem* system, const char* configPath);
static inline void Ornaments_InitializePhysics(OrnamentSystem* system, const AnimationFrame* frame);
static inline void Ornaments_UpdatePhysics(OrnamentSystem* system, const AnimationFrame* frame, float deltaTime);
static inline void Ornaments_CollectForRendering(OrnamentSystem* system, BoneRenderData** renderBones, int* renderBonesCount, int* renderBonesCapacity, Camera camera);
static inline void Ornaments_ApplyPreset(BoneOrnament* ornament, OrnamentPhysicsPreset preset);
static inline Vector3 Ornaments_GetAnchorPosition(const AnimationFrame* frame, const char* anchorName);
static inline BoneOrientation Ornaments_GetAnchorOrientation(const AnimationFrame* frame, const char* anchorName);
static inline void Ornaments_SolveChainConstraint(BoneOrnament* child, BoneOrnament* parent);

static inline void SlashTrail_InitSystem(SlashTrailSystem* sys);
static inline void SlashTrail_FreeSystem(SlashTrailSystem* sys);
static inline void SlashTrail_Activate(SlashTrailSystem* sys, int slashIndex, const SlashEventConfig* config);
static inline void SlashTrail_Deactivate(SlashTrailSystem* sys, int slashIndex);
static inline void SlashTrail_UpdateTrail(SlashTrailSystem* sys, float deltaTime, int slashIndex, Vector3 bonePosition);
static inline void SlashTrail_Draw(SlashTrailSystem* sys, Camera camera);
static inline void SlashTrail_Tick(SlashTrailSystem* sys, float deltaTime, int currentFrame,
                                   const SlashEventConfig* events, int eventCount,
                                   const AnimationFrame* frame, const char* personId,
                                   Vector3 worldPos, Vector3 pivot, Matrix rot, bool applyWorldTransform);
static inline Vector3 SlashTrail__GetBonePos(const AnimationFrame* frame, const char* personId, const char* boneName);

static inline void Model3D_InitSystem(Model3DAttachmentSystem* sys);
static inline void Model3D_FreeSystem(Model3DAttachmentSystem* sys);
static inline void Model3D_LoadFromClip(Model3DAttachmentSystem* sys, const AnimationClipMetadata* clip);
static inline void Model3D_Tick(Model3DAttachmentSystem* sys, int currentFrame);
static inline void Model3D_Draw(Model3DAttachmentSystem* sys, const AnimationFrame* frame,
                                const char* personId, Vector3 worldPos, Vector3 pivot,
                                Matrix rot, bool applyWorldTransform);

static inline BonesRenderer* BonesRenderer_Create(void);
static inline void BonesRenderer_Free(BonesRenderer* renderer);
static inline bool BonesRenderer_Init(BonesRenderer* renderer);
static inline int BonesRenderer_LoadTexture(BonesRenderer* renderer, const char* path);
static inline void BonesRenderer_RenderFrame(BonesRenderer* renderer,
                                              BoneRenderData* bones, int boneCount,
                                              HeadRenderData* heads, int headCount,
                                              TorsoRenderData* torsos, int torsoCount,
                                              Vector3 autoCenter, bool autoCenterCalculated);

static inline bool ResizeRenderBonesArray(BoneRenderData** renderBones, int* renderBonesCapacity, int newCapacity);
static inline void CollectBonesForRendering(const BonesAnimation* animation, Camera camera, BoneRenderData** renderBones,
                                            int* renderBonesCount, int* renderBonesCapacity, BoneConfig* boneConfigs,
                                            int boneConfigCount, TextureSetCollection* textureSets, OrnamentSystem* ornaments);
static inline void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads,
                                            int* headCount, int* headCapacity, BoneConfig* boneConfigs,
                                            int boneConfigCount, TextureSetCollection* textureSets);
static inline void CollectTorsosForRendering(const BonesAnimation* animation, TorsoRenderData** torsos,
                                             int* torsoCount, int* torsoCapacity, BoneConfig* boneConfigs,
                                             int boneConfigCount, TextureSetCollection* textureSets);
static inline void EnrichBoneRenderDataWithOrientation(BoneRenderData* renderBone, const Person* person);

static inline BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int boneCount, const char* boneName);
static inline const Person* FindPersonByBoneName(const AnimationFrame* frame, const char* boneName);
static inline BoneConnectionPositions GetBoneConnectionPositionsEx(const BoneRenderData* boneData, const Person* person);

static inline void CalculateHandBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored, const char* boneName);
static inline void CalculateHandBoneRenderDataWithOrientation(const BoneRenderData* boneData, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored);
static inline void CalculateLimbBoneRenderData(const BoneRenderData* boneData, const Person* person, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored);
static inline void CalculateAsymmetricBoneRenderData(const BoneRenderData* boneData, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored);
static inline void CalculateTorsoRenderData(const TorsoRenderData* torsoData, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored);
static inline void CalculateHeadRenderData(const HeadRenderData* headData, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored);

static inline Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows, bool mirrored, bool* outMirrored);
static inline void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size, float rotationDeg, bool mirrored, const char* boneName);
static inline void DrawBonetileCustomWithRoll(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
                                              float rotationDeg, bool mirrored, bool neighborValid, Vector3 neighborPos,
                                              const BoneRenderData* boneData, const Person* person);
static inline void DrawHeadBillboard(Texture2D texture, Camera camera, const HeadRenderData* headData, int physCols, int physRows);
static inline void DrawTorsoBillboard(Texture2D texture, Camera camera, const TorsoRenderData* torsoData, int physCols, int physRows);

static AnimationController* AnimController_Create(void* bonesAnimation, TextureSetCollection* textureSets);
static inline void AnimController_Free(AnimationController* controller);
static inline bool AnimController_LoadClipMetadata(AnimationController* controller, const char* jsonPath);
static inline bool AnimController_PlayClip(AnimationController* controller, const char* clipName);
static inline void AnimController_Update(AnimationController* controller, float deltaTime);
static inline int AnimController_GetCurrentFrame(const AnimationController* controller);
static inline SlashHitboxInfo AnimController_GetActiveSlashHitbox(const AnimationController* controller,
                                    const AnimationFrame* frame, const char* personId,
                                    Vector3 worldPos, Vector3 pivot, Matrix rot,
                                    bool applyWorldTransform, int slashIndexHint);

static inline AnimatedCharacter* CreateAnimatedCharacter(const char* textureConfigPath, const char* textureSetsPath);
static inline void DestroyAnimatedCharacter(AnimatedCharacter* character);
static inline void UpdateAnimatedCharacter(AnimatedCharacter* character, float deltaTime);
static inline void DrawAnimatedCharacterTransformed(AnimatedCharacter* character, Camera camera, Vector3 worldPosition, float worldRotation);
static inline void SetCharacterFrame(AnimatedCharacter* character, int frame);
static inline void SetCharacterAutoPlay(AnimatedCharacter* character, bool autoPlay);
static inline void RestartAnimation(AnimatedCharacter* character);
static inline void ResetCharacterAutoCenter(AnimatedCharacter* character);
static inline void LockAnimationRootXZ(AnimatedCharacter* character, bool lock);
static inline float GetCurrentAnimDuration(const AnimatedCharacter* character);
static inline bool IsSlashActiveForCharacter(const AnimatedCharacter* character);
static inline Vector3 GetActiveSlashBonePos(const AnimatedCharacter* character, Vector3 fallback);
static inline void SetAnimationTransitionDuration(float duration);
static inline bool BonesInterpolateFrames(BonesAnimation* animation, int frameA, int frameB, int framesToAdd);
static inline bool BonesInsertEmptyFrame(BonesAnimation* animation, int position);
static inline bool BonesCopyFrame(BonesAnimation* animation, int sourceFrame, int targetFrame);
static inline void DrawAnimatedCharacter(AnimatedCharacter* character, Camera camera);
static inline void SetCharacterBillboards(AnimatedCharacter* character, bool heads, bool torsos);

static inline BonesError BonesInit(BonesAnimation* animation, int maxFrames) {
    if (!animation) return BONES_ERROR_NULL_POINTER;
    if (maxFrames <= 0 || maxFrames > MAX_FRAMES) maxFrames = MAX_FRAMES;

    memset(animation, 0, sizeof(BonesAnimation));
    animation->frames = calloc(maxFrames, sizeof(AnimationFrame));
    if (!animation->frames) return BONES_ERROR_MEMORY_ALLOCATION;

    animation->maxFrames    = maxFrames;
    animation->currentFrame = -1;
    return BONES_SUCCESS;
}

static inline void BonesFree(BonesAnimation* animation) {
    if (!animation) return;
    free(animation->frames);
    memset(animation, 0, sizeof(BonesAnimation));
    animation->currentFrame = -1;
}

static inline BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber) {
    if (!animation) return BONES_ERROR_NULL_POINTER;
    if (!animation->isLoaded) return BONES_ERROR_EMPTY_DATA;
    if (frameNumber < 0 || frameNumber >= animation->frameCount) return BONES_ERROR_FRAME_OUT_OF_RANGE;
    animation->currentFrame = frameNumber;
    return BONES_SUCCESS;
}

static inline int BonesGetCurrentFrame(const BonesAnimation* animation) {
    return animation ? animation->currentFrame : -1;
}

static inline int BonesGetFrameCount(const BonesAnimation* animation) {
    return animation ? animation->frameCount : 0;
}

static inline bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber) {
    return animation && animation->isLoaded && frameNumber >= 0 &&
           frameNumber < animation->frameCount && animation->frames[frameNumber].valid;
}

static inline bool BonesIsPositionValid(Vector3 position) {
    return !isnan(position.x) && !isnan(position.y) && !isnan(position.z) &&
           isfinite(position.x) && isfinite(position.y) && isfinite(position.z);
}

static inline bool ParseJSONFloat(const char* json, const char* key, float* outValue) {
    char searchKey[128];
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    const char* pos = strstr(json, searchKey);
    return pos && sscanf(pos + strlen(searchKey), "%f", outValue) == 1;
}

static inline bool ParseJSONInt(const char* json, const char* key, int* outValue) {
    char searchKey[128];
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    const char* pos = strstr(json, searchKey);
    return pos && sscanf(pos + strlen(searchKey), "%d", outValue) == 1;
}

static inline bool ParseJSONString(const char* json, const char* key, char* outValue, int maxLen) {
    char searchKey[128];
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    const char* pos = strstr(json, searchKey);
    if (!pos) return false;

    const char* start = strchr(pos + strlen(searchKey), '"');
    if (!start) return false;
    start++;
    const char* end = strchr(start, '"');
    if (!end) return false;

    int len = (int)(end - start);
    if (len >= maxLen) len = maxLen - 1;
    strncpy(outValue, start, len);
    outValue[len] = '\0';
    return true;
}

static inline bool ParseJSONFloatInBlock(const char* blockStart, const char* blockEnd, const char* key, float* outValue) {
    char searchKey[128];
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    const char* pos = strstr(blockStart, searchKey);
    return pos && pos < blockEnd && sscanf(pos + strlen(searchKey), "%f", outValue) == 1;
}

static inline BonesError ParseJSONFrame(const char* jsonData, int* outFrameNum, Person* outPersons, int* outPersonCount) {
    if (!jsonData || !outFrameNum || !outPersons || !outPersonCount) return BONES_ERROR_NULL_POINTER;

    *outPersonCount = 0;
    const char* frameStart = strstr(jsonData, "\"frame_");
    if (!frameStart || sscanf(frameStart, "\"frame_%d\"", outFrameNum) != 1)
        return BONES_ERROR_INVALID_JSON;

    static const char* expectedBones[] = {
        "Nose","LEye","REye","LEar","REar",
        "LShoulder","RShoulder","LElbow","RElbow",
        "LWrist","RWrist","LHip","RHip",
        "LKnee","RKnee","LAnkle","RAnkle","Neck"
    };

    const char* personStart = strstr(frameStart, "\"person_");
    while (personStart && *outPersonCount < MAX_PERSONS) {
        Person* p = &outPersons[*outPersonCount];
        memset(p, 0, sizeof(Person));
        if (sscanf(personStart, "\"person_%15[^\"]\"", p->personId) != 1) break;
        p->active = true;

        const char* nextPerson = strstr(personStart + 1, "\"person_");

        for (int b = 0; b < 18 && p->boneCount < MAX_BONES_PER_PERSON; b++) {
            char searchPattern[64];
            snprintf(searchPattern, sizeof(searchPattern), "\"%s\":", expectedBones[b]);
            const char* bonePos = strstr(personStart, searchPattern);
            if (!bonePos || (nextPerson && bonePos > nextPerson)) continue;

            double x, y, z;
            if (sscanf(strstr(bonePos, "\"x\":"), "\"x\": %lf", &x) != 1) continue;
            if (sscanf(strstr(bonePos, "\"y\":"), "\"y\": %lf", &y) != 1) continue;
            if (sscanf(strstr(bonePos, "\"z\":"), "\"z\": %lf", &z) != 1) continue;

            Bone* bone = &p->bones[p->boneCount];
            strncpy(bone->name, expectedBones[b], MAX_BONE_NAME_LENGTH - 1);
            bone->name[MAX_BONE_NAME_LENGTH - 1] = '\0';
            bone->position.position = (Vector3){
                (float)(x * 2.0 - 1.0),
                (float)(1.0 - y),
                (float)(z * 2.0 - 1.0)
            };
            bone->position.valid      = BonesIsPositionValid(bone->position.position);
            bone->position.confidence = 1.0f;
            bone->size                = (Vector2){ g_renderConfig.defaultBoneSize, g_renderConfig.defaultBoneSize };
            bone->visible             = true;
            p->boneCount++;
        }

        if (p->boneCount > 0) (*outPersonCount)++;
        personStart = nextPerson;
    }

    return (*outPersonCount > 0) ? BONES_SUCCESS : BONES_ERROR_EMPTY_DATA;
}

static inline BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString) {
    if (!animation || !jsonString || !animation->frames) return BONES_ERROR_NULL_POINTER;

    animation->frameCount   = 0;
    animation->currentFrame = -1;
    animation->isLoaded     = false;

    const char* searchPos = jsonString;
    while (animation->frameCount < animation->maxFrames) {
        const char* nextFrame = strstr(searchPos, "\"frame_");
        if (!nextFrame) break;

        AnimationFrame* frame = &animation->frames[animation->frameCount];
        memset(frame, 0, sizeof(AnimationFrame));

        if (ParseJSONFrame(nextFrame, &frame->frameNumber, frame->persons, &frame->personCount) == BONES_SUCCESS) {
            frame->valid              = true;
            frame->isOriginalKeyframe = true;
            animation->frameCount++;
        }
        searchPos = nextFrame + 1;
    }

    if (animation->frameCount > 0) {
        animation->currentFrame = 0;
        animation->isLoaded     = true;
        return BONES_SUCCESS;
    }
    return BONES_ERROR_EMPTY_DATA;
}

static inline BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath) {
    if (!animation || !jsonFilePath) return BONES_ERROR_NULL_POINTER;
    char* jsonData = LoadFileText(jsonFilePath);
    if (!jsonData) return BONES_ERROR_FILE_NOT_FOUND;

    BonesError result = BonesLoadFromString(animation, jsonData);
    if (result == BONES_SUCCESS) {
        strncpy(animation->filePath, jsonFilePath, sizeof(animation->filePath) - 1);
        animation->filePath[sizeof(animation->filePath) - 1] = '\0';
    }

    UnloadFileText(jsonData);
    return result;
}

static inline void InterpolateBone(const Bone* boneA, const Bone* boneB, Bone* result, float t) {
    strncpy(result->name, boneA->name, MAX_BONE_NAME_LENGTH - 1);
    result->name[MAX_BONE_NAME_LENGTH - 1] = '\0';

    if (boneA->position.valid && boneB->position.valid) {
        result->position.position.x = boneA->position.position.x * (1.0f - t) + boneB->position.position.x * t;
        result->position.position.y = boneA->position.position.y * (1.0f - t) + boneB->position.position.y * t;
        result->position.position.z = boneA->position.position.z * (1.0f - t) + boneB->position.position.z * t;
        result->position.valid = true;
    } else {
        result->position = boneA->position.valid ? boneA->position : boneB->position;
    }

    result->position.confidence = boneA->position.confidence * (1.0f - t) + boneB->position.confidence * t;
    result->size.x    = boneA->size.x * (1.0f - t) + boneB->size.x * t;
    result->size.y    = boneA->size.y * (1.0f - t) + boneB->size.y * t;
    result->visible       = boneA->visible || boneB->visible;
    result->textureIndex  = boneA->textureIndex;

    float rotDiff = boneB->rotation - boneA->rotation;
    if (rotDiff >  180.0f) rotDiff -= 360.0f;
    if (rotDiff < -180.0f) rotDiff += 360.0f;
    result->rotation = boneA->rotation + rotDiff * t;
    result->mirrored = boneA->mirrored;
}

static inline void InterpolatePerson(const Person* personA, const Person* personB, Person* result, float t) {
    strncpy(result->personId, personA->personId, 15);
    result->personId[15] = '\0';
    result->active    = personA->active || personB->active;
    result->boneCount = personA->boneCount > personB->boneCount ? personA->boneCount : personB->boneCount;

    for (int i = 0; i < result->boneCount; i++) {
        if (i < personA->boneCount && i < personB->boneCount)
            InterpolateBone(&personA->bones[i], &personB->bones[i], &result->bones[i], t);
        else if (i < personA->boneCount)
            result->bones[i] = personA->bones[i];
        else
            result->bones[i] = personB->bones[i];
    }
}

static inline bool BonesCreateMissingFrames(BonesAnimation* animation) {
    if (!animation || !animation->isLoaded || animation->frameCount < 2) return false;

    int minFrame = animation->frames[0].frameNumber;
    int maxFrame = animation->frames[0].frameNumber;
    for (int i = 0; i < animation->frameCount; i++) {
        if (animation->frames[i].frameNumber < minFrame) minFrame = animation->frames[i].frameNumber;
        if (animation->frames[i].frameNumber > maxFrame) maxFrame = animation->frames[i].frameNumber;
    }

    int totalNeeded = maxFrame - minFrame + 1;
    if (totalNeeded > animation->maxFrames) {
        AnimationFrame* expanded = (AnimationFrame*)realloc(animation->frames, totalNeeded * sizeof(AnimationFrame));
        if (!expanded) return false;
        animation->frames    = expanded;
        animation->maxFrames = totalNeeded;
        for (int i = animation->frameCount; i < totalNeeded; i++)
            memset(&animation->frames[i], 0, sizeof(AnimationFrame));
    }

    AnimationFrame* temp = (AnimationFrame*)calloc(totalNeeded, sizeof(AnimationFrame));
    if (!temp) return false;

    for (int frameNum = minFrame; frameNum <= maxFrame; frameNum++) {
        int targetIdx   = frameNum - minFrame;
        int existingIdx = -1;
        for (int i = 0; i < animation->frameCount; i++) {
            if (animation->frames[i].frameNumber == frameNum) { existingIdx = i; break; }
        }

        if (existingIdx >= 0) {
            temp[targetIdx] = animation->frames[existingIdx];
            continue;
        }

        int prevIdx = -1, prevNum = -1, nextIdx = -1, nextNum = -1;
        for (int i = 0; i < animation->frameCount; i++) {
            int fn = animation->frames[i].frameNumber;
            if (fn < frameNum && (prevNum < 0 || fn > prevNum)) { prevIdx = i; prevNum = fn; }
            if (fn > frameNum && (nextNum < 0 || fn < nextNum)) { nextIdx = i; nextNum = fn; }
        }

        AnimationFrame* dest = &temp[targetIdx];
        memset(dest, 0, sizeof(AnimationFrame));
        dest->frameNumber       = frameNum;
        dest->valid             = true;
        dest->isOriginalKeyframe = false;

        if (prevIdx >= 0 && nextIdx >= 0) {
            AnimationFrame* fA = &animation->frames[prevIdx];
            AnimationFrame* fB = &animation->frames[nextIdx];
            float raw = (float)(frameNum - prevNum) / (float)(nextNum - prevNum);
            float t   = raw * raw * (3.0f - 2.0f * raw);

            dest->personCount = fA->personCount > fB->personCount ? fA->personCount : fB->personCount;
            for (int p = 0; p < dest->personCount; p++) {
                if (p < fA->personCount && p < fB->personCount)
                    InterpolatePerson(&fA->persons[p], &fB->persons[p], &dest->persons[p], t);
                else if (p < fA->personCount) dest->persons[p] = fA->persons[p];
                else                          dest->persons[p] = fB->persons[p];
            }
        } else if (prevIdx >= 0) {
            *dest = animation->frames[prevIdx];
            dest->frameNumber        = frameNum;
            dest->isOriginalKeyframe = false;
        } else if (nextIdx >= 0) {
            *dest = animation->frames[nextIdx];
            dest->frameNumber        = frameNum;
            dest->isOriginalKeyframe = false;
        }
    }

    memcpy(animation->frames, temp, totalNeeded * sizeof(AnimationFrame));
    animation->frameCount = totalNeeded;
    free(temp);
    return true;
}

static inline bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename) {
    char* buffer = LoadFileText(filename);
    if (!buffer) return false;

    int lineCount = 0;
    for (const char* p = buffer; *p; p++) if (*p == '\n') lineCount++;

    if (lineCount == 0) { UnloadFileText(buffer); return false; }

    free(system->configs);
    system->configs = calloc(lineCount, sizeof(BoneTextureConfig));
    if (!system->configs) { UnloadFileText(buffer); return false; }

    system->configCapacity = lineCount;
    system->configCount    = 0;

    char lineBuffer[512];
    const char* lineStart = buffer;

    for (const char* ptr = buffer; *ptr && system->configCount < system->configCapacity; ptr++) {
        if (*ptr != '\n') continue;
        int lineLen = (int)(ptr - lineStart);
        if (lineLen > 5 && lineLen < 511 && *lineStart != '#' && *lineStart != '\n') {
            memcpy(lineBuffer, lineStart, lineLen);
            lineBuffer[lineLen] = '\0';

            char boneName[MAX_BONE_NAME_LENGTH], texturePath[MAX_FILE_PATH_LENGTH];
            int   visibleMode;
            float size;
            if (sscanf(lineBuffer, "%31s %255s %d %f", boneName, texturePath, &visibleMode, &size) == 4) {
                BoneTextureConfig* cfg = &system->configs[system->configCount];
                strncpy(cfg->boneName,    boneName,    MAX_BONE_NAME_LENGTH  - 1);
                cfg->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
                strncpy(cfg->texturePath, texturePath, MAX_FILE_PATH_LENGTH  - 1);
                cfg->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                cfg->visibleMode = visibleMode;
                cfg->size    = size;
                system->configCount++;
            }
        }
        lineStart = ptr + 1;
    }

    UnloadFileText(buffer);
    system->loaded = (system->configCount > 0);
    return system->loaded;
}

static inline void LoadBoneConfigurations(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount) {
    free(*boneConfigs);
    *boneConfigs     = NULL;
    *boneConfigCount = 0;

    if (!textureSystem->loaded || textureSystem->configCount == 0) return;

    *boneConfigCount = textureSystem->configCount;
    *boneConfigs     = calloc(*boneConfigCount, sizeof(BoneConfig));
    if (!*boneConfigs) return;

    for (int i = 0; i < *boneConfigCount; i++) {
        const BoneTextureConfig* src = &textureSystem->configs[i];
        BoneConfig*              dst = &(*boneConfigs)[i];
        strncpy(dst->boneName,    src->boneName,    MAX_BONE_NAME_LENGTH - 1);
        dst->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
        strncpy(dst->texturePath, src->texturePath, MAX_FILE_PATH_LENGTH - 1);
        dst->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
        dst->visibleMode = src->visibleMode;
        dst->visible = (src->visibleMode != 0);
        dst->size    = src->size;
        dst->valid   = true;
    }
}

static inline BoneConfig* FindBoneConfig(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    for (int i = 0; i < boneConfigCount; i++)
        if (strcmp(boneConfigs[i].boneName, boneName) == 0) return &boneConfigs[i];
    return NULL;
}

static inline const char* GetTexturePathForBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName, TextureSetCollection* textureSets) {
    if (textureSets && textureSets->loaded) {
        const char* t = BonesTextureSets_GetActiveTexture(textureSets, boneName);
        if (t) return t;
    }
    BoneConfig* cfg = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return cfg ? cfg->texturePath : "default.png";
}

static inline bool IsBoneVisible(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* cfg = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return cfg ? (cfg->visibleMode != 0) : true;
}

static inline int GetBoneVisibleMode(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* cfg = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return cfg ? cfg->visibleMode : 1;
}

static inline bool IsAsymmetricBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    return GetBoneVisibleMode(boneConfigs, boneConfigCount, boneName) == 2;
}

static inline float GetBoneSize(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* cfg = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return cfg ? cfg->size : 0.35f;
}

static inline void CleanupTextureSystem(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount) {
    if (textureSystem->configs) {
        free(textureSystem->configs);
        memset(textureSystem, 0, sizeof(SimpleTextureSystem));
    }
    free(*boneConfigs);
    *boneConfigs     = NULL;
    *boneConfigCount = 0;
}

static inline TextureSetCollection* BonesTextureSets_Create(void) {
    return (TextureSetCollection*)calloc(1, sizeof(TextureSetCollection));
}

static inline void BonesTextureSets_Free(TextureSetCollection* collection) {
    free(collection);
}

static inline BoneTextureSet* BonesTextureSets_FindSet(TextureSetCollection* collection, const char* boneName) {
    if (!collection || !boneName) return NULL;
    for (int i = 0; i < collection->setCount; i++)
        if (strcmp(collection->sets[i].boneName, boneName) == 0) return &collection->sets[i];
    return NULL;
}

static inline const char* BonesTextureSets_GetActiveTexture(const TextureSetCollection* collection, const char* boneName) {
    if (!collection || !boneName || !collection->loaded) return NULL;
    for (int i = 0; i < collection->setCount; i++) {
        const BoneTextureSet* set = &collection->sets[i];
        if (strcmp(set->boneName, boneName) == 0 && set->valid &&
            set->activeVariantIndex >= 0 && set->activeVariantIndex < set->variantCount)
            return set->variants[set->activeVariantIndex].texturePath;
    }
    return NULL;
}

static inline bool BonesTextureSets_SetVariant(TextureSetCollection* collection, const char* boneName, const char* variantName) {
    BoneTextureSet* set = BonesTextureSets_FindSet(collection, boneName);
    if (!set) return false;
    for (int i = 0; i < set->variantCount; i++) {
        if (strcmp(set->variants[i].variantName, variantName) == 0) {
            set->activeVariantIndex = i;
            return true;
        }
    }
    return false;
}

static inline bool BonesTextureSets_LoadFromFile(TextureSetCollection* collection, const char* filePath) {
    if (!collection || !filePath) return false;
    FILE* file = fopen(filePath, "r");
    if (!file) return false;

    char line[512], boneName[BONES_AE_MAX_NAME], variantName[BONES_AE_MAX_NAME], texturePath[BONES_AE_MAX_PATH];
    collection->setCount = 0;
    collection->loaded   = false;

    while (fgets(line, sizeof(line), file) && collection->setCount < BONES_MAX_TEXTURE_SETS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        if (sscanf(line, "%63s %63s %255s", boneName, variantName, texturePath) != 3) continue;

        BoneTextureSet* set = BonesTextureSets_FindSet(collection, boneName);
        if (!set) {
            if (collection->setCount >= BONES_MAX_TEXTURE_SETS) break;
            set = &collection->sets[collection->setCount];
            memset(set, 0, sizeof(BoneTextureSet));
            strncpy(set->boneName, boneName, BONES_AE_MAX_NAME - 1);
            set->boneName[BONES_AE_MAX_NAME - 1] = '\0';
            set->valid = true;
            collection->setCount++;
        }

        if (set->variantCount >= BONES_MAX_TEXTURE_VARIANTS) continue;

        BoneTextureVariant* variant = &set->variants[set->variantCount];
        strncpy(variant->variantName, variantName, BONES_AE_MAX_NAME - 1);
        variant->variantName[BONES_AE_MAX_NAME - 1] = '\0';
        strncpy(variant->texturePath, texturePath, BONES_AE_MAX_PATH - 1);
        variant->texturePath[BONES_AE_MAX_PATH - 1] = '\0';
        variant->valid = true;
        set->variantCount++;
    }

    fclose(file);
    collection->loaded = (collection->setCount > 0);
    return collection->loaded;
}

static AnimationController* AnimController_Create(void* bonesAnimation, TextureSetCollection* textureSets) {
    AnimationController* ctrl = (AnimationController*)calloc(1, sizeof(AnimationController));
    if (!ctrl) return NULL;

    ctrl->clips = (AnimationClipMetadata*)calloc(BONES_MAX_ANIM_CLIPS, sizeof(AnimationClipMetadata));
    if (!ctrl->clips) { free(ctrl); return NULL; }

    ctrl->bonesAnimation     = bonesAnimation;
    ctrl->textureSets        = textureSets;
    ctrl->clipCount          = 0;
    ctrl->clipCapacity       = BONES_MAX_ANIM_CLIPS;
    ctrl->currentClipIndex   = -1;
    ctrl->localTime          = 0.0f;
    ctrl->playing            = false;
    ctrl->currentFrameInJSON = 0;
    ctrl->valid              = true;
    return ctrl;
}

static inline void AnimController_Free(AnimationController* controller) {
    if (!controller) return;
    free(controller->clips);
    free(controller);
}

static inline bool AnimController_LoadClipMetadata(AnimationController* controller, const char* jsonPath) {
    if (!controller || !jsonPath || !controller->clips) return false;
    if (controller->clipCount >= controller->clipCapacity) return false;

    FILE* file = fopen(jsonPath, "rb");
    if (!file) return false;

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* jsonData = (char*)malloc(fileSize + 1);
    if (!jsonData) { fclose(file); return false; }
    fread(jsonData, 1, fileSize, file);
    jsonData[fileSize] = '\0';
    fclose(file);

    AnimationClipMetadata* clip = &controller->clips[controller->clipCount];
    memset(clip, 0, sizeof(AnimationClipMetadata));

    if (!ParseJSONString(jsonData, "name", clip->name, BONES_AE_MAX_NAME)) strcpy(clip->name, "unnamed");
    if (!ParseJSONFloat(jsonData, "fps",         &clip->fps))        clip->fps        = 30.0f;
    if (!ParseJSONInt  (jsonData, "start_frame", &clip->startFrame)) clip->startFrame = 0;
    if (!ParseJSONInt  (jsonData, "end_frame",   &clip->endFrame))   clip->endFrame   = 60;
    clip->loop = strstr(jsonData, "\"loop\": true") || strstr(jsonData, "\"loop\":true");

    const char* eventsStart = strstr(jsonData, "\"events\":");
    if (eventsStart) {
        eventsStart = strchr(eventsStart, '[');
        const char* arrEnd = NULL;
        if (eventsStart) {
            int depth = 0;
            for (const char* p = eventsStart; *p; p++) {
                if (*p == '[') depth++;
                else if (*p == ']') { if (--depth == 0) { arrEnd = p; break; } }
            }
        }

        if (eventsStart && arrEnd) {
            const char* eventPos = eventsStart;
            while (1) {
                eventPos = strchr(eventPos + 1, '{');
                if (!eventPos || eventPos >= arrEnd) break;

                const char* blockEnd = eventPos + 1;
                int depth = 1;
                while (*blockEnd && depth > 0) {
                    if (*blockEnd == '{') depth++;
                    else if (*blockEnd == '}') depth--;
                    blockEnd++;
                }

                char eventType[32] = {0};
                if (!ParseJSONString(eventPos, "type", eventType, sizeof(eventType))) continue;

                if (strcmp(eventType, "slash") == 0 && clip->slashEventCount < 16) {
                    SlashEventConfig* se = &clip->slashEvents[clip->slashEventCount];
                    memset(se, 0, sizeof(SlashEventConfig));
                    se->trailEnabled = true;
                    se->widthStart   = 0.10f;
                    se->widthEnd     = 0.0f;
                    se->colorStart   = (Color){255, 200,  80, 220};
                    se->colorEnd     = (Color){255,  60,  10,   0};
                    se->lifetime     = 0.20f;
                    se->segments     = 24;

                    ParseJSONInt   (eventPos, "frame_start", &se->frameStart);
                    ParseJSONInt   (eventPos, "frame_end",   &se->frameEnd);
                    ParseJSONString(eventPos, "bone",        se->boneName,    BONES_AE_MAX_NAME);
                    ParseJSONString(eventPos, "texture",     se->texturePath, BONES_AE_MAX_PATH);
                    ParseJSONFloatInBlock(eventPos, blockEnd, "emit_offset_x", &se->emitOffsetX);
                    ParseJSONFloatInBlock(eventPos, blockEnd, "emit_offset_y", &se->emitOffsetY);
                    ParseJSONFloatInBlock(eventPos, blockEnd, "emit_offset_z", &se->emitOffsetZ);

                    const char* trailTag  = strstr(eventPos, "\"trail\"");
                    const char* nextBrace = strchr(eventPos + 1, '{');
                    if (trailTag && nextBrace && trailTag < nextBrace + 64) {
                        const char* ts = strchr(trailTag, '{');
                        if (ts) {
                            float fw = 0; int iv = 0;
                            if (ParseJSONFloat(ts, "width_start", &fw)) se->widthStart = fw;
                            if (ParseJSONFloat(ts, "width_end",   &fw)) se->widthEnd   = fw;
                            if (ParseJSONFloat(ts, "lifetime",    &fw)) se->lifetime   = fw;
                            if (ParseJSONInt  (ts, "segments",    &iv)) se->segments   = iv;

                            const char* cs = strstr(ts, "\"color_start\"");
                            if (cs) {
                                const char* arr = strchr(cs, '[');
                                if (arr) {
                                    se->colorStart.r = (unsigned char)atoi(arr + 1);
                                    const char* p2 = strchr(arr+1,','); if (p2) {
                                    se->colorStart.g = (unsigned char)atoi(p2 + 1);
                                    const char* p3 = strchr(p2+1,','); if (p3) {
                                    se->colorStart.b = (unsigned char)atoi(p3 + 1);
                                    const char* p4 = strchr(p3+1,','); if (p4)
                                    se->colorStart.a = (unsigned char)atoi(p4 + 1); }}
                                }
                            }
                            const char* ce = strstr(ts, "\"color_end\"");
                            if (ce) {
                                const char* arr = strchr(ce, '[');
                                if (arr) {
                                    se->colorEnd.r = (unsigned char)atoi(arr + 1);
                                    const char* p2 = strchr(arr+1,','); if (p2) {
                                    se->colorEnd.g = (unsigned char)atoi(p2 + 1);
                                    const char* p3 = strchr(p2+1,','); if (p3) {
                                    se->colorEnd.b = (unsigned char)atoi(p3 + 1);
                                    const char* p4 = strchr(p3+1,','); if (p4)
                                    se->colorEnd.a = (unsigned char)atoi(p4 + 1); }}
                                }
                            }
                        }
                    }
                    if (se->segments > SLASH_TRAIL_MAX_SEGMENTS) se->segments = SLASH_TRAIL_MAX_SEGMENTS;
                    if (se->segments < 2) se->segments = 2;
                    clip->slashEventCount++;
                    continue;
                }

                if (strcmp(eventType, "model3d") == 0 && clip->model3dEventCount < BONES_MAX_MODEL3D_EVENTS) {
                    Model3DEventConfig* m = &clip->model3dEvents[clip->model3dEventCount];
                    memset(m, 0, sizeof(Model3DEventConfig));
                    m->scale = 1.0f;
                    ParseJSONInt   (eventPos, "frame_start", &m->frameStart);
                    ParseJSONInt   (eventPos, "frame_end",   &m->frameEnd);
                    ParseJSONString(eventPos, "bone",        m->boneName,  BONES_MODEL3D_MAX_BONE);
                    ParseJSONString(eventPos, "model",       m->modelPath, BONES_MODEL3D_MAX_PATH);
                    ParseJSONFloat (eventPos, "scale",       &m->scale);
                    ParseJSONFloat (eventPos, "offset_x",    &m->offset.x);
                    ParseJSONFloat (eventPos, "offset_y",    &m->offset.y);
                    ParseJSONFloat (eventPos, "offset_z",    &m->offset.z);
                    ParseJSONFloat (eventPos, "rot_x",       &m->rotationEuler.x);
                    ParseJSONFloat (eventPos, "rot_y",       &m->rotationEuler.y);
                    ParseJSONFloat (eventPos, "rot_z",       &m->rotationEuler.z);
                    m->valid = (m->modelPath[0] != '\0');
                    clip->model3dEventCount++;
                    continue;
                }

                if (clip->eventCount >= BONES_MAX_ANIM_EVENTS) continue;
                AnimationEvent* ev = &clip->events[clip->eventCount];
                memset(ev, 0, sizeof(AnimationEvent));

                float eventTime;
                if (!ParseJSONFloat(eventPos, "time", &eventTime)) continue;
                ev->time = eventTime;

                if      (strcmp(eventType, "texture") == 0) ev->type = ANIM_EVENT_TEXTURE;
                else if (strcmp(eventType, "sound")   == 0) ev->type = ANIM_EVENT_SOUND;
                else                                         ev->type = ANIM_EVENT_CUSTOM;

                char tmp[BONES_AE_MAX_NAME] = {0};
                ParseJSONString(eventPos, "bone",    tmp, BONES_AE_MAX_NAME); strncpy(ev->boneName,    tmp, BONES_AE_MAX_NAME - 1);
                ParseJSONString(eventPos, "variant", tmp, BONES_AE_MAX_NAME); strncpy(ev->variantName, tmp, BONES_AE_MAX_NAME - 1);
                ParseJSONString(eventPos, "person",  tmp, BONES_AE_MAX_NAME); strncpy(ev->personId,    tmp, BONES_AE_MAX_NAME - 1);
                ev->processed = false;
                ev->valid     = true;
                clip->eventCount++;
            }
        }
    }

    clip->valid = true;
    controller->clipCount++;
    free(jsonData);
    return true;
}

static inline bool AnimController_PlayClip(AnimationController* controller, const char* clipName) {
    if (!controller || !clipName || !controller->clips) return false;

    int foundIndex = -1;
    for (int i = 0; i < controller->clipCount; i++) {
        if (strcmp(controller->clips[i].name, clipName) == 0) { foundIndex = i; break; }
    }
    if (foundIndex < 0) {
        if (controller->clipCount > 0) {
            foundIndex = 0;
            TraceLog(LOG_WARNING, "AnimController_PlayClip: '%s' no encontrado, usando '%s'",
                     clipName, controller->clips[0].name);
        } else {
            TraceLog(LOG_ERROR, "AnimController_PlayClip: '%s' no encontrado y no hay clips", clipName);
            return false;
        }
    }

    controller->currentClipIndex = foundIndex;
    controller->localTime        = 0.0f;
    controller->playing          = true;

    if (controller->bonesAnimation) {
        BonesAnimation* anim = (BonesAnimation*)controller->bonesAnimation;
        if (anim->isLoaded && anim->frameCount > 0) {
            int minFrame = anim->frames[0].frameNumber;
            int maxFrame = anim->frames[0].frameNumber;
            for (int f = 1; f < anim->frameCount; f++) {
                if (anim->frames[f].frameNumber < minFrame) minFrame = anim->frames[f].frameNumber;
                if (anim->frames[f].frameNumber > maxFrame) maxFrame = anim->frames[f].frameNumber;
            }
            controller->clips[foundIndex].startFrame = minFrame;
            controller->clips[foundIndex].endFrame   = maxFrame;
        }
    }

    controller->currentFrameInJSON = controller->clips[foundIndex].startFrame;
    for (int j = 0; j < controller->clips[foundIndex].eventCount; j++)
        controller->clips[foundIndex].events[j].processed = false;

    return true;
}

static inline int AnimController_GetCurrentFrame(const AnimationController* controller) {
    if (!controller || controller->currentClipIndex < 0) return 0;
    return controller->currentFrameInJSON;
}

static inline void AnimController_Update(AnimationController* controller, float deltaTime) {
    if (!controller || !controller->playing || controller->currentClipIndex < 0 || !controller->clips) return;

    AnimationClipMetadata* clip = &controller->clips[controller->currentClipIndex];
    if (!clip->valid) return;

    controller->localTime += deltaTime;
    int   totalFrames  = clip->endFrame - clip->startFrame + 1;
    float clipDuration = (float)totalFrames / clip->fps;

    if (controller->localTime >= clipDuration) {
        if (clip->loop) {
            controller->localTime = fmodf(controller->localTime, clipDuration);
            for (int i = 0; i < clip->eventCount; i++) clip->events[i].processed = false;
        } else {
            controller->localTime = clipDuration - 0.001f;
            controller->playing   = false;
        }
    }

    controller->currentFrameInJSON = clip->startFrame + (int)roundf(controller->localTime * clip->fps);
    if (controller->currentFrameInJSON > clip->endFrame)
        controller->currentFrameInJSON = clip->endFrame;

    for (int i = 0; i < clip->eventCount; i++) {
        AnimationEvent* ev = &clip->events[i];
        if (!ev->valid || ev->processed || controller->localTime < ev->time) continue;
        if (ev->type == ANIM_EVENT_TEXTURE && controller->textureSets)
            BonesTextureSets_SetVariant(controller->textureSets, ev->boneName, ev->variantName);
        ev->processed = true;
    }
}

static inline SlashHitboxInfo AnimController_GetActiveSlashHitbox(
        const AnimationController* controller,
        const AnimationFrame*      frame,
        const char*                personId,
        Vector3                    worldPos,
        Vector3                    pivot,
        Matrix                     rot,
        bool                       applyWorldTransform,
        int                        slashIndexHint)
{
    SlashHitboxInfo result = {0};
    if (!controller || !frame || controller->currentClipIndex < 0) return result;

    const AnimationClipMetadata* clip = &controller->clips[controller->currentClipIndex];
    if (!clip->valid || clip->slashEventCount == 0) return result;

    int currentFrame = controller->currentFrameInJSON;
    int found = 0;

    for (int s = 0; s < clip->slashEventCount; s++) {
        const SlashEventConfig* se = &clip->slashEvents[s];
        if (currentFrame < se->frameStart || currentFrame > se->frameEnd) continue;
        if (found < slashIndexHint) { found++; continue; }

        Vector3 bonePos = SlashTrail__GetBonePos(frame, personId, se->boneName);
        if (applyWorldTransform) {
            Vector3 rel    = Vector3Subtract(bonePos, pivot);
            Vector3 rotated = Vector3Transform(rel, rot);
            bonePos = Vector3Add(Vector3Add(worldPos, pivot), rotated);
        }

        result.active       = true;
        result.bonePosition = bonePos;
        result.frameStart   = se->frameStart;
        result.frameEnd     = se->frameEnd;
        result.slashIndex   = s;
        strncpy(result.boneName, se->boneName, BONES_AE_MAX_NAME - 1);
        result.boneName[BONES_AE_MAX_NAME - 1] = '\0';
        return result;
    }
    return result;
}

static inline BonesRenderer* BonesRenderer_Create(void) {
    BonesRenderer* r = (BonesRenderer*)calloc(1, sizeof(BonesRenderer));
    if (r) { r->physCols = 4; r->physRows = 4; }
    return r;
}

static inline void BonesRenderer_Free(BonesRenderer* renderer) {
    if (!renderer) return;
    for (int i = 0; i < renderer->textureCount; i++) UnloadTexture(renderer->textures[i]);
    free(renderer);
}

static inline bool BonesRenderer_Init(BonesRenderer* renderer) {
    if (!renderer) return false;
    renderer->camera = (Camera){
        .position   = {0.0f, 0.6f, 2.5f},
        .target     = {0.0f, 0.6f, 0.0f},
        .up         = {0.0f, 1.0f, 0.0f},
        .fovy       = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };
    return true;
}

static inline int BonesRenderer_LoadTexture(BonesRenderer* renderer, const char* path) {
    if (!renderer || !path) return 0;

    for (int i = 0; i < renderer->textureCount; i++)
        if (strcmp(renderer->texturePaths[i], path) == 0) return i;

    if (renderer->textureCount >= MAX_TEXTURES) return 0;

    Image img = LoadImage(path);
    if (img.data == NULL) {
        img = GenImageColor(1024, 1024, CLITERAL(Color){60, 120, 220, 255});
        ImageDrawText(&img, path, 8, 8, 128, WHITE);
    }
    ImageAlphaPremultiply(&img);
    renderer->textures[renderer->textureCount] = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(renderer->textures[renderer->textureCount], TEXTURE_FILTER_POINT);
    strncpy(renderer->texturePaths[renderer->textureCount], path, MAX_FILE_PATH_LENGTH - 1);
    renderer->texturePaths[renderer->textureCount][MAX_FILE_PATH_LENGTH - 1] = '\0';
    return renderer->textureCount++;
}

static inline BonesRenderConfig BonesGetDefaultRenderConfig(void) { return g_renderConfig; }
static inline void BonesSetRenderConfig(const BonesRenderConfig* config) { if (config) g_renderConfig = *config; }

static void DrawQuadTextured3D(Texture2D tex, Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
                                float u0, float v0t, float u1, float v1t) {
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlTexCoord2f(u0, v0t); rlVertex3f(v0.x, v0.y, v0.z);
    rlTexCoord2f(u1, v0t); rlVertex3f(v1.x, v1.y, v1.z);
    rlTexCoord2f(u1, v1t); rlVertex3f(v2.x, v2.y, v2.z);
    rlTexCoord2f(u0, v1t); rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
    rlSetTexture(0);
}

static void DrawQuadTextured3D_UVs(Texture2D tex, Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
                                    Vector2 uv0, Vector2 uv1, Vector2 uv2, Vector2 uv3) {
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlTexCoord2f(uv0.x, uv0.y); rlVertex3f(v0.x, v0.y, v0.z);
    rlTexCoord2f(uv1.x, uv1.y); rlVertex3f(v1.x, v1.y, v1.z);
    rlTexCoord2f(uv2.x, uv2.y); rlVertex3f(v2.x, v2.y, v2.z);
    rlTexCoord2f(uv3.x, uv3.y); rlVertex3f(v3.x, v3.y, v3.z);
    rlEnd();
    rlSetTexture(0);
}

static inline Vector3 SafeNormalize(Vector3 v) {
    float len = Vector3Length(v);
    return (len < 1e-6f) ? (Vector3){0, 0, 1} : Vector3Scale(v, 1.0f / len);
}

static inline bool IsWristBone(const char* boneName) {
    return boneName && (strcmp(boneName, "LWrist") == 0 || strcmp(boneName, "RWrist") == 0);
}

static inline Vector3 RotateVectorAroundAxis(Vector3 vector, Vector3 axis, float angle) {
    if (fabsf(angle) < 1e-6f) return vector;
    float cosA = cosf(angle), sinA = sinf(angle), omC = 1.0f - cosA;
    axis = SafeNormalize(axis);
    return (Vector3){
        vector.x * (cosA + axis.x*axis.x*omC) + vector.y * (axis.x*axis.y*omC - axis.z*sinA) + vector.z * (axis.x*axis.z*omC + axis.y*sinA),
        vector.x * (axis.y*axis.x*omC + axis.z*sinA) + vector.y * (cosA + axis.y*axis.y*omC) + vector.z * (axis.y*axis.z*omC - axis.x*sinA),
        vector.x * (axis.z*axis.x*omC - axis.y*sinA) + vector.y * (axis.z*axis.y*omC + axis.x*sinA) + vector.z * (cosA + axis.z*axis.z*omC)
    };
}

static inline Vector3 GetStablePerpendicularVector(Vector3 forward) {
    forward = SafeNormalize(forward);
    Vector3 candidates[3] = {{1,0,0},{0,1,0},{0,0,1}};
    Vector3 best = candidates[1];
    float minDot = fabsf(Vector3DotProduct(forward, candidates[1]));
    for (int i = 0; i < 3; i++) {
        float dot = fabsf(Vector3DotProduct(forward, candidates[i]));
        if (dot < minDot) { minDot = dot; best = candidates[i]; }
    }
    return best;
}

static inline CachedBones CacheBones(const Person* person) {
    CachedBones cache = {0};
    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;
        const char* n = bone->name;
        Vector3     p = bone->position.position;
        if      (strcmp(n,"Neck")       == 0) { cache.neck       = p; cache.hasNeck       = true; }
        else if (strcmp(n,"LShoulder")  == 0) { cache.lShoulder  = p; cache.hasLShoulder  = true; cache.shoulderCount++; }
        else if (strcmp(n,"RShoulder")  == 0) { cache.rShoulder  = p; cache.hasRShoulder  = true; cache.shoulderCount++; }
        else if (strcmp(n,"LHip")       == 0) { cache.lHip       = p; cache.hasLHip       = true; cache.hipCount++; }
        else if (strcmp(n,"RHip")       == 0) { cache.rHip       = p; cache.hasRHip       = true; cache.hipCount++; }
    }
    return cache;
}

static inline Vector3 GetBonePositionByName(const Person* person, const char* boneName) {
    if (!person || !boneName) return (Vector3){0,0,0};

    if (strcmp(boneName, "FRONT_CALCULATED") == 0) {
        Vector3 head = GetBonePositionByName(person, "Nose");
        Vector3 neck = GetBonePositionByName(person, "Neck");
        Vector3 dir  = Vector3Subtract(neck, head);
        dir.y = 0.0f;
        if (Vector3Length(dir) < 1e-6f) dir = (Vector3){0,0,1};
        return Vector3Add(head, Vector3Scale(SafeNormalize(dir), -10.15f));
    }
    if (strcmp(boneName, "REAR_CALCULATED") == 0) {
        Vector3 head = GetBonePositionByName(person, "HEAD_CALCULATED");
        Vector3 neck = GetBonePositionByName(person, "Neck");
        Vector3 dir  = SafeNormalize((Vector3){
            -(neck.z - head.z), 0, neck.x - head.x
        });
        return Vector3Add(head, Vector3Scale(dir, 10.15f));
    }
    if (strcmp(boneName, "HEAD_CALCULATED") == 0) return CalculateHeadPosition(person);

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (bone->position.valid && strcmp(bone->name, boneName) == 0)
            return bone->position.position;
    }
    return (Vector3){0,0,0};
}

static inline Vector3 CalculateHeadPosition(const Person* person) {
    if (!person || person->boneCount == 0) return (Vector3){0,0,0};

    Vector3 eyeCenter = {0,0,0};
    int     eyeCount  = 0;
    Vector3 neckPos   = {0,0,0};
    bool    hasNeck   = false;
    Vector3 nosePos   = {0,0,0};
    bool    hasNose   = false;
    Vector3 lEar = {0,0,0}, rEar = {0,0,0};
    bool    hasLEar = false, hasREar = false;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;
        const char* n = bone->name;
        Vector3     p = bone->position.position;
        if      (strcmp(n,"Neck") == 0)   { neckPos = p; hasNeck = true; }
        else if (strcmp(n,"Nose") == 0)   { nosePos = p; hasNose = true; }
        else if (strcmp(n,"LEye") == 0 || strcmp(n,"REye") == 0) { eyeCenter = Vector3Add(eyeCenter, p); eyeCount++; }
        else if (strcmp(n,"LEar") == 0)   { lEar = p; hasLEar = true; }
        else if (strcmp(n,"REar") == 0)   { rEar = p; hasREar = true; }
    }
    if (eyeCount > 0) eyeCenter = Vector3Scale(eyeCenter, 1.0f / eyeCount);

    if (hasNose && eyeCount >= 2) {
        Vector3 earCenter = (hasLEar && hasREar) ? Vector3Scale(Vector3Add(lEar, rEar), 0.5f) : eyeCenter;
        Vector3 faceCenter = Vector3Scale(Vector3Add(eyeCenter, earCenter), 0.5f);
        Vector3 frontDir = SafeNormalize(Vector3Subtract(nosePos, earCenter));

        Vector3 headPos = Vector3Add(faceCenter, Vector3Scale(frontDir, HEAD_DEPTH_OFFSET));
        float   vertComp = Vector3Subtract(nosePos, eyeCenter).y;
        float   upOff    = (vertComp < -0.01f) ? 0.015f : (vertComp > 0.01f) ? 0.045f : 0.03f;
        headPos.y += upOff;
        return headPos;
    }
    if (eyeCount > 0 && hasNeck) {
        return (Vector3){
            neckPos.x * 0.7f + eyeCenter.x * 0.3f,
            eyeCenter.y + 0.03f,
            hasNose ? neckPos.z * 0.8f + nosePos.z * 0.2f : neckPos.z * 0.9f + eyeCenter.z * 0.1f - 0.01f
        };
    }
    return (Vector3){0,0,0};
}

static inline Vector3 CalculateChestPosition(const Person* person) {
    if (!person || person->boneCount == 0) return (Vector3){0,0,0};
    CachedBones c = CacheBones(person);

    if (c.hasNeck && c.shoulderCount > 0) {
        Vector3 shoulderCenter = (c.hasLShoulder && c.hasRShoulder)
            ? Vector3Scale(Vector3Add(c.lShoulder, c.rShoulder), 0.5f)
            : (c.hasLShoulder ? c.lShoulder : c.rShoulder);
        return (Vector3){
            (c.neck.x + shoulderCenter.x) * 0.5f,
            c.neck.y + CHEST_OFFSET_Y,
            c.neck.z + CHEST_OFFSET_Z
        };
    }
    if (c.shoulderCount > 0 || c.hasNeck) {
        Vector3 total = {0,0,0}; int count = 0;
        if (c.hasNeck)       { total = Vector3Add(total, c.neck);       count++; }
        if (c.hasLShoulder)  { total = Vector3Add(total, c.lShoulder);  count++; }
        if (c.hasRShoulder)  { total = Vector3Add(total, c.rShoulder);  count++; }
        Vector3 result = Vector3Scale(total, 1.0f / count);
        result.y += CHEST_FALLBACK_Y;
        return result;
    }
    return (Vector3){0,0,0};
}

static inline Vector3 CalculateHipPosition(const Person* person) {
    if (!person || person->boneCount == 0) return (Vector3){0,0,0};
    CachedBones c = CacheBones(person);
    if (c.hipCount == 0) return (Vector3){0,0,0};

    Vector3 hipCenter = {0,0,0}; int hipPtCount = 0;
    if (c.hasLHip) { hipCenter = Vector3Add(hipCenter, c.lHip); hipPtCount++; }
    if (c.hasRHip) { hipCenter = Vector3Add(hipCenter, c.rHip); hipPtCount++; }
    hipCenter = Vector3Scale(hipCenter, 1.0f / hipPtCount);

    Vector3 torsoCenter = Vector3Add(hipCenter, hipCenter); int tcc = 2;
    if (c.shoulderCount > 0) {
        Vector3 sc = {0,0,0}; int spc = 0;
        if (c.hasLShoulder) { sc = Vector3Add(sc, c.lShoulder); spc++; }
        if (c.hasRShoulder) { sc = Vector3Add(sc, c.rShoulder); spc++; }
        torsoCenter = Vector3Add(torsoCenter, Vector3Scale(sc, 1.0f / spc)); tcc++;
    }
    torsoCenter = Vector3Scale(torsoCenter, 1.0f / tcc);

    Vector3 hipDir = {0,0,1};
    if (c.shoulderCount > 0) {
        Vector3 sc = {0,0,0}; int spc = 0;
        if (c.hasLShoulder) { sc = Vector3Add(sc, c.lShoulder); spc++; }
        if (c.hasRShoulder) { sc = Vector3Add(sc, c.rShoulder); spc++; }
        sc = Vector3Scale(sc, 1.0f / spc);
        Vector3 spineVec = Vector3Subtract(hipCenter, sc);
        float   len      = Vector3Length(spineVec);
        if (len > 1e-6f) hipDir = Vector3Scale(spineVec, 1.0f / len);
    }

    Vector3 hipPos   = Vector3Add(torsoCenter, Vector3Scale(hipDir, 0.01f));
    Vector3 chestPos = CalculateChestPosition(person);
    if (Vector3Length(chestPos) > 0.0f) {
        Vector3 chestToHip = Vector3Subtract(hipCenter, chestPos);
        float   dist       = Vector3Length(chestToHip);
        if (dist > 0.12f)
            hipPos = Vector3Add(chestPos, Vector3Scale(Vector3Scale(chestToHip, 1.0f/dist), 0.10f));
    }
    hipPos.y += HIP_OFFSET_Y;
    return hipPos;
}

static inline VirtualSpine CalculateVirtualSpine(const Person* person) {
    VirtualSpine spine = {0};
    if (!person || person->boneCount == 0) return spine;
    CachedBones c = CacheBones(person);
    if (!c.hasLShoulder || !c.hasRShoulder || !c.hasLHip || !c.hasRHip) return spine;

    spine.chestPosition = CalculateChestPosition(person);
    spine.hipPosition   = CalculateHipPosition(person);

    Vector3 spineVec = Vector3Subtract(spine.chestPosition, spine.hipPosition);
    float   spineLen = Vector3Length(spineVec);
    if (spineLen < 1e-6f) return spine;
    spine.spineDirection = Vector3Scale(spineVec, 1.0f / spineLen);

    Vector3 shoulderLine = Vector3Subtract(c.rShoulder, c.lShoulder);
    float   shoulderLen  = Vector3Length(shoulderLine);
    if (shoulderLen < 1e-6f) return spine;
    spine.spineRight   = Vector3Scale(shoulderLine, 1.0f / shoulderLen);
    spine.spineForward = Vector3CrossProduct(spine.spineRight, spine.spineDirection);

    float fwdLen = Vector3Length(spine.spineForward);
    if (fwdLen < 1e-6f) return spine;
    spine.spineForward = Vector3Scale(spine.spineForward, 1.0f / fwdLen);
    spine.spineRight   = Vector3CrossProduct(spine.spineDirection, spine.spineForward);
    float rightLen = Vector3Length(spine.spineRight);
    if (rightLen > 1e-6f) spine.spineRight = Vector3Scale(spine.spineRight, 1.0f / rightLen);

    spine.valid = true;
    return spine;
}

static inline TorsoOrientation CreateOrientation(Vector3 pos, Vector3 forward, Vector3 up, Vector3 right) {
    TorsoOrientation o = {0};
    o.position = pos; o.forward = forward; o.up = up; o.right = right;
    o.yaw      = atan2f(forward.x, forward.z);
    o.pitch    = atan2f(-forward.y, sqrtf(forward.x*forward.x + forward.z*forward.z));
    o.roll     = atan2f(right.y,    sqrtf(right.x*right.x     + right.z*right.z));
    o.valid    = true;
    return o;
}

static inline TorsoOrientation CalculateChestOrientation(const Person* person) {
    VirtualSpine spine = CalculateVirtualSpine(person);
    if (!spine.valid) {
        Vector3 pos = CalculateChestPosition(person);
        return (Vector3Length(pos) > 0.0f)
            ? CreateOrientation(pos, (Vector3){0,0,1}, (Vector3){0,1,0}, (Vector3){1,0,0})
            : (TorsoOrientation){0};
    }
    return CreateOrientation(spine.chestPosition, spine.spineForward, spine.spineDirection, spine.spineRight);
}

static inline TorsoOrientation CalculateHipOrientation(const Person* person) {
    VirtualSpine spine = CalculateVirtualSpine(person);
    if (!spine.valid) {
        Vector3 pos = CalculateHipPosition(person);
        return (Vector3Length(pos) > 0.0f)
            ? CreateOrientation(pos, (Vector3){0,0,1}, (Vector3){0,1,0}, (Vector3){1,0,0})
            : (TorsoOrientation){0};
    }
    return CreateOrientation(spine.hipPosition, spine.spineForward, spine.spineDirection, spine.spineRight);
}

static inline HeadOrientation CalculateHeadOrientation(const Person* person) {
    HeadOrientation o = {.valid = false};
    if (!person || person->boneCount == 0) return o;

    Vector3 nose = {0,0,0}, lEye = {0,0,0}, rEye = {0,0,0}, lEar = {0,0,0}, rEar = {0,0,0};
    bool hasNose = false, hasLEye = false, hasREye = false, hasLEar = false, hasREar = false;

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (!bone->position.valid) continue;
        const char* n = bone->name;
        if      (strcmp(n,"Nose") == 0) { nose = bone->position.position; hasNose = true; }
        else if (strcmp(n,"LEye") == 0) { lEye = bone->position.position; hasLEye = true; }
        else if (strcmp(n,"REye") == 0) { rEye = bone->position.position; hasREye = true; }
        else if (strcmp(n,"LEar") == 0) { lEar = bone->position.position; hasLEar = true; }
        else if (strcmp(n,"REar") == 0) { rEar = bone->position.position; hasREar = true; }
    }

    if (!hasNose || !((hasLEye && hasREye) || (hasLEar && hasREar))) {
        Vector3 fallback = CalculateHeadPosition(person);
        if (Vector3Length(fallback) > 0.0f) { o.position = fallback; o.valid = true; }
        return o;
    }

    o.position = CalculateHeadPosition(person);
    Vector3 rightVec, backRef;

    if (hasLEar && hasREar)       { rightVec = Vector3Normalize(Vector3Subtract(rEar, lEar));  backRef = Vector3Scale(Vector3Add(lEar, rEar), 0.5f); }
    else if (hasLEye && hasREye)  { rightVec = Vector3Normalize(Vector3Subtract(rEye, lEye));  backRef = Vector3Scale(Vector3Add(lEye, rEye), 0.5f); }
    else                          { rightVec = (Vector3){1,0,0}; backRef = o.position; }

    Vector3 forward = Vector3Normalize(Vector3Subtract(nose, backRef));
    if (Vector3Length(forward) < 1e-6f) forward = (Vector3){0,0,1};
    Vector3 up = Vector3Normalize(Vector3CrossProduct(rightVec, forward));
    if (Vector3Length(up) < 1e-6f) up = (Vector3){0,1,0};
    rightVec = Vector3Normalize(Vector3CrossProduct(forward, up));

    o.forward = forward; o.up = up; o.right = rightVec;
    o.yaw     = atan2f(forward.x, forward.z);
    o.pitch   = atan2f(-forward.y, sqrtf(forward.x*forward.x + forward.z*forward.z));
    o.roll    = atan2f(up.x, sqrtf(up.y*up.y + up.z*up.z));
    o.valid   = true;
    return o;
}

static inline BoneOrientation CalculateBoneOrientation(const char* boneName, const Person* person, Vector3 bonePosition) {
    BoneOrientation o = {0};
    o.position = bonePosition;
    if (!boneName || !person) return o;

    const char* primaryConn   = NULL;
    const char* secondaryConn = NULL;
    bool        reverseForward = false;
    for (int i = 0; BONE_ORIENTATIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_ORIENTATIONS[i].boneName, boneName) == 0) {
            primaryConn    = BONE_ORIENTATIONS[i].primaryConnection;
            secondaryConn  = BONE_ORIENTATIONS[i].secondaryConnection;
            reverseForward = BONE_ORIENTATIONS[i].reverseForward;
            break;
        }
    }

    Vector3 forward = {0,0,1}, up = {0,1,0}, right = {1,0,0};

    if (primaryConn && primaryConn[0]) {
        Vector3 primaryPos  = GetBonePositionByName(person, primaryConn);
        float   primaryDist = Vector3Length(Vector3Subtract(primaryPos, bonePosition));
        if (primaryDist > 1e-4f) {
            forward = Vector3Subtract(primaryPos, bonePosition);
            if (reverseForward) forward = Vector3Scale(forward, -1.0f);
            forward = SafeNormalize(forward);

            if (secondaryConn && secondaryConn[0]) {
                Vector3 secPos  = GetBonePositionByName(person, secondaryConn);
                float   secDist = Vector3Length(Vector3Subtract(secPos, bonePosition));
                if (secDist > 1e-4f) {
                    Vector3 toSec = SafeNormalize(Vector3Subtract(secPos, bonePosition));
                    right = Vector3CrossProduct(forward, toSec);
                    if (Vector3Length(right) > 1e-4f) {
                        right = SafeNormalize(right);
                        up    = SafeNormalize(Vector3CrossProduct(right, forward));
                    } else {
                        Vector3 tmp = GetStablePerpendicularVector(forward);
                        right = SafeNormalize(Vector3CrossProduct(forward, tmp));
                        up    = SafeNormalize(Vector3CrossProduct(right, forward));
                    }
                } else {
                    Vector3 tmp = GetStablePerpendicularVector(forward);
                    right = SafeNormalize(Vector3CrossProduct(forward, tmp));
                    up    = SafeNormalize(Vector3CrossProduct(right, forward));
                }
            } else {
                Vector3 tmp = GetStablePerpendicularVector(forward);
                right = SafeNormalize(Vector3CrossProduct(forward, tmp));
                up    = SafeNormalize(Vector3CrossProduct(right, forward));
            }
        }
    }

    for (int i = 0; BONE_ANGLE_OFFSETS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_ANGLE_OFFSETS[i].boneName, boneName) != 0) continue;
        float yawR   = BONE_ANGLE_OFFSETS[i].yawOffset   * (PI / 180.0f);
        float pitchR = BONE_ANGLE_OFFSETS[i].pitchOffset * (PI / 180.0f);
        float rollR  = BONE_ANGLE_OFFSETS[i].rollOffset  * (PI / 180.0f);
        if (fabsf(yawR)   > 1e-6f) { forward = RotateVectorAroundAxis(forward, up,      yawR);   right = RotateVectorAroundAxis(right, up,      yawR);   }
        if (fabsf(pitchR) > 1e-6f) { forward = RotateVectorAroundAxis(forward, right,   pitchR); up    = RotateVectorAroundAxis(up,    right,   pitchR); }
        if (fabsf(rollR)  > 1e-6f) { right   = RotateVectorAroundAxis(right,   forward, rollR);  up    = RotateVectorAroundAxis(up,    forward, rollR);  }
        break;
    }

    forward = SafeNormalize(forward);
    right   = SafeNormalize(Vector3CrossProduct(forward, up));
    up      = SafeNormalize(Vector3CrossProduct(right, forward));

    o.forward = forward; o.up = up; o.right = right;
    o.yaw   = atan2f(forward.x, forward.z);
    float horizDist = sqrtf(forward.x*forward.x + forward.z*forward.z);
    o.pitch = atan2f(-forward.y, fmaxf(horizDist, 1e-6f));
    Vector3 worldUp = {0,1,0};
    Vector3 projR   = SafeNormalize(Vector3Subtract(right, Vector3Scale(forward, Vector3DotProduct(right, forward))));
    o.roll  = atan2f(Vector3DotProduct(projR, worldUp), Vector3DotProduct(projR, Vector3CrossProduct(forward, worldUp)));
    o.valid = true;
    return o;
}

static inline bool ShouldUseVariableHeight(const char* boneName) {
    if (!boneName) return false;
    static const char* bones[] = {"LShoulder","LElbow","RShoulder","RElbow","LHip","LKnee","RHip","RKnee"};
    for (int i = 0; i < 8; i++) if (strcmp(boneName, bones[i]) == 0) return true;
    return false;
}

static inline bool ShouldFlipBoneTexture(const char* boneName) {
    if (!boneName) return false;
    static const char* bones[] = {"LShoulder","LElbow","RShoulder","RElbow","LHip","LKnee","RHip","RKnee"};
    for (int i = 0; i < 8; i++) if (strcmp(boneName, bones[i]) == 0) return true;
    return false;
}

static inline bool ShouldRenderHead(const Person* person) {
    if (!person || !person->active) return false;
    int n = 0;
    for (int i = 0; i < person->boneCount; i++) {
        const Bone* b = &person->bones[i];
        if (!b->position.valid) continue;
        char c = b->name[0];
        if ((c == 'N' && strcmp(b->name,"Nose") == 0) ||
            ((c == 'L' || c == 'R') && (strstr(b->name,"Eye") || strstr(b->name,"Ear")))) n++;
    }
    return n >= 2;
}

static inline bool ShouldRenderChest(const Person* person) {
    return person && person->active && CacheBones(person).shoulderCount >= 1;
}

static inline bool ShouldRenderHip(const Person* person) {
    return person && person->active && CacheBones(person).hipCount >= 1;
}

static inline bool GetBoneConnectionsWithPriority(const char* boneName, char connections[3][32], float priorities[3]) {
    for (int i = 0; BONE_CONNECTIONS[i].boneName[0]; i++) {
        if (strcmp(BONE_CONNECTIONS[i].boneName, boneName) != 0) continue;
        for (int j = 0; j < 3; j++) {
            strncpy(connections[j], BONE_CONNECTIONS[i].connections[j], 31);
            connections[j][31] = '\0';
            priorities[j] = BONE_CONNECTIONS[i].priority[j];
        }
        return true;
    }
    return false;
}

static inline Vector3 CalculateBoneMidpoint(const char* boneName, const Person* person) {
    if (!person || !boneName) return (Vector3){0,0,0};
    if (ShouldUseVariableHeight(boneName)) return GetBonePositionByName(person, boneName);

    if (strcmp(boneName, "Neck") == 0) {
        Vector3 neck = GetBonePositionByName(person, "Neck");
        if (neck.x == 0 && neck.y == 0 && neck.z == 0) return neck;
        Vector3 head = CalculateHeadPosition(person);
        if (head.x == 0 && head.y == 0 && head.z == 0) return neck;

        Vector3 lS = GetBonePositionByName(person, "LShoulder");
        Vector3 rS = GetBonePositionByName(person, "RShoulder");
        Vector3 center = neck;
        if ((lS.x||lS.y||lS.z) && (rS.x||rS.y||rS.z))
            center = (Vector3){(lS.x+rS.x)*0.5f, (lS.y+rS.y)*0.5f, (lS.z+rS.z)*0.5f};
        return (Vector3){(center.x+head.x)*0.5f, (center.y+head.y)*0.5f, (center.z+head.z)*0.5f};
    }

    const char* connName = NULL; float projFactor = 1.0f;
    for (int i = 0; MIDPOINT_CONNECTIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(MIDPOINT_CONNECTIONS[i].boneName, boneName) == 0) {
            connName   = MIDPOINT_CONNECTIONS[i].connectedBone;
            projFactor = MIDPOINT_CONNECTIONS[i].projectionFactor;
            break;
        }
    }
    if (!connName) return GetBonePositionByName(person, boneName);

    Vector3 bonePos = GetBonePositionByName(person, boneName);
    Vector3 connPos = GetBonePositionByName(person, connName);

    if (strstr(boneName, "Wrist") && projFactor != 1.0f) {
        if ((bonePos.x||bonePos.y||bonePos.z) && (connPos.x||connPos.y||connPos.z)) {
            Vector3 dir = Vector3Subtract(bonePos, connPos);
            return Vector3Add(connPos, Vector3Scale(dir, projFactor));
        }
        return bonePos;
    }
    if (strstr(boneName, "Ankle") && strstr(connName, "FOOT_FORWARD")) {
        if (bonePos.x||bonePos.y||bonePos.z)
            return (Vector3){bonePos.x, bonePos.y - 0.025f, bonePos.z + 0.008f};
        return bonePos;
    }
    if (!(bonePos.x||bonePos.y||bonePos.z)) return (Vector3){0,0,0};
    if (!(connPos.x||connPos.y||connPos.z))  return bonePos;
    return (Vector3){(bonePos.x+connPos.x)*0.5f, (bonePos.y+connPos.y)*0.5f, (bonePos.z+connPos.z)*0.5f};
}

static inline bool ResizeRenderBonesArray(BoneRenderData** renderBones, int* renderBonesCapacity, int newCapacity) {
    if (newCapacity <= 0 || newCapacity > 10000 || newCapacity <= *renderBonesCapacity) return true;
    BoneRenderData* newArr = realloc(*renderBones, sizeof(BoneRenderData) * newCapacity);
    if (!newArr) return false;
    memset(newArr + *renderBonesCapacity, 0, sizeof(BoneRenderData) * (newCapacity - *renderBonesCapacity));
    *renderBones         = newArr;
    *renderBonesCapacity = newCapacity;
    return true;
}

static inline void EnrichBoneRenderDataWithOrientation(BoneRenderData* renderBone, const Person* person) {
    if (!renderBone || !person) return;
    if (IsWristBone(renderBone->boneName)) { renderBone->orientation.valid = false; return; }

    BoneOrientation orient = CalculateBoneOrientation(renderBone->boneName, person, renderBone->position);
    if (orient.valid) {
        renderBone->orientation = orient;
    } else {
        renderBone->orientation.position = renderBone->position;
        renderBone->orientation.forward  = (Vector3){0,0,1};
        renderBone->orientation.up       = (Vector3){0,1,0};
        renderBone->orientation.right    = (Vector3){1,0,0};
        renderBone->orientation.valid    = true;
    }
}

static inline BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int boneCount, const char* boneName) {
    for (int i = 0; i < boneCount; i++)
        if (strcmp(bones[i].boneName, boneName) == 0) return &bones[i];
    return NULL;
}

static inline const Person* FindPersonByBoneName(const AnimationFrame* frame, const char* boneName) {
    if (!frame || !boneName) return NULL;
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++)
            if (strcmp(person->bones[b].name, boneName) == 0) return person;
    }
    return NULL;
}

static inline BoneConnectionPositions GetBoneConnectionPositionsEx(const BoneRenderData* boneData, const Person* person) {
    BoneConnectionPositions result = {0};
    if (!boneData || !boneData->valid) return result;
    result.pos0 = boneData->position;
    result.pos1 = result.pos0;

    for (int i = 0; BONE_CONNECTIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_CONNECTIONS[i].boneName, boneData->boneName) != 0) continue;
        for (int k = 1; k < 3; k++) {
            const char* neighborName = BONE_CONNECTIONS[i].connections[k];
            if (!neighborName || !neighborName[0] || strcmp(neighborName, boneData->boneName) == 0) continue;
            if (person) {
                Vector3 p = GetBonePositionByName(person, neighborName);
                if (Vector3Length(p) > 1e-6f) { result.pos1 = p; return result; }
            }
        }
        return result;
    }
    return result;
}

static inline void CollectBonesForRendering(const BonesAnimation* animation, Camera camera,
        BoneRenderData** renderBones, int* renderBonesCount, int* renderBonesCapacity,
        BoneConfig* boneConfigs, int boneConfigCount, TextureSetCollection* textureSets, OrnamentSystem* ornaments)
{
    *renderBonesCount = 0;
    if (!animation->isLoaded) return;
    int curFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, curFrame)) return;
    const AnimationFrame* frame = &animation->frames[curFrame];

    int estimatedBones = 0;
    for (int p = 0; p < frame->personCount; p++)
        if (frame->persons[p].active) estimatedBones += frame->persons[p].boneCount;
    if (!ResizeRenderBonesArray(renderBones, renderBonesCapacity, estimatedBones + 10)) return;

    static char processedBones[2000][MAX_BONE_NAME_LENGTH + 20];
    int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;

        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid || !bone->visible || !BonesIsPositionValid(bone->position.position)) continue;

            BoneConfig* cfg = FindBoneConfig(boneConfigs, boneConfigCount, bone->name);
            if (!cfg) {
                char fc = bone->name[0];
                bool known = (fc == 'N' && strstr(bone->name,"Nose")) ||
                    ((fc == 'L' || fc == 'R') && (strstr(bone->name,"Eye") || strstr(bone->name,"Ear") ||
                     strstr(bone->name,"Shoulder") || strstr(bone->name,"Elbow") ||
                     strstr(bone->name,"Wrist") || strstr(bone->name,"Hip") ||
                     strstr(bone->name,"Knee") || strstr(bone->name,"Ankle"))) ||
                    (fc == 'H' && (strstr(bone->name,"Head") || strstr(bone->name,"Hip"))) ||
                    (fc == 'C' && strstr(bone->name,"Chest")) ||
                    (fc == 'S' && (strstr(bone->name,"Shoulder") || strstr(bone->name,"Spine")));
                if (!known) continue;
            } else if (cfg->visibleMode == 0) continue;

            char boneKey[MAX_BONE_NAME_LENGTH + 20];
            int keyLen = snprintf(boneKey, sizeof(boneKey), "%s_%s", person->personId, bone->name);
            bool alreadyProcessed = false;
            for (int i = 0; i < processedCount; i++) {
                if (memcmp(processedBones[i], boneKey, keyLen + 1) == 0) { alreadyProcessed = true; break; }
            }
            if (alreadyProcessed) continue;
            if (processedCount < 2000) { memcpy(processedBones[processedCount++], boneKey, keyLen + 1); }

            Vector3 midPos = CalculateBoneMidpoint(bone->name, person);
            if (!BonesIsPositionValid(midPos)) continue;
            float distance = Vector3Distance(camera.position, midPos);
            if (distance > 50.0f) continue;

            BoneRenderData* rd = &(*renderBones)[*renderBonesCount];
            rd->position = midPos;
            rd->distance = distance;
            rd->valid    = true;

            const char* activeTexture = (textureSets && textureSets->loaded)
                ? BonesTextureSets_GetActiveTexture(textureSets, bone->name) : NULL;

            if (activeTexture) {
                strncpy(rd->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
                rd->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                rd->visible       = cfg ? (cfg->visibleMode != 0) : true;
                rd->isAsymmetric  = cfg ? (cfg->visibleMode == 2) : false;
                rd->size          = cfg ? cfg->size : 0.35f;
            } else if (cfg) {
                strncpy(rd->texturePath, cfg->texturePath, MAX_FILE_PATH_LENGTH - 1);
                rd->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                rd->visible      = (cfg->visibleMode != 0);
                rd->isAsymmetric = (cfg->visibleMode == 2);
                rd->size         = cfg->size;
            } else {
                strncpy(rd->texturePath, GetTexturePathForBone(boneConfigs, boneConfigCount, bone->name, textureSets), MAX_FILE_PATH_LENGTH - 1);
                rd->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                rd->visible      = IsBoneVisible(boneConfigs, boneConfigCount, bone->name);
                rd->isAsymmetric = false;
                rd->size         = GetBoneSize(boneConfigs, boneConfigCount, bone->name);
            }

            strncpy(rd->boneName,  bone->name,       MAX_BONE_NAME_LENGTH - 1); rd->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
            strncpy(rd->personId,  person->personId, 15);                        rd->personId[15] = '\0';
            EnrichBoneRenderDataWithOrientation(rd, person);
            (*renderBonesCount)++;
        }
    }

    if (ornaments && ornaments->loaded)
        Ornaments_CollectForRendering(ornaments, renderBones, renderBonesCount, renderBonesCapacity, camera);

    for (int i = 0; i < *renderBonesCount - 1; i++)
        for (int j = 0; j < *renderBonesCount - i - 1; j++)
            if ((*renderBones)[j].distance < (*renderBones)[j+1].distance) {
                BoneRenderData tmp = (*renderBones)[j];
                (*renderBones)[j]   = (*renderBones)[j+1];
                (*renderBones)[j+1] = tmp;
            }
}

static inline void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads,
        int* headCount, int* headCapacity, BoneConfig* boneConfigs, int boneConfigCount, TextureSetCollection* textureSets)
{
    *headCount = 0;
    if (!animation->isLoaded) return;
    int curFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, curFrame)) return;
    const AnimationFrame* frame = &animation->frames[curFrame];

    if (*headCapacity < frame->personCount) {
        HeadRenderData* newArr = (HeadRenderData*)realloc(*heads, sizeof(HeadRenderData) * frame->personCount);
        if (!newArr) return;
        *heads        = newArr;
        *headCapacity = frame->personCount;
    }

    static char processed[200][25]; int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!ShouldRenderHead(person)) continue;

        char key[25]; snprintf(key, sizeof(key), "%s_head", person->personId);
        bool already = false;
        for (int i = 0; i < processedCount; i++) if (strcmp(processed[i], key) == 0) { already = true; break; }
        if (already) continue;
        if (processedCount < 200) { strncpy(processed[processedCount], key, 24); processed[processedCount++][24] = '\0'; }

        HeadRenderData* head = &(*heads)[*headCount];
        memset(head, 0, sizeof(HeadRenderData));
        head->position    = CalculateHeadPosition(person);
        head->orientation = CalculateHeadOrientation(person);
        head->valid       = true;
        head->visible     = true;

        const char* activeTexture = (textureSets && textureSets->loaded)
            ? BonesTextureSets_GetActiveTexture(textureSets, "Head") : NULL;

        if (activeTexture) {
            strncpy(head->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
            head->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
        } else {
            BoneConfig* cfg = FindBoneConfig(boneConfigs, boneConfigCount, "Head");
            if (cfg) { strncpy(head->texturePath, cfg->texturePath, MAX_FILE_PATH_LENGTH - 1); head->size = cfg->size; }
            else      { strncpy(head->texturePath, "data/textures/hil/Head.png", MAX_FILE_PATH_LENGTH - 1); head->size = 0.25f; }
            head->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
        }
        if (activeTexture) {
            BoneConfig* cfg = FindBoneConfig(boneConfigs, boneConfigCount, "Head");
            head->size = cfg ? cfg->size : 0.25f;
        }

        strncpy(head->personId, person->personId, 15); head->personId[15] = '\0';
        (*headCount)++;
    }
}

static inline void CollectTorsosForRendering(const BonesAnimation* animation, TorsoRenderData** torsos,
        int* torsoCount, int* torsoCapacity, BoneConfig* boneConfigs, int boneConfigCount, TextureSetCollection* textureSets)
{
    *torsoCount = 0;
    if (!animation->isLoaded) return;
    int curFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, curFrame)) return;
    const AnimationFrame* frame = &animation->frames[curFrame];

    int estimated = frame->personCount * 2;
    if (*torsoCapacity < estimated) {
        TorsoRenderData* newArr = (TorsoRenderData*)realloc(*torsos, sizeof(TorsoRenderData) * estimated);
        if (!newArr) return;
        *torsos        = newArr;
        *torsoCapacity = estimated;
    }

    static char processed[200][25]; int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];

        for (int isHip = 0; isHip <= 1; isHip++) {
            if (isHip ? !ShouldRenderHip(person) : !ShouldRenderChest(person)) continue;

            char key[25]; snprintf(key, sizeof(key), "%s_%s", person->personId, isHip ? "hip" : "chest");
            bool already = false;
            for (int i = 0; i < processedCount; i++) if (strcmp(processed[i], key) == 0) { already = true; break; }
            if (already) continue;
            if (processedCount < 200) { strncpy(processed[processedCount], key, 24); processed[processedCount++][24] = '\0'; }

            TorsoRenderData* td = &(*torsos)[*torsoCount];
            memset(td, 0, sizeof(TorsoRenderData));

            if (isHip) {
                td->position    = CalculateHipPosition(person);
                td->orientation = CalculateHipOrientation(person);
                td->type        = TORSO_HIP;
            } else {
                td->position    = CalculateChestPosition(person);
                td->orientation = CalculateChestOrientation(person);
                td->type        = TORSO_CHEST;
            }
            td->person = person;
            if (!td->orientation.valid && Vector3Length(td->position) < 1e-6f) continue;
            td->valid = true; td->visible = true;

            const char* bonePart = isHip ? "Hip" : "Chest";
            const char* defTex   = isHip ? "tex/Hip.png" : "tex/Chest.png";
            float       defSize  = isHip ? 0.35f : 0.4f;

            const char* activeTexture = (textureSets && textureSets->loaded)
                ? BonesTextureSets_GetActiveTexture(textureSets, bonePart) : NULL;

            BoneConfig* cfgPart = FindBoneConfig(boneConfigs, boneConfigCount, bonePart);
            td->isAsymmetric = cfgPart ? (cfgPart->visibleMode == 2) : false;

            if (activeTexture) {
                strncpy(td->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
                td->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
            } else {
                BoneConfig* cfg = FindBoneConfig(boneConfigs, boneConfigCount, bonePart);
                if (cfg) { strncpy(td->texturePath, cfg->texturePath, MAX_FILE_PATH_LENGTH - 1); td->size = cfg->size; td->visible = (cfg->visibleMode != 0); }
                else      { strncpy(td->texturePath, defTex,           MAX_FILE_PATH_LENGTH - 1); td->size = defSize; }
                td->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
            }
            if (activeTexture) {
                BoneConfig* cfg = FindBoneConfig(boneConfigs, boneConfigCount, bonePart);
                td->size    = cfg ? cfg->size                : defSize;
                td->visible = cfg ? (cfg->visibleMode != 0) : true;
            }

            strncpy(td->personId, person->personId, 15); td->personId[15] = '\0';
            (*torsoCount)++;
        }
    }
}

static inline Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows,
                                        bool mirrored, bool* outMirrored) {
    if (logicalCol < 0)  logicalCol  = 0;
    if (logicalRow < 0)  logicalRow  = 0;
    if (physCols   <= 0) physCols    = ATLAS_COLS;
    if (physRows   <= 0) physRows    = ATLAS_ROWS;
    if (logicalCol >= physCols) logicalCol = physCols - 1;
    if (logicalRow >= physRows) logicalRow = physRows - 1;

    float cellW = (float)tex.width  / (float)physCols;
    float cellH = (float)tex.height / (float)physRows;
    if (outMirrored) *outMirrored = mirrored;
    return (Rectangle){logicalCol * cellW, logicalRow * cellH, cellW, cellH};
}

static inline void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
                                       float rotationDeg, bool mirrored, const char* boneName) {
    Vector3 camFwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right  = Vector3Normalize(Vector3CrossProduct(camFwd, camera.up));
    Vector3 up     = Vector3Normalize(Vector3CrossProduct(right, camFwd));

    float a = rotationDeg * (PI / 180.0f);
    Vector3 newRight = Vector3Subtract(Vector3Scale(right, cosf(a)), Vector3Scale(up, sinf(a)));
    Vector3 newUp    = Vector3Add(Vector3Scale(right, sinf(a)),      Vector3Scale(up, cosf(a)));

    Vector3 halfX = Vector3Scale(newRight, size.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp,    size.y * 0.5f);
    Vector3 p0 = Vector3Subtract(Vector3Subtract(pos, halfX), halfY);
    Vector3 p1 = Vector3Add(Vector3Subtract(pos, halfY), halfX);
    Vector3 p2 = Vector3Add(Vector3Add(pos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(pos, halfY), halfX);

    float texW = (float)tex.width, texH = (float)tex.height;
    float uL = src.x / texW, uR = (src.x + src.width)  / texW;
    float vT = src.y / texH, vB = (src.y + src.height) / texH;
    if (src.width  < 0) { float t = uL; uL = uR; uR = t; }
    if (src.height < 0) { float t = vT; vT = vB; vB = t; }

    bool needsVFlip = ShouldFlipBoneTexture(boneName);
    float v0t = needsVFlip ? vT : vB;
    float v1t = needsVFlip ? vB : vT;
    if (mirrored) { float t = uL; uL = uR; uR = t; }

    DrawQuadTextured3D(tex, p0, p1, p2, p3, uL, v0t, uR, v1t);
}

static inline void DrawBonetileCustomWithRoll(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
        float rotationDeg, bool mirrored, bool neighborValid, Vector3 neighborPos,
        const BoneRenderData* boneData, const Person* person)
{
    Vector3 camToPos = Vector3Subtract(pos, camera.position);
    float dist = Vector3Length(camToPos);
    if (dist > 0.0f) {
        camToPos = Vector3Scale(camToPos, 1.0f / dist);
        float camPitch = atan2f(-camToPos.y, sqrtf(camToPos.x*camToPos.x + camToPos.z*camToPos.z)) * RAD2DEG;
        if (fabsf(camPitch) > 50.0f && strcmp(boneData->boneName, "Neck") != 0) {
            Vector3 center   = GetBonePositionByName(person, "Neck");
            Vector3 camFwd   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camFwd, camera.up));
            Vector3 camUp    = Vector3Normalize(Vector3CrossProduct(camRight, camFwd));
            Vector3 toCenter = Vector3Normalize(Vector3Subtract(center, boneData->position));
            Vector3 proj     = Vector3Subtract(toCenter, Vector3Scale(camFwd, Vector3DotProduct(toCenter, camFwd)));
            float   projLen  = Vector3Length(proj);
            proj = Vector3Scale(proj, 1.0f / projLen);
            rotationDeg   = atan2f(Vector3DotProduct(proj, camRight), Vector3DotProduct(proj, camUp)) * RAD2DEG + 180.0f;
            neighborValid = false;
        }
    }

    Vector3 camFwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right  = Vector3Normalize(Vector3CrossProduct(camFwd, camera.up));
    Vector3 up     = Vector3Normalize(Vector3CrossProduct(right, camFwd));

    float rollExtraDeg = 0.0f;
    if (neighborValid) {
        Vector3 dir = Vector3Subtract(neighborPos, pos);
        if (Vector3Length(dir) > 0.0001f) {
            dir = Vector3Normalize(dir);
            rollExtraDeg = atan2f(Vector3DotProduct(dir, right), Vector3DotProduct(dir, up)) * (180.0f / PI);
        }
    }

    float scaleUp = 0.5f, scaleDown = 0.5f;
    for (int i = 0; BONE_SCALE_FACTORS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_SCALE_FACTORS[i].boneName, boneData->boneName) == 0) {
            scaleUp   = BONE_SCALE_FACTORS[i].scaleFactorUp;
            scaleDown = BONE_SCALE_FACTORS[i].scaleFactorDown;
            break;
        }
    }

    Vector2 actualSize = size;
    Vector3 actualPos  = pos;

    if (ShouldUseVariableHeight(boneData->boneName) && neighborValid) {
        float   neighborDist = Vector3Distance(pos, neighborPos);
        Vector3 boneDir      = Vector3Normalize(Vector3Subtract(neighborPos, pos));
        Vector3 camDir       = Vector3Normalize(Vector3Subtract(camera.position, pos));
        float   parallel     = Vector3DotProduct(boneDir, camDir);
        Vector3 projBone     = Vector3Subtract(boneDir, Vector3Scale(camDir, parallel));
        float   visRatio     = fmaxf(0.3f, Vector3Length(projBone));

        actualSize.y = neighborDist * visRatio * (scaleUp + scaleDown);
        float   offsetFactor = scaleDown - scaleUp;
        Vector3 toNeighbor   = Vector3Normalize(Vector3Subtract(neighborPos, pos));
        actualPos = Vector3Add(pos, Vector3Scale(toNeighbor, neighborDist * offsetFactor * 0.5f));
    }

    float a = (rotationDeg + rollExtraDeg) * (PI / 180.0f);
    Vector3 newRight = Vector3Subtract(Vector3Scale(right, cosf(a)), Vector3Scale(up, sinf(a)));
    Vector3 newUp    = Vector3Add(Vector3Scale(right, sinf(a)),      Vector3Scale(up, cosf(a)));
    Vector3 halfX = Vector3Scale(newRight, actualSize.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp,    actualSize.y * 0.5f);
    Vector3 p0 = Vector3Subtract(Vector3Subtract(actualPos, halfX), halfY);
    Vector3 p1 = Vector3Add(Vector3Subtract(actualPos, halfY), halfX);
    Vector3 p2 = Vector3Add(Vector3Add(actualPos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(actualPos, halfY), halfX);

    float texW = (float)tex.width, texH = (float)tex.height;
    float uL = src.x / texW, uR = (src.x + src.width)  / texW;
    float vT = src.y / texH, vB = (src.y + src.height) / texH;
    if (src.width  < 0) { float t = uL; uL = uR; uR = t; }
    if (src.height < 0) { float t = vT; vT = vB; vB = t; }

    bool needsVFlip = ShouldFlipBoneTexture(boneData->boneName);
    float v0t = needsVFlip ? vT : vB;
    float v1t = needsVFlip ? vB : vT;
    if (mirrored) { float t = uL; uL = uR; uR = t; }

    Vector2 uv0 = {uL,v0t}, uv1 = {uR,v0t}, uv2 = {uR,v1t}, uv3 = {uL,v1t};
    DrawQuadTextured3D_UVs(tex, p0, p1, p2, p3, uv0, uv1, uv2, uv3);
}

static inline void CalculateHandBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
                                                float* outRotation, bool* outMirrored, const char* boneName) {
    static const int rightHandIndices[3][8] = {
        {23,22, 2,15,16,17,18,24},
        { 9, 4, 0, 5, 6, 7, 8,14},
        {20,19, 1,10,11,12,13,21}
    };
    static const int leftHandIndices[3][8] = {
        {16,15, 2,22,23,24,18,17},
        { 6, 5, 0, 4, 9,14, 8, 7},
        {11,10, 1,19,20,21,13,12}
    };

    Vector3 camDir = SafeNormalize(Vector3Subtract(camera.position, bonePos));
    bool isWrist   = boneName && IsWristBone(boneName);
    bool isLeft    = boneName && (strcmp(boneName, "LWrist") == 0);

    float yawOffRad = 0.0f, pitchOffRad = 0.0f;
    if (boneName) {
        for (int i = 0; BONE_ANGLE_OFFSETS[i].boneName[0] != '\0'; i++) {
            if (strcmp(BONE_ANGLE_OFFSETS[i].boneName, boneName) == 0) {
                yawOffRad   = BONE_ANGLE_OFFSETS[i].yawOffset   * (PI / 180.0f);
                pitchOffRad = BONE_ANGLE_OFFSETS[i].pitchOffset * (PI / 180.0f);
                break;
            }
        }
    }

    if (fabsf(yawOffRad) > 1e-6f) {
        float cY = cosf(yawOffRad), sY = sinf(yawOffRad);
        float nx = camDir.x*cY - camDir.z*sY, nz = camDir.x*sY + camDir.z*cY;
        camDir.x = nx; camDir.z = nz;
    }
    if (fabsf(pitchOffRad) > 1e-6f) {
        float hD = sqrtf(camDir.x*camDir.x + camDir.z*camDir.z);
        if (hD > 1e-6f) {
            float cP = cosf(pitchOffRad), sP = sinf(pitchOffRad);
            float nY  = camDir.y * cP - hD * sP;
            float nH  = camDir.y * sP + hD * cP;
            float sc  = nH / hD;
            camDir.x *= sc; camDir.z *= sc; camDir.y = nY;
        }
    }

    if (!isWrist) { float t = camDir.x; camDir.x = camDir.z; camDir.z = t; }

    float yaw   = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;
    float pitchDeg   = atan2f(camDir.y, sqrtf(camDir.x*camDir.x + camDir.z*camDir.z)) * RAD2DEG;
    float normYaw    = yaw * RAD2DEG + 22.5f;
    if (normYaw >= 360.0f) normYaw -= 360.0f;
    int sector = (int)(normYaw / 45.0f) % 8;

    if (isWrist) {
        int row = (pitchDeg >= 22.5f) ? 0 : (pitchDeg >= -22.5f) ? 1 : 2;
        row    = (row < 0) ? 0 : (row > 2) ? 2 : row;
        sector = (sector < 0) ? 0 : (sector > 7) ? 7 : sector;
        *outChosenIndex = isLeft ? leftHandIndices[row][sector] : rightHandIndices[row][sector];
        *outMirrored    = isLeft;
        *outRotation    = 0.0f;
        if (*outChosenIndex < 0) *outChosenIndex = 0;
        if (*outChosenIndex > 24) *outChosenIndex %= 25;
    } else {
        *outMirrored = false;
    }

    if (pitchDeg >= 52.5f)  { *outChosenIndex = 22; *outRotation = sector * 45.0f + 180.0f; *outMirrored = isLeft; }
    else if (pitchDeg <= -51.0f) { *outChosenIndex = 3; *outRotation = 0.0f; *outMirrored = isLeft; }
}

static inline void CalculateHandBoneRenderDataWithOrientation(const BoneRenderData* boneData, Camera camera,
                                                               int* outChosenIndex, float* outRotation, bool* outMirrored) {
    static const int rightHandIndices[3][8] = {
        {23,22, 2,15,16,17,18,24},
        { 9, 4, 0, 5, 6, 7, 8,14},
        {20,19, 1,10,11,12,13,21}
    };
    static const int leftHandIndices[3][8] = {
        {16,15, 2,22,23,24,18,17},
        { 6, 5, 0, 4, 9,14, 8, 7},
        {11,10, 1,19,20,21,13,12}
    };

    bool isLeft = (strcmp(boneData->boneName, "LWrist") == 0);
    Vector3 camDir = SafeNormalize(Vector3Subtract(boneData->position, camera.position));
    Vector3 local  = {
        Vector3DotProduct(camDir, boneData->orientation.right),
        Vector3DotProduct(camDir, boneData->orientation.up),
        Vector3DotProduct(camDir, boneData->orientation.forward)
    };

    float localYaw = atan2f(local.x, local.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localPitch = -atan2f(local.y, sqrtf(local.x*local.x + local.z*local.z)) * RAD2DEG;
    float normYaw    = localYaw * RAD2DEG + 22.5f;
    if (normYaw >= 360.0f) normYaw -= 360.0f;
    int sector = (8 - (int)(normYaw / 45.0f) % 8) % 8;

    if (localPitch >= 52.5f)   { *outChosenIndex = 22; *outRotation = sector*45.0f+180.0f; *outMirrored = isLeft; }
    else if (localPitch <= -51.0f) { *outChosenIndex = 3; *outRotation = 0.0f; *outMirrored = isLeft; }
    else {
        int row = (localPitch >= 22.5f) ? 2 : (localPitch >= -22.5f) ? 1 : 0;
        *outChosenIndex = isLeft ? leftHandIndices[row][sector] : rightHandIndices[row][sector];
        *outMirrored    = isLeft;
        *outRotation    = 0.0f;
    }
}

static inline void CalculateLimbBoneRenderData(const BoneRenderData* boneData, const Person* person, Camera camera,
                                                int* outChosenIndex, float* outRotation, bool* outMirrored) {
    if (!boneData->orientation.valid) {
        CalculateHandBoneRenderData(boneData->position, camera, outChosenIndex, outRotation, outMirrored, boneData->boneName);
        return;
    }

    static const int indices[3][8] = {
        {0, 4, 5, 6, 7, 6, 5, 4},
        {2,12,13,14,15,14,13,12},
        {1, 8, 9,10,11,10, 9, 8}
    };

    Vector3 camDir  = SafeNormalize(Vector3Subtract(boneData->position, camera.position));
    Vector3 local   = {
        Vector3DotProduct(camDir, boneData->orientation.right),
        Vector3DotProduct(camDir, boneData->orientation.up),
        Vector3DotProduct(camDir, boneData->orientation.forward)
    };

    float localYaw   = atan2f(local.x, local.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float pitchDeg   = atan2f(local.y, sqrtf(local.x*local.x + local.z*local.z)) * RAD2DEG;

    bool invertPitch = boneData->boneName[0] &&
        strcmp(boneData->boneName,"Neck")      != 0 &&
        strcmp(boneData->boneName,"RShoulder") != 0 &&
        strcmp(boneData->boneName,"RHip")      != 0 &&
        strcmp(boneData->boneName,"RElbow")    != 0 &&
        strcmp(boneData->boneName,"RKnee")     != 0 &&
        strcmp(boneData->boneName,"LAnkle")    != 0;
    if (invertPitch) pitchDeg = -pitchDeg;

    float compFactor = 0.0f;
    if (person && boneData->boneName[0]) {
        if      (strstr(boneData->boneName,"Hip")      || strstr(boneData->boneName,"Shoulder")) compFactor = strstr(boneData->boneName,"Hip") ? 0.25f : 0.15f;
        else if (strstr(boneData->boneName,"Knee")     || strstr(boneData->boneName,"Elbow"))    compFactor = strstr(boneData->boneName,"Knee") ? 0.25f : 0.15f;
    }
    if (compFactor > 0.0f && person) {
        BoneConnectionPositions bp = GetBoneConnectionPositionsEx(boneData, person);
        Vector3 camF  = SafeNormalize(Vector3Subtract(camera.target, camera.position));
        Vector3 camR  = SafeNormalize(Vector3CrossProduct(camF, camera.up));
        Vector3 camU  = SafeNormalize(Vector3CrossProduct(camR, camF));
        Vector3 bVec  = Vector3Subtract(bp.pos1, bp.pos0);
        float   bLen  = Vector3Length(bVec);
        if (bLen > 1e-6f) {
            bVec = Vector3Scale(bVec, 1.0f / bLen);
            Vector3 mid    = (Vector3){(bp.pos0.x+bp.pos1.x)*0.5f,(bp.pos0.y+bp.pos1.y)*0.5f,(bp.pos0.z+bp.pos1.z)*0.5f};
            Vector3 toCam  = Vector3Subtract(camera.position, mid);
            float   tCamL  = Vector3Length(toCam);
            if (tCamL > 1e-6f) {
                toCam = Vector3Scale(toCam, 1.0f / tCamL);
                float d = Vector3DotProduct(bVec, toCam);
                d = fmaxf(-1.0f, fminf(1.0f, d));
                float angleDeg = acosf(d) * RAD2DEG;
                if (angleDeg > 45.0f) {
                    Vector3 bInCam = {Vector3DotProduct(bVec,camR), Vector3DotProduct(bVec,camU), Vector3DotProduct(bVec,camF)};
                    float hLen = sqrtf(bInCam.x*bInCam.x + bInCam.z*bInCam.z);
                    float bPitchCam = atan2f(bInCam.y, hLen) * RAD2DEG;
                    float intensity = fminf((angleDeg - 45.0f) / 45.0f, 1.0f);
                    float comp = fmaxf(fminf(bPitchCam * compFactor * intensity, 12.0f), -12.0f);
                    pitchDeg += comp;
                }
            }
        }
    }

    float normYaw = localYaw * RAD2DEG + 22.5f;
    if (normYaw >= 360.0f) normYaw -= 360.0f;
    int   sector  = (int)(normYaw / 45.0f) % 8;
    float rotComp = 0.0f;

    if (pitchDeg >= 70.5f)  { *outChosenIndex = 3;  *outRotation = 0.0f; *outMirrored = false; return; }
    if (pitchDeg <= -60.0f) { *outChosenIndex = 15; *outRotation = (8-sector)*45.0f+180.0f; *outMirrored = true; return; }

    int row = (pitchDeg >= 22.5f) ? 2 : (pitchDeg >= -22.5f) ? 0 : 1;
    *outChosenIndex = indices[row][sector];
    *outRotation    = 0.0f;
    *outMirrored    = (sector >= 1 && sector <= 4);
    if (boneData->boneName[0] == 'R') *outMirrored = !(*outMirrored);

    if (strcmp(boneData->boneName, "Neck") == 0) {
        Vector3 estHead = Vector3Add(boneData->position, Vector3Scale(boneData->orientation.up, 0.15f));
        Vector3 dir     = SafeNormalize(Vector3Subtract(estHead, boneData->position));
        Vector3 camFwd  = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        Vector3 camR    = Vector3Normalize(Vector3CrossProduct(camFwd, camera.up));
        Vector3 camU    = Vector3Normalize(Vector3CrossProduct(camR, camFwd));
        Vector3 projDir = Vector3Subtract(dir, Vector3Scale(camFwd, Vector3DotProduct(dir, camFwd)));
        if (Vector3Length(projDir) > 0.001f) {
            projDir = SafeNormalize(projDir);
            *outRotation = fmodf(atan2f(Vector3DotProduct(projDir, camR), Vector3DotProduct(projDir, camU)) * RAD2DEG + 180.0f, 360.0f);
        }
    }

    if (person) {
        BoneConnectionPositions bp = GetBoneConnectionPositionsEx(boneData, person);
        Vector3 bVec = Vector3Subtract(bp.pos1, bp.pos0);
        float   bLen = Vector3Length(bVec);
        if (bLen > 1e-6f) {
            bVec = SafeNormalize(bVec);
            float dotV = Vector3DotProduct(bVec, (Vector3){0,1,0});
            float angleDeg = acosf(fabsf(dotV)) * RAD2DEG;
            if (angleDeg > 30.0f) {
                Vector3 camToPos = SafeNormalize(Vector3Subtract(boneData->position, camera.position));
                Vector3 projBone = Vector3Subtract(bVec, Vector3Scale(camToPos, Vector3DotProduct(bVec, camToPos)));
                float   projLen  = Vector3Length(projBone);
                if (projLen > 1e-4f) {
                    projBone = SafeNormalize(projBone);
                    Vector3 cR = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(camera.target, camera.position), camera.up));
                    Vector3 cU = Vector3Normalize(Vector3CrossProduct(cR, Vector3Subtract(camera.target, camera.position)));
                    rotComp = atan2f(Vector3DotProduct(projBone, cR), Vector3DotProduct(projBone, cU)) * RAD2DEG;
                    float diag = fminf((angleDeg - 30.0f) / 60.0f, 1.0f);
                    float factor = 0.0f;
                    if      (strstr(boneData->boneName,"LHip") || strstr(boneData->boneName,"RHip")) factor = -0.6f;
                    else if (strstr(boneData->boneName,"Knee"))     factor = 0.5f;
                    else if (strstr(boneData->boneName,"Shoulder")) factor = 0.3f;
                    else if (strstr(boneData->boneName,"Elbow"))    factor = 0.3f;
                    rotComp = fmaxf(fminf(rotComp * factor * diag, 45.0f), -45.0f);
                }
            }
        }
    }

    if ((sector == 0 || sector == 4) && pitchDeg < 60.0f && pitchDeg > -60.0f) {
        rotComp *= 0.01f;
        if (strstr(boneData->boneName,"Shoulder") || strstr(boneData->boneName,"Elbow") ||
            strstr(boneData->boneName,"Hip")       || strstr(boneData->boneName,"Knee"))
            rotComp = 0.0f;
    }
    if (((sector >= 7 || sector <= 1) || (sector >= 3 && sector <= 5)) && pitchDeg < 45.0f && pitchDeg > -45.0f)
        rotComp *= 0.3f;

    *outRotation += rotComp;
    while (*outRotation >= 360.0f) *outRotation -= 360.0f;
    while (*outRotation <    0.0f) *outRotation += 360.0f;
}

static const int asymTorsoIndices[3][8] = {
    {  1,  19,  20,  21,  13,  12,  11,  10  },
    {  0,   4,   9,  14,   8,   7,   6,   5  },
    {  2,  22,  23,  24,  18,  17,  16,  15  }
};

static inline void CalculateAsymmetricBoneRenderData(const BoneRenderData* boneData, Camera camera,
                                                     int* outChosenIndex, float* outRotation, bool* outMirrored) {

    Vector3 camDir = SafeNormalize(Vector3Subtract(boneData->position, camera.position));
    Vector3 local;
    if (boneData->orientation.valid) {
        local = (Vector3){
            Vector3DotProduct(camDir, boneData->orientation.right),
            Vector3DotProduct(camDir, boneData->orientation.up),
            Vector3DotProduct(camDir, boneData->orientation.forward)
        };
    } else {
        local = (Vector3){ camDir.x, camDir.y, camDir.z };
    }

    float localYaw = atan2f(local.x, local.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localPitch = -atan2f(local.y, sqrtf(local.x*local.x + local.z*local.z)) * RAD2DEG;
    float normYaw    = localYaw * RAD2DEG + 22.5f;
    if (normYaw >= 360.0f) normYaw -= 360.0f;
    int sector = (8 - (int)(normYaw / 45.0f) % 8) % 8;

    if (localPitch >= 52.5f) {
        *outChosenIndex = 3;
        *outRotation = sector * 45.0f + 180.0f;
        *outMirrored = false;
        return;
    }
    if (localPitch <= -51.0f) {
        *outChosenIndex = 2;
        *outRotation = 0.0f;
        *outMirrored = false;
        return;
    }

    int row = (localPitch >= 22.5f) ? 0 : (localPitch >= -22.5f) ? 1 : 2;
    *outChosenIndex = asymTorsoIndices[row][sector];
    *outRotation = 0.0f;
    *outMirrored = false;
}

static inline void CalculateTorsoRenderData(const TorsoRenderData* torsoData, Camera camera,
                                             int* outChosenIndex, float* outRotation, bool* outMirrored) {
    if (!torsoData->orientation.valid) {
        CalculateHandBoneRenderData(torsoData->position, camera, outChosenIndex, outRotation, outMirrored, "");
        return;
    }
    static const int indices[3][8] = {
        {0, 4, 5, 6, 7, 6, 5, 4},
        {2,12,13,14,15,14,13,12},
        {1, 8, 9,10,11,10, 9, 8}
    };

    Vector3 camDir = SafeNormalize(Vector3Subtract(torsoData->position, camera.position));
    Vector3 local  = {
        Vector3DotProduct(camDir, torsoData->orientation.right),
        Vector3DotProduct(camDir, torsoData->orientation.up),
        Vector3DotProduct(camDir, torsoData->orientation.forward)
    };
    float localYaw   = atan2f(local.x, local.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float pitchDeg   = -atan2f(local.y, sqrtf(local.x*local.x + local.z*local.z)) * RAD2DEG;
    float normYaw    = localYaw * RAD2DEG + 22.5f;
    if (normYaw >= 360.0f) normYaw -= 360.0f;
    int sector = (int)(normYaw / 45.0f) % 8;

    if (pitchDeg >= 70.0f)  { *outChosenIndex = 3;  *outRotation = sector*45.0f+180.0f; *outMirrored = false; }
    else if (pitchDeg <= -70.0f) { *outChosenIndex = 15; *outRotation = fmodf((8-sector)*45.0f+180.0f, 360.0f); *outMirrored = true; }
    else {
        int row = (pitchDeg >= 22.5f) ? 2 : (pitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[row][sector];
        *outRotation    = 0.0f;
        *outMirrored    = (sector >= 1 && sector <= 4);
    }

    if (torsoData->person && pitchDeg < 70.0f && pitchDeg > -70.0f) {
        Vector3 other = (torsoData->type == TORSO_CHEST)
            ? CalculateHipPosition(torsoData->person)
            : CalculateChestPosition(torsoData->person);
        if (Vector3Length(other) > 0.001f) {
            Vector3 torsoVec = Vector3Subtract(other, torsoData->position);
            float   tLen     = Vector3Length(torsoVec);
            if (tLen > 0.001f) {
                torsoVec = SafeNormalize(torsoVec);
                float wPitch = atan2f(torsoVec.y, sqrtf(torsoVec.x*torsoVec.x + torsoVec.z*torsoVec.z)) * RAD2DEG;
                bool viewRight = (local.x > 0.0f);
                if (torsoData->type == TORSO_CHEST)
                    *outRotation = viewRight ? -wPitch - 80.0f : wPitch + 80.0f;
                else
                    *outRotation = viewRight ? wPitch - 80.0f : -wPitch + 80.0f;
                while (*outRotation >= 360.0f) *outRotation -= 360.0f;
                while (*outRotation <    0.0f) *outRotation += 360.0f;
            }
        }
    }
}

static inline void CalculateHeadRenderData(const HeadRenderData* headData, Camera camera,
                                            int* outChosenIndex, float* outRotation, bool* outMirrored) {
    if (!headData->orientation.valid) {
        CalculateHandBoneRenderData(headData->position, camera, outChosenIndex, outRotation, outMirrored, "Head");
        return;
    }
    static const int indices[3][8] = {
        { 0, 4, 5, 6, 7, 6, 5, 4},
        { 2,12,13,14,15,14,13,12},
        { 1, 8, 9,10,11,10, 9, 8}
    };

    Vector3 camDir = Vector3Subtract(camera.position, headData->position);
    Vector3 local  = {
        -Vector3DotProduct(camDir, headData->orientation.right),
         Vector3DotProduct(camDir, headData->orientation.up),
         Vector3DotProduct(camDir, headData->orientation.forward)
    };

    float localYaw   = atan2f(local.x, local.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;
    float localYawDeg = localYaw * RAD2DEG;
    float horizDist   = sqrtf(local.x*local.x + local.z*local.z);
    float pitchDeg    = atan2f(local.y, horizDist) * RAD2DEG;

    int sector;
    if (pitchDeg >= 65.0f || pitchDeg <= -65.0f) {
        Vector3 hDiff = {camera.position.x - headData->position.x, 0.0f, camera.position.z - headData->position.z};
        float   hDist = Vector3Length(hDiff);
        if (hDist > 0.05f) {
            float wYaw = atan2f(hDiff.x, hDiff.z);
            if (wYaw < 0.0f) wYaw += 2.0f * PI;
            float normWYaw = wYaw * RAD2DEG + 22.5f;
            if (normWYaw >= 360.0f) normWYaw -= 360.0f;
            sector = (int)(normWYaw / 45.0f) % 8;
        } else sector = 0;
    } else {
        float normYaw = localYawDeg + 22.5f;
        if (normYaw >= 360.0f) normYaw -= 360.0f;
        sector = (int)(normYaw / 45.0f);
    }

    if (pitchDeg >= 50.0f)   { *outChosenIndex = 3;  *outRotation = sector*45.0f+180.0f; *outMirrored = false; }
    else if (pitchDeg <= -70.0f) {
        *outChosenIndex = 15;
        *outRotation = fmodf((8-sector)*45.0f+180.0f, 360.0f);
        *outMirrored = true;
    } else {
        int row = (pitchDeg >= 22.5f) ? 2 : (pitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[row][sector];
        *outRotation    = 0.0f;
        *outMirrored    = !(sector >= 5 && sector <= 7);
    }
}

static inline void DrawHeadBillboard(Texture2D texture, Camera camera, const HeadRenderData* headData, int physCols, int physRows) {
    if (!headData || !headData->valid || !headData->visible) return;
    int ci; float rot; bool mir;
    CalculateHeadRenderData(headData, camera, &ci, &rot, &mir);
    bool fm;
    Rectangle src = SrcFromLogical(texture, ci % ATLAS_COLS, ci / ATLAS_COLS, physCols, physRows, mir, &fm);
    DrawBonetileCustom(texture, camera, src, headData->position, (Vector2){headData->size, headData->size}, rot, fm, "Head");
}

static inline void DrawTorsoBillboard(Texture2D texture, Camera camera, const TorsoRenderData* torsoData, int physCols, int physRows) {
    if (!torsoData || !torsoData->valid || !torsoData->visible) return;
    int ci; float rot; bool mir;

    int cols = torsoData->isAsymmetric ? ASYM_ATLAS_COLS : physCols;
    int rows = torsoData->isAsymmetric ? ASYM_ATLAS_ROWS : physRows;

    if (torsoData->isAsymmetric) {
        BoneRenderData tempBone = {0};
        tempBone.position = torsoData->position;
        tempBone.orientation.position = torsoData->orientation.position;
        tempBone.orientation.forward  = torsoData->orientation.forward;
        tempBone.orientation.up       = torsoData->orientation.up;
        tempBone.orientation.right    = torsoData->orientation.right;
        tempBone.orientation.yaw      = torsoData->orientation.yaw;
        tempBone.orientation.pitch    = torsoData->orientation.pitch;
        tempBone.orientation.roll     = torsoData->orientation.roll;
        tempBone.orientation.valid    = torsoData->orientation.valid;
        strncpy(tempBone.boneName, "AsymTorso", MAX_BONE_NAME_LENGTH - 1);
        tempBone.isAsymmetric = true;

        CalculateAsymmetricBoneRenderData(&tempBone, camera, &ci, &rot, &mir);
        mir = false;
    } else {
        CalculateTorsoRenderData(torsoData, camera, &ci, &rot, &mir);
    }

    bool fm;
    Rectangle src = SrcFromLogical(texture, ci % cols, ci / cols, cols, rows, mir, &fm);
    DrawBonetileCustom(texture, camera, src, torsoData->position, (Vector2){torsoData->size, torsoData->size}, rot, fm, "");
}

static inline bool DetectZFighting(RenderItem* items, int itemCount) {
    bool has = false;
    for (int i = 0; i < itemCount; i++) {
        items[i].hasZFighting = false;
        for (int j = i+1; j < itemCount; j++)
            if (fabsf(items[i].distance - items[j].distance) < Z_FIGHTING_THRESHOLD)
                { items[i].hasZFighting = items[j].hasZFighting = has = true; }
    }
    return has;
}

static inline void SortRenderItems(RenderItem* items, int itemCount) {
    for (int i = 0; i < itemCount - 1; i++) {
        bool swapped = false;
        for (int j = 0; j < itemCount - i - 1; j++) {
            if (items[j].distance + items[j].depthBias < items[j+1].distance + items[j+1].depthBias) {
                RenderItem t = items[j]; items[j] = items[j+1]; items[j+1] = t;
                swapped = true;
            }
        }
        if (!swapped) break;
    }
}

static void RenderBoneInternal(BonesRenderer* renderer, const BoneRenderData* bone,
                                Vector3 renderPosition, const AnimationFrame* frame,
                                BoneRenderData* allBones, int boneCount)
{
    int texIndex = BonesRenderer_LoadTexture(renderer, bone->texturePath);
    Texture2D tex = renderer->textures[texIndex];
    bool isWrist  = IsWristBone(bone->boneName);
    bool isAsym   = bone->isAsymmetric;
    bool useHandAtlas = isWrist || isAsym;
    int  cols = useHandAtlas ? ASYM_ATLAS_COLS : renderer->physCols;
    int  rows = useHandAtlas ? ASYM_ATLAS_ROWS : renderer->physRows;
    float cellW = (float)tex.width  / (float)cols;
    float cellH = (float)tex.height / (float)rows;
    Vector2 worldSize = { bone->size * (cellW / cellH), bone->size };

    int ci = 0; float rot = 0.0f; bool mir = false;
    const Person* bonePerson = frame ? FindPersonByBoneName(frame, bone->boneName) : NULL;

    if (useHandAtlas) {
        if (isWrist) {
            if (bone->orientation.valid)
                CalculateHandBoneRenderDataWithOrientation(bone, renderer->camera, &ci, &rot, &mir);
            else
                CalculateHandBoneRenderData(bone->position, renderer->camera, &ci, &rot, &mir, bone->boneName);
        } else {
            CalculateAsymmetricBoneRenderData(bone, renderer->camera, &ci, &rot, &mir);
        }
    } else if (bone->orientation.valid) {
        CalculateLimbBoneRenderData(bone, bonePerson, renderer->camera, &ci, &rot, &mir);
    }

    int maxIdx = cols * rows - 1;
    if (ci < 0) ci = 0;
    if (ci > maxIdx) ci %= (maxIdx + 1);

    bool fm;
    Rectangle src = SrcFromLogical(tex, ci % cols, ci / cols, cols, rows, mir, &fm);

    char   conns[3][32]; float prios[3];
    Vector3 neighborPos = {0}; bool haveNeighbor = false;
    if (GetBoneConnectionsWithPriority(bone->boneName, conns, prios)) {
        for (int k = 0; k < 3 && !haveNeighbor; k++) {
            if (!conns[k][0]) continue;
            BoneRenderData* nb = FindRenderBoneByName(allBones, boneCount, conns[k]);
            if (nb && nb->valid && nb->visible && Vector3Distance(bone->position, nb->position) > MIN_DISTANCE_THRESHOLD) {
                neighborPos  = nb->position;
                haveNeighbor = true;
            }
        }
    }
    DrawBonetileCustomWithRoll(tex, renderer->camera, src, renderPosition, worldSize, rot, fm,
                               haveNeighbor, neighborPos, bone, bonePerson);
}

static inline void BonesRenderer_RenderFrame(BonesRenderer* renderer,
        BoneRenderData* bones, int boneCount,
        HeadRenderData* heads, int headCount,
        TorsoRenderData* torsos, int torsoCount,
        Vector3 autoCenter, bool autoCenterCalculated)
{
    if (!renderer) return;
    BeginMode3D(renderer->camera);
    (void)autoCenter; (void)autoCenterCalculated;

    int totalItems = boneCount + headCount + torsoCount;
    if (totalItems > 0) {
        static RenderItem renderItems[MAX_RENDER_ITEMS];
        int itemCount = 0;
        Vector3 camPos = renderer->camera.position;

        for (int i = 0; i < torsoCount && itemCount < MAX_RENDER_ITEMS; i++) {
            if (!torsos[i].valid || !torsos[i].visible) continue;
            renderItems[itemCount++] = (RenderItem){0, i, Vector3Distance(camPos, torsos[i].position), TORSO_BIAS + INDEX_BIAS*i, false};
        }
        for (int i = 0; i < boneCount && itemCount < MAX_RENDER_ITEMS; i++) {
            if (!bones[i].valid || !bones[i].visible) continue;
            renderItems[itemCount++] = (RenderItem){1, i, Vector3Distance(camPos, bones[i].position), BONE_BIAS + INDEX_BIAS*i, false};
        }
        for (int i = 0; i < headCount && itemCount < MAX_RENDER_ITEMS; i++) {
            if (!heads[i].valid || !heads[i].visible) continue;
            renderItems[itemCount++] = (RenderItem){2, i, Vector3Distance(camPos, heads[i].position), HEAD_BIAS + INDEX_BIAS*i, false};
        }

        DetectZFighting(renderItems, itemCount);
        SortRenderItems(renderItems, itemCount);

        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        rlEnableDepthTest();
        rlDisableDepthMask();

        for (int i = 0; i < itemCount; i++) {
            RenderItem* item = &renderItems[i];

            if (item->type == 2) {
                const HeadRenderData* curHead = &heads[item->index];
                for (int j = 0; j < boneCount; j++) {
                    const BoneRenderData* bone = &bones[j];
                    if (!bone->valid || !bone->visible || strcmp(bone->boneName, "Neck") != 0) continue;
                    if (Vector3Distance(bone->position, curHead->position) < 2.0f) {
                        Vector3 toCam = Vector3Subtract(camPos, bone->position);
                        float   dist  = Vector3Length(toCam);
                        Vector3 renderPos = Vector3Add(bone->position,
                            dist > MIN_DISTANCE_THRESHOLD ? Vector3Scale(Vector3Normalize(toCam), BONE_BIAS + INDEX_BIAS*j) : (Vector3){0});
                        RenderBoneInternal(renderer, bone, renderPos, NULL, bones, boneCount);
                    }
                }
            }

            Vector3 itemPos;
            switch (item->type) {
                case 0: itemPos = torsos[item->index].position; break;
                case 1: itemPos = bones[item->index].position;  break;
                case 2: itemPos = heads[item->index].position;  break;
                default: continue;
            }

            Vector3 toCam = Vector3Subtract(camPos, itemPos);
            float   dist  = Vector3Length(toCam);
            Vector3 renderOffset = dist > MIN_DISTANCE_THRESHOLD
                ? Vector3Scale(Vector3Normalize(toCam), item->depthBias) : (Vector3){0};

            switch (item->type) {
                case 0: {
                    TorsoRenderData adj = torsos[item->index];
                    adj.position = Vector3Add(adj.position, renderOffset);
                    int ti = BonesRenderer_LoadTexture(renderer, adj.texturePath);
                    DrawTorsoBillboard(renderer->textures[ti], renderer->camera, &adj, renderer->physCols, renderer->physRows);
                    break;
                }
                case 1: {
                    const BoneRenderData* b = &bones[item->index];
                    if (strcmp(b->boneName, "Neck") != 0)
                        RenderBoneInternal(renderer, b, Vector3Add(b->position, renderOffset), NULL, bones, boneCount);
                    break;
                }
                case 2: {
                    HeadRenderData adj = heads[item->index];
                    adj.position = Vector3Add(adj.position, renderOffset);
                    int ti = BonesRenderer_LoadTexture(renderer, adj.texturePath);
                    DrawHeadBillboard(renderer->textures[ti], renderer->camera, &adj, renderer->physCols, renderer->physRows);
                    break;
                }
            }
        }

        EndBlendMode();
        rlEnableDepthMask();
    }
    EndMode3D();
}

static inline OrnamentSystem* Ornaments_Create(void) {
    return (OrnamentSystem*)calloc(1, sizeof(OrnamentSystem));
}

static inline void Ornaments_Free(OrnamentSystem* system) { free(system); }

static inline void Ornaments_ApplyPreset(BoneOrnament* ornament, OrnamentPhysicsPreset preset) {
    if (!ornament) return;
    ornament->physicsPreset = preset;
    switch (preset) {
        case PHYSICS_STIFF:    ornament->stiffness = 100.0f; ornament->damping = 0.95f; ornament->gravityScale = 0.05f; break;
        case PHYSICS_MEDIUM:   ornament->stiffness =  80.0f; ornament->damping = 0.88f; ornament->gravityScale = 0.18f; break;
        case PHYSICS_SOFT:     ornament->stiffness =  60.0f; ornament->damping = 0.82f; ornament->gravityScale = 0.28f; break;
        case PHYSICS_VERYSOFT: ornament->stiffness =  50.0f; ornament->damping = 0.75f; ornament->gravityScale = 0.35f; break;
    }
}

static inline bool Ornaments_LoadFromConfig(OrnamentSystem* system, const char* configPath) {
    if (!system || !configPath) return false;
    char* buffer = LoadFileText(configPath);
    if (!buffer) return false;

    system->ornamentCount = 0;
    char lineBuffer[512];
    const char* lineStart = buffer;

    for (const char* ptr = buffer; *ptr && system->ornamentCount < MAX_ORNAMENTS; ptr++) {
        if (*ptr != '\n') continue;
        int lineLen = (int)(ptr - lineStart);
        if (lineLen > 5 && lineLen < 511 && *lineStart == '@') {
            memcpy(lineBuffer, lineStart, lineLen);
            lineBuffer[lineLen] = '\0';

            BoneOrnament* orn = &system->ornaments[system->ornamentCount];
            memset(orn, 0, sizeof(BoneOrnament));

            char presetStr[64], parentStr[MAX_BONE_NAME_LENGTH];
            int visible; float ox, oy, oz;

            if (sscanf(lineBuffer, "@%31s %31s %f,%f,%f %255s %d %f %63s %31s",
                       orn->name, orn->anchorBoneName, &ox, &oy, &oz,
                       orn->texturePath, &visible, &orn->size, parentStr, presetStr) == 10)
            {
                orn->offsetFromAnchor = (Vector3){ox, oy, oz};
                orn->visible          = (visible != 0);

                if (strcmp(parentStr, "-") == 0) {
                    orn->isChained       = false;
                    orn->chainParentIndex = -1;
                } else {
                    orn->isChained = true;
                    strncpy(orn->chainParentName, parentStr, MAX_BONE_NAME_LENGTH - 1);
                    orn->chainRestLength = Vector3Length(orn->offsetFromAnchor);
                }

                if      (strcmp(presetStr, "stiff")    == 0) Ornaments_ApplyPreset(orn, PHYSICS_STIFF);
                else if (strcmp(presetStr, "medium")   == 0) Ornaments_ApplyPreset(orn, PHYSICS_MEDIUM);
                else if (strcmp(presetStr, "soft")     == 0) Ornaments_ApplyPreset(orn, PHYSICS_SOFT);
                else if (strcmp(presetStr, "verysoft") == 0) Ornaments_ApplyPreset(orn, PHYSICS_VERYSOFT);

                orn->valid = true;
                system->ornamentCount++;
            }
        }
        lineStart = ptr + 1;
    }
    UnloadFileText(buffer);

    for (int i = 0; i < system->ornamentCount; i++) {
        BoneOrnament* orn = &system->ornaments[i];
        if (!orn->isChained) continue;
        for (int j = 0; j < system->ornamentCount; j++) {
            if (strcmp(system->ornaments[j].name, orn->chainParentName) == 0) {
                orn->chainParentIndex = j; break;
            }
        }
    }

    system->loaded = (system->ornamentCount > 0);
    if (system->loaded) TraceLog(LOG_INFO, "Loaded %d ornaments", system->ornamentCount);
    return system->loaded;
}

static inline Vector3 Ornaments_GetAnchorPosition(const AnimationFrame* frame, const char* anchorName) {
    if (!frame || !anchorName) return (Vector3){0,0,0};
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (strcmp(bone->name, anchorName) == 0 && bone->position.valid)
                return bone->position.position;
        }
        if      (strcmp(anchorName, "Head")  == 0) return CalculateHeadPosition(person);
        else if (strcmp(anchorName, "Chest") == 0) return CalculateChestPosition(person);
        else if (strcmp(anchorName, "Hip")   == 0) return CalculateHipPosition(person);
        Vector3 mid = CalculateBoneMidpoint(anchorName, person);
        if (Vector3Length(mid) > 0.001f) return mid;
    }
    return (Vector3){0,0,0};
}

static inline BoneOrientation Ornaments_GetAnchorOrientation(const AnimationFrame* frame, const char* anchorName) {
    BoneOrientation null = {0};
    if (!frame || !anchorName) return null;
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (strcmp(bone->name, anchorName) == 0 && bone->position.valid)
                return CalculateBoneOrientation(anchorName, person, bone->position.position);
        }
        if (strcmp(anchorName, "Head") == 0) {
            HeadOrientation h = CalculateHeadOrientation(person);
            return (BoneOrientation){h.position, h.forward, h.up, h.right, h.yaw, h.pitch, h.roll, h.valid};
        }
        if (strcmp(anchorName, "Chest") == 0) {
            TorsoOrientation t = CalculateChestOrientation(person);
            return (BoneOrientation){t.position, t.forward, t.up, t.right, t.yaw, t.pitch, t.roll, t.valid};
        }
        if (strcmp(anchorName, "Hip") == 0) {
            TorsoOrientation t = CalculateHipOrientation(person);
            return (BoneOrientation){t.position, t.forward, t.up, t.right, t.yaw, t.pitch, t.roll, t.valid};
        }
        Vector3 pos = CalculateBoneMidpoint(anchorName, person);
        if (Vector3Length(pos) > 0.001f) return CalculateBoneOrientation(anchorName, person, pos);
    }
    return null;
}

static inline void Ornaments_InitializePhysics(OrnamentSystem* system, const AnimationFrame* frame) {
    if (!system || !frame) return;
    for (int i = 0; i < system->ornamentCount; i++) {
        BoneOrnament* orn = &system->ornaments[i];
        if (!orn->valid || !orn->visible) continue;

        Vector3       anchorPos;
        BoneOrientation anchorOrient;
        if (orn->isChained && orn->chainParentIndex >= 0) {
            anchorPos    = system->ornaments[orn->chainParentIndex].currentPosition;
            anchorOrient = (BoneOrientation){.forward={0,0,1},.up={0,1,0},.right={1,0,0},.valid=true};
        } else {
            anchorPos    = Ornaments_GetAnchorPosition(frame, orn->anchorBoneName);
            anchorOrient = Ornaments_GetAnchorOrientation(frame, orn->anchorBoneName);
        }

        Vector3 worldOffset = {0,0,0};
        if (anchorOrient.valid) {
            Vector3 lo = orn->offsetFromAnchor;
            worldOffset.x = lo.x*anchorOrient.right.x + lo.y*anchorOrient.up.x + lo.z*anchorOrient.forward.x;
            worldOffset.y = lo.x*anchorOrient.right.y + lo.y*anchorOrient.up.y + lo.z*anchorOrient.forward.y;
            worldOffset.z = lo.x*anchorOrient.right.z + lo.y*anchorOrient.up.z + lo.z*anchorOrient.forward.z;
        } else {
            worldOffset = orn->offsetFromAnchor;
        }

        orn->currentPosition  = Vector3Add(anchorPos, worldOffset);
        orn->previousPosition = orn->currentPosition;
        orn->velocity         = (Vector3){0,0,0};
        orn->initialized      = true;
    }
}

static inline void Ornaments_SolveChainConstraint(BoneOrnament* child, BoneOrnament* parent) {
    if (!child || !parent || !child->isChained) return;
    Vector3 toParent = Vector3Subtract(parent->currentPosition, child->currentPosition);
    float   dist     = Vector3Length(toParent);
    if (dist < 0.0001f) return;
    Vector3 correction = Vector3Scale(Vector3Normalize(toParent), (dist - child->chainRestLength) * 0.5f);
    child->currentPosition = Vector3Add(child->currentPosition, correction);
}

static inline void ClampOrnamentToAnchorY(BoneOrnament* orn, Vector3 anchorPos, float maxDownOffset) {
    float minY = anchorPos.y + orn->offsetFromAnchor.y - maxDownOffset;
    if (orn->currentPosition.y < minY) { orn->currentPosition.y = minY; orn->velocity.y = 0.0f; }
}

static inline void Ornaments_UpdatePhysics(OrnamentSystem* system, const AnimationFrame* frame, float deltaTime) {
    if (!system || !frame || deltaTime <= 0.0f || !system->loaded) return;
    if (deltaTime > 0.1f) deltaTime = 0.1f;

    for (int i = 0; i < system->ornamentCount; i++) {
        BoneOrnament* orn = &system->ornaments[i];
        if (!orn->valid || !orn->visible || orn->isChained) continue;

        Vector3       anchorPos    = Ornaments_GetAnchorPosition(frame, orn->anchorBoneName);
        BoneOrientation anchorOrient = Ornaments_GetAnchorOrientation(frame, orn->anchorBoneName);
        Vector3 worldOffset = {0,0,0};
        if (anchorOrient.valid) {
            Vector3 lo = orn->offsetFromAnchor;
            worldOffset.x = lo.x*anchorOrient.right.x + lo.y*anchorOrient.up.x + lo.z*anchorOrient.forward.x;
            worldOffset.y = lo.x*anchorOrient.right.y + lo.y*anchorOrient.up.y + lo.z*anchorOrient.forward.y;
            worldOffset.z = lo.x*anchorOrient.right.z + lo.y*anchorOrient.up.z + lo.z*anchorOrient.forward.z;
        } else worldOffset = orn->offsetFromAnchor;

        Vector3 targetPos   = Vector3Add(anchorPos, worldOffset);
        orn->velocity = Vector3Add(orn->velocity, Vector3Scale(Vector3Subtract(targetPos, orn->currentPosition), orn->stiffness * deltaTime));
        orn->velocity = Vector3Add(orn->velocity, Vector3Scale((Vector3){0,-9.8f*orn->gravityScale,0}, deltaTime));
        orn->velocity = Vector3Scale(orn->velocity, orn->damping);
        orn->previousPosition = orn->currentPosition;
        orn->currentPosition  = Vector3Add(orn->currentPosition, Vector3Scale(orn->velocity, deltaTime));
        ClampOrnamentToAnchorY(orn, anchorPos, 0.05f);
    }

    for (int i = 0; i < system->ornamentCount; i++) {
        BoneOrnament* orn = &system->ornaments[i];
        if (!orn->valid || !orn->visible || !orn->isChained) continue;
        BoneOrnament* parent    = &system->ornaments[orn->chainParentIndex];
        Vector3       anchorPos = Ornaments_GetAnchorPosition(frame, orn->anchorBoneName);
        Vector3       targetPos = Vector3Add(parent->currentPosition, orn->offsetFromAnchor);

        orn->velocity = Vector3Add(orn->velocity, Vector3Scale(Vector3Subtract(targetPos, orn->currentPosition), orn->stiffness * deltaTime));
        orn->velocity = Vector3Add(orn->velocity, Vector3Scale((Vector3){0,-9.8f*orn->gravityScale,0}, deltaTime));
        orn->velocity = Vector3Scale(orn->velocity, orn->damping);
        orn->previousPosition = orn->currentPosition;
        orn->currentPosition  = Vector3Add(orn->currentPosition, Vector3Scale(orn->velocity, deltaTime));
        Ornaments_SolveChainConstraint(orn, parent);
        ClampOrnamentToAnchorY(orn, anchorPos, 0.10f);
    }
}

static inline void Ornaments_CollectForRendering(OrnamentSystem* system,
        BoneRenderData** renderBones, int* renderBonesCount, int* renderBonesCapacity, Camera camera)
{
    if (!system || !system->loaded || !renderBones || !renderBonesCount || !renderBonesCapacity) return;

    for (int i = 0; i < system->ornamentCount; i++) {
        BoneOrnament* orn = &system->ornaments[i];
        if (!orn->valid || !orn->visible || !orn->initialized || !orn->name[0]) continue;

        float distance = Vector3Distance(camera.position, orn->currentPosition);
        if (distance > 50.0f) continue;
        if (*renderBonesCount >= *renderBonesCapacity) {
            if (!ResizeRenderBonesArray(renderBones, renderBonesCapacity, *renderBonesCapacity + 32)) return;
        }

        BoneRenderData* rd = &(*renderBones)[*renderBonesCount];
        memset(rd, 0, sizeof(BoneRenderData));
        if (!orn->texturePath[0]) continue;

        strncpy(rd->boneName,    orn->name,        MAX_BONE_NAME_LENGTH - 1); rd->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
        strncpy(rd->texturePath, orn->texturePath, MAX_FILE_PATH_LENGTH  - 1); rd->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

        rd->position = orn->currentPosition;
        rd->distance = distance;
        rd->size     = orn->size;
        rd->visible  = true;
        rd->valid    = true;

        rd->orientation.position = rd->position;
        rd->orientation.forward  = (Vector3){0,0,-1};
        rd->orientation.up       = (Vector3){0,1,0};
        rd->orientation.right    = (Vector3){-1,0,0};

        Vector3 toCamera = Vector3Subtract(camera.position, rd->position);
        rd->orientation.yaw   = (Vector3Length(toCamera) > 0.001f) ? atan2f(toCamera.x, toCamera.z) : 0.0f;
        rd->orientation.pitch = 0.0f;
        rd->orientation.roll  = 0.0f;
        rd->orientation.valid = true;
        (*renderBonesCount)++;
    }
}

static inline Vector3 SlashTrail__GetBonePos(const AnimationFrame* frame, const char* personId, const char* boneName) {
    if (!frame || !boneName) return (Vector3){0,0,0};
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (personId && personId[0] && strcmp(person->personId, personId) != 0) continue;
        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (strcmp(bone->name, boneName) == 0 && bone->position.valid)
                return bone->position.position;
        }
        if (strcmp(boneName,"RightHand") == 0 || strcmp(boneName,"RWrist") == 0) return GetBonePositionByName(person, "RWrist");
        if (strcmp(boneName,"LeftHand")  == 0 || strcmp(boneName,"LWrist") == 0) return GetBonePositionByName(person, "LWrist");
        if (strcmp(boneName,"Head")      == 0) return CalculateHeadPosition(person);
    }
    return (Vector3){0,0,0};
}

static inline Color SlashTrail__LerpColor(Color a, Color b, float t) {
    t = fmaxf(0.0f, fminf(1.0f, t));
    return (Color){
        (unsigned char)(a.r + (int)((b.r - a.r) * t)),
        (unsigned char)(a.g + (int)((b.g - a.g) * t)),
        (unsigned char)(a.b + (int)((b.b - a.b) * t)),
        (unsigned char)(a.a + (int)((b.a - a.a) * t))
    };
}

static inline float SlashTrail__LerpF(float a, float b, float t) {
    return a + (b - a) * fmaxf(0.0f, fminf(1.0f, t));
}

static inline Vector3 SlashTrail__GetEmitPos(const AnimationFrame* frame, const char* personId, const SlashEventConfig* ev) {
    if (!frame || !ev) return (Vector3){0,0,0};
    Vector3 bonePos = SlashTrail__GetBonePos(frame, personId, ev->boneName);
    if (fabsf(ev->emitOffsetX) < 0.0001f && fabsf(ev->emitOffsetY) < 0.0001f && fabsf(ev->emitOffsetZ) < 0.0001f)
        return bonePos;

    const Person* person = NULL;
    for (int p = 0; p < frame->personCount; p++) {
        if (!frame->persons[p].active) continue;
        if (personId && personId[0]) { if (strcmp(frame->persons[p].personId, personId) == 0) { person = &frame->persons[p]; break; } }
        else { person = &frame->persons[p]; break; }
    }
    if (!person) return bonePos;

    Vector3 fwd = {0,0,1}, up = {0,1,0}, right = {1,0,0};
    const char* bn = ev->boneName;
    const char* prox = strcmp(bn,"RWrist")==0 ? "RElbow" : strcmp(bn,"LWrist")==0 ? "LElbow" :
                       strcmp(bn,"RElbow")==0 ? "RShoulder" : strcmp(bn,"LElbow")==0 ? "LShoulder" :
                       strcmp(bn,"RAnkle")==0 ? "RKnee"  : strcmp(bn,"LAnkle")==0 ? "LKnee"  :
                       strcmp(bn,"RKnee") ==0 ? "RHip"   : strcmp(bn,"LKnee") ==0 ? "LHip"   : NULL;
    Vector3 proxPos = prox ? GetBonePositionByName(person, prox) : (Vector3){bonePos.x, bonePos.y - 0.1f, bonePos.z};
    Vector3 dir = Vector3Subtract(bonePos, proxPos);
    float   len = Vector3Length(dir);
    if (len > 1e-4f) {
        fwd = Vector3Scale(dir, 1.0f / len);
        Vector3 wu = {0,1,0};
        if (fabsf(Vector3DotProduct(fwd, wu)) > 0.95f) wu = (Vector3){0,0,1};
        right = SafeNormalize(Vector3CrossProduct(fwd, wu));
        up    = SafeNormalize(Vector3CrossProduct(right, fwd));
    }

    return (Vector3){
        bonePos.x + right.x*ev->emitOffsetX + up.x*ev->emitOffsetY + fwd.x*ev->emitOffsetZ,
        bonePos.y + right.y*ev->emitOffsetX + up.y*ev->emitOffsetY + fwd.y*ev->emitOffsetZ,
        bonePos.z + right.z*ev->emitOffsetX + up.z*ev->emitOffsetY + fwd.z*ev->emitOffsetZ,
    };
}

static inline void SlashTrail_InitSystem(SlashTrailSystem* sys) {
    if (sys) memset(sys, 0, sizeof(SlashTrailSystem));
}

static inline void SlashTrail_FreeSystem(SlashTrailSystem* sys) {
    if (!sys) return;
    for (int i = 0; i < SLASH_TRAIL_MAX_ACTIVE; i++) {
        SlashTrail* t = &sys->trails[i];
        if (t->hasTexture) { UnloadTexture(t->texture); t->hasTexture = false; }
    }
    memset(sys, 0, sizeof(SlashTrailSystem));
}

static inline void SlashTrail_Activate(SlashTrailSystem* sys, int slashIndex, const SlashEventConfig* config) {
    if (!sys || !config) return;
    for (int i = 0; i < SLASH_TRAIL_MAX_ACTIVE; i++)
        if (sys->trails[i].slashIndex == slashIndex && sys->trails[i].alive) return;

    SlashTrail* slot = NULL;
    for (int i = 0; i < SLASH_TRAIL_MAX_ACTIVE; i++)
        if (!sys->trails[i].alive) { slot = &sys->trails[i]; break; }
    if (!slot) return;

    memset(slot, 0, sizeof(SlashTrail));
    slot->active     = true;
    slot->alive      = true;
    slot->slashIndex = slashIndex;
    slot->config     = *config;

    int maxSeg = (config->segments > 0 ? config->segments : 24);
    if (maxSeg > SLASH_TRAIL_MAX_SEGMENTS) maxSeg = SLASH_TRAIL_MAX_SEGMENTS;
    slot->config.segments = maxSeg;
    slot->spawnRate       = config->lifetime / (float)maxSeg;
    if (slot->spawnRate < 0.006f) slot->spawnRate = 0.006f;

    if (config->texturePath[0]) {
        slot->texture    = LoadTexture(config->texturePath);
        slot->hasTexture = (slot->texture.id != 0);
    }
}

static inline void SlashTrail_Deactivate(SlashTrailSystem* sys, int slashIndex) {
    if (!sys) return;
    for (int i = 0; i < SLASH_TRAIL_MAX_ACTIVE; i++)
        if (sys->trails[i].slashIndex == slashIndex) sys->trails[i].active = false;
}

static inline void SlashTrail_UpdateTrail(SlashTrailSystem* sys, float deltaTime, int slashIndex, Vector3 bonePosition) {
    if (!sys) return;
    for (int i = 0; i < SLASH_TRAIL_MAX_ACTIVE; i++) {
        SlashTrail* trail = &sys->trails[i];
        if (!trail->alive || trail->slashIndex != slashIndex) continue;

        for (int p = 0; p < trail->config.segments; p++) {
            SlashTrailPoint* pt = &trail->points[p];
            if (!pt->active) continue;
            pt->age += deltaTime;
            if (pt->age >= pt->lifetime) pt->active = false;
        }

        if (trail->active) {
            trail->spawnTimer += deltaTime;
            if (trail->spawnTimer >= trail->spawnRate) {
                trail->spawnTimer = 0.0f;
                int slot = -1;
                for (int p = 0; p < trail->config.segments; p++) {
                    if (!trail->points[p].active) { slot = p; break; }
                }
                if (slot >= 0) {
                    trail->points[slot].position = bonePosition;
                    trail->points[slot].age      = 0.0f;
                    trail->points[slot].lifetime = trail->config.lifetime;
                    trail->points[slot].active   = true;
                    if (trail->pointCount < trail->config.segments) trail->pointCount++;
                }
            }
        }

        bool anyAlive = false;
        for (int p = 0; p < trail->config.segments; p++) if (trail->points[p].active) { anyAlive = true; break; }
        if (!anyAlive && !trail->active) {
            if (trail->hasTexture) { UnloadTexture(trail->texture); trail->hasTexture = false; }
            memset(trail, 0, sizeof(SlashTrail));
        }
        trail->alive = anyAlive;
    }
}

static inline void SlashTrail_Draw(SlashTrailSystem* sys, Camera camera) {
    if (!sys) return;
    (void)camera;

    rlDisableBackfaceCulling();
    rlDisableDepthTest();
    BeginBlendMode(BLEND_ADDITIVE);

    for (int i = 0; i < SLASH_TRAIL_MAX_ACTIVE; i++) {
        SlashTrail* trail = &sys->trails[i];
        if (!trail->alive) continue;

        static SlashTrailPoint* livePoints[SLASH_TRAIL_MAX_SEGMENTS];
        int lc = 0;
        for (int p = 0; p < trail->config.segments; p++)
            if (trail->points[p].active) livePoints[lc++] = &trail->points[p];
        if (lc < 2) continue;

        Vector3 axis = {0, 1, 0};

        for (int p = 0; p < lc - 1; p++) {
            SlashTrailPoint* curr = livePoints[p];
            SlashTrailPoint* next = livePoints[p + 1];

            float tC = curr->age / curr->lifetime;
            float tN = next->age / next->lifetime;
            Color colA = SlashTrail__LerpColor(trail->config.colorStart, trail->config.colorEnd, tC);
            Color colB = SlashTrail__LerpColor(trail->config.colorStart, trail->config.colorEnd, tN);
            Color colC = colB;

            float wCurr = SlashTrail__LerpF(trail->config.widthStart, trail->config.widthEnd, tC);
            float wNext = SlashTrail__LerpF(trail->config.widthStart, trail->config.widthEnd, tN);

            Vector3 segDir = Vector3Subtract(next->position, curr->position);
            if (Vector3Length(segDir) < 1e-6f) continue;
            segDir = SafeNormalize(segDir);
            Vector3 perp = Vector3Normalize(Vector3CrossProduct(segDir, axis));

            Vector3 c0 = Vector3Add(curr->position, Vector3Scale(perp,  wCurr * 0.5f));
            Vector3 c1 = Vector3Add(curr->position, Vector3Scale(perp, -wCurr * 0.5f));
            Vector3 n0 = Vector3Add(next->position, Vector3Scale(perp,  wNext * 0.5f));
            Vector3 n1 = Vector3Add(next->position, Vector3Scale(perp, -wNext * 0.5f));

            DrawTriangle3D(c0, n0, c1, colA);
            DrawTriangle3D(n0, n1, c1, colB);
            DrawTriangle3D(c1, n0, c0, colA);
            DrawTriangle3D(c1, n1, n0, colB);

            if (trail->hasTexture && trail->texture.id > 0) {
                rlSetTexture(trail->texture.id);
                rlBegin(RL_QUADS);
                float uC = (float)p       / (float)(lc - 1);
                float uN = (float)(p + 1) / (float)(lc - 1);
                rlColor4ub(colA.r, colA.g, colA.b, colA.a); rlTexCoord2f(uC, 0.0f); rlVertex3f(c0.x, c0.y, c0.z);
                rlColor4ub(colB.r, colB.g, colB.b, colB.a); rlTexCoord2f(uN, 0.0f); rlVertex3f(n0.x, n0.y, n0.z);
                rlColor4ub(colC.r, colC.g, colC.b, colC.a); rlTexCoord2f(uN, 1.0f); rlVertex3f(n1.x, n1.y, n1.z);
                rlColor4ub(colA.r, colA.g, colA.b, colA.a); rlTexCoord2f(uC, 1.0f); rlVertex3f(c1.x, c1.y, c1.z);
                rlEnd();
                rlSetTexture(0);
            }
        }
    }

    EndBlendMode();
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
}

static inline void SlashTrail_Tick(SlashTrailSystem* sys, float deltaTime, int currentFrame,
        const SlashEventConfig* events, int eventCount, const AnimationFrame* frame,
        const char* personId, Vector3 worldPos, Vector3 pivot, Matrix rot, bool applyWorldTransform)
{
    if (!sys || !events || !frame) return;

    for (int e = 0; e < eventCount; e++) {
        const SlashEventConfig* ev = &events[e];
        bool inRange = (currentFrame >= ev->frameStart && currentFrame <= ev->frameEnd);

        bool exists = false;
        for (int i = 0; i < SLASH_TRAIL_MAX_ACTIVE; i++)
            if (sys->trails[i].alive && sys->trails[i].slashIndex == e) { exists = true; break; }

        if (inRange && !exists) SlashTrail_Activate(sys, e, ev);
        if (!inRange)           SlashTrail_Deactivate(sys, e);

        if (inRange) {
            Vector3 bonePos = SlashTrail__GetEmitPos(frame, personId, ev);
            if (applyWorldTransform) {
                Vector3 rel = Vector3Subtract(bonePos, pivot);
                bonePos = Vector3Add(Vector3Add(worldPos, pivot), Vector3Transform(rel, rot));
            }
            SlashTrail_UpdateTrail(sys, deltaTime, e, bonePos);
        } else if (exists) {
            for (int ti = 0; ti < SLASH_TRAIL_MAX_ACTIVE; ti++) {
                SlashTrail* tr = &sys->trails[ti];
                if (!tr->alive || tr->slashIndex != e) continue;
                tr->active    = false;
                tr->spawnTimer = 0.0f;
                bool anyAlive = false;
                for (int p = 0; p < tr->config.segments; p++) {
                    SlashTrailPoint* pt = &tr->points[p];
                    if (!pt->active) continue;
                    pt->age += deltaTime;
                    if (pt->age >= pt->lifetime) pt->active = false;
                    else anyAlive = true;
                }
                if (!anyAlive) {
                    if (tr->hasTexture) { UnloadTexture(tr->texture); tr->hasTexture = false; }
                    memset(tr, 0, sizeof(SlashTrail));
                } else tr->alive = true;
            }
        }
    }
}

static inline void Model3D_InitSystem(Model3DAttachmentSystem* sys) {
    if (sys) memset(sys, 0, sizeof(Model3DAttachmentSystem));
}

static inline void Model3D_FreeSystem(Model3DAttachmentSystem* sys) {
    if (!sys) return;
    for (int i = 0; i < sys->instanceCount; i++)
        if (sys->instances[i].loaded) { UnloadModel(sys->instances[i].model); sys->instances[i].loaded = false; }
    sys->instanceCount = 0;
}

static inline void Model3D_LoadFromClip(Model3DAttachmentSystem* sys, const AnimationClipMetadata* clip) {
    if (!sys || !clip || !clip->valid) return;

    int eventCount = clip->model3dEventCount;
    if (eventCount <= 0 || eventCount > BONES_MAX_MODEL3D_EVENTS) return;

    Model3DEventConfig events[BONES_MAX_MODEL3D_EVENTS];
    for (int i = 0; i < eventCount; i++) events[i] = clip->model3dEvents[i];

    Model3D_FreeSystem(sys);

    for (int i = 0; i < eventCount && sys->instanceCount < BONES_MAX_MODEL3D_EVENTS; i++) {
        const Model3DEventConfig* cfg = &events[i];
        if (!cfg->valid || !cfg->modelPath[0]) continue;

        Model3DInstance* inst = &sys->instances[sys->instanceCount];
        inst->config  = *cfg;
        inst->visible = false;
        inst->loaded  = false;

        if (FileExists(cfg->modelPath)) {
            inst->model  = LoadModel(cfg->modelPath);
            inst->loaded = true;
        }
        sys->instanceCount++;
    }
}

static inline void Model3D_Tick(Model3DAttachmentSystem* sys, int currentFrame) {
    if (!sys) return;
    for (int i = 0; i < sys->instanceCount; i++) {
        Model3DInstance* inst = &sys->instances[i];
        inst->visible = inst->loaded &&
            currentFrame >= inst->config.frameStart && currentFrame <= inst->config.frameEnd;
    }
}

static inline void Model3D_Draw(Model3DAttachmentSystem* sys, const AnimationFrame* frame,
                                 const char* personId, Vector3 worldPos, Vector3 pivot,
                                 Matrix rot, bool applyWorldTransform)
{
    if (!sys || !frame) return;

    for (int i = 0; i < sys->instanceCount; i++) {
        Model3DInstance* inst = &sys->instances[i];
        if (!inst->visible || !inst->loaded) continue;

        Vector3 bonePos = SlashTrail__GetBonePos(frame, personId, inst->config.boneName);

        const Person* person = NULL;
        for (int p = 0; p < frame->personCount; p++) {
            if (!frame->persons[p].active) continue;
            if (personId && personId[0]) { if (strcmp(frame->persons[p].personId, personId) == 0) { person = &frame->persons[p]; break; } }
            else { person = &frame->persons[p]; break; }
        }

        Vector3 bFwd = {0,0,1}, bUp = {0,1,0}, bRight = {1,0,0};
        if (person) {
            const char* bn = inst->config.boneName;
            const char* prox = strcmp(bn,"RWrist")==0 ? "RElbow"  : strcmp(bn,"LWrist")==0 ? "LElbow"  :
                               strcmp(bn,"RElbow")==0 ? "RShoulder": strcmp(bn,"LElbow")==0 ? "LShoulder":
                               strcmp(bn,"RAnkle")==0 ? "RKnee"   : strcmp(bn,"LAnkle")==0 ? "LKnee"   :
                               strcmp(bn,"RKnee") ==0 ? "RHip"    : strcmp(bn,"LKnee") ==0 ? "LHip"    : NULL;
            Vector3 proxPos = prox ? GetBonePositionByName(person, prox) : (Vector3){bonePos.x, bonePos.y-0.1f, bonePos.z};
            Vector3 dir = Vector3Subtract(bonePos, proxPos);
            float   len = Vector3Length(dir);
            if (len > 1e-4f) {
                bFwd = Vector3Scale(dir, 1.0f / len);
                Vector3 wu = {0,1,0};
                if (fabsf(Vector3DotProduct(bFwd, wu)) > 0.95f) wu = (Vector3){0,0,1};
                bRight = SafeNormalize(Vector3CrossProduct(bFwd, wu));
                bUp    = SafeNormalize(Vector3CrossProduct(bRight, bFwd));
            }
        }

        if (applyWorldTransform) {
            Vector3 rel = Vector3Subtract(bonePos, pivot);
            bonePos = Vector3Add(Vector3Add(worldPos, pivot), Vector3Transform(rel, rot));
            bFwd    = Vector3Transform(bFwd,   rot);
            bUp     = Vector3Transform(bUp,    rot);
            bRight  = Vector3Transform(bRight, rot);
        }

        if (fabsf(inst->config.rotationEuler.x) > 0.001f) {
            float a = DEG2RAD * inst->config.rotationEuler.x;
            Vector3 nF = SafeNormalize(Vector3Add(Vector3Scale(bFwd, cosf(a)), Vector3Scale(bUp, sinf(a))));
            bUp  = SafeNormalize(Vector3CrossProduct(bRight, nF));
            bFwd = nF;
        }
        if (fabsf(inst->config.rotationEuler.y) > 0.001f) {
            float a = DEG2RAD * inst->config.rotationEuler.y;
            Vector3 nF = SafeNormalize(Vector3Add(Vector3Scale(bFwd, cosf(a)), Vector3Scale(bRight, -sinf(a))));
            bRight = SafeNormalize(Vector3CrossProduct(nF, bUp));
            bFwd   = nF;
        }
        if (fabsf(inst->config.rotationEuler.z) > 0.001f) {
            float a = DEG2RAD * inst->config.rotationEuler.z;
            Vector3 nU = SafeNormalize(Vector3Add(Vector3Scale(bUp, cosf(a)), Vector3Scale(bRight, sinf(a))));
            bRight = SafeNormalize(Vector3CrossProduct(bFwd, nU));
            bUp    = nU;
        }

        bonePos = Vector3Add(bonePos, Vector3Scale(bRight, inst->config.offset.x));
        bonePos = Vector3Add(bonePos, Vector3Scale(bUp,    inst->config.offset.y));
        bonePos = Vector3Add(bonePos, Vector3Scale(bFwd,   inst->config.offset.z));

        float s = inst->config.scale;
        inst->model.transform = (Matrix){
            bRight.x*s, bUp.x*s, bFwd.x*s, bonePos.x,
            bRight.y*s, bUp.y*s, bFwd.y*s, bonePos.y,
            bRight.z*s, bUp.z*s, bFwd.z*s, bonePos.z,
            0.0f,       0.0f,    0.0f,      1.0f
        };
        DrawModel(inst->model, (Vector3){0,0,0}, 1.0f, WHITE);
    }
}

static inline void CopyAnimationFrame(AnimationFrame* dest, const AnimationFrame* src) {
    if (!dest || !src) return;
    dest->frameNumber = src->frameNumber;
    dest->valid       = src->valid;
    dest->personCount = src->personCount;

    for (int p = 0; p < src->personCount && p < MAX_PERSONS; p++) {
        Person*       dp = &dest->persons[p];
        const Person* sp = &src->persons[p];
        strncpy(dp->personId, sp->personId, 15); dp->personId[15] = '\0';
        dp->active    = sp->active;
        dp->boneCount = sp->boneCount;

        for (int b = 0; b < sp->boneCount && b < MAX_BONES_PER_PERSON; b++) {
            Bone*       db = &dp->bones[b];
            const Bone* sb = &sp->bones[b];
            strncpy(db->name, sb->name, MAX_BONE_NAME_LENGTH - 1);
            db->name[MAX_BONE_NAME_LENGTH - 1] = '\0';
            db->position     = sb->position;
            db->size         = sb->size;
            db->rotation     = sb->rotation;
            db->visible      = sb->visible;
            db->textureIndex = sb->textureIndex;
            db->mirrored     = sb->mirrored;
        }
    }
}

static inline void CaptureCurrentFrame(AnimatedCharacter* character) {
    if (!character || !character->animation.isLoaded ||
        character->currentFrame < 0 ||
        character->currentFrame >= character->animation.frameCount)
    {
        g_hasValidFromFrame = false;
        return;
    }
    CopyAnimationFrame(&g_transitionFromFrame, &character->animation.frames[character->currentFrame]);
    g_hasValidFromFrame = true;
}

static inline void StartAnimationTransition(void) {
    if (!g_hasValidFromFrame) return;
    g_isTransitioning = true;
    g_transitionTime  = 0.0f;
}

static void InterpolateTransitionFrames(const AnimationFrame* fromFrame, const AnimationFrame* toFrame,
                                         AnimationFrame* result, float t)
{
    if (!fromFrame || !toFrame || !result) return;
    float easedT = 1.0f - powf(1.0f - t, 3.0f);

    result->frameNumber  = toFrame->frameNumber;
    result->valid        = true;
    result->personCount  = fromFrame->personCount > toFrame->personCount
                         ? fromFrame->personCount : toFrame->personCount;

    for (int p = 0; p < result->personCount; p++) {
        Person*       dp         = &result->persons[p];
        const Person* fromPerson = p < fromFrame->personCount ? &fromFrame->persons[p] : NULL;
        const Person* toPerson   = p < toFrame->personCount   ? &toFrame->persons[p]   : NULL;

        if (!fromPerson && toPerson) {
            strncpy(dp->personId, toPerson->personId, 15); dp->personId[15] = '\0';
            dp->active = toPerson->active; dp->boneCount = toPerson->boneCount;
            for (int b = 0; b < toPerson->boneCount; b++) dp->bones[b] = toPerson->bones[b];
            continue;
        }
        if (fromPerson && !toPerson) {
            strncpy(dp->personId, fromPerson->personId, 15); dp->personId[15] = '\0';
            dp->active = fromPerson->active; dp->boneCount = fromPerson->boneCount;
            for (int b = 0; b < fromPerson->boneCount; b++) dp->bones[b] = fromPerson->bones[b];
            continue;
        }
        if (!fromPerson && !toPerson) continue;

        strncpy(dp->personId, fromPerson->personId, 15); dp->personId[15] = '\0';
        dp->active    = fromPerson->active || toPerson->active;
        dp->boneCount = fromPerson->boneCount > toPerson->boneCount
                      ? fromPerson->boneCount : toPerson->boneCount;

        for (int b = 0; b < dp->boneCount; b++) {
            Bone*       db = &dp->bones[b];
            const Bone* fb = b < fromPerson->boneCount ? &fromPerson->bones[b] : NULL;
            const Bone* tb = b < toPerson->boneCount   ? &toPerson->bones[b]   : NULL;

            if (!fb && tb) { *db = *tb; continue; }
            if (fb && !tb) { *db = *fb; continue; }
            if (!fb && !tb) continue;

            strncpy(db->name, fb->name, MAX_BONE_NAME_LENGTH - 1);
            db->name[MAX_BONE_NAME_LENGTH - 1] = '\0';

            if (fb->position.valid && tb->position.valid) {
                db->position.position.x = fb->position.position.x*(1.0f-easedT) + tb->position.position.x*easedT;
                db->position.position.y = fb->position.position.y*(1.0f-easedT) + tb->position.position.y*easedT;
                db->position.position.z = fb->position.position.z*(1.0f-easedT) + tb->position.position.z*easedT;
                db->position.valid      = true;
            } else if (fb->position.valid) {
                db->position = fb->position;
                db->position.confidence = fb->position.confidence * (1.0f - easedT);
            } else if (tb->position.valid) {
                db->position = tb->position;
                db->position.confidence = tb->position.confidence * easedT;
            } else {
                db->position.valid = false;
            }

            db->position.confidence = fb->position.confidence*(1.0f-easedT) + tb->position.confidence*easedT;
            db->size.x    = fb->size.x*(1.0f-easedT) + tb->size.x*easedT;
            db->size.y    = fb->size.y*(1.0f-easedT) + tb->size.y*easedT;
            float rotDiff = tb->rotation - fb->rotation;
            if (rotDiff >  180.0f) rotDiff -= 360.0f;
            if (rotDiff < -180.0f) rotDiff += 360.0f;
            db->rotation     = fb->rotation + rotDiff * easedT;
            db->visible      = fb->visible || tb->visible;
            db->textureIndex = tb->textureIndex;
            db->mirrored     = tb->mirrored;
        }
    }
}

static inline bool LoadAnimation(AnimatedCharacter* character,
                                  const char* animationPath,
                                  const char* metadataPath)
{
    if (!character) return false;

    CaptureCurrentFrame(character);

    AnimController_Free(character->animController);
    character->animController = NULL;

    for (int i = 0; i < SLASH_TRAIL_MAX_ACTIVE; i++)
        SlashTrail_Deactivate(&character->slashTrails, character->slashTrails.trails[i].slashIndex);

    Model3D_FreeSystem(&character->model3dAttachments);
    character->hasWorldTransform = false;

    if (BonesLoadFromJSON(&character->animation, animationPath) != BONES_SUCCESS) {
        g_hasValidFromFrame = false;
        return false;
    }

    BonesCreateMissingFrames(&character->animation);
    character->maxFrames    = BonesGetFrameCount(&character->animation);
    character->currentFrame = 0;
    BonesSetFrame(&character->animation, 0);
    StartAnimationTransition();

    character->animController = AnimController_Create(&character->animation, character->textureSets);
    if (!character->animController) return false;

    if (metadataPath && AnimController_LoadClipMetadata(character->animController, metadataPath)) {
        const char* clipName = (character->animController->clipCount > 0)
            ? character->animController->clips[0].name : "default";

        bool played = AnimController_PlayClip(character->animController, clipName);
        if (played &&
            character->animController->currentClipIndex >= 0 &&
            character->animController->currentClipIndex < character->animController->clipCount &&
            character->animController->clips)
        {
            AnimationClipMetadata clipCopy =
                character->animController->clips[character->animController->currentClipIndex];
            if (clipCopy.valid)
                Model3D_LoadFromClip(&character->model3dAttachments, &clipCopy);
        }
    }

    bool hasSlash = false;
    if (character->animController && character->animController->clips) {
        for (int ci = 0; ci < character->animController->clipCount; ci++)
            if (character->animController->clips[ci].slashEventCount > 0) { hasSlash = true; break; }
    }
    if (hasSlash) {
        SlashTrail_FreeSystem(&character->slashTrails);
        SlashTrail_InitSystem(&character->slashTrails);
    }

    character->forceUpdate        = true;
    character->lastProcessedFrame = -1;
    return true;
}

static inline void SetAnimationTransitionDuration(float duration) {
    if (duration > 0.0f && duration < 1.0f) g_transitionDuration = duration;
}

static inline void ResetCharacterAutoCenter(AnimatedCharacter* character) {
    if (character) character->autoCenterCalculated = false;
}

static inline void LockAnimationRootXZ(AnimatedCharacter* character, bool lock) {
    if (character) character->lockRootXZ = lock;
}

static inline float GetCurrentAnimDuration(const AnimatedCharacter* character) {
    if (!character || !character->animController) return 0.0f;
    const AnimationController* ctrl = character->animController;
    if (ctrl->currentClipIndex < 0 || ctrl->currentClipIndex >= ctrl->clipCount) return 0.0f;
    const AnimationClipMetadata* clip = &ctrl->clips[ctrl->currentClipIndex];
    int frames = clip->endFrame - clip->startFrame + 1;
    return (clip->fps > 0.0f) ? (float)frames / clip->fps : 0.0f;
}

static inline bool IsSlashActiveForCharacter(const AnimatedCharacter* character) {
    if (!character || !character->animController) return false;
    const AnimationController* ctrl = character->animController;
    if (ctrl->currentClipIndex < 0 || ctrl->currentClipIndex >= ctrl->clipCount) return false;
    const AnimationClipMetadata* clip = &ctrl->clips[ctrl->currentClipIndex];
    int cf = ctrl->currentFrameInJSON;
    for (int s = 0; s < clip->slashEventCount; s++)
        if (cf >= clip->slashEvents[s].frameStart && cf <= clip->slashEvents[s].frameEnd) return true;
    return false;
}

static inline Vector3 GetActiveSlashBonePos(const AnimatedCharacter* character, Vector3 fallback) {
    if (!character || !character->animController) return fallback;
    const AnimationController* ctrl = character->animController;
    if (ctrl->currentClipIndex < 0 || ctrl->currentClipIndex >= ctrl->clipCount) return fallback;
    const AnimationClipMetadata* clip = &ctrl->clips[ctrl->currentClipIndex];
    int cf = ctrl->currentFrameInJSON;

    for (int s = 0; s < clip->slashEventCount; s++) {
        const SlashEventConfig* se = &clip->slashEvents[s];
        if (cf < se->frameStart || cf > se->frameEnd) continue;
        for (int f = 0; f < character->animation.frameCount; f++) {
            const AnimationFrame* fr = &character->animation.frames[f];
            if (fr->frameNumber != cf || !fr->valid) continue;
            for (int p = 0; p < fr->personCount; p++) {
                const Person* person = &fr->persons[p];
                if (!person->active) continue;
                for (int b = 0; b < person->boneCount; b++) {
                    const Bone* bone = &person->bones[b];
                    if (strcmp(bone->name, se->boneName) != 0 || !bone->position.valid) continue;
                    Vector3 bp = bone->position.position;
                    if (character->hasWorldTransform) {
                        Vector3 rel     = Vector3Subtract(bp, character->worldPivot);
                        Vector3 rotated = Vector3Transform(rel, MatrixRotateY(character->worldRotation));
                        bp = Vector3Add(Vector3Add(character->worldPosition, character->worldPivot), rotated);
                    }
                    return bp;
                }
            }
        }
    }
    return fallback;
}

static inline AnimatedCharacter* CreateAnimatedCharacter(const char* textureConfigPath, const char* textureSetsPath) {
    AnimatedCharacter* character = (AnimatedCharacter*)calloc(1, sizeof(AnimatedCharacter));
    if (!character) return NULL;

    character->renderer = BonesRenderer_Create();
    if (!character->renderer || !BonesRenderer_Init(character->renderer)) {
        free(character);
        return NULL;
    }

    character->textureSets = BonesTextureSets_Create();
    if (textureSetsPath && !BonesTextureSets_LoadFromFile(character->textureSets, textureSetsPath))
        fprintf(stderr, "[WARN] No se pudo cargar texture set: %s\n", textureSetsPath);

    if (textureConfigPath) {
        LoadSimpleTextureConfig(&character->textureSystem, textureConfigPath);
        LoadBoneConfigurations(&character->textureSystem, &character->boneConfigs, &character->boneConfigCount);
    }

    if (BonesInit(&character->animation, 1000) != BONES_SUCCESS) {
        BonesRenderer_Free(character->renderer);
        BonesTextureSets_Free(character->textureSets);
        free(character);
        return NULL;
    }

    character->renderHeadBillboards  = true;
    character->renderTorsoBillboards = true;
    character->autoPlay              = true;
    character->autoPlaySpeed         = 0.1f;
    character->lastProcessedFrame    = -1;

    character->renderConfig                    = BonesGetDefaultRenderConfig();
    character->renderConfig.drawDebugSpheres   = true;
    character->renderConfig.debugColor         = GREEN;
    character->renderConfig.debugSphereRadius  = 0.035f;
    character->renderConfig.enableDepthSorting = true;
    BonesSetRenderConfig(&character->renderConfig);

    character->ornaments = Ornaments_Create();
    if (textureConfigPath)
        Ornaments_LoadFromConfig(character->ornaments, textureConfigPath);

    SlashTrail_InitSystem(&character->slashTrails);
    Model3D_InitSystem(&character->model3dAttachments);

    return character;
}

static inline void DestroyAnimatedCharacter(AnimatedCharacter* character) {
    if (!character) return;
    SlashTrail_FreeSystem(&character->slashTrails);
    Model3D_FreeSystem(&character->model3dAttachments);
    AnimController_Free(character->animController);
    BonesTextureSets_Free(character->textureSets);
    BonesRenderer_Free(character->renderer);
    free(character->renderBones);
    free(character->renderHeads);
    free(character->renderTorsos);
    CleanupTextureSystem(&character->textureSystem, &character->boneConfigs, &character->boneConfigCount);
    BonesFree(&character->animation);
    Ornaments_Free(character->ornaments);
    free(character);
}

static inline void UpdateAnimatedCharacter(AnimatedCharacter* character, float deltaTime) {
    if (!character) return;

    if (character->animController && character->autoPlay) {
        AnimController_Update(character->animController, deltaTime);
        int fc = AnimController_GetCurrentFrame(character->animController);
        if (fc != character->currentFrame) {
            BonesSetFrame(&character->animation, fc);
            character->currentFrame = fc;
            character->forceUpdate  = true;
        }
    }

    static AnimationFrame transitionFrame;
    bool usingTransition = false;

    if (g_isTransitioning) {
        g_transitionTime += deltaTime;
        float t = g_transitionTime / g_transitionDuration;
        if (t >= 1.0f) {
            g_isTransitioning = false;
        } else if (character->animation.isLoaded &&
                   character->currentFrame >= 0 &&
                   character->currentFrame < character->animation.frameCount) {
            CopyAnimationFrame(&g_transitionToFrame, &character->animation.frames[character->currentFrame]);
            InterpolateTransitionFrames(&g_transitionFromFrame, &g_transitionToFrame, &transitionFrame, t);
            usingTransition = true;
        } else {
            g_isTransitioning = false;
        }
    }

    static AnimationFrame interpolatedFrame;
    bool usingInterpolation = false;

    if (!usingTransition && character->animation.isLoaded && character->autoPlay &&
        character->currentFrame >= 0 && character->currentFrame < character->animation.frameCount - 1 &&
        character->animController && character->animController->currentClipIndex >= 0)
    {
        AnimationClipMetadata* clip =
            &character->animController->clips[character->animController->currentClipIndex];
        if (clip->fps > 0.0f) {
            float frameDuration = 1.0f / clip->fps;
            float timeInFrame   = fmodf(character->animController->localTime, frameDuration);
            float t             = timeInFrame / frameDuration;
            t = t * t * (3.0f - 2.0f * t);

            AnimationFrame* curF  = &character->animation.frames[character->currentFrame];
            AnimationFrame* nextF = &character->animation.frames[character->currentFrame + 1];

            if (t > 0.05f && t < 0.95f && curF->valid && nextF->valid) {
                memset(&interpolatedFrame, 0, sizeof(AnimationFrame));
                interpolatedFrame.frameNumber = curF->frameNumber;
                interpolatedFrame.valid       = true;
                interpolatedFrame.personCount = curF->personCount;

                for (int p = 0; p < interpolatedFrame.personCount; p++) {
                    Person*       dp  = &interpolatedFrame.persons[p];
                    Person*       pA  = &curF->persons[p];
                    Person*       pB  = (p < nextF->personCount) ? &nextF->persons[p] : pA;

                    strncpy(dp->personId, pA->personId, 15); dp->personId[15] = '\0';
                    dp->active    = pA->active;
                    dp->boneCount = pA->boneCount;

                    for (int b = 0; b < dp->boneCount; b++) {
                        Bone* bA = &pA->bones[b];
                        Bone* bB = (b < pB->boneCount) ? &pB->bones[b] : bA;
                        InterpolateBone(bA, bB, &dp->bones[b], t);
                    }
                }
                usingInterpolation = true;
            }
        }
    }

    const AnimationFrame* frameToUse = NULL;
    if (usingTransition)      frameToUse = &transitionFrame;
    else if (usingInterpolation) frameToUse = &interpolatedFrame;
    else if (character->animation.isLoaded && BonesIsValidFrame(&character->animation, character->currentFrame))
        frameToUse = &character->animation.frames[character->currentFrame];

    if (frameToUse && frameToUse->valid) {
        Vector3 totalPos = {0,0,0}; int validCount = 0;
        for (int p = 0; p < frameToUse->personCount; p++) {
            const Person* person = &frameToUse->persons[p];
            if (!person->active) continue;
            Vector3 headPos  = CalculateHeadPosition(person);
            Vector3 chestPos = CalculateChestPosition(person);
            Vector3 hipPos   = CalculateHipPosition(person);
            if (Vector3Length(headPos)  > 0.01f) { totalPos = Vector3Add(totalPos, headPos);  validCount++; }
            if (Vector3Length(chestPos) > 0.01f) { totalPos = Vector3Add(totalPos, chestPos); validCount++; }
            if (Vector3Length(hipPos)   > 0.01f) { totalPos = Vector3Add(totalPos, hipPos);   validCount++; }
        }
        if (validCount > 0) {
            Vector3 newCenter = Vector3Scale(totalPos, 1.0f / validCount);
            if (!character->autoCenterCalculated) character->autoCenter = newCenter;
            else character->autoCenter = Vector3Lerp(character->autoCenter, newCenter, 0.3f);
            character->autoCenterCalculated = true;
        }
    }

    if (character->ornaments && character->ornaments->loaded && frameToUse && frameToUse->valid) {
        if (!character->ornaments->ornaments[0].initialized)
            Ornaments_InitializePhysics(character->ornaments, frameToUse);
        Ornaments_UpdatePhysics(character->ornaments, frameToUse, deltaTime);
    }

    if (!usingTransition && character->animController && character->animController->currentClipIndex >= 0) {
        AnimationClipMetadata* activeClip =
            &character->animController->clips[character->animController->currentClipIndex];

        if (activeClip->slashEventCount > 0 && character->animation.isLoaded) {
            int jsonFrame = character->animController->currentFrameInJSON;
            const AnimationFrame* realFrame = NULL;
            for (int f = 0; f < character->animation.frameCount; f++) {
                if (character->animation.frames[f].frameNumber == jsonFrame) {
                    realFrame = &character->animation.frames[f]; break;
                }
            }
            if (realFrame && realFrame->valid) {
                const char* personId = (realFrame->personCount > 0) ? realFrame->persons[0].personId : "";
                SlashTrail_Tick(&character->slashTrails, deltaTime, jsonFrame,
                    activeClip->slashEvents, activeClip->slashEventCount, realFrame, personId,
                    character->worldPosition, character->worldPivot,
                    MatrixRotateY(character->worldRotation), character->hasWorldTransform);
                Model3D_Tick(&character->model3dAttachments, jsonFrame);
            }
        }
        if (activeClip->slashEventCount == 0 && character->model3dAttachments.instanceCount > 0)
            Model3D_Tick(&character->model3dAttachments, character->animController->currentFrameInJSON);
    }

    if (character->forceUpdate || character->currentFrame != character->lastProcessedFrame ||
        usingInterpolation || usingTransition)
    {
        character->renderBonesCount  = 0;
        character->renderHeadsCount  = 0;
        character->renderTorsosCount = 0;

        AnimationFrame backup; bool needsRestore = false;
        if (usingTransition || usingInterpolation) {
            backup = character->animation.frames[character->currentFrame];
            character->animation.frames[character->currentFrame] =
                usingTransition ? transitionFrame : interpolatedFrame;
            needsRestore = true;
        }

        CollectBonesForRendering(&character->animation, character->renderer->camera,
            &character->renderBones, &character->renderBonesCount, &character->renderBonesCapacity,
            character->boneConfigs, character->boneConfigCount,
            character->textureSets, character->ornaments);

        if (character->renderHeadBillboards)
            CollectHeadsForRendering(&character->animation, &character->renderHeads,
                &character->renderHeadsCount, &character->renderHeadsCapacity,
                character->boneConfigs, character->boneConfigCount, character->textureSets);

        if (character->renderTorsoBillboards)
            CollectTorsosForRendering(&character->animation, &character->renderTorsos,
                &character->renderTorsosCount, &character->renderTorsosCapacity,
                character->boneConfigs, character->boneConfigCount, character->textureSets);

        if (needsRestore)
            character->animation.frames[character->currentFrame] = backup;

        character->lastProcessedFrame = character->currentFrame;
        character->forceUpdate        = false;
    }
}

static inline Vector3 RotatePointAroundPivot(Vector3 point, Vector3 pivot, Vector3 worldPos, Matrix rotY) {
    return Vector3Add(Vector3Add(worldPos, pivot), Vector3Transform(Vector3Subtract(point, pivot), rotY));
}

static inline void DrawModel3DWithDepthOrder(Model3DAttachmentSystem* attachments,
        const AnimationFrame* frame, const char* personId,
        Vector3 worldPos, Vector3 pivot, Matrix rot, bool applyTransform,
        Camera camera, Vector3 characterCenter, bool drawBefore)
{
    if (!attachments || attachments->instanceCount == 0 || !frame) return;

    float modelDist = 1e9f; bool hasVisible = false;
    for (int i = 0; i < attachments->instanceCount; i++) {
        Model3DInstance* inst = &attachments->instances[i];
        if (!inst->visible || !inst->loaded) continue;
        hasVisible = true;
        Vector3 bp = SlashTrail__GetBonePos(frame, personId, inst->config.boneName);
        if (applyTransform) {
            Vector3 rel = Vector3Subtract(bp, pivot);
            bp = Vector3Add(Vector3Add(worldPos, pivot), Vector3Transform(rel, rot));
        }
        float d = Vector3Distance(camera.position, bp);
        if (d < modelDist) modelDist = d;
    }
    if (!hasVisible) return;

    float charDist = Vector3Distance(camera.position, characterCenter);
    bool  isFarther = (modelDist > charDist);

    if ((drawBefore && isFarther) || (!drawBefore && !isFarther)) {
        BeginMode3D(camera);
        rlDisableDepthTest();
        Model3D_Draw(attachments, frame, personId, worldPos, pivot, rot, applyTransform);
        rlEnableDepthTest();
        EndMode3D();
    }
}

static inline void DrawAnimatedCharacterTransformed(AnimatedCharacter* character, Camera camera,
                                                     Vector3 worldPosition, float worldRotation)
{
    if (!character || !character->animation.isLoaded) return;

    character->worldPosition     = worldPosition;
    character->worldRotation     = worldRotation;
    character->worldPivot        = character->autoCenter;
    character->hasWorldTransform = true;

    Camera origCam = character->renderer->camera;
    character->renderer->camera  = camera;

    Matrix  rot   = MatrixRotateY(worldRotation);
    Vector3 pivot = character->autoCenter;

    int bc = character->renderBonesCount;
    int hc = character->renderHeadsCount;
    int tc = character->renderTorsosCount;

    BoneRenderData*   bonesCopy  = bc > 0 ? malloc(sizeof(BoneRenderData)  * bc) : NULL;
    HeadRenderData*   headsCopy  = hc > 0 ? malloc(sizeof(HeadRenderData)  * hc) : NULL;
    TorsoRenderData*  torsosCopy = tc > 0 ? malloc(sizeof(TorsoRenderData) * tc) : NULL;

    if (bonesCopy && bc > 0)  memcpy(bonesCopy,  character->renderBones,  sizeof(BoneRenderData)  * bc);
    if (headsCopy && hc > 0)  memcpy(headsCopy,  character->renderHeads,  sizeof(HeadRenderData)  * hc);
    if (torsosCopy && tc > 0) memcpy(torsosCopy, character->renderTorsos, sizeof(TorsoRenderData) * tc);

    AnimationFrame* transformedFrame = NULL;
    if (tc > 0 && character->animation.isLoaded &&
        character->currentFrame >= 0 && character->currentFrame < character->animation.frameCount)
    {
        transformedFrame = malloc(sizeof(AnimationFrame));
        if (transformedFrame) {
            *transformedFrame = character->animation.frames[character->currentFrame];
            for (int p = 0; p < transformedFrame->personCount; p++) {
                Person* person = &transformedFrame->persons[p];
                for (int b = 0; b < person->boneCount; b++) {
                    Bone* bone = &person->bones[b];
                    if (bone->position.valid)
                        bone->position.position = RotatePointAroundPivot(bone->position.position, pivot, worldPosition, rot);
                }
            }
        }
    }

    if (bonesCopy) {
        for (int i = 0; i < bc; i++) {
            BoneRenderData* b = &bonesCopy[i];
            if (!b->valid) continue;
            b->position = RotatePointAroundPivot(b->position, pivot, worldPosition, rot);
            if (b->orientation.valid) {
                b->orientation.forward = SafeNormalize(Vector3Transform(b->orientation.forward, rot));
                b->orientation.up      = SafeNormalize(Vector3Transform(b->orientation.up,      rot));
                b->orientation.right   = SafeNormalize(Vector3Transform(b->orientation.right,   rot));
                b->orientation.position = b->position;
                b->orientation.yaw    += worldRotation;
                while (b->orientation.yaw >  PI) b->orientation.yaw -= 2.0f * PI;
                while (b->orientation.yaw < -PI) b->orientation.yaw += 2.0f * PI;
            }
            if (IsWristBone(b->boneName)) {
                Vector3 charFwd = SafeNormalize((Vector3){ sinf(worldRotation), 0.0f, cosf(worldRotation) });
                Vector3 up      = {0,1,0};
                Vector3 right   = SafeNormalize(Vector3CrossProduct(charFwd, up));
                b->orientation.forward  = charFwd;
                b->orientation.right    = right;
                b->orientation.up       = SafeNormalize(Vector3CrossProduct(right, charFwd));
                b->orientation.position = b->position;
                b->orientation.yaw      = atan2f(charFwd.x, charFwd.z) + PI;
                while (b->orientation.yaw >  PI) b->orientation.yaw -= 2.0f * PI;
                while (b->orientation.yaw < -PI) b->orientation.yaw += 2.0f * PI;
                b->orientation.valid = true;
            }
        }
    }

    if (headsCopy) {
        for (int i = 0; i < hc; i++) {
            HeadRenderData* h = &headsCopy[i];
            if (!h->valid) continue;
            h->position = RotatePointAroundPivot(h->position, pivot, worldPosition, rot);
            if (h->orientation.valid) {
                h->orientation.forward  = SafeNormalize(Vector3Transform(h->orientation.forward, rot));
                h->orientation.up       = SafeNormalize(Vector3Transform(h->orientation.up,      rot));
                h->orientation.right    = SafeNormalize(Vector3Transform(h->orientation.right,   rot));
                h->orientation.position = h->position;
                h->orientation.yaw     += worldRotation;
            }
        }
    }

    if (torsosCopy) {
        for (int i = 0; i < tc; i++) {
            TorsoRenderData* t = &torsosCopy[i];
            if (!t->valid) continue;
            if (transformedFrame && t->person) {
                for (int p = 0; p < transformedFrame->personCount; p++) {
                    if (strcmp(transformedFrame->persons[p].personId, t->personId) == 0) {
                        t->person = &transformedFrame->persons[p]; break;
                    }
                }
            }
            if (t->orientation.valid) {
                t->orientation.forward  = SafeNormalize(Vector3Transform(t->orientation.forward, rot));
                t->orientation.up       = SafeNormalize(Vector3Transform(t->orientation.up,      rot));
                t->orientation.right    = SafeNormalize(Vector3Transform(t->orientation.right,   rot));
                t->orientation.yaw     += worldRotation;
                while (t->orientation.yaw >  PI) t->orientation.yaw -= 2.0f * PI;
                while (t->orientation.yaw < -PI) t->orientation.yaw += 2.0f * PI;
            }
            t->position = RotatePointAroundPivot(t->position, pivot, worldPosition, rot);
            if (t->orientation.valid) t->orientation.position = t->position;
            t->disableCompensation = false;
        }
    }

    Vector3 transformedCenter = Vector3Add(worldPosition, pivot);

    const AnimationFrame* rf  = NULL;
    const char*           pid = "";
    if (character->currentFrame >= 0 && character->currentFrame < character->animation.frameCount) {
        rf  = &character->animation.frames[character->currentFrame];
        pid = (rf->personCount > 0) ? rf->persons[0].personId : "";
    }

    DrawModel3DWithDepthOrder(&character->model3dAttachments, rf, pid,
        character->worldPosition, character->worldPivot, rot, true, camera, transformedCenter, true);

    BonesRenderer_RenderFrame(character->renderer,
        bonesCopy  ? bonesCopy  : character->renderBones,  bc,
        headsCopy  ? headsCopy  : character->renderHeads,   hc,
        torsosCopy ? torsosCopy : character->renderTorsos, tc,
        transformedCenter, character->autoCenterCalculated);

    DrawModel3DWithDepthOrder(&character->model3dAttachments, rf, pid,
        character->worldPosition, character->worldPivot, rot, true, camera, transformedCenter, false);

    free(bonesCopy); free(headsCopy); free(torsosCopy); free(transformedFrame);
    character->renderer->camera = origCam;

    bool hasLiveTrail = false;
    for (int i = 0; i < SLASH_TRAIL_MAX_ACTIVE; i++)
        if (character->slashTrails.trails[i].alive) { hasLiveTrail = true; break; }
    if (hasLiveTrail) {
        BeginMode3D(camera);
        SlashTrail_Draw(&character->slashTrails, camera);
        EndMode3D();
    }
}

static inline void SetCharacterFrame(AnimatedCharacter* character, int frame) {
    if (!character || frame < 0 || frame >= character->maxFrames) return;
    character->currentFrame = frame;
    BonesSetFrame(&character->animation, frame);
    character->forceUpdate = true;
    if (character->animController)
        character->animController->currentFrameInJSON = frame;
}

static inline void SetCharacterAutoPlay(AnimatedCharacter* character, bool autoPlay) {
    if (!character) return;
    character->autoPlay = autoPlay;
    if (!autoPlay || !character->animController) return;

    if (character->animController->currentClipIndex >= 0) {
        AnimationClipMetadata* clip =
            &character->animController->clips[character->animController->currentClipIndex];
        if (!clip->loop && !character->animController->playing)
            RestartAnimation(character);
        else
            character->animController->playing = true;
    }
}

static inline void RestartAnimation(AnimatedCharacter* character) {
    if (!character) return;
    if (character->animation.isLoaded && character->animation.frameCount > 0)
        SetCharacterFrame(character, 0);

    if (character->animController) {
        character->animController->localTime = 0.0f;
        character->animController->playing   = true;
        if (character->animController->currentClipIndex >= 0) {
            AnimationClipMetadata* clip =
                &character->animController->clips[character->animController->currentClipIndex];
            for (int i = 0; i < clip->eventCount; i++)
                clip->events[i].processed = false;
        }
    }
    character->autoPlay = true;
}

static inline bool BonesInterpolateFrames(BonesAnimation* animation, int frameA, int frameB, int framesToAdd) {
    if (!animation || frameA < 0 || frameB <= frameA || frameB >= animation->frameCount ||
        framesToAdd <= 0 || animation->frameCount + framesToAdd > animation->maxFrames) return false;

    for (int i = animation->frameCount - 1; i >= frameB; i--) {
        animation->frames[i + framesToAdd] = animation->frames[i];
        animation->frames[i + framesToAdd].frameNumber = i + framesToAdd;
    }

    AnimationFrame* srcA = &animation->frames[frameA];
    AnimationFrame* srcB = &animation->frames[frameB + framesToAdd];

    for (int i = 0; i < framesToAdd; i++) {
        int   newIdx = frameA + i + 1;
        float t      = (float)(i + 1) / (float)(framesToAdd + 1);

        AnimationFrame* dest = &animation->frames[newIdx];
        dest->frameNumber        = newIdx;
        dest->personCount        = srcA->personCount > srcB->personCount ? srcA->personCount : srcB->personCount;
        dest->valid              = true;
        dest->isOriginalKeyframe = false;

        for (int p = 0; p < dest->personCount; p++) {
            if      (p < srcA->personCount && p < srcB->personCount) InterpolatePerson(&srcA->persons[p], &srcB->persons[p], &dest->persons[p], t);
            else if (p < srcA->personCount)                          dest->persons[p] = srcA->persons[p];
            else                                                     dest->persons[p] = srcB->persons[p];
        }
    }

    animation->frameCount += framesToAdd;
    return true;
}

static inline bool BonesInsertEmptyFrame(BonesAnimation* animation, int position) {
    if (!animation || position < 0 || position > animation->frameCount ||
        animation->frameCount >= animation->maxFrames) return false;

    for (int i = animation->frameCount; i > position; i--) {
        animation->frames[i] = animation->frames[i - 1];
        animation->frames[i].frameNumber = i;
    }

    AnimationFrame* newFrame = &animation->frames[position];
    memset(newFrame, 0, sizeof(AnimationFrame));
    newFrame->frameNumber = position;
    newFrame->valid       = true;
    animation->frameCount++;
    return true;
}

static inline bool BonesCopyFrame(BonesAnimation* animation, int sourceFrame, int targetFrame) {
    if (!animation || sourceFrame < 0 || sourceFrame >= animation->frameCount ||
        targetFrame < 0 || targetFrame >= animation->maxFrames) return false;

    animation->frames[targetFrame]             = animation->frames[sourceFrame];
    animation->frames[targetFrame].frameNumber = targetFrame;
    if (targetFrame >= animation->frameCount) animation->frameCount = targetFrame + 1;
    return true;
}

static inline void DrawAnimatedCharacter(AnimatedCharacter* character, Camera camera) {
    if (!character || !character->animation.isLoaded) return;
    character->renderer->camera = camera;

    const AnimationFrame* rf  = NULL;
    const char*           pid = "";
    if (character->currentFrame >= 0 && character->currentFrame < character->animation.frameCount) {
        rf  = &character->animation.frames[character->currentFrame];
        pid = (rf->personCount > 0) ? rf->persons[0].personId : "";
    }

    float modelDist = 1e9f; bool hasVisibleModel = false;
    if (character->model3dAttachments.instanceCount > 0 && rf) {
        for (int i = 0; i < character->model3dAttachments.instanceCount; i++) {
            Model3DInstance* inst = &character->model3dAttachments.instances[i];
            if (!inst->visible || !inst->loaded) continue;
            hasVisibleModel = true;
            float d = Vector3Distance(camera.position, SlashTrail__GetBonePos(rf, pid, inst->config.boneName));
            if (d < modelDist) modelDist = d;
        }
    }
    float charDist = Vector3Distance(camera.position, character->autoCenter);

    if (hasVisibleModel && modelDist > charDist) {
        BeginMode3D(camera); rlDisableDepthTest();
        Model3D_Draw(&character->model3dAttachments, rf, pid,
            character->worldPosition, character->worldPivot,
            MatrixRotateY(character->worldRotation), character->hasWorldTransform);
        rlEnableDepthTest(); EndMode3D();
    }

    BonesRenderer_RenderFrame(character->renderer,
        character->renderBones,  character->renderBonesCount,
        character->renderHeads,  character->renderHeadsCount,
        character->renderTorsos, character->renderTorsosCount,
        character->autoCenter, character->autoCenterCalculated);

    if (hasVisibleModel && modelDist <= charDist) {
        BeginMode3D(camera); rlDisableDepthTest();
        Model3D_Draw(&character->model3dAttachments, rf, pid,
            character->worldPosition, character->worldPivot,
            MatrixRotateY(character->worldRotation), character->hasWorldTransform);
        rlEnableDepthTest(); EndMode3D();
    }

    BeginMode3D(camera);
    SlashTrail_Draw(&character->slashTrails, camera);
    EndMode3D();
}

static inline void SetCharacterBillboards(AnimatedCharacter* character, bool heads, bool torsos) {
    if (!character) return;
    character->renderHeadBillboards  = heads;
    character->renderTorsoBillboards = torsos;
    character->forceUpdate           = true;
}

#endif