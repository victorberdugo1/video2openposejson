#include "bones_core.h"
#include "raylib.h"

// TextureSetCollection* g_textureSets = NULL;

static AnimationFrame g_transitionFromFrame;
static AnimationFrame g_transitionToFrame;
static bool g_isTransitioning = false;
static float g_transitionTime = 0.0f;
static float g_transitionDuration = 0.85f;
static bool g_hasValidFromFrame = false;

static BonesRenderConfig g_renderConfig = {
    .defaultBoneSize = 0.35f,
    .drawDebugSpheres = false,
    .enableDepthSorting = true,
    .debugColor = RED,
    .debugSphereRadius = 0.035f,
    .showInvalidBones = false
};

static const struct {
    const char* boneName;
    const char* connections[3];
    float priority[3];
} BONE_CONNECTIONS[] = {
    {"LShoulder", {"LShoulder", "LElbow", ""}, {1.0f, 0.8f, 0.0f}},
    {"LElbow", {"LElbow", "LWrist", ""}, {1.0f, 0.8f, 0.0f}},
    {"LWrist", {"LWrist", "LElbow", ""}, {1.0f, 0.0f, 0.0f}},
    {"RShoulder", {"RShoulder", "RElbow", ""}, {1.0f, 0.8f, 0.0f}},
    {"RElbow", {"RElbow", "RWrist", ""}, {1.0f, 0.8f, 0.0f}},
    {"RWrist", {"RElbow", "RWrist", ""}, {1.0f, 0.0f, 0.0f}},
    {"LHip", {"LHip", "LKnee", ""}, {1.0f, 0.8f, 0.0f}},
    {"LKnee", {"LKnee", "LAnkle", ""}, {1.0f, 0.8f, 0.0f}},
    {"LAnkle", {"LKnee", "LAnkle", ""}, {1.0f, 0.8f, 0.0f}},
    {"RHip", {"RHip", "RKnee", ""}, {1.0f, 0.8f, 0.0f}},
    {"RKnee", {"RKnee", "RAnkle", ""}, {1.0f, 0.8f, 0.0f}},
    {"RAnkle", {"RKnee", "RAnkle", ""}, {1.0f, 0.8f, 0.0f}},
    {"Neck", {"Neck", "Head", ""},  {0.8f, 1.0f, 0.0f}},
    {"", {"", "", ""}, {0.0f, 0.0f, 0.0f}}
};

static const struct {
    const char* boneName;
    const char* primaryConnection;
    const char* secondaryConnection;
    bool reverseForward;
    bool isLimb;
    bool useStableOrientation;
} BONE_ORIENTATIONS[] = {
    {"LShoulder", "LElbow", "Neck", false, true, true},
    {"LElbow", "Neck", "LWrist", true, true, true},
    {"LWrist", "LElbow", "", false, false, true},
    {"RShoulder", "RElbow", "Neck", true, true, true},
    {"RElbow", "Neck", "RWrist", false, true, true},
    {"RWrist", "RElbow", "", false, false, true},
    {"LHip", "LKnee", "Hip", true, true, true},
    {"LKnee", "LAnkle", "LHip", true, true, true},
    {"LAnkle", "LKnee", "FRONT_CALCULATED", true, false, true},
    {"RHip", "RKnee", "Hip", false, true, true},
    {"RKnee", "RAnkle", "RHip", true, true, true},
    {"RAnkle", "RKnee", "REAR_CALCULATED", false, false, true},
    {"Neck", "HEAD_CALCULATED", "", true, false, true},
    {"", "", "", false, false, false}
};

static const struct {
    const char* boneName;
    float yawOffset;
    float pitchOffset;
    float rollOffset;
} BONE_ANGLE_OFFSETS[] = {
    {"LShoulder", 90.0f, 180.0f, -90.0f},
    {"LElbow", 90.0f, 225.0f, -90.0f},
    {"LWrist", 90.0f, 180.0f, 90.0f},
    {"RShoulder", -90.0f, 0.0f, 90.0f},
    {"RElbow", -90.0f, -90.0f, 90.0f},
    {"RWrist", 90.0f, 180.0f, 90.0f},
    {"LHip", 90.0f, -45.0f, 90.0f},
    {"LKnee", 90.0f, 90.0f, 90.0f},
    {"LAnkle",  90.0f, -90.0f, 90.0f},
    {"RHip", -90.0f, -45.0f, -90.0f},
    {"RKnee", -90.0f, 90.0f, 90.0f},
    {"RAnkle", 90.0f, 180.0f, 90.0f},
    {"Neck", -90.0f, 180.0f, -90.0f},
    {"", 0.0f, 0.0f, 0.0f}
};

static const struct {
    const char* boneName;
    float scaleFactorUp;
    float scaleFactorDown;
} BONE_SCALE_FACTORS[] = {
    {"LShoulder", 0.3f, 1.2f},
    {"RShoulder", 0.3f, 1.2f},
    {"LElbow", 0.0f, 0.9f},
    {"RElbow", 0.0f, 0.9f},
    {"LHip", 0.1f, 1.0f},
    {"RHip", 0.1f, 1.0f},
    {"LKnee", 0.1f, 1.0f},
    {"RKnee", 0.1f, 1.0f},
    {"", 0.5f, 0.5f}
};

static const float HEAD_DEPTH_OFFSET = 0.04f;
static const float CHEST_OFFSET_Y = -0.06f;
static const float CHEST_OFFSET_Z = -0.005f;
static const float CHEST_FALLBACK_Y = -0.08f;
static const float HIP_OFFSET_Y = -0.02f;

// ============================================================================
// INITIALIZATION AND CLEANUP
// ============================================================================

AnimatedCharacter* CreateAnimatedCharacter(const char* textureConfigPath, const char* textureSetsPath) {
    AnimatedCharacter* character = (AnimatedCharacter*)calloc(1, sizeof(AnimatedCharacter));
    if (!character) return NULL;

    character->renderer = BonesRenderer_Create();
    if (!character->renderer || !BonesRenderer_Init(character->renderer)) {
        free(character);
        return NULL;
    }

    character->textureSets = BonesTextureSets_Create();
    if (textureSetsPath && !BonesTextureSets_LoadFromFile(character->textureSets, textureSetsPath)) {
        // Advertencia pero continuamos
    }

    // ELIMINADO: g_textureSets = character->textureSets;

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

    character->renderHeadBillboards = true;
    character->renderTorsoBillboards = true;
    character->autoPlay = true;
    character->autoPlaySpeed = 0.1f;
    character->lastProcessedFrame = -1;

    character->renderConfig = BonesGetDefaultRenderConfig();
    character->renderConfig.drawDebugSpheres = true;
    character->renderConfig.debugColor = GREEN;
    character->renderConfig.debugSphereRadius = 0.035f;
    character->renderConfig.enableDepthSorting = true;
    BonesSetRenderConfig(&character->renderConfig);

    return character;
}

void DestroyAnimatedCharacter(AnimatedCharacter* character) {
    if (!character) return;

    AnimController_Free(character->animController);
    BonesTextureSets_Free(character->textureSets);
    BonesRenderer_Free(character->renderer);
    free(character->renderBones);
    free(character->renderHeads);
    free(character->renderTorsos);
    CleanupTextureSystem(&character->textureSystem, &character->boneConfigs, &character->boneConfigCount);
    BonesFree(&character->animation);
    
    // ELIMINADO: g_textureSets = NULL;
    
    free(character);
}

BonesError BonesInit(BonesAnimation* animation, int maxFrames) {
    if (!animation) return BONES_ERROR_NULL_POINTER;

    if (maxFrames <= 0 || maxFrames > MAX_FRAMES) maxFrames = MAX_FRAMES;

    memset(animation, 0, sizeof(BonesAnimation));
    animation->frames = calloc(maxFrames, sizeof(AnimationFrame));

    if (!animation->frames) return BONES_ERROR_MEMORY_ALLOCATION;

    animation->maxFrames = maxFrames;
    animation->currentFrame = -1;
    return BONES_SUCCESS;
}

void BonesFree(BonesAnimation* animation) {
    if (animation) {
        free(animation->frames);
        memset(animation, 0, sizeof(BonesAnimation));
        animation->currentFrame = -1;
    }
}

BonesRenderer* BonesRenderer_Create(void) {
    BonesRenderer* renderer = (BonesRenderer*)calloc(1, sizeof(BonesRenderer));
    if (!renderer) return NULL;
    
    renderer->textureCount = 0;
    renderer->physCols = 4;
    renderer->physRows = 4;
    
    return renderer;
}

void BonesRenderer_Free(BonesRenderer* renderer) {
    if (!renderer) return;
    
    for (int i = 0; i < renderer->textureCount; i++) {
        UnloadTexture(renderer->textures[i]);
    }
    
    free(renderer);
}

bool BonesRenderer_Init(BonesRenderer* renderer) {
    if (!renderer) return false;
    
    renderer->camera = (Camera){
        .position = {0.0f, 0.6f, 2.5f},
        .target = {0.0f, 0.6f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE
    };
    
    return true;
}

TextureSetCollection* BonesTextureSets_Create(void) {
    TextureSetCollection* collection;
    collection = (TextureSetCollection*)calloc(1, sizeof(TextureSetCollection));
    if (!collection) return NULL;
    collection->loaded = false;
    collection->setCount = 0;
    return collection;
}

void BonesTextureSets_Free(TextureSetCollection* collection) {
    if (collection) free(collection);
}

void CleanupTextureSystem(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int*boneConfigCount) {
    if (textureSystem->configs) {
        free(textureSystem->configs);
        memset(textureSystem, 0, sizeof(SimpleTextureSystem));
    }
    if (*boneConfigs) {
        free(*boneConfigs);
        *boneConfigs = NULL;
        *boneConfigCount = 0;
    }
}

// ============================================================================
// ANIMATION FRAME MANAGEMENT
// ============================================================================

static void CopyAnimationFrame(AnimationFrame* dest, const AnimationFrame* src) {
    if (!dest || !src) return;
    
    dest->frameNumber = src->frameNumber;
    dest->valid = src->valid;
    dest->personCount = src->personCount;
    
    for (int p = 0; p < src->personCount && p < MAX_PERSONS; p++) {
        Person* destPerson = &dest->persons[p];
        const Person* srcPerson = &src->persons[p];
        
        strncpy(destPerson->personId, srcPerson->personId, 15);
        destPerson->personId[15] = '\0';
        
        destPerson->active = srcPerson->active;
        destPerson->boneCount = srcPerson->boneCount;
        
        for (int b = 0; b < srcPerson->boneCount && b < MAX_BONES_PER_PERSON; b++) {
            Bone* destBone = &destPerson->bones[b];
            const Bone* srcBone = &srcPerson->bones[b];
            
            strncpy(destBone->name, srcBone->name, MAX_BONE_NAME_LENGTH - 1);
            destBone->name[MAX_BONE_NAME_LENGTH - 1] = '\0';
            
            destBone->position.position = srcBone->position.position;
            destBone->position.valid = srcBone->position.valid;
            destBone->position.confidence = srcBone->position.confidence;
            
            destBone->size = srcBone->size;
            destBone->rotation = srcBone->rotation;
            destBone->visible = srcBone->visible;
            destBone->textureIndex = srcBone->textureIndex;
            destBone->mirrored = srcBone->mirrored;
        }
    }
}

static void InterpolateTransitionFrames(const AnimationFrame* fromFrame, 
                                       const AnimationFrame* toFrame,
                                       AnimationFrame* result, 
                                       float t) {
    if (!fromFrame || !toFrame || !result) return;
    
    float easedT = 1.0f - powf(1.0f - t, 3.0f);
    
    result->frameNumber = toFrame->frameNumber;
    result->valid = true;
    result->personCount = fromFrame->personCount > toFrame->personCount ? 
                          fromFrame->personCount : toFrame->personCount;
    
    for (int p = 0; p < result->personCount; p++) {
        Person* destPerson = &result->persons[p];
        const Person* fromPerson = p < fromFrame->personCount ? &fromFrame->persons[p] : NULL;
        const Person* toPerson = p < toFrame->personCount ? &toFrame->persons[p] : NULL;
        
        if (!fromPerson && toPerson) {
            strncpy(destPerson->personId, toPerson->personId, 15);
            destPerson->personId[15] = '\0';
            destPerson->active = toPerson->active;
            destPerson->boneCount = toPerson->boneCount;
            for (int b = 0; b < toPerson->boneCount; b++) {
                destPerson->bones[b] = toPerson->bones[b];
            }
            continue;
        }
        
        if (fromPerson && !toPerson) {
            strncpy(destPerson->personId, fromPerson->personId, 15);
            destPerson->personId[15] = '\0';
            destPerson->active = fromPerson->active;
            destPerson->boneCount = fromPerson->boneCount;
            for (int b = 0; b < fromPerson->boneCount; b++) {
                destPerson->bones[b] = fromPerson->bones[b];
            }
            continue;
        }
        
        if (!fromPerson && !toPerson) continue;
        
        strncpy(destPerson->personId, fromPerson->personId, 15);
        destPerson->personId[15] = '\0';
        destPerson->active = fromPerson->active || toPerson->active;
        destPerson->boneCount = fromPerson->boneCount > toPerson->boneCount ? 
                               fromPerson->boneCount : toPerson->boneCount;
        
        for (int b = 0; b < destPerson->boneCount; b++) {
            Bone* destBone = &destPerson->bones[b];
            const Bone* fromBone = b < fromPerson->boneCount ? &fromPerson->bones[b] : NULL;
            const Bone* toBone = b < toPerson->boneCount ? &toPerson->bones[b] : NULL;
            
            if (!fromBone && toBone) {
                *destBone = *toBone;
                continue;
            }
            
            if (fromBone && !toBone) {
                *destBone = *fromBone;
                continue;
            }
            
            if (!fromBone && !toBone) continue;
            
            strncpy(destBone->name, fromBone->name, MAX_BONE_NAME_LENGTH - 1);
            destBone->name[MAX_BONE_NAME_LENGTH - 1] = '\0';
            
            if (fromBone->position.valid && toBone->position.valid) {
                destBone->position.position.x = fromBone->position.position.x * (1.0f - easedT) + 
                                               toBone->position.position.x * easedT;
                destBone->position.position.y = fromBone->position.position.y * (1.0f - easedT) + 
                                               toBone->position.position.y * easedT;
                destBone->position.position.z = fromBone->position.position.z * (1.0f - easedT) + 
                                               toBone->position.position.z * easedT;
                destBone->position.valid = true;
            } else if (fromBone->position.valid) {
                destBone->position = fromBone->position;
                destBone->position.confidence = fromBone->position.confidence * (1.0f - easedT);
            } else if (toBone->position.valid) {
                destBone->position = toBone->position;
                destBone->position.confidence = toBone->position.confidence * easedT;
            } else {
                destBone->position.valid = false;
            }
            
            destBone->position.confidence = (fromBone->position.confidence * (1.0f - easedT) + 
                                            toBone->position.confidence * easedT);
            
            destBone->size.x = fromBone->size.x * (1.0f - easedT) + toBone->size.x * easedT;
            destBone->size.y = fromBone->size.y * (1.0f - easedT) + toBone->size.y * easedT;
            
            float rotDiff = toBone->rotation - fromBone->rotation;
            if (rotDiff > 180.0f) rotDiff -= 360.0f;
            if (rotDiff < -180.0f) rotDiff += 360.0f;
            destBone->rotation = fromBone->rotation + rotDiff * easedT;
            
            destBone->visible = fromBone->visible || toBone->visible;
            destBone->textureIndex = toBone->textureIndex;
            destBone->mirrored = toBone->mirrored;
        }
    }
}

static void CaptureCurrentFrame(AnimatedCharacter* character) {
    if (!character || !character->animation.isLoaded) {
        g_hasValidFromFrame = false;
        return;
    }
    
    if (character->currentFrame < 0 || character->currentFrame >= character->animation.frameCount) {
        g_hasValidFromFrame = false;
        return;
    }
    
    const AnimationFrame* currentFrame = &character->animation.frames[character->currentFrame];
    CopyAnimationFrame(&g_transitionFromFrame, currentFrame);
    g_hasValidFromFrame = true;
}

static void StartAnimationTransition(void) {
    if (!g_hasValidFromFrame) {
        return;
    }
    
    g_isTransitioning = true;
    g_transitionTime = 0.0f;
}

void SetAnimationTransitionDuration(float duration) {
    if (duration > 0.0f && duration < 1.0f) {
        g_transitionDuration = duration;
    }
}

static void InterpolateBone(const Bone* boneA, const Bone* boneB, Bone* result, float t) {
    strncpy(result->name, boneA->name, MAX_BONE_NAME_LENGTH - 1);
    result->name[MAX_BONE_NAME_LENGTH - 1] = '\0';
    
    result->position.position.x = boneA->position.position.x * (1.0f - t) + boneB->position.position.x * t;
    result->position.position.y = boneA->position.position.y * (1.0f - t) + boneB->position.position.y * t;
    result->position.position.z = boneA->position.position.z * (1.0f - t) + boneB->position.position.z * t;
    
    result->position.valid = boneA->position.valid && boneB->position.valid;
    result->position.confidence = (boneA->position.confidence + boneB->position.confidence) * 0.5f;
    
    result->size.x = boneA->size.x * (1.0f - t) + boneB->size.x * t;
    result->size.y = boneA->size.y * (1.0f - t) + boneB->size.y * t;
    
    result->visible = boneA->visible || boneB->visible;
    result->textureIndex = boneA->textureIndex;
    result->rotation = boneA->rotation * (1.0f - t) + boneB->rotation * t;
    result->mirrored = boneA->mirrored;
}

static void InterpolatePerson(const Person* personA, const Person* personB, Person* result, float t) {
    strncpy(result->personId, personA->personId, 15);
    result->personId[15] = '\0';
    
    result->active = personA->active || personB->active;
    result->boneCount = personA->boneCount > personB->boneCount ? personA->boneCount : personB->boneCount;
    
    for (int i = 0; i < result->boneCount; i++) {
        if (i < personA->boneCount && i < personB->boneCount) {
            InterpolateBone(&personA->bones[i], &personB->bones[i], &result->bones[i], t);
        } else if (i < personA->boneCount) {
            result->bones[i] = personA->bones[i];
        } else {
            result->bones[i] = personB->bones[i];
        }
    }
}

bool BonesInterpolateFrames(BonesAnimation* animation, int frameA, int frameB, int framesToAdd) {
    if (!animation || frameA < 0 || frameB <= frameA || frameB >= animation->frameCount ||
        framesToAdd <= 0 || animation->frameCount + framesToAdd > animation->maxFrames) {
        return false;
    }
    
    for (int i = animation->frameCount - 1; i >= frameB; i--) {
        animation->frames[i + framesToAdd] = animation->frames[i];
        animation->frames[i + framesToAdd].frameNumber = i + framesToAdd;
    }
    
    AnimationFrame* srcA = &animation->frames[frameA];
    AnimationFrame* srcB = &animation->frames[frameB + framesToAdd];
    
    for (int i = 0; i < framesToAdd; i++) {
        int newFrameIndex = frameA + i + 1;
        float t = (float)(i + 1) / (float)(framesToAdd + 1);
        
        AnimationFrame* dest = &animation->frames[newFrameIndex];
        dest->frameNumber = newFrameIndex;
        dest->personCount = srcA->personCount > srcB->personCount ? srcA->personCount : srcB->personCount;
        dest->valid = true;
        
        for (int p = 0; p < dest->personCount; p++) {
            if (p < srcA->personCount && p < srcB->personCount) {
                InterpolatePerson(&srcA->persons[p], &srcB->persons[p], &dest->persons[p], t);
            } else if (p < srcA->personCount) {
                dest->persons[p] = srcA->persons[p];
            } else {
                dest->persons[p] = srcB->persons[p];
            }
        }
    }
    
    animation->frameCount += framesToAdd;
    
    return true;
}

bool BonesInsertEmptyFrame(BonesAnimation* animation, int position) {
    if (!animation || position < 0 || position > animation->frameCount ||
        animation->frameCount >= animation->maxFrames) {
        return false;
    }
    
    for (int i = animation->frameCount; i > position; i--) {
        animation->frames[i] = animation->frames[i - 1];
        animation->frames[i].frameNumber = i;
    }
    
    AnimationFrame* newFrame = &animation->frames[position];
    memset(newFrame, 0, sizeof(AnimationFrame));
    newFrame->frameNumber = position;
    newFrame->valid = true;
    
    animation->frameCount++;
    
    return true;
}

bool BonesCopyFrame(BonesAnimation* animation, int sourceFrame, int targetFrame) {
    if (!animation || sourceFrame < 0 || sourceFrame >= animation->frameCount ||
        targetFrame < 0 || targetFrame >= animation->maxFrames) {
        return false;
    }
    
    animation->frames[targetFrame] = animation->frames[sourceFrame];
    animation->frames[targetFrame].frameNumber = targetFrame;
    
    if (targetFrame >= animation->frameCount) {
        animation->frameCount = targetFrame + 1;
    }
    
    return true;
}

bool BonesCreateMissingFrames(BonesAnimation* animation) {
    if (!animation || !animation->isLoaded || animation->frameCount < 2) {
        return false;
    }

    int minFrame = animation->frames[0].frameNumber;
    int maxFrame = animation->frames[0].frameNumber;
    
    for (int i = 0; i < animation->frameCount; i++) {
        if (animation->frames[i].frameNumber < minFrame) minFrame = animation->frames[i].frameNumber;
        if (animation->frames[i].frameNumber > maxFrame) maxFrame = animation->frames[i].frameNumber;
    }
    
    int totalFramesNeeded = maxFrame - minFrame + 1;
    
    if (totalFramesNeeded > animation->maxFrames) {
        AnimationFrame* expandedFrames = (AnimationFrame*)realloc(animation->frames, 
                                                                   totalFramesNeeded * sizeof(AnimationFrame));
        if (!expandedFrames) {
            return false;
        }
        
        animation->frames = expandedFrames;
        animation->maxFrames = totalFramesNeeded;
        
        for (int i = animation->frameCount; i < totalFramesNeeded; i++) {
            memset(&animation->frames[i], 0, sizeof(AnimationFrame));
        }
    }
    
    AnimationFrame* tempFrames = (AnimationFrame*)calloc(totalFramesNeeded, sizeof(AnimationFrame));
    if (!tempFrames) {
        return false;
    }
    
    int framesCreated = 0;
    
    for (int frameNum = minFrame; frameNum <= maxFrame; frameNum++) {
        int targetIndex = frameNum - minFrame;
        
        int existingIndex = -1;
        for (int i = 0; i < animation->frameCount; i++) {
            if (animation->frames[i].frameNumber == frameNum) {
                existingIndex = i;
                break;
            }
        }
        
        if (existingIndex >= 0) {
            tempFrames[targetIndex] = animation->frames[existingIndex];
            tempFrames[targetIndex].isOriginalKeyframe = animation->frames[existingIndex].isOriginalKeyframe;
        } else {
            int prevFrameIdx = -1;
            int prevFrameNum = -1;
            for (int i = 0; i < animation->frameCount; i++) {
                if (animation->frames[i].frameNumber < frameNum) {
                    if (prevFrameNum < 0 || animation->frames[i].frameNumber > prevFrameNum) {
                        prevFrameIdx = i;
                        prevFrameNum = animation->frames[i].frameNumber;
                    }
                }
            }
            
            int nextFrameIdx = -1;
            int nextFrameNum = -1;
            for (int i = 0; i < animation->frameCount; i++) {
                if (animation->frames[i].frameNumber > frameNum) {
                    if (nextFrameNum < 0 || animation->frames[i].frameNumber < nextFrameNum) {
                        nextFrameIdx = i;
                        nextFrameNum = animation->frames[i].frameNumber;
                    }
                }
            }
            
            if (prevFrameIdx >= 0 && nextFrameIdx >= 0) {
                AnimationFrame* frameA = &animation->frames[prevFrameIdx];
                AnimationFrame* frameB = &animation->frames[nextFrameIdx];
                
                float t = (float)(frameNum - prevFrameNum) / (float)(nextFrameNum - prevFrameNum);
                t = t * t * (3.0f - 2.0f * t);
                
                AnimationFrame* destFrame = &tempFrames[targetIndex];
                memset(destFrame, 0, sizeof(AnimationFrame));
                
                destFrame->frameNumber = frameNum;
                destFrame->valid = true;
                destFrame->isOriginalKeyframe = false;
                destFrame->personCount = frameA->personCount > frameB->personCount ? 
                                        frameA->personCount : frameB->personCount;
                
                for (int p = 0; p < destFrame->personCount; p++) {
                    Person* destPerson = &destFrame->persons[p];
                    
                    if (p < frameA->personCount && p < frameB->personCount) {
                        Person* personA = &frameA->persons[p];
                        Person* personB = &frameB->persons[p];
                        
                        strncpy(destPerson->personId, personA->personId, 15);
                        destPerson->personId[15] = '\0';
                        destPerson->active = personA->active || personB->active;
                        destPerson->boneCount = personA->boneCount > personB->boneCount ? 
                                               personA->boneCount : personB->boneCount;
                        
                        for (int b = 0; b < destPerson->boneCount; b++) {
                            Bone* destBone = &destPerson->bones[b];
                            
                            if (b < personA->boneCount && b < personB->boneCount) {
                                Bone* boneA = &personA->bones[b];
                                Bone* boneB = &personB->bones[b];
                                
                                strncpy(destBone->name, boneA->name, MAX_BONE_NAME_LENGTH - 1);
                                destBone->name[MAX_BONE_NAME_LENGTH - 1] = '\0';
                                
                                destBone->position.position.x = boneA->position.position.x * (1.0f - t) + 
                                                               boneB->position.position.x * t;
                                destBone->position.position.y = boneA->position.position.y * (1.0f - t) + 
                                                               boneB->position.position.y * t;
                                destBone->position.position.z = boneA->position.position.z * (1.0f - t) + 
                                                               boneB->position.position.z * t;
                                
                                destBone->position.valid = boneA->position.valid && boneB->position.valid;
                                destBone->position.confidence = (boneA->position.confidence + boneB->position.confidence) * 0.5f;
                                
                                destBone->size.x = boneA->size.x * (1.0f - t) + boneB->size.x * t;
                                destBone->size.y = boneA->size.y * (1.0f - t) + boneB->size.y * t;
                                
                                float rotDiff = boneB->rotation - boneA->rotation;
                                if (rotDiff > 180.0f) rotDiff -= 360.0f;
                                if (rotDiff < -180.0f) rotDiff += 360.0f;
                                destBone->rotation = boneA->rotation + rotDiff * t;
                                
                                destBone->visible = boneA->visible || boneB->visible;
                                destBone->textureIndex = boneA->textureIndex;
                                destBone->mirrored = boneA->mirrored;
                            } else if (b < personA->boneCount) {
                                *destBone = personA->bones[b];
                            } else {
                                *destBone = personB->bones[b];
                            }
                        }
                    } else if (p < frameA->personCount) {
                        *destPerson = frameA->persons[p];
                    } else {
                        *destPerson = frameB->persons[p];
                    }
                }
                
                framesCreated++;
            } else if (prevFrameIdx >= 0) {
                tempFrames[targetIndex] = animation->frames[prevFrameIdx];
                tempFrames[targetIndex].frameNumber = frameNum;
                tempFrames[targetIndex].isOriginalKeyframe = false;
            } else if (nextFrameIdx >= 0) {
                tempFrames[targetIndex] = animation->frames[nextFrameIdx];
                tempFrames[targetIndex].frameNumber = frameNum;
                tempFrames[targetIndex].isOriginalKeyframe = false;
            }
        }
    }
    
    memcpy(animation->frames, tempFrames, totalFramesNeeded * sizeof(AnimationFrame));
    animation->frameCount = totalFramesNeeded;
    
    free(tempFrames);
    
    return true;
}

// ============================================================================
// ANIMATION LOADING AND PLAYBACK
// ============================================================================

bool LoadAnimation(AnimatedCharacter* character, const char* animationPath, const char* metadataPath) {
    if (!character) return false;

    CaptureCurrentFrame(character);

    if (character->animController) {
        AnimController_Free(character->animController);
        character->animController = NULL;
    }

    if (BonesLoadFromJSON(&character->animation, animationPath) != BONES_SUCCESS) {
        g_hasValidFromFrame = false;
        return false;
    }

    if (!BonesCreateMissingFrames(&character->animation)) {
    }

    character->maxFrames = BonesGetFrameCount(&character->animation);
    character->currentFrame = 0;
    BonesSetFrame(&character->animation, 0);

    StartAnimationTransition();

    character->animController = AnimController_Create(&character->animation, character->textureSets);
    if (!character->animController) {
        return false;
    }

    if (metadataPath) {
        if (AnimController_LoadClipMetadata(character->animController, metadataPath)) {
            const char* clipName = "default";
            if (character->animController->clipCount > 0) {
                clipName = character->animController->clips[0].name;
            }
            
            if (AnimController_PlayClip(character->animController, clipName)) {
            } else {
            }
        } else {
        }
    }

    character->forceUpdate = true;
    character->lastProcessedFrame = -1;

    return true;
}

void UpdateAnimatedCharacter(AnimatedCharacter* character, float deltaTime) {
    if (!character) return;

    if (character->animController && character->autoPlay) {
        AnimController_Update(character->animController, deltaTime);
        
        int frameFromController = AnimController_GetCurrentFrame(character->animController);
        if (frameFromController != character->currentFrame) {
            BonesSetFrame(&character->animation, frameFromController);
            character->currentFrame = frameFromController;
            character->forceUpdate = true;
        }
    }

    static AnimationFrame transitionFrame;
    bool usingTransition = false;
    
    if (g_isTransitioning) {
        g_transitionTime += deltaTime;
        float t = g_transitionTime / g_transitionDuration;
        
        if (t >= 1.0f) {
            g_isTransitioning = false;
        } else {
            if (character->animation.isLoaded && 
                character->currentFrame >= 0 && 
                character->currentFrame < character->animation.frameCount) {
                
                CopyAnimationFrame(&g_transitionToFrame, &character->animation.frames[character->currentFrame]);
                
                InterpolateTransitionFrames(&g_transitionFromFrame, &g_transitionToFrame, 
                                          &transitionFrame, t);
                usingTransition = true;
            } else {
                g_isTransitioning = false;
            }
        }
    }

    static AnimationFrame interpolatedFrame;
    bool usingInterpolation = false;
    
    if (!usingTransition && character->animation.isLoaded && character->autoPlay && 
        character->currentFrame >= 0 && character->currentFrame < character->animation.frameCount - 1) {
        
        AnimationFrame* currentFrame = &character->animation.frames[character->currentFrame];
        AnimationFrame* nextFrame = &character->animation.frames[character->currentFrame + 1];
        
        float t = 0.0f;
        if (character->animController && character->animController->currentClipIndex >= 0) {
            AnimationClipMetadata* clip = &character->animController->clips[character->animController->currentClipIndex];
            if (clip->fps > 0.0f) {
                float frameDuration = 1.0f / clip->fps;
                float timeInFrame = fmodf(character->animController->localTime, frameDuration);
                t = timeInFrame / frameDuration;
                
                t = t * t * (3.0f - 2.0f * t);
                
                if (t > 0.05f && t < 0.95f && currentFrame->valid && nextFrame->valid) {
                    memset(&interpolatedFrame, 0, sizeof(AnimationFrame));
                    interpolatedFrame.frameNumber = currentFrame->frameNumber;
                    interpolatedFrame.valid = true;
                    interpolatedFrame.personCount = currentFrame->personCount;
                    
                    for (int p = 0; p < interpolatedFrame.personCount; p++) {
                        Person* destPerson = &interpolatedFrame.persons[p];
                        Person* personA = &currentFrame->persons[p];
                        Person* personB = p < nextFrame->personCount ? &nextFrame->persons[p] : personA;
                        
                        strncpy(destPerson->personId, personA->personId, 15);
                        destPerson->personId[15] = '\0';
                        destPerson->active = personA->active;
                        destPerson->boneCount = personA->boneCount;
                        
                        for (int b = 0; b < destPerson->boneCount; b++) {
                            Bone* destBone = &destPerson->bones[b];
                            Bone* boneA = &personA->bones[b];
                            Bone* boneB = b < personB->boneCount ? &personB->bones[b] : boneA;
                            
                            strncpy(destBone->name, boneA->name, MAX_BONE_NAME_LENGTH - 1);
                            destBone->name[MAX_BONE_NAME_LENGTH - 1] = '\0';
                            
                            if (boneA->position.valid && boneB->position.valid) {
                                destBone->position.position.x = boneA->position.position.x * (1.0f - t) + 
                                                               boneB->position.position.x * t;
                                destBone->position.position.y = boneA->position.position.y * (1.0f - t) + 
                                                               boneB->position.position.y * t;
                                destBone->position.position.z = boneA->position.position.z * (1.0f - t) + 
                                                               boneB->position.position.z * t;
                                destBone->position.valid = true;
                            } else {
                                destBone->position = boneA->position;
                            }
                            
                            destBone->position.confidence = (boneA->position.confidence + boneB->position.confidence) * 0.5f;
                            
                            destBone->size.x = boneA->size.x * (1.0f - t) + boneB->size.x * t;
                            destBone->size.y = boneA->size.y * (1.0f - t) + boneB->size.y * t;
                            
                            float rotDiff = boneB->rotation - boneA->rotation;
                            if (rotDiff > 180.0f) rotDiff -= 360.0f;
                            if (rotDiff < -180.0f) rotDiff += 360.0f;
                            destBone->rotation = boneA->rotation + rotDiff * t;
                            
                            destBone->visible = boneA->visible;
                            destBone->textureIndex = boneA->textureIndex;
                            destBone->mirrored = boneA->mirrored;
                        }
                    }
                    
                    usingInterpolation = true;
                }
            }
        }
    }
    
    const AnimationFrame* frameToUse = NULL;
    
    if (usingTransition) {
        frameToUse = &transitionFrame;
    } else if (usingInterpolation) {
        frameToUse = &interpolatedFrame;
    } else if (character->animation.isLoaded && BonesIsValidFrame(&character->animation, character->currentFrame)) {
        frameToUse = &character->animation.frames[character->currentFrame];
    }

    if (frameToUse && frameToUse->valid) {
        Vector3 totalPos = {0, 0, 0};
        int validBoneCount = 0;

        for (int p = 0; p < frameToUse->personCount; p++) {
            const Person* person = &frameToUse->persons[p];
            if (!person->active) continue;

            Vector3 headPos = CalculateHeadPosition(person);
            Vector3 chestPos = CalculateChestPosition(person);
            Vector3 hipPos = CalculateHipPosition(person);

            if (Vector3Length(headPos) > 0.01f) { totalPos = Vector3Add(totalPos, headPos); validBoneCount++; }
            if (Vector3Length(chestPos) > 0.01f) { totalPos = Vector3Add(totalPos, chestPos); validBoneCount++; }
            if (Vector3Length(hipPos) > 0.01f) { totalPos = Vector3Add(totalPos, hipPos); validBoneCount++; }
        }

        if (validBoneCount > 0) {
            Vector3 newCenter = Vector3Scale(totalPos, 1.0f / validBoneCount);
            
            if (!character->autoCenterCalculated) {
                character->autoCenter = newCenter;
            } else {
                character->autoCenter = Vector3Lerp(character->autoCenter, newCenter, 0.3f);
            }
            
            character->autoCenterCalculated = true;
        }
    }

    if (character->forceUpdate || 
        character->currentFrame != character->lastProcessedFrame || 
        usingInterpolation || 
        usingTransition) {
        
        character->renderBonesCount = 0;
        character->renderHeadsCount = 0;
        character->renderTorsosCount = 0;

        AnimationFrame backupFrame;
        bool needsRestore = false;
        
        if (usingTransition || usingInterpolation) {
            backupFrame = character->animation.frames[character->currentFrame];
            character->animation.frames[character->currentFrame] = usingTransition ? transitionFrame : interpolatedFrame;
            needsRestore = true;
        }

        // MODIFICADO: Pasar textureSets a cada función
        CollectBonesForRendering(&character->animation, character->renderer->camera, 
                                &character->renderBones, &character->renderBonesCount,
                                &character->renderBonesCapacity, 
                                character->boneConfigs, character->boneConfigCount,
                                character->textureSets);  // <-- AÑADIDO
        
        if (character->renderHeadBillboards) {
            CollectHeadsForRendering(&character->animation, &character->renderHeads, 
                                   &character->renderHeadsCount, &character->renderHeadsCapacity,
                                   character->boneConfigs, character->boneConfigCount,
                                   character->textureSets);  // <-- AÑADIDO
        }

        if (character->renderTorsoBillboards) {
            CollectTorsosForRendering(&character->animation, &character->renderTorsos,
                                    &character->renderTorsosCount, &character->renderTorsosCapacity,
                                    character->boneConfigs, character->boneConfigCount,
                                    character->textureSets);  // <-- AÑADIDO
        }

        if (needsRestore) {
            character->animation.frames[character->currentFrame] = backupFrame;
        }

        character->lastProcessedFrame = character->currentFrame;
        character->forceUpdate = false;
    }
}

void SetCharacterFrame(AnimatedCharacter* character, int frame) {
    if (!character) return;
    
    if (frame >= 0 && frame < character->maxFrames) {
        character->currentFrame = frame;
        BonesSetFrame(&character->animation, frame);
        character->forceUpdate = true;
        
        if (character->animController) {
            character->animController->currentFrameInJSON = frame;
        }
    }
}

void SetCharacterAutoPlay(AnimatedCharacter* character, bool autoPlay) {
    if (character) {
        character->autoPlay = autoPlay;
    }
}

void SetCharacterBillboards(AnimatedCharacter* character, bool heads, bool torsos) {
    if (character) {
        character->renderHeadBillboards = heads;
        character->renderTorsoBillboards = torsos;
        character->forceUpdate = true;
    }
}

// ============================================================================
// BONE POSITION CALCULATIONS
// ============================================================================

Vector3 GetBonePositionByName(const Person* person, const char* boneName) {
    if (!person || !boneName) return (Vector3) { 0, 0, 0 };

if (strcmp(boneName, "FRONT_CALCULATED") == 0) {
    Vector3 head = GetBonePositionByName(person, "Nose");
    Vector3 neck = GetBonePositionByName(person, "Neck");

    Vector3 dir = Vector3Subtract(neck, head);
    dir.y = 0.0f;
    if (Vector3Length(dir) < 1e-6f) dir = (Vector3){0, 0, 1};
    dir = SafeNormalize(dir);

    const float offset = 10.15f;
    return Vector3Add(head, Vector3Scale(dir, -offset));
}
if (strcmp(boneName, "REAR_CALCULATED") == 0) {
    Vector3 head = GetBonePositionByName(person, "HEAD_CALCULATED");
    Vector3 neck = GetBonePositionByName(person, "Neck");

    Vector3 dir = Vector3Subtract(neck, head);
    dir.y = 0;
    dir = SafeNormalize(dir);

    Vector3 forward = (Vector3){ -dir.z, dir.y, dir.x };

    float offset = 10.15f;
    return Vector3Add(head, Vector3Scale(forward, offset));
}

    if (strcmp(boneName, "HEAD_CALCULATED") == 0) {
        return CalculateHeadPosition(person);
    }

    for (int i = 0; i < person->boneCount; i++) {
        const Bone* bone = &person->bones[i];
        if (bone->position.valid && strcmp(bone->name, boneName) == 0) {
            return bone->position.position;
        }
    }
    return (Vector3) { 0, 0, 0 };
}

Vector3 SafeNormalize(Vector3 v) {
    float length = Vector3Length(v);
    if (length < 1e-6f) return (Vector3) { 0, 0, 1 };
    return Vector3Scale(v, 1.0f / length);
}

static Vector3 GetStablePerpendicularVector(Vector3 forward) {
    forward = SafeNormalize(forward);

    Vector3 candidates[3] = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1}
    };

    Vector3 bestCandidate = candidates[1];
    float minDot = fabs(Vector3DotProduct(forward, candidates[1]));

    for (int i = 0; i < 3; i++) {
        float dot = fabs(Vector3DotProduct(forward, candidates[i]));
        if (dot < minDot) {
            minDot = dot;
            bestCandidate = candidates[i];
        }
    }

    return bestCandidate;
}

static Vector3 RotateVectorAroundAxis(Vector3 vector, Vector3 axis, float angle) {
    if (fabs(angle) < 1e-6f) return vector;

    float cosAngle = cosf(angle);
    float sinAngle = sinf(angle);
    float oneMinusCos = 1.0f - cosAngle;

    axis = SafeNormalize(axis);

    Vector3 result;
    result.x = vector.x * (cosAngle + axis.x * axis.x * oneMinusCos) +
        vector.y * (axis.x * axis.y * oneMinusCos - axis.z * sinAngle) +
        vector.z * (axis.x * axis.z * oneMinusCos + axis.y * sinAngle);

    result.y = vector.x * (axis.y * axis.x * oneMinusCos + axis.z * sinAngle) +
        vector.y * (cosAngle + axis.y * axis.y * oneMinusCos) +
        vector.z * (axis.y * axis.z * oneMinusCos - axis.x * sinAngle);

    result.z = vector.x * (axis.z * axis.x * oneMinusCos - axis.y * sinAngle) +
        vector.y * (axis.z * axis.y * oneMinusCos + axis.x * sinAngle) +
        vector.z * (cosAngle + axis.z * axis.z * oneMinusCos);

    return result;
}

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

    if (hasNose && eyeCount > 0 && (hasLEar || hasREar || hasNeck)) {
        Vector3 faceCenter = { 0, 0, 0 };
        int facePointCount = 0;

        faceCenter = Vector3Add(faceCenter, nosePos);
        faceCenter = Vector3Add(faceCenter, nosePos);
        facePointCount += 2;

        faceCenter = Vector3Add(faceCenter, eyeCenter);
        facePointCount++;

        Vector3 backReference;
        bool hasBackReference = false;

        if (hasLEar && hasREar) {
            backReference = Vector3Scale(Vector3Add(lEar, rEar), 0.5f);
            hasBackReference = true;
        }
        else if (hasLEar || hasREar) {
            backReference = hasLEar ? lEar : rEar;
            hasBackReference = true;
        }
        else if (hasNeck) {
            backReference = neckPos;
            hasBackReference = true;
        }

        if (hasBackReference) {
            faceCenter = Vector3Add(faceCenter, backReference);
            facePointCount++;
        }

        faceCenter = Vector3Scale(faceCenter, 1.0f / facePointCount);

        Vector3 frontDirection = { 0, 0, 1 };
        if (hasBackReference) {
            Vector3 noseToBack = Vector3Subtract(backReference, nosePos);
            float backDistance = Vector3Length(noseToBack);
            if (backDistance > 1e-6f) {
                frontDirection = Vector3Scale(noseToBack, 1.0f / backDistance);
            }
        }

        Vector3 headPos = Vector3Add(faceCenter, Vector3Scale(frontDirection, HEAD_DEPTH_OFFSET));

        Vector3 eyeToNose = Vector3Subtract(nosePos, eyeCenter);
        float verticalComponent = eyeToNose.y;

        float dynamicUpOffset = 0.03f;
        if (verticalComponent < -0.01f) {
            dynamicUpOffset = 0.015f;
        }
        else if (verticalComponent > 0.01f) {
            dynamicUpOffset = 0.045f;
        }

        headPos.y += dynamicUpOffset;

        return headPos;
    }

    if (eyeCount > 0 && hasNeck) {
        Vector3 headPos = {
            neckPos.x * 0.7f + eyeCenter.x * 0.3f,
            eyeCenter.y,
            hasNose ? neckPos.z * 0.8f + nosePos.z * 0.2f : neckPos.z * 0.9f + eyeCenter.z * 0.1f
        };

        headPos.z -= 0.01f;
        headPos.y += 0.03f;

        return headPos;
    }

    return (Vector3) { 0, 0, 0 };
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

    Vector3 hipCenter = { 0, 0, 0 };
    int hipPointCount = 0;

    if (cache.hasLHip) {
        hipCenter = Vector3Add(hipCenter, cache.lHip);
        hipPointCount++;
    }
    if (cache.hasRHip) {
        hipCenter = Vector3Add(hipCenter, cache.rHip);
        hipPointCount++;
    }

    hipCenter = Vector3Scale(hipCenter, 1.0f / hipPointCount);

    Vector3 hipTorsoCenter = { 0, 0, 0 };
    int hipTorsoCenterCount = 0;

    hipTorsoCenter = Vector3Add(hipTorsoCenter, hipCenter);
    hipTorsoCenter = Vector3Add(hipTorsoCenter, hipCenter);
    hipTorsoCenterCount += 2;

    if (cache.shoulderCount > 0) {
        Vector3 shoulderCenter = { 0, 0, 0 };
        int shoulderPointCount = 0;

        if (cache.hasLShoulder) {
            shoulderCenter = Vector3Add(shoulderCenter, cache.lShoulder);
            shoulderPointCount++;
        }
        if (cache.hasRShoulder) {
            shoulderCenter = Vector3Add(shoulderCenter, cache.rShoulder);
            shoulderPointCount++;
        }

        if (shoulderPointCount > 0) {
            shoulderCenter = Vector3Scale(shoulderCenter, 1.0f / shoulderPointCount);
            hipTorsoCenter = Vector3Add(hipTorsoCenter, shoulderCenter);
            hipTorsoCenterCount++;
        }
    }

    hipTorsoCenter = Vector3Scale(hipTorsoCenter, 1.0f / hipTorsoCenterCount);

    Vector3 hipDirection = { 0, 0, 1 };
    if (cache.shoulderCount > 0) {
        Vector3 shoulderCenter = { 0, 0, 0 };
        int shoulderPointCount = 0;

        if (cache.hasLShoulder) {
            shoulderCenter = Vector3Add(shoulderCenter, cache.lShoulder);
            shoulderPointCount++;
        }
        if (cache.hasRShoulder) {
            shoulderCenter = Vector3Add(shoulderCenter, cache.rShoulder);
            shoulderPointCount++;
        }

        if (shoulderPointCount > 0) {
            shoulderCenter = Vector3Scale(shoulderCenter, 1.0f / shoulderPointCount);
            Vector3 shoulderToHip = Vector3Subtract(hipCenter, shoulderCenter);
            float torsoLength = Vector3Length(shoulderToHip);
            if (torsoLength > 1e-6f) {
                hipDirection = Vector3Scale(shoulderToHip, 1.0f / torsoLength);
            }
        }
    }

    Vector3 hipPos = Vector3Add(hipTorsoCenter, Vector3Scale(hipDirection, 0.01f));

    Vector3 chestPos = CalculateChestPosition(person);
    if (Vector3Length(chestPos) > 0.0f) {
        Vector3 chestToOriginalHip = Vector3Subtract(hipCenter, chestPos);
        float torsoDistance = Vector3Length(chestToOriginalHip);

        if (torsoDistance > 0.12f) {
            Vector3 torsoDirection = Vector3Scale(chestToOriginalHip, 1.0f / torsoDistance);
            hipPos = Vector3Add(chestPos, Vector3Scale(torsoDirection, 0.10f));
        }
    }

    hipPos.y += HIP_OFFSET_Y;

    return hipPos;
}

// ============================================================================
// ORIENTATION CALCULATIONS
// ============================================================================

VirtualSpine CalculateVirtualSpine(const Person* person) {
    VirtualSpine spine = { 0 };
    if (!person || person->boneCount == 0) return spine;

    CachedBones cache = CacheBones(person);

    if (!cache.hasLShoulder || !cache.hasRShoulder || !cache.hasLHip || !cache.hasRHip) {
        return spine;
    }

    spine.chestPosition = CalculateChestPosition(person);
    spine.hipPosition = CalculateHipPosition(person);

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

    orientation.position = CalculateHeadPosition(person);

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

BoneOrientation CalculateBoneOrientation(const char* boneName, const Person* person, Vector3 bonePosition) {
    BoneOrientation orientation = { 0 };
    orientation.position = bonePosition;
    orientation.valid = false;

    if (!boneName || !person) return orientation;

    const char* primaryConn = NULL;
    const char* secondaryConn = NULL;
    bool reverseForward = false;

    for (int i = 0; BONE_ORIENTATIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_ORIENTATIONS[i].boneName, boneName) == 0) {
            primaryConn = BONE_ORIENTATIONS[i].primaryConnection;
            secondaryConn = BONE_ORIENTATIONS[i].secondaryConnection;
            reverseForward = BONE_ORIENTATIONS[i].reverseForward;
            break;
        }
    }

    Vector3 forward = { 0, 0, 1 };
    Vector3 up = { 0, 1, 0 };
    Vector3 right = { 1, 0, 0 };

    if (primaryConn && strlen(primaryConn) > 0) {
        Vector3 primaryPos = GetBonePositionByName(person, primaryConn);
        float primaryDistance = Vector3Length(Vector3Subtract(primaryPos, bonePosition));

        if (primaryDistance > 1e-4f) {
            forward = Vector3Subtract(primaryPos, bonePosition);
            if (reverseForward) {
                forward = Vector3Scale(forward, -1.0f);
            }
            forward = SafeNormalize(forward);

            if (secondaryConn && strlen(secondaryConn) > 0) {
                Vector3 secondaryPos = GetBonePositionByName(person, secondaryConn);
                float secondaryDistance = Vector3Length(Vector3Subtract(secondaryPos, bonePosition));

                if (secondaryDistance > 1e-4f) {
                    Vector3 toSecondary = Vector3Subtract(secondaryPos, bonePosition);
                    toSecondary = SafeNormalize(toSecondary);

                    right = Vector3CrossProduct(forward, toSecondary);
                    float rightLength = Vector3Length(right);

                    if (rightLength > 1e-4f) {
                        right = Vector3Scale(right, 1.0f / rightLength);
                        up = Vector3CrossProduct(right, forward);
                        up = SafeNormalize(up);
                    }
                    else {
                        Vector3 tempUp = GetStablePerpendicularVector(forward);
                        right = Vector3CrossProduct(forward, tempUp);
                        right = SafeNormalize(right);
                        up = Vector3CrossProduct(right, forward);
                        up = SafeNormalize(up);
                    }
                }
                else {
                    Vector3 tempUp = GetStablePerpendicularVector(forward);
                    right = Vector3CrossProduct(forward, tempUp);
                    right = SafeNormalize(right);
                    up = Vector3CrossProduct(right, forward);
                    up = SafeNormalize(up);
                }
            }
            else {
                Vector3 tempUp = GetStablePerpendicularVector(forward);
                right = Vector3CrossProduct(forward, tempUp);
                right = SafeNormalize(right);
                up = Vector3CrossProduct(right, forward);
                up = SafeNormalize(up);
            }
        }
    }

    for (int i = 0; BONE_ANGLE_OFFSETS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_ANGLE_OFFSETS[i].boneName, boneName) == 0) {
            float yawRad = BONE_ANGLE_OFFSETS[i].yawOffset * (PI / 180.0f);
            float pitchRad = BONE_ANGLE_OFFSETS[i].pitchOffset * (PI / 180.0f);
            float rollRad = BONE_ANGLE_OFFSETS[i].rollOffset * (PI / 180.0f);

            if (fabs(yawRad) > 1e-6f) {
                forward = RotateVectorAroundAxis(forward, up, yawRad);
                right = RotateVectorAroundAxis(right, up, yawRad);
            }

            if (fabs(pitchRad) > 1e-6f) {
                forward = RotateVectorAroundAxis(forward, right, pitchRad);
                up = RotateVectorAroundAxis(up, right, pitchRad);
            }

            if (fabs(rollRad) > 1e-6f) {
                right = RotateVectorAroundAxis(right, forward, rollRad);
                up = RotateVectorAroundAxis(up, forward, rollRad);
            }
            break;
        }
    }

    forward = SafeNormalize(forward);
    right = SafeNormalize(right);
    up = SafeNormalize(up);

    right = Vector3CrossProduct(forward, up);
    right = SafeNormalize(right);
    up = Vector3CrossProduct(right, forward);
    up = SafeNormalize(up);

    orientation.forward = forward;
    orientation.up = up;
    orientation.right = right;

    orientation.yaw = atan2f(forward.x, forward.z);
    float horizDistance = sqrtf(forward.x * forward.x + forward.z * forward.z);
    orientation.pitch = atan2f(-forward.y, fmaxf(horizDistance, 1e-6f));

    Vector3 worldUp = { 0, 1, 0 };
    Vector3 projectedRight = Vector3Subtract(right, Vector3Scale(forward, Vector3DotProduct(right, forward)));
    projectedRight = SafeNormalize(projectedRight);
    orientation.roll = atan2f(Vector3DotProduct(projectedRight, worldUp),
        Vector3DotProduct(projectedRight, Vector3CrossProduct(forward, worldUp)));

    orientation.valid = true;
    return orientation;
}

// ============================================================================
// RENDERING DATA COLLECTION
// ============================================================================

static bool ShouldUseVariableHeight(const char* boneName) {
    if (!boneName) return false;

    static const char* variableHeightBones[] = {
        "LShoulder", "LElbow", "RShoulder", "RElbow",
        "LHip", "LKnee", "RHip", "RKnee"
    };

    for (int i = 0; i < 8; i++) {
        if (strcmp(boneName, variableHeightBones[i]) == 0) return true;
    }
    return false;
}

static Vector3 CalculateBoneMidpoint(const char* boneName, const Person* person) {
    if (!person || !boneName) return (Vector3) { 0, 0, 0 };

    if (ShouldUseVariableHeight(boneName)) {
        return GetBonePositionByName(person, boneName);
    }

    if (strcmp(boneName, "Neck") == 0) {
        Vector3 originalNeck = GetBonePositionByName(person, "Neck");
        if (originalNeck.x == 0 && originalNeck.y == 0 && originalNeck.z == 0) return originalNeck;

        Vector3 calculatedHead = CalculateHeadPosition(person);
        if (calculatedHead.x == 0 && calculatedHead.y == 0 && calculatedHead.z == 0) return originalNeck;

        Vector3 lShoulder = GetBonePositionByName(person, "LShoulder");
        Vector3 rShoulder = GetBonePositionByName(person, "RShoulder");
        Vector3 shoulderCenter = originalNeck;

        if ((lShoulder.x || lShoulder.y || lShoulder.z) && (rShoulder.x || rShoulder.y || rShoulder.z)) {
            shoulderCenter = (Vector3){
                (lShoulder.x + rShoulder.x) * 0.5f,
                (lShoulder.y + rShoulder.y) * 0.5f,
                (lShoulder.z + rShoulder.z) * 0.5f
            };
        }

        return (Vector3) {
            (shoulderCenter.x + calculatedHead.x) * 0.5f,
            (shoulderCenter.y + calculatedHead.y) * 0.5f,
            (shoulderCenter.z + calculatedHead.z) * 0.5f
        };
    }

    const char* connectedBoneName = NULL;
    float projectionFactor = 1.0f;

    for (int i = 0; MIDPOINT_CONNECTIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(MIDPOINT_CONNECTIONS[i].boneName, boneName) == 0) {
            connectedBoneName = MIDPOINT_CONNECTIONS[i].connectedBone;
            projectionFactor = MIDPOINT_CONNECTIONS[i].projectionFactor;
            break;
        }
    }

    if (!connectedBoneName) return GetBonePositionByName(person, boneName);

    Vector3 bonePos = GetBonePositionByName(person, boneName);
    Vector3 connectedPos = GetBonePositionByName(person, connectedBoneName);

    if (strstr(boneName, "Wrist") && projectionFactor != 1.0f) {
        if ((bonePos.x || bonePos.y || bonePos.z) && (connectedPos.x || connectedPos.y || connectedPos.z)) {
            Vector3 forearmVector = {
                bonePos.x - connectedPos.x,
                bonePos.y - connectedPos.y,
                bonePos.z - connectedPos.z
            };
            return (Vector3) {
                connectedPos.x + forearmVector.x * projectionFactor,
                connectedPos.y + forearmVector.y * projectionFactor,
                connectedPos.z + forearmVector.z * projectionFactor
            };
        }
        return bonePos;
    }

    if (strstr(boneName, "Ankle") && strstr(connectedBoneName, "FOOT_FORWARD")) {
        if (bonePos.x || bonePos.y || bonePos.z) {
            Vector3 footPosition = bonePos;
            footPosition.z += 0.008f;
            footPosition.y -= 0.025f;
            return footPosition;
        }
        return bonePos;
    }

    if (!(bonePos.x || bonePos.y || bonePos.z)) return (Vector3) { 0, 0, 0 };
    if (!(connectedPos.x || connectedPos.y || connectedPos.z)) return bonePos;

    return (Vector3) {
        (bonePos.x + connectedPos.x) * 0.5f,
        (bonePos.y + connectedPos.y) * 0.5f,
        (bonePos.z + connectedPos.z) * 0.5f
    };
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

bool ShouldRenderChest(const Person* person) {
    if (!person || !person->active) return false;
    return CacheBones(person).shoulderCount >= 1;
}

bool ShouldRenderHip(const Person* person) {
    if (!person || !person->active) return false;
    return CacheBones(person).hipCount >= 1;
}

/*static void CollectRenderItems(const AnimationFrame* frame, void** items, int* itemCount, int* itemCapacity,
    size_t itemSize, bool isHead, bool isTorso, BoneConfig* boneConfigs, int boneConfigCount,
    bool (*shouldRender)(const Person*), Vector3 (*calcPosition)(const Person*),
    void (*calcOrientation)(const Person*, void*), const char* defaultTexture, float defaultSize) {
    (void)calcOrientation;
    *itemCount = 0;
    if (!frame) return;

    int estimatedCount = frame->personCount * (isTorso ? 2 : 1);
    if (*itemCapacity < estimatedCount) {
        void* newArray = realloc(*items, itemSize * estimatedCount);
        if (!newArray) return;
        *items = newArray;
        *itemCapacity = estimatedCount;
    }

    static char processed[200][25];
    int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!shouldRender(person)) continue;

        char key[25];
        snprintf(key, sizeof(key), "%s_%s", person->personId, isTorso ? (isHead ? "head" : "chest") : "head");

        bool alreadyProcessed = false;
        for (int i = 0; i < processedCount; i++) {
            if (strcmp(processed[i], key) == 0) {
                alreadyProcessed = true;
                break;
            }
        }
        if (alreadyProcessed) continue;

        if (processedCount < 200) {
            strncpy(processed[processedCount], key, 24);
            processed[processedCount][24] = '\0';
            processedCount++;
        }

        void* item = (char*)(*items) + (*itemCount * itemSize);
        memset(item, 0, itemSize);

        if (isHead) {
            HeadRenderData* head = (HeadRenderData*)item;
            head->position = calcPosition(person);
            head->orientation = CalculateHeadOrientation(person);
            head->valid = true;
            head->visible = true;

            bool textureFound = false;
            if (g_textureSets && g_textureSets->loaded) {
                const char* activeTexture = BonesTextureSets_GetActiveTexture(g_textureSets, "Head");
                if (activeTexture) {
                    strncpy(head->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
                    head->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                    textureFound = true;
                }
            }
            
            if (!textureFound) {
                BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, "Head");
                if (config) {
                    strncpy(head->texturePath, config->texturePath, MAX_FILE_PATH_LENGTH - 1);
                    head->size = config->size;
                } else {
                    strncpy(head->texturePath, defaultTexture, MAX_FILE_PATH_LENGTH - 1);
                    head->size = defaultSize;
                }
                head->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
            } else {
                BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, "Head");
                head->size = config ? config->size : defaultSize;
            }

            strncpy(head->personId, person->personId, 15);
            head->personId[15] = '\0';
        } else {
            TorsoRenderData* torso = (TorsoRenderData*)item;
            torso->position = calcPosition(person);
            torso->type = isTorso ? TORSO_CHEST : TORSO_HIP;
            torso->person = person;
            torso->valid = true;
            torso->visible = true;

            if (isTorso) {
                torso->orientation = CalculateChestOrientation(person);
            } else {
                torso->orientation = CalculateHipOrientation(person);
            }

            BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, isTorso ? "Chest" : "Hip");
            if (config) {
                strncpy(torso->texturePath, config->texturePath, MAX_FILE_PATH_LENGTH - 1);
                torso->size = config->size;
            } else {
                strncpy(torso->texturePath, defaultTexture, MAX_FILE_PATH_LENGTH - 1);
                torso->size = defaultSize;
            }
            torso->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';

            strncpy(torso->personId, person->personId, 15);
            torso->personId[15] = '\0';
        }

        (*itemCount)++;
    }
}*/

void CollectHeadsForRendering(const BonesAnimation* animation, HeadRenderData** heads,
    int* headCount, int* headCapacity, BoneConfig* boneConfigs, int boneConfigCount,
    TextureSetCollection* textureSets) {  // <-- AÑADIDO PARÁMETRO
    
    if (!animation->isLoaded) {
        *headCount = 0;
        return;
    }

    int currentFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, currentFrame)) {
        *headCount = 0;
        return;
    }

    const AnimationFrame* frame = &animation->frames[currentFrame];
    
    *headCount = 0;
    int estimatedHeads = frame->personCount;
    
    if (*headCapacity < estimatedHeads) {
        HeadRenderData* newArray = (HeadRenderData*)realloc(*heads, sizeof(HeadRenderData) * estimatedHeads);
        if (!newArray) return;
        *heads = newArray;
        *headCapacity = estimatedHeads;
    }

    static char processedHeads[200][25];
    int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!ShouldRenderHead(person)) continue;

        char headKey[25];
        snprintf(headKey, sizeof(headKey), "%s_head", person->personId);

        bool alreadyProcessed = false;
        for (int i = 0; i < processedCount; i++) {
            if (strcmp(processedHeads[i], headKey) == 0) {
                alreadyProcessed = true;
                break;
            }
        }
        if (alreadyProcessed) continue;

        if (processedCount < 200) {
            strncpy(processedHeads[processedCount], headKey, 24);
            processedHeads[processedCount][24] = '\0';
            processedCount++;
        }

        HeadRenderData* head = &(*heads)[*headCount];
        memset(head, 0, sizeof(HeadRenderData));
        
        head->position = CalculateHeadPosition(person);
        head->orientation = CalculateHeadOrientation(person);
        head->valid = true;
        head->visible = true;

        bool textureFound = false;
        // MODIFICADO: Usar textureSets pasado como parámetro
        if (textureSets && textureSets->loaded) {
            const char* activeTexture = BonesTextureSets_GetActiveTexture(textureSets, "Head");
            if (activeTexture) {
                strncpy(head->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
                head->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                textureFound = true;
            }
        }
        
        if (!textureFound) {
            BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, "Head");
            if (config) {
                strncpy(head->texturePath, config->texturePath, MAX_FILE_PATH_LENGTH - 1);
                head->size = config->size;
            } else {
                strncpy(head->texturePath, "data/textures/hil/Head.png", MAX_FILE_PATH_LENGTH - 1);
                head->size = 0.25f;
            }
            head->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
        } else {
            BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, "Head");
            head->size = config ? config->size : 0.25f;
        }

        strncpy(head->personId, person->personId, 15);
        head->personId[15] = '\0';

        (*headCount)++;
    }
}

void CollectTorsosForRendering(const BonesAnimation* animation, TorsoRenderData** torsos,
    int* torsoCount, int* torsoCapacity, BoneConfig* boneConfigs, int boneConfigCount,
    TextureSetCollection* textureSets) {  // <-- AÑADIDO PARÁMETRO

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
                torsoData->person = person;

                if (!torsoData->orientation.valid && Vector3Length(torsoData->position) < 1e-6f) {
                    continue;
                }

                torsoData->valid = true;
                torsoData->visible = true;

                // MODIFICADO: Intentar obtener textura del textureSets
                bool textureFound = false;
                if (textureSets && textureSets->loaded) {
                    const char* activeTexture = BonesTextureSets_GetActiveTexture(textureSets, "Chest");
                    if (activeTexture) {
                        strncpy(torsoData->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
                        torsoData->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                        textureFound = true;
                    }
                }

                if (!textureFound) {
                    BoneConfig* chestConfig = FindBoneConfig(boneConfigs, boneConfigCount, "Chest");
                    if (chestConfig) {
                        strncpy(torsoData->texturePath, chestConfig->texturePath, MAX_FILE_PATH_LENGTH - 1);
                        torsoData->size = chestConfig->size;
                        torsoData->visible = chestConfig->visible;
                    } else {
                        strncpy(torsoData->texturePath, "tex/Chest.png", MAX_FILE_PATH_LENGTH - 1);
                        torsoData->size = 0.4f;
                        torsoData->visible = true;
                    }
                    torsoData->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                } else {
                    BoneConfig* chestConfig = FindBoneConfig(boneConfigs, boneConfigCount, "Chest");
                    torsoData->size = chestConfig ? chestConfig->size : 0.4f;
                    torsoData->visible = chestConfig ? chestConfig->visible : true;
                }

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
                torsoData->person = person;

                if (!torsoData->orientation.valid && Vector3Length(torsoData->position) < 1e-6f) {
                    continue;
                }

                torsoData->valid = true;
                torsoData->visible = true;

                // MODIFICADO: Intentar obtener textura del textureSets
                bool textureFound = false;
                if (textureSets && textureSets->loaded) {
                    const char* activeTexture = BonesTextureSets_GetActiveTexture(textureSets, "Hip");
                    if (activeTexture) {
                        strncpy(torsoData->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
                        torsoData->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                        textureFound = true;
                    }
                }

                if (!textureFound) {
                    BoneConfig* hipConfig = FindBoneConfig(boneConfigs, boneConfigCount, "Hip");
                    if (hipConfig) {
                        strncpy(torsoData->texturePath, hipConfig->texturePath, MAX_FILE_PATH_LENGTH - 1);
                        torsoData->size = hipConfig->size;
                        torsoData->visible = hipConfig->visible;
                    } else {
                        strncpy(torsoData->texturePath, "tex/Hip.png", MAX_FILE_PATH_LENGTH - 1);
                        torsoData->size = 0.35f;
                        torsoData->visible = true;
                    }
                    torsoData->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                } else {
                    BoneConfig* hipConfig = FindBoneConfig(boneConfigs, boneConfigCount, "Hip");
                    torsoData->size = hipConfig ? hipConfig->size : 0.35f;
                    torsoData->visible = hipConfig ? hipConfig->visible : true;
                }

                strncpy(torsoData->personId, person->personId, 15);
                torsoData->personId[15] = '\0';

                (*torsoCount)++;
            }
        }
    }
}

bool ResizeRenderBonesArray(BoneRenderData** renderBones, int* renderBonesCapacity, int newCapacity) {
    if (newCapacity <= 0 || newCapacity > 10000 || newCapacity <= *renderBonesCapacity) return true;

    BoneRenderData* newArray = realloc(*renderBones, sizeof(BoneRenderData) * newCapacity);
    if (!newArray) return false;

    memset(newArray + *renderBonesCapacity, 0, sizeof(BoneRenderData) * (newCapacity - *renderBonesCapacity));
    *renderBones = newArray;
    *renderBonesCapacity = newCapacity;
    return true;
}

void CollectBonesForRendering(const BonesAnimation* animation, Camera camera, BoneRenderData** renderBones,
    int* renderBonesCount, int* renderBonesCapacity, BoneConfig* boneConfigs, int boneConfigCount,
    TextureSetCollection* textureSets) {  // <-- AÑADIDO PARÁMETRO
    
    *renderBonesCount = 0;
    if (!animation->isLoaded) return;

    int currentFrame = BonesGetCurrentFrame(animation);
    if (!BonesIsValidFrame(animation, currentFrame)) return;

    const AnimationFrame* frame = &animation->frames[currentFrame];

    int estimatedBones = 0;
    for (int p = 0; p < frame->personCount; p++) {
        if (frame->persons[p].active) estimatedBones += frame->persons[p].boneCount;
    }

    if (!ResizeRenderBonesArray(renderBones, renderBonesCapacity, estimatedBones + 10)) return;

    static char processedBones[2000][MAX_BONE_NAME_LENGTH + 20];
    int processedCount = 0;

    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;

        for (int b = 0; b < person->boneCount; b++) {
            const Bone* bone = &person->bones[b];
            if (!bone->position.valid || !bone->visible || !BonesIsPositionValid(bone->position.position)) continue;

            BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, bone->name);
            if (!config) {
                char firstChar = bone->name[0];
                bool isKnown = (firstChar == 'N' && strstr(bone->name, "Nose")) ||
                    ((firstChar == 'L' || firstChar == 'R') &&
                        (strstr(bone->name, "Eye") || strstr(bone->name, "Ear") ||
                            strstr(bone->name, "Shoulder") || strstr(bone->name, "Elbow") ||
                            strstr(bone->name, "Wrist") || strstr(bone->name, "Hip") ||
                            strstr(bone->name, "Knee") || strstr(bone->name, "Ankle"))) ||
                    (firstChar == 'H' && (strstr(bone->name, "Head") || strstr(bone->name, "Hip"))) ||
                    (firstChar == 'C' && strstr(bone->name, "Chest")) ||
                    (firstChar == 'S' && (strstr(bone->name, "Shoulder") || strstr(bone->name, "Spine")));

                if (!isKnown) continue;
            }
            else if (!config->visible) continue;

            char boneKey[MAX_BONE_NAME_LENGTH + 20];
            int keyLen = snprintf(boneKey, sizeof(boneKey), "%s_%s", person->personId, bone->name);

            bool alreadyProcessed = false;
            for (int i = 0; i < processedCount; i++) {
                if (memcmp(processedBones[i], boneKey, keyLen + 1) == 0) {
                    alreadyProcessed = true;
                    break;
                }
            }

            if (alreadyProcessed) continue;

            if (processedCount < 2000) {
                memcpy(processedBones[processedCount], boneKey, keyLen + 1);
                processedCount++;
            }

            Vector3 midpointPos = CalculateBoneMidpoint(bone->name, person);
            if (!BonesIsPositionValid(midpointPos)) continue;

            float distance = Vector3Distance(camera.position, midpointPos);
            if (distance > 50.0f) continue;

            BoneRenderData* renderBone = &(*renderBones)[*renderBonesCount];
            renderBone->position = midpointPos;
            renderBone->distance = distance;
            renderBone->valid = true;

            // MODIFICADO: Intentar obtener textura del textureSets primero
            bool textureFound = false;
            if (textureSets && textureSets->loaded) {
                const char* activeTexture = BonesTextureSets_GetActiveTexture(textureSets, bone->name);
                if (activeTexture) {
                    strncpy(renderBone->texturePath, activeTexture, MAX_FILE_PATH_LENGTH - 1);
                    renderBone->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                    textureFound = true;
                }
            }

            if (!textureFound) {
                if (config) {
                    strncpy(renderBone->texturePath, config->texturePath, MAX_FILE_PATH_LENGTH - 1);
                    renderBone->visible = config->visible;
                    renderBone->size = config->size;
                } else {
                    strncpy(renderBone->texturePath, GetTexturePathForBone(boneConfigs, boneConfigCount, bone->name, textureSets), MAX_FILE_PATH_LENGTH - 1);
                    renderBone->visible = IsBoneVisible(boneConfigs, boneConfigCount, bone->name);
                    renderBone->size = GetBoneSize(boneConfigs, boneConfigCount, bone->name);
                }
                renderBone->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
            } else {
                if (config) {
                    renderBone->visible = config->visible;
                    renderBone->size = config->size;
                } else {
                    renderBone->visible = true;
                    renderBone->size = 0.35f;
                }
            }

            strncpy(renderBone->boneName, bone->name, MAX_BONE_NAME_LENGTH - 1);
            renderBone->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
            strncpy(renderBone->personId, person->personId, 15);
            renderBone->personId[15] = '\0';

            EnrichBoneRenderDataWithOrientation(renderBone, person);
            (*renderBonesCount)++;
        }
    }

    if (*renderBonesCount > 1) {
        for (int i = 0; i < *renderBonesCount - 1; i++) {
            for (int j = 0; j < *renderBonesCount - i - 1; j++) {
                if ((*renderBones)[j].distance < (*renderBones)[j + 1].distance) {
                    BoneRenderData temp = (*renderBones)[j];
                    (*renderBones)[j] = (*renderBones)[j + 1];
                    (*renderBones)[j + 1] = temp;
                }
            }
        }
    }
}

void EnrichBoneRenderDataWithOrientation(BoneRenderData* renderBone, const Person* person) {
    if (!renderBone || !person) return;

    BoneOrientation orientation = CalculateBoneOrientation(renderBone->boneName, person, renderBone->position);

    if (orientation.valid) {
        renderBone->orientation.position = orientation.position;
        renderBone->orientation.forward = orientation.forward;
        renderBone->orientation.up = orientation.up;
        renderBone->orientation.right = orientation.right;
        renderBone->orientation.yaw = orientation.yaw;
        renderBone->orientation.pitch = orientation.pitch;
        renderBone->orientation.roll = orientation.roll;
        renderBone->orientation.valid = true;
    }
    else {
        renderBone->orientation.position = renderBone->position;
        renderBone->orientation.forward = (Vector3){ 0, 0, 1 };
        renderBone->orientation.up = (Vector3){ 0, 1, 0 };
        renderBone->orientation.right = (Vector3){ 1, 0, 0 };
        renderBone->orientation.yaw = 0.0f;
        renderBone->orientation.pitch = 0.0f;
        renderBone->orientation.roll = 0.0f;
        renderBone->orientation.valid = true;
    }
}

// ============================================================================
// RENDERING CALCULATIONS
// ============================================================================

BoneRenderData* FindRenderBoneByName(BoneRenderData* bones, int boneCount, const char* boneName) {
    for (int i = 0; i < boneCount; i++) {
        if (strcmp(bones[i].boneName, boneName) == 0) {
            return &bones[i];
        }
    }
    return NULL;
}

BoneConnectionPositions GetBoneConnectionPositionsEx(const BoneRenderData* boneData, const Person* person) {
    BoneConnectionPositions result = { 0 };

    if (!boneData || !boneData->valid) return result;

    result.pos0 = boneData->position;
    result.pos1 = result.pos0;

    for (int i = 0; BONE_CONNECTIONS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_CONNECTIONS[i].boneName, boneData->boneName) != 0) continue;

        for (int k = 1; k < 3; k++) {
            const char* neighborName = BONE_CONNECTIONS[i].connections[k];
            if (!neighborName || neighborName[0] == '\0') continue;
            if (strcmp(neighborName, boneData->boneName) == 0) continue;

            if (person) {
                Vector3 p = GetBonePositionByName(person, neighborName);
                if (Vector3Length(p) > 1e-6f) {
                    result.pos1 = p;
                    return result;
                }
            }
        }
        return result;
    }
    return result;
}

void CalculateLimbBoneRenderData(const BoneRenderData* boneData, const Person* person, Camera camera, int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!boneData->orientation.valid) {
        CalculateHandBoneRenderData(boneData->position, camera, outChosenIndex, outRotation, outMirrored, boneData->boneName);
        return;
    }

    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},
        {2,12,13,14,15,14,13,12},
        {1,8,9,10,11,10,9,8}
    };

    Vector3 camDir = Vector3Subtract(boneData->position, camera.position);
    camDir = SafeNormalize(camDir);

    Vector3 localCamDir = {
        Vector3DotProduct(camDir, boneData->orientation.right),
        Vector3DotProduct(camDir, boneData->orientation.up),
        Vector3DotProduct(camDir, boneData->orientation.forward)
    };

    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;

    float localPitchDeg = atan2f(localCamDir.y,
        sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z)) * RAD2DEG;

    bool shouldInvertPitch = false;
    if (boneData->boneName[0] != '\0') {
        if (strcmp(boneData->boneName, "Neck") != 0 &&
            strcmp(boneData->boneName, "RShoulder") != 0 &&
            strcmp(boneData->boneName, "RHip") != 0 && 
            strcmp(boneData->boneName, "RElbow") != 0 &&
            strcmp(boneData->boneName, "RKnee") != 0 &&
            strcmp(boneData->boneName, "LAnkle") != 0) {
            shouldInvertPitch = true;
        }
    }
    if (shouldInvertPitch) {
        localPitchDeg = -localPitchDeg;
    }

    if (person) {
        BoneConnectionPositions p = GetBoneConnectionPositionsEx(boneData, person);

        Vector3 camF = Vector3Subtract(camera.target, camera.position);
        camF = SafeNormalize(camF);

        Vector3 camR = Vector3CrossProduct(camF, camera.up);
        camR = SafeNormalize(camR);

        Vector3 camU = Vector3CrossProduct(camR, camF);
        camU = SafeNormalize(camU);

        Vector3 boneVec = Vector3Subtract(p.pos1, p.pos0);
        float boneLen = Vector3Length(boneVec);
        if (boneLen > 1e-6f) {
            boneVec = Vector3Scale(boneVec, 1.0f / boneLen);

            Vector3 mid = {
                (p.pos0.x + p.pos1.x) * 0.5f,
                (p.pos0.y + p.pos1.y) * 0.5f,
                (p.pos0.z + p.pos1.z) * 0.5f
            };

            Vector3 toCam = Vector3Subtract(camera.position, mid);
            float toCamLen = Vector3Length(toCam);
            if (toCamLen > 1e-6f) {
                toCam = Vector3Scale(toCam, 1.0f / toCamLen);

                float d = Vector3DotProduct(boneVec, toCam);
                if (d > 1.0f) d = 1.0f;
                if (d < -1.0f) d = -1.0f;
                float angleBetweenDeg = acosf(d) * RAD2DEG;

                Vector3 boneInCam = {
                    Vector3DotProduct(boneVec, camR),
                    Vector3DotProduct(boneVec, camU),
                    Vector3DotProduct(boneVec, camF)
                };

                float horizLen = sqrtf(boneInCam.x * boneInCam.x + boneInCam.z * boneInCam.z);
                float bonePitchInCameraDeg = atan2f(boneInCam.y, horizLen) * RAD2DEG;

                float compensationFactor = 0.0f;

                if (boneData->boneName[0] != '\0') {
                    if (strstr(boneData->boneName, "Hip") != NULL) {
                        compensationFactor = 0.25f;
                    }
                    else if (strstr(boneData->boneName, "Knee") != NULL) {
                        compensationFactor = 0.25f;
                    }
                    else if (strstr(boneData->boneName, "Shoulder") != NULL) {
                        compensationFactor = 0.15f;
                    }
                    else if (strstr(boneData->boneName, "Elbow") != NULL) {
                        compensationFactor = 0.15f;
                    }
                }

                if (compensationFactor > 0.0f && angleBetweenDeg > 45.0f) {
                    float angleIntensity = (angleBetweenDeg - 45.0f) / 45.0f;
                    angleIntensity = fminf(angleIntensity, 1.0f);

                    float compensation = bonePitchInCameraDeg * compensationFactor * angleIntensity;
                    float maxCompensation = 12.0f;
                    compensation = fmaxf(fminf(compensation, maxCompensation), -maxCompensation);
                    localPitchDeg += compensation;
                }
            }
        }
    }

    float normalizedYaw = localYaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    int sector = (int)(normalizedYaw / 45.0f) % 8;

    float rotationCompensation = 0.0f;

    if (localPitchDeg >= 70.5f) {
        *outChosenIndex = 3;
        *outRotation = 0.0f;
        *outMirrored = false;
        return;
    }
    else if (localPitchDeg <= -60.0f) {
        *outChosenIndex = 15;
        *outRotation = (8 - sector) * 45.0f + 180.0f;
        *outMirrored = true;
        return;
    }
    else {
        int row = (localPitchDeg >= 22.5f) ? 2 : (localPitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[row][sector];
        *outRotation = 0.0f;
        *outMirrored = (sector >= 1 && sector <= 4);

        if (boneData->boneName[0] == 'R') {
            *outMirrored = !(*outMirrored);
        }

        if (person) {
            BoneConnectionPositions p = GetBoneConnectionPositionsEx(boneData, person);
            Vector3 boneVec = Vector3Subtract(p.pos1, p.pos0);
            float boneLen = Vector3Length(boneVec);

            if (boneLen > 1e-6f) {
                boneVec = SafeNormalize(boneVec);

                Vector3 verticalUp = { 0, 1, 0 };
                float dotWithVertical = Vector3DotProduct(boneVec, verticalUp);
                float angleFromVerticalDeg = acosf(fabs(dotWithVertical)) * RAD2DEG;

                float diagonalThreshold = 30.0f;

                if (angleFromVerticalDeg > diagonalThreshold) {
                    Vector3 camToPos = Vector3Subtract(boneData->position, camera.position);
                    camToPos = SafeNormalize(camToPos);

                    Vector3 projectedBone = Vector3Subtract(boneVec,
                        Vector3Scale(camToPos, Vector3DotProduct(boneVec, camToPos)));
                    float projLen = Vector3Length(projectedBone);

                    if (projLen > 1e-4f) {
                        projectedBone = SafeNormalize(projectedBone);

                        Vector3 camRight = Vector3Normalize(Vector3CrossProduct(
                            Vector3Subtract(camera.target, camera.position), camera.up));
                        Vector3 camUp = Vector3Normalize(Vector3CrossProduct(camRight,
                            Vector3Subtract(camera.target, camera.position)));

                        float rightDot = Vector3DotProduct(projectedBone, camRight);
                        float upDot = Vector3DotProduct(projectedBone, camUp);

                        rotationCompensation = atan2f(rightDot, upDot) * RAD2DEG;

                        float diagonalIntensity = (angleFromVerticalDeg - diagonalThreshold) / (90.0f - diagonalThreshold);
                        diagonalIntensity = fminf(diagonalIntensity, 1.0f);

                        float rotationFactor = 0.0f;
                        if (boneData->boneName[0] != '\0') {
                            if (strstr(boneData->boneName, "LHip") != NULL) {
                                rotationFactor = -0.6f;
                            }
                            else if (strstr(boneData->boneName, "RHip") != NULL) {
                                rotationFactor = -0.6f;
                            }
                            else if (strstr(boneData->boneName, "Knee") != NULL) {
                                rotationFactor = 0.5f;
                            }
                            else if (strstr(boneData->boneName, "Shoulder") != NULL) {
                                rotationFactor = 0.3f;
                            }
                            else if (strstr(boneData->boneName, "Elbow") != NULL) {
                                rotationFactor = 0.3f;
                            }
                        }

                        rotationCompensation *= rotationFactor * diagonalIntensity;

                        float maxRotationCompensation = 45.0f;
                        rotationCompensation = fmaxf(fminf(rotationCompensation, maxRotationCompensation),
                            -maxRotationCompensation);
                    }
                }
            }
        }
    }

    /*if (strcmp(boneData->boneName, "Neck") == 0 && person) {
        Vector3 headPos = CalculateHeadPosition(person);
        Vector3 neckToHead = Vector3Subtract(headPos, boneData->position);
        if (Vector3Length(neckToHead) > 0.001f) {
            Vector3 camRight = Vector3Normalize(Vector3CrossProduct(
                Vector3Subtract(camera.target, camera.position), camera.up));
            Vector3 camUp = Vector3Normalize(Vector3CrossProduct(camRight,
                Vector3Subtract(camera.target, camera.position)));

            neckToHead = SafeNormalize(neckToHead);
            rotationCompensation += atan2f(Vector3DotProduct(neckToHead, camRight),
                Vector3DotProduct(neckToHead, camUp)) * RAD2DEG;
        }
    }*/

if (strcmp(boneData->boneName, "Neck") == 0) {
    Vector3 estimatedHeadPos = Vector3Add(boneData->position, 
                                         Vector3Scale(boneData->orientation.up, 0.15f));
    
    Vector3 direction = Vector3Subtract(estimatedHeadPos, boneData->position);
    
    if (Vector3Length(direction) > 0.001f) {
        direction = SafeNormalize(direction);
        
        Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
        Vector3 camUp = Vector3Normalize(Vector3CrossProduct(camRight, camForward));
        
        Vector3 projectedDir = Vector3Subtract(direction, 
            Vector3Scale(camForward, Vector3DotProduct(direction, camForward)));
        
        if (Vector3Length(projectedDir) > 0.001f) {
            projectedDir = SafeNormalize(projectedDir);
            
            float angle = atan2f(Vector3DotProduct(projectedDir, camRight),
                               Vector3DotProduct(projectedDir, camUp)) * RAD2DEG;
            angle = fmodf(angle + 180.0f, 360.0f);
            *outRotation = angle;
            

        }
    }
}


    if ((sector == 0 || sector == 4) &&
        localPitchDeg < 60.0f && localPitchDeg > -60.0f) {

        rotationCompensation *= 0.01f;

        if (boneData->boneName[0] != '\0') {
            if (strcmp(boneData->boneName, "LShoulder") == 0 ||
                strcmp(boneData->boneName, "RShoulder") == 0 ||
                strcmp(boneData->boneName, "LElbow") == 0 ||
                strcmp(boneData->boneName, "RElbow") == 0 ||
                strcmp(boneData->boneName, "LHip") == 0 ||
                strcmp(boneData->boneName, "RHip") == 0 ||
                strcmp(boneData->boneName, "LKnee") == 0 ||
                strcmp(boneData->boneName, "RKnee") == 0) {
                rotationCompensation = 0.0f;
            }
        }
    }

    if (((sector >= 7 || sector <= 1) || (sector >= 3 && sector <= 5)) &&
        localPitchDeg < 45.0f && localPitchDeg > -45.0f) {
        rotationCompensation *= 0.3f;
    }

    *outRotation += rotationCompensation;

    while (*outRotation >= 360.0f) *outRotation -= 360.0f;
    while (*outRotation < 0.0f) *outRotation += 360.0f;
}

bool IsWristBone(const char* boneName) {
    if (!boneName) return false;
    return (strcmp(boneName, "LWrist") == 0) || (strcmp(boneName, "RWrist") == 0);
}

void CalculateHandBoneRenderData(Vector3 bonePos, Camera camera, int* outChosenIndex,
    float* outRotation, bool* outMirrored, const char* boneName) {

    static const int handIndices[3][8] = {
        {23, 22, 2, 15, 16, 17, 18, 24},
        {9, 4, 0, 5, 6, 7, 8, 14},
        {20, 19, 1, 10, 11, 12, 13, 21}
    };

    Vector3 camDir = Vector3Subtract(camera.position, bonePos);
    camDir = SafeNormalize(camDir);

    bool isWrist = (boneName != NULL) && IsWristBone(boneName);

    float yawOffsetRad = 0.0f, pitchOffsetRad = 0.0f;
    if (boneName) {
        for (int i = 0; BONE_ANGLE_OFFSETS[i].boneName[0] != '\0'; i++) {
            if (strcmp(BONE_ANGLE_OFFSETS[i].boneName, boneName) == 0) {
                yawOffsetRad = BONE_ANGLE_OFFSETS[i].yawOffset * (PI / 180.0f);
                pitchOffsetRad = BONE_ANGLE_OFFSETS[i].pitchOffset * (PI / 180.0f);
                break;
            }
        }
    }

    if (fabs(yawOffsetRad) > 1e-6f) {
        float cosYaw = cosf(yawOffsetRad), sinYaw = sinf(yawOffsetRad);
        float newX = camDir.x * cosYaw - camDir.z * sinYaw;
        float newZ = camDir.x * sinYaw + camDir.z * cosYaw;
        camDir.x = newX; camDir.z = newZ;
    }

    if (fabs(pitchOffsetRad) > 1e-6f) {
        float horizDist = sqrtf(camDir.x * camDir.x + camDir.z * camDir.z);
        if (horizDist > 1e-6f) {
            float cosP = cosf(pitchOffsetRad), sinP = sinf(pitchOffsetRad);
            float newY = camDir.y * cosP - horizDist * sinP;
            float newHoriz = camDir.y * sinP + horizDist * cosP;
            float scale = newHoriz / horizDist;
            camDir.x *= scale; camDir.z *= scale; camDir.y = newY;
        }
    }

    if (!isWrist) {
        float tmp = camDir.x; camDir.x = camDir.z; camDir.z = tmp;
    }

    float yaw = atan2f(camDir.x, camDir.z);
    if (yaw < 0.0f) yaw += 2.0f * PI;
    float pitchDeg = atan2f(camDir.y, sqrtf(camDir.x * camDir.x + camDir.z * camDir.z)) * RAD2DEG;

    float normalizedYaw = yaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;
    int sector = (int)(normalizedYaw / 45.0f) % 8;

    if (isWrist) {
        int pitchRow;
        if (pitchDeg >= 22.5f) {
            pitchRow = 0;
        }
        else if (pitchDeg >= -22.5f) {
            pitchRow = 1;
        }
        else {
            pitchRow = 2;
        }

        if (pitchRow < 0) pitchRow = 0;
        if (pitchRow > 2) pitchRow = 2;
        if (sector < 0) sector = 0;
        if (sector > 7) sector = 7;

        *outChosenIndex = handIndices[pitchRow][sector];
        *outRotation = 0.0f;
        *outMirrored = false;

        if (*outChosenIndex < 0) *outChosenIndex = 0;
        if (*outChosenIndex > 24) *outChosenIndex = *outChosenIndex % 25;
    }

    if (pitchDeg >= 52.5f) {
        *outChosenIndex = 22;
        *outRotation = sector * 45.0f + 180.0f;
        *outMirrored = false;
    }
    else if (pitchDeg <= -51.0f) {
        *outChosenIndex = 3;
    }
}

void CalculateTorsoRenderData(const TorsoRenderData* torsoData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {

    if (!torsoData->orientation.valid) {
        CalculateHandBoneRenderData(torsoData->position, camera, outChosenIndex, outRotation, outMirrored, "");
        return;
    }

    static const int indices[3][8] = {
        {0,4,5,6,7,6,5,4},
        {2,12,13,14,15,14,13,12},
        {1,8,9,10,11,10,9,8}
    };

    Vector3 camDir = Vector3Subtract(torsoData->position, camera.position);
    camDir = SafeNormalize(camDir);

    Vector3 localCamDir = {
        Vector3DotProduct(camDir, torsoData->orientation.right),
        Vector3DotProduct(camDir, torsoData->orientation.up),
        Vector3DotProduct(camDir, torsoData->orientation.forward)
    };

    float localYaw = atan2f(localCamDir.x, localCamDir.z);
    if (localYaw < 0.0f) localYaw += 2.0f * PI;

    float localPitchDeg = atan2f(localCamDir.y,
        sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z)) * RAD2DEG;

    localPitchDeg = -localPitchDeg;

    float normalizedYaw = localYaw * RAD2DEG + 22.5f;
    if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;

    int sector = (int)(normalizedYaw / 45.0f) % 8;

    if (localPitchDeg >= 70.0f) {
        *outChosenIndex = 3;
        *outRotation = sector * 45.0f + 180.0f;
        *outMirrored = false;
    }
    else if (localPitchDeg <= -70.0f) {
        *outChosenIndex = 15;
        *outRotation = (8 - sector) * 45.0f + 180.0f;
        if (*outRotation >= 360.0f) *outRotation -= 360.0f;
        *outMirrored = true;
    }
    else {
        int row = (localPitchDeg >= 22.5f) ? 2 : (localPitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[row][sector];
        *outRotation = 0.0f;
        *outMirrored = (sector >= 1 && sector <= 4);
    }

    if (torsoData->type == TORSO_CHEST && torsoData->person &&
        localPitchDeg < 70.0f && localPitchDeg > -70.0f && outRotation) {

        Vector3 hipPosition = CalculateHipPosition(torsoData->person);

        Vector3 chestToHip = Vector3Subtract(hipPosition, torsoData->position);
        float distance = Vector3Length(chestToHip);
        if (distance > 0.001f) {
            chestToHip = Vector3Scale(chestToHip, 1.0f / distance);

            float pitchToHip = atan2f(chestToHip.y,
                sqrtf(chestToHip.x * chestToHip.x + chestToHip.z * chestToHip.z)) * RAD2DEG;

            Vector3 camToTorso = Vector3Subtract(torsoData->position, camera.position);
            camToTorso = SafeNormalize(camToTorso);
            Vector3 torsoRight = torsoData->orientation.right;
            float sideDot = Vector3DotProduct(camToTorso, torsoRight);
            bool viewingFromRight = (sideDot > 0.0f);

            if (viewingFromRight) {
                *outRotation = -pitchToHip - 80.0f;
            }
            else {
                *outRotation = pitchToHip + 80.0f;
            }

            while (*outRotation >= 360.0f) *outRotation -= 360.0f;
            while (*outRotation < 0.0f) *outRotation += 360.0f;
        }
    }

    if (torsoData->type == TORSO_HIP && torsoData->person &&
        localPitchDeg < 70.0f && localPitchDeg > -70.0f) {

        Vector3 chestPosition = CalculateChestPosition(torsoData->person);

        Vector3 hipToChest = Vector3Subtract(chestPosition, torsoData->position);

        float distance = Vector3Length(hipToChest);
        if (distance > 0.001f) {
            hipToChest = Vector3Scale(hipToChest, 1.0f / distance);

            float pitchToChest = atan2f(hipToChest.y,
                sqrtf(hipToChest.x * hipToChest.x + hipToChest.z * hipToChest.z)) * RAD2DEG;

            Vector3 camToTorso = Vector3Subtract(torsoData->position, camera.position);
            Vector3 torsoRight = torsoData->orientation.right;
            float sideDot = Vector3DotProduct(camToTorso, torsoRight);
            bool viewingFromRight = (sideDot > 0.0f);

            if (viewingFromRight) {
                *outRotation = pitchToChest - 80;
            }
            else {
                *outRotation = -pitchToChest + 80;
            }

            while (*outRotation >= 360.0f) *outRotation -= 360.0f;
            while (*outRotation < 0.0f) *outRotation += 360.0f;
        }
    }
}

void CalculateHeadRenderData(const HeadRenderData* headData, Camera camera,
    int* outChosenIndex, float* outRotation, bool* outMirrored) {
    if (!headData->orientation.valid) {
        CalculateHandBoneRenderData(headData->position, camera, outChosenIndex, outRotation, outMirrored, "Head");
        return;
    }
    static const int indices[3][8] = {
        {  0,  4,  5,  6,  7,  6,  5,  4 },
        {  2, 12, 13, 14, 15, 14, 13, 12 },
        {  1,  8,  9, 10, 11, 10,  9,  8 }
    };
    Vector3 camDir = Vector3Subtract(camera.position, headData->position);
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
    int sector;

    if (localPitchDeg >= 65.0f || localPitchDeg <= -65.0f) {
        Vector3 horizontalDiff = {
            camera.position.x - headData->position.x,
            0.0f,
            camera.position.z - headData->position.z
        };
        float horizontalDistance = Vector3Length(horizontalDiff);
        if (horizontalDistance > 0.05f) {
            float worldYaw = atan2f(horizontalDiff.x, horizontalDiff.z);
            if (worldYaw < 0.0f) worldYaw += 2.0f * PI;
            float normalizedWorldYaw = worldYaw * RAD2DEG + 22.5f;
            if (normalizedWorldYaw >= 360.0f) normalizedWorldYaw -= 360.0f;
            sector = (int)(normalizedWorldYaw / 45.0f) % 8;
        }
        else {
            sector = 0;
        }
    }
    else {
        float normalizedYaw = localYawDeg + 22.5f;
        if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;
        sector = (int)(normalizedYaw / 45.0f);
    }
    if (localPitchDeg >= 50.0f) {
        *outChosenIndex = 3;
        *outRotation = sector * 45.0f + 180.0f;
        *outMirrored = false;
    }
    else if (localPitchDeg <= -70.0f) {
        *outChosenIndex = 15;
        *outRotation = (8 - sector) * 45.0f + 180.0f;
        if (*outRotation >= 360.0f) *outRotation -= 360.0f;
        *outMirrored = true;
    }
    else {
        int chosenRow = (localPitchDeg >= 22.5f) ? 2 : (localPitchDeg >= -22.5f) ? 0 : 1;
        *outChosenIndex = indices[chosenRow][sector];
        *outRotation = 0.0f;
        *outMirrored = !(sector >= 5 && sector <= 7);
    }
}

// ============================================================================
// RENDERING
// ============================================================================

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

Rectangle SrcFromLogical(Texture2D tex, int logicalCol, int logicalRow, int physCols, int physRows,
    bool mirrored, bool* outMirrored) {
    if (logicalCol < 0) logicalCol = 0;
    if (logicalRow < 0) logicalRow = 0;

    if (physCols <= 0) physCols = ATLAS_COLS;
    if (physRows <= 0) physRows = ATLAS_ROWS;

    float cellW = (float)tex.width / (float)physCols;
    float cellH = (float)tex.height / (float)physRows;

    if (logicalCol >= physCols) logicalCol = physCols - 1;
    if (logicalRow >= physRows) logicalRow = physRows - 1;

    if (outMirrored) *outMirrored = mirrored;
    return (Rectangle) {
        logicalCol * cellW,
        logicalRow * cellH,
        cellW,
        cellH
    };
}

static void DrawQuadTextured3D_UVs(Texture2D tex,
    Vector3 v0, Vector3 v1, Vector3 v2, Vector3 v3,
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

static bool ShouldFlipBoneTexture(const char* boneName) {
    if (!boneName) return false;

    static const char* flipBones[] = {
        "LShoulder", "LElbow", "RShoulder", "RElbow",
        "LHip", "LKnee", "RHip", "RKnee"
    };

    for (int i = 0; i < 8; i++) {
        if (strcmp(boneName, flipBones[i]) == 0) return true;
    }
    return false;
}

void DrawBonetileCustom(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, const char* boneName) {
    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    float a = rotationDeg * (PI / 180.0f);
    Vector3 newRight = Vector3Subtract(Vector3Scale(right, cosf(a)), Vector3Scale(up, sinf(a)));
    Vector3 newUp = Vector3Add(Vector3Scale(right, sinf(a)), Vector3Scale(up, cosf(a)));

    Vector3 halfX = Vector3Scale(newRight, size.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp, size.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(pos, halfX), halfY);
    Vector3 p1 = Vector3Add(Vector3Subtract(pos, halfY), halfX);
    Vector3 p2 = Vector3Add(Vector3Add(pos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(pos, halfY), halfX);

    float texW = (float)tex.width, texH = (float)tex.height;
    float u_left = src.x / texW, u_right = (src.x + src.width) / texW;
    float v_top = src.y / texH, v_bottom = (src.y + src.height) / texH;

    if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
    if (src.height < 0) { float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

    bool needsVFlip = ShouldFlipBoneTexture(boneName);
    float v0t = needsVFlip ? v_top : v_bottom;
    float v1t = needsVFlip ? v_bottom : v_top;

    if (mirrored) { float tmp = u_left; u_left = u_right; u_right = tmp; }

    DrawQuadTextured3D(tex, p0, p1, p2, p3, u_left, v0t, u_right, v1t);
}

void DrawBonetileCustomWithRoll(Texture2D tex, Camera camera, Rectangle src, Vector3 pos, Vector2 size,
    float rotationDeg, bool mirrored, bool neighborValid, Vector3 neighborPos, const BoneRenderData* boneData,
    const Person* person) {

    Vector3 camToPos = Vector3Subtract(pos, camera.position);
    float distance = Vector3Length(camToPos);
    if (distance > 0.0f) {
        camToPos = Vector3Scale(camToPos, 1.0f / distance);
        float cameraPitchDeg = atan2f(-camToPos.y, sqrtf(camToPos.x * camToPos.x + camToPos.z * camToPos.z)) * RAD2DEG;

        if (fabs(cameraPitchDeg) > 50.0f && strcmp(boneData->boneName, "Neck") != 0) {
            Vector3 personCenter = GetBonePositionByName(person, "Neck");
            Vector3 toCenter = Vector3Normalize(Vector3Subtract(personCenter, boneData->position));
            Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
            Vector3 camUp = Vector3Normalize(Vector3CrossProduct(camRight, camForward));
            Vector3 projectedToCenter = Vector3Subtract(toCenter,
                Vector3Scale(camForward, Vector3DotProduct(toCenter, camForward)));
            float projLength = Vector3Length(projectedToCenter);
            projectedToCenter = Vector3Scale(projectedToCenter, 1.0f / projLength);
            float rightComponent = Vector3DotProduct(projectedToCenter, camRight);
            float upComponent = Vector3DotProduct(projectedToCenter, camUp);

            rotationDeg = atan2f(rightComponent, upComponent) * RAD2DEG + 180.0f;
            neighborValid = false;
        }
    }

    Vector3 camForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(camForward, camera.up));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, camForward));

    float rollExtraDeg = 0.0f;
    if (neighborValid) {
        Vector3 dir = Vector3Subtract(neighborPos, pos);
        if (Vector3Length(dir) > 0.0001f) {
            dir = Vector3Normalize(dir);
            rollExtraDeg = atan2f(Vector3DotProduct(dir, right), Vector3DotProduct(dir, up)) * (180.0f / PI);
        }
    }

    float a = (rotationDeg + rollExtraDeg) * (PI / 180.0f);
    Vector3 newRight = Vector3Subtract(Vector3Scale(right, cosf(a)), Vector3Scale(up, sinf(a)));
    Vector3 newUp = Vector3Add(Vector3Scale(right, sinf(a)), Vector3Scale(up, cosf(a)));

    Vector2 actualSize = size;
    Vector3 actualPos = pos;

    float scaleUp = 0.5f;
    float scaleDown = 0.5f;
    
    for (int i = 0; BONE_SCALE_FACTORS[i].boneName[0] != '\0'; i++) {
        if (strcmp(BONE_SCALE_FACTORS[i].boneName, boneData->boneName) == 0) {
            scaleUp = BONE_SCALE_FACTORS[i].scaleFactorUp;
            scaleDown = BONE_SCALE_FACTORS[i].scaleFactorDown;
            break;
        }
    }

    if (ShouldUseVariableHeight(boneData->boneName) && neighborValid) {
        float neighborDistance = Vector3Distance(pos, neighborPos);
        
        Vector3 boneDir = Vector3Normalize(Vector3Subtract(neighborPos, pos));
        Vector3 camDir = Vector3Normalize(Vector3Subtract(camera.position, pos));
        
        float parallelComponent = Vector3DotProduct(boneDir, camDir);
        Vector3 projectedBone = Vector3Subtract(boneDir, 
            Vector3Scale(camDir, parallelComponent));
        
        float visibleLengthRatio = Vector3Length(projectedBone);
        
        visibleLengthRatio = fmaxf(0.3f, visibleLengthRatio);
        
        float totalExtension = scaleUp + scaleDown;
        actualSize.y = neighborDistance * visibleLengthRatio * totalExtension;
        
        float offsetFactor = scaleDown - scaleUp;
        
        Vector3 toNeighbor = Vector3Subtract(neighborPos, pos);
        if (Vector3Length(toNeighbor) > 0.0001f) {
            toNeighbor = Vector3Normalize(toNeighbor);
            actualPos = Vector3Add(pos, Vector3Scale(toNeighbor, neighborDistance * offsetFactor * 0.5f));
        }
    }

    Vector3 halfX = Vector3Scale(newRight, actualSize.x * 0.5f);
    Vector3 halfY = Vector3Scale(newUp, actualSize.y * 0.5f);

    Vector3 p0 = Vector3Subtract(Vector3Subtract(actualPos, halfX), halfY);
    Vector3 p1 = Vector3Add(Vector3Subtract(actualPos, halfY), halfX);
    Vector3 p2 = Vector3Add(Vector3Add(actualPos, halfX), halfY);
    Vector3 p3 = Vector3Subtract(Vector3Add(actualPos, halfY), halfX);

    float texW = (float)tex.width, texH = (float)tex.height;
    float u_left = src.x / texW, u_right = (src.x + src.width) / texW;
    float v_top = src.y / texH, v_bottom = (src.y + src.height) / texH;

    if (src.width < 0) { float tmp = u_left; u_left = u_right; u_right = tmp; }
    if (src.height < 0) { float tmp = v_top; v_top = v_bottom; v_bottom = tmp; }

    bool needsVFlip = ShouldFlipBoneTexture(boneData->boneName);
    float v0t = needsVFlip ? v_top : v_bottom;
    float v1t = needsVFlip ? v_bottom : v_top;

    if (mirrored) { float tmp = u_left; u_left = u_right; u_right = tmp; }

    Vector2 uv0 = { u_left, v0t }, uv1 = { u_right, v0t }, uv2 = { u_right, v1t }, uv3 = { u_left, v1t };
    DrawQuadTextured3D_UVs(tex, p0, p1, p2, p3, uv0, uv1, uv2, uv3);
}

void DrawTorsoBillboard(Texture2D texture, Camera camera, const TorsoRenderData* torsoData, int physCols, int physRows) {
    if (!torsoData || !torsoData->valid || !torsoData->visible) return;
    int chosenIndex;
    float rotation;
    bool mirrored;
    CalculateTorsoRenderData(torsoData, camera, &chosenIndex, &rotation, &mirrored);

    if (torsoData->orientation.valid) {
        Vector3 camDir = Vector3Subtract(torsoData->position, camera.position);
        camDir = SafeNormalize(camDir);
        Vector3 localCamDir = {
            Vector3DotProduct(camDir, torsoData->orientation.right),
            Vector3DotProduct(camDir, torsoData->orientation.up),
            Vector3DotProduct(camDir, torsoData->orientation.forward)
        };

        float localYaw = atan2f(localCamDir.x, localCamDir.z);
        if (localYaw < 0.0f) localYaw += 2.0f * PI;

        float localPitchDeg = atan2f(localCamDir.y,
            sqrtf(localCamDir.x * localCamDir.x + localCamDir.z * localCamDir.z)) * RAD2DEG;
        localPitchDeg = -localPitchDeg;

        float normalizedYaw = localYaw * RAD2DEG + 22.5f;
        if (normalizedYaw >= 360.0f) normalizedYaw -= 360.0f;
        int sector = (int)(normalizedYaw / 45.0f) % 8;

        if ((sector == 0 || sector == 4) &&
            localPitchDeg < 70.0f && localPitchDeg > -70.0f) {
            rotation = 0.0f;
        }
    }

    int logicalCol = chosenIndex % ATLAS_COLS;
    int logicalRow = chosenIndex / ATLAS_COLS;
    bool finalMirror;
    Rectangle src = SrcFromLogical(texture, logicalCol, logicalRow, physCols, physRows, mirrored, &finalMirror);
    Vector2 worldSize = { torsoData->size, torsoData->size };
    DrawBonetileCustom(texture, camera, src, torsoData->position, worldSize, rotation, finalMirror, "");
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

void DrawAnimatedCharacter(AnimatedCharacter* character, Camera camera) {
    if (!character || !character->animation.isLoaded) return;

    character->renderer->camera = camera;

    BonesRenderer_RenderFrame(character->renderer,
                             character->renderBones, character->renderBonesCount,
                             character->renderHeads, character->renderHeadsCount,
                             character->renderTorsos, character->renderTorsosCount,
                             character->autoCenter, character->autoCenterCalculated);
}

// ============================================================================
// RENDERER CORE
// ============================================================================

static bool DetectZFighting(RenderItem* items, int itemCount) {
    bool hasZFighting = false;
    for (int i = 0; i < itemCount; i++) {
        items[i].hasZFighting = false;
        for (int j = i + 1; j < itemCount; j++) {
            if (fabs(items[i].distance - items[j].distance) < Z_FIGHTING_THRESHOLD) {
                items[i].hasZFighting = items[j].hasZFighting = true;
                hasZFighting = true;
            }
        }
    }
    return hasZFighting;
}

static void SortRenderItems(RenderItem* items, int itemCount) {
    for (int i = 0; i < itemCount - 1; i++) {
        bool swapped = false;
        for (int j = 0; j < itemCount - i - 1; j++) {
            float distanceA = items[j].distance + items[j].depthBias;
            float distanceB = items[j + 1].distance + items[j + 1].depthBias;
            if (distanceA < distanceB) {
                RenderItem temp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = temp;
                swapped = true;
            }
        }
        if (!swapped) break;
    }
}

static void RenderBoneInternal(BonesRenderer* renderer, const BoneRenderData* bone, 
                              Vector3 renderPosition, const AnimationFrame* frame,
                              BoneRenderData* allBones, int boneCount) {
    int texIndex = BonesRenderer_LoadTexture(renderer, bone->texturePath);
    Texture2D currentTex = renderer->textures[texIndex];

    bool isWrist = IsWristBone(bone->boneName);
    int usedCols = isWrist ? 5 : renderer->physCols;
    int usedRows = isWrist ? 5 : renderer->physRows;

    float physCellW = (float)currentTex.width / (float)usedCols;
    float physCellH = (float)currentTex.height / (float)usedRows;
    float aspect = physCellW / physCellH;
    Vector2 worldSize = {bone->size * aspect, bone->size};

    int chosenIndex = 0;
    float rotation = 0.0f;
    bool mirrored = false;

    const Person* bonePerson = frame ? FindPersonByBoneName(frame, bone->boneName) : NULL;

    if (isWrist) {
        CalculateHandBoneRenderData(bone->position, renderer->camera, &chosenIndex, &rotation, &mirrored, bone->boneName);
    } else if (bone->orientation.valid) {
        CalculateLimbBoneRenderData(bone, bonePerson, renderer->camera, &chosenIndex, &rotation, &mirrored);
    }

    int maxIndex = usedCols * usedRows - 1;
    if (chosenIndex < 0) chosenIndex = 0;
    if (chosenIndex > maxIndex) chosenIndex %= (maxIndex + 1);

    int logicalCol = chosenIndex % usedCols;
    int logicalRow = chosenIndex / usedCols;
    bool finalMirror = isWrist ? false : mirrored;
    Rectangle src = SrcFromLogical(currentTex, logicalCol, logicalRow, usedCols, usedRows, finalMirror, &finalMirror);

    char conns[3][32];
    float prios[3];
    Vector3 neighborPos = {0};
    bool haveNeighbor = false;
    if (GetBoneConnectionsWithPriority(bone->boneName, conns, prios)) {
        for (int k = 0; k < 3 && !haveNeighbor; k++) {
            if (conns[k][0] == '\0') continue;
            BoneRenderData* nb = FindRenderBoneByName(allBones, boneCount, conns[k]);
            if (nb && nb->valid && nb->visible && Vector3Distance(bone->position, nb->position) > MIN_DISTANCE_THRESHOLD) {
                neighborPos = nb->position;
                haveNeighbor = true;
            }
        }
    }

    DrawBonetileCustomWithRoll(currentTex, renderer->camera, src, renderPosition, worldSize, rotation,
        finalMirror, haveNeighbor, neighborPos, bone, bonePerson);
}

int BonesRenderer_LoadTexture(BonesRenderer* renderer, const char* path) {
    if (!renderer || !path) return 0;

    for (int i = 0; i < renderer->textureCount; i++) {
        if (strcmp(renderer->texturePaths[i], path) == 0) return i;
    }

    if (renderer->textureCount >= MAX_TEXTURES) return 0;

    Image img = LoadImage(path);
    if (img.data == NULL) {
        img = GenImageColor(1024, 1024, CLITERAL(Color){60, 120, 220, 255});
        ImageDrawText(&img, path, 8, 8, 128, WHITE);
    }

    renderer->textures[renderer->textureCount] = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(renderer->textures[renderer->textureCount], TEXTURE_FILTER_POINT);
    strncpy(renderer->texturePaths[renderer->textureCount], path, MAX_FILE_PATH_LENGTH - 1);
    renderer->texturePaths[renderer->textureCount][MAX_FILE_PATH_LENGTH - 1] = '\0';

    return renderer->textureCount++;
}

void BonesRenderer_SetAtlasDimensions(BonesRenderer* renderer, int cols, int rows) {
    if (renderer) {
        renderer->physCols = cols;
        renderer->physRows = rows;
    }
}

void BonesRenderer_RenderFrame(BonesRenderer* renderer, 
                              BoneRenderData* bones, int boneCount,
                              HeadRenderData* heads, int headCount, 
                              TorsoRenderData* torsos, int torsoCount,
                              Vector3 autoCenter, bool autoCenterCalculated) {
    if (!renderer) return;

    BeginMode3D(renderer->camera);
    
    if (autoCenterCalculated) {
        BonesRenderer_DrawAutoCenter(renderer, autoCenter);
    }

    int totalItems = boneCount + headCount + torsoCount;
    if (totalItems > 0) {
        static RenderItem renderItems[MAX_RENDER_ITEMS];
        int itemCount = 0;
        Vector3 camPos = renderer->camera.position;

        for (int i = 0; i < torsoCount && itemCount < MAX_RENDER_ITEMS; i++) {
            const TorsoRenderData* torso = &torsos[i];
            if (!torso->valid || !torso->visible) continue;
            renderItems[itemCount++] = (RenderItem){
                .type = 0, .index = i,
                .distance = Vector3Distance(camPos, torso->position),
                .depthBias = TORSO_BIAS + (INDEX_BIAS * i)
            };
        }

        for (int i = 0; i < boneCount && itemCount < MAX_RENDER_ITEMS; i++) {
            const BoneRenderData* bone = &bones[i];
            if (!bone->valid || !bone->visible) continue;
            renderItems[itemCount++] = (RenderItem){
                .type = 1, .index = i,
                .distance = Vector3Distance(camPos, bone->position),
                .depthBias = BONE_BIAS + (INDEX_BIAS * i)
            };
        }

        for (int i = 0; i < headCount && itemCount < MAX_RENDER_ITEMS; i++) {
            const HeadRenderData* head = &heads[i];
            if (!head->valid || !head->visible) continue;
            renderItems[itemCount++] = (RenderItem){
                .type = 2, .index = i,
                .distance = Vector3Distance(camPos, head->position),
                .depthBias = HEAD_BIAS + (INDEX_BIAS * i)
            };
        }

        DetectZFighting(renderItems, itemCount);
        SortRenderItems(renderItems, itemCount);

        BeginBlendMode(BLEND_ALPHA);
        rlDisableDepthTest();

        const AnimationFrame* frame = NULL;

        for (int i = 0; i < itemCount; i++) {
            RenderItem* item = &renderItems[i];

            if (item->type == 2) {
                const HeadRenderData* currentHead = &heads[item->index];

                for (int j = 0; j < boneCount; j++) {
                    const BoneRenderData* bone = &bones[j];
                    if (!bone->valid || !bone->visible) continue;
                    if (strcmp(bone->boneName, "Neck") != 0) continue;

                    float neckHeadDistance = Vector3Distance(bone->position, currentHead->position);
                    if (neckHeadDistance < 2.0f) {
                        Vector3 toCam = Vector3Subtract(camPos, bone->position);
                        float distance = Vector3Length(toCam);
                        Vector3 renderOffset = {0};
                        if (distance > MIN_DISTANCE_THRESHOLD) {
                            Vector3 toCamNorm = Vector3Normalize(toCam);
                            renderOffset = Vector3Scale(toCamNorm, BONE_BIAS + (INDEX_BIAS * j));
                        }
                        Vector3 renderPosition = Vector3Add(bone->position, renderOffset);
                        RenderBoneInternal(renderer, bone, renderPosition, frame, bones, boneCount);
                    }
                }
            }

            Vector3 itemPos;
            switch (item->type) {
                case 0: itemPos = torsos[item->index].position; break;
                case 1: itemPos = bones[item->index].position; break;
                case 2: itemPos = heads[item->index].position; break;
                default: continue;
            }

            Vector3 toCam = Vector3Subtract(camPos, itemPos);
            float distance = Vector3Length(toCam);
            Vector3 renderOffset = {0};
            if (distance > MIN_DISTANCE_THRESHOLD) {
                Vector3 toCamNorm = Vector3Normalize(toCam);
                renderOffset = Vector3Scale(toCamNorm, item->depthBias);
            }

            switch (item->type) {
                case 0: {
                    const TorsoRenderData* torso = &torsos[item->index];
                    TorsoRenderData adjusted = *torso;
                    adjusted.position = Vector3Add(torso->position, renderOffset);
                    int texIndex = BonesRenderer_LoadTexture(renderer, torso->texturePath);
                    DrawTorsoBillboard(renderer->textures[texIndex], renderer->camera, &adjusted, 
                                      renderer->physCols, renderer->physRows);
                    break;
                }
                case 1: {
                    const BoneRenderData* bone = &bones[item->index];
                    if (strcmp(bone->boneName, "Neck") != 0) {
                        Vector3 renderPosition = Vector3Add(bone->position, renderOffset);
                        RenderBoneInternal(renderer, bone, renderPosition, frame, bones, boneCount);
                    }
                    break;
                }
                case 2: {
                    const HeadRenderData* head = &heads[item->index];
                    HeadRenderData adjusted = *head;
                    adjusted.position = Vector3Add(head->position, renderOffset);
                    int texIndex = BonesRenderer_LoadTexture(renderer, head->texturePath);
                    DrawHeadBillboard(renderer->textures[texIndex], renderer->camera, &adjusted, 
                                     renderer->physCols, renderer->physRows);
                    break;
                }
            }
        }

        EndBlendMode();
    }

    EndMode3D();
}

void BonesRenderer_DrawAutoCenter(BonesRenderer* renderer, Vector3 autoCenter) {
    if (renderer) {
        DrawSphereWires(autoCenter, 0.05f, 8, 8, ORANGE);
    }
}

// ============================================================================
// ANIMATION CONTROLLER
// ============================================================================

AnimationController* AnimController_Create(void* bonesAnimation, TextureSetCollection* textureSets) {
    AnimationController* controller;
    controller = (AnimationController*)calloc(1, sizeof(AnimationController));
    if (!controller) return NULL;
    
    controller->bonesAnimation = bonesAnimation;
    controller->textureSets = textureSets;
    controller->clipCount = 0;
    controller->currentClipIndex = -1;
    controller->localTime = 0.0f;
    controller->playing = false;
    controller->currentFrameInJSON = 0;
    controller->valid = true;
    
    return controller;
}

void AnimController_Free(AnimationController* controller) {
    if (controller) free(controller);
}

static bool ParseJSONFloat(const char* json, const char* key, float* outValue) {
    char searchKey[128];
    const char* pos;
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    pos = strstr(json, searchKey);
    if (pos && sscanf(pos + strlen(searchKey), "%f", outValue) == 1) {
        return true;
    }
    return false;
}

static bool ParseJSONInt(const char* json, const char* key, int* outValue) {
    char searchKey[128];
    const char* pos;
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    pos = strstr(json, searchKey);
    if (pos && sscanf(pos + strlen(searchKey), "%d", outValue) == 1) {
        return true;
    }
    return false;
}

static bool ParseJSONString(const char* json, const char* key, char* outValue, int maxLen) {
    char searchKey[128];
    const char* pos;
    const char* start;
    const char* end;
    int len;
    
    snprintf(searchKey, sizeof(searchKey), "\"%s\":", key);
    pos = strstr(json, searchKey);
    if (!pos) return false;
    
    start = strchr(pos + strlen(searchKey), '"');
    if (!start) return false;
    start++;
    
    end = strchr(start, '"');
    if (!end) return false;
    
    len = (int)(end - start);
    if (len >= maxLen) len = maxLen - 1;
    
    strncpy(outValue, start, len);
    outValue[len] = '\0';
    return true;
}

bool AnimController_LoadClipMetadata(AnimationController* controller, const char* jsonPath) {
    FILE* file;
    char* jsonData;
    long fileSize;
    AnimationClipMetadata* clip;
    const char* eventsStart;
    const char* eventPos;
    AnimationEvent* event;
    float eventTime;
    char eventType[32];
    char boneName[BONES_AE_MAX_NAME];
    char variantName[BONES_AE_MAX_NAME];
    char personId[BONES_AE_MAX_NAME];
    bool loopValue;
    
    if (!controller || !jsonPath) return false;
    
    file = fopen(jsonPath, "rb");
    if (!file) {
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    jsonData = (char*)malloc(fileSize + 1);
    if (!jsonData) {
        fclose(file);
        return false;
    }
    
    fread(jsonData, 1, fileSize, file);
    jsonData[fileSize] = '\0';
    fclose(file);
    
    if (controller->clipCount >= BONES_MAX_ANIM_CLIPS) {
        free(jsonData);
        return false;
    }
    
    clip = &controller->clips[controller->clipCount];
    memset(clip, 0, sizeof(AnimationClipMetadata));
    
    if (!ParseJSONString(jsonData, "name", clip->name, BONES_AE_MAX_NAME)) {
        strcpy(clip->name, "unnamed");
    }
    
    if (!ParseJSONFloat(jsonData, "fps", &clip->fps)) {
        clip->fps = 30.0f;
    }
    
    if (!ParseJSONInt(jsonData, "start_frame", &clip->startFrame)) {
        clip->startFrame = 0;
    }
    
    if (!ParseJSONInt(jsonData, "end_frame", &clip->endFrame)) {
        clip->endFrame = 60;
    }
    
    loopValue = false;
    if (strstr(jsonData, "\"loop\": true") || strstr(jsonData, "\"loop\":true")) {
        loopValue = true;
    }
    clip->loop = loopValue;
    
    clip->eventCount = 0;
    eventsStart = strstr(jsonData, "\"events\":");
    if (eventsStart) {
        eventsStart = strchr(eventsStart, '[');
        if (eventsStart) {
            eventPos = eventsStart;
            while (clip->eventCount < BONES_MAX_ANIM_EVENTS) {
                eventPos = strchr(eventPos + 1, '{');
                if (!eventPos || eventPos > strstr(eventsStart, "]")) break;
                
                event = &clip->events[clip->eventCount];
                memset(event, 0, sizeof(AnimationEvent));
                
                if (!ParseJSONFloat(eventPos, "time", &eventTime)) continue;
                event->time = eventTime;
                
                if (!ParseJSONString(eventPos, "type", eventType, sizeof(eventType))) continue;
                
                if (strcmp(eventType, "texture") == 0) {
                    event->type = ANIM_EVENT_TEXTURE;
                } else if (strcmp(eventType, "sound") == 0) {
                    event->type = ANIM_EVENT_SOUND;
                } else {
                    event->type = ANIM_EVENT_CUSTOM;
                }
                
                ParseJSONString(eventPos, "bone", boneName, BONES_AE_MAX_NAME);
                strncpy(event->boneName, boneName, BONES_AE_MAX_NAME - 1);
                
                ParseJSONString(eventPos, "variant", variantName, BONES_AE_MAX_NAME);
                strncpy(event->variantName, variantName, BONES_AE_MAX_NAME - 1);
                
                if (ParseJSONString(eventPos, "person", personId, BONES_AE_MAX_NAME)) {
                    strncpy(event->personId, personId, BONES_AE_MAX_NAME - 1);
                }
                
                event->processed = false;
                event->valid = true;
                
                clip->eventCount++;
            }
        }
    }
    
    clip->valid = true;
    controller->clipCount++;
    
    free(jsonData);
    return true;
}

bool AnimController_PlayClip(AnimationController* controller, const char* clipName) {
    int i;
    if (!controller || !clipName) return false;
    
    for (i = 0; i < controller->clipCount; i++) {
        if (strcmp(controller->clips[i].name, clipName) == 0) {
            controller->currentClipIndex = i;
            controller->localTime = 0.0f;
            controller->playing = true;
            
            if (controller->bonesAnimation) {
                BonesAnimation* anim = (BonesAnimation*)controller->bonesAnimation;
                if (anim->isLoaded && anim->frameCount > 0) {
                    int minFrame = anim->frames[0].frameNumber;
                    int maxFrame = anim->frames[0].frameNumber;
                    
                    for (int f = 1; f < anim->frameCount; f++) {
                        if (anim->frames[f].frameNumber < minFrame) {
                            minFrame = anim->frames[f].frameNumber;
                        }
                        if (anim->frames[f].frameNumber > maxFrame) {
                            maxFrame = anim->frames[f].frameNumber;
                        }
                    }
                    
                    controller->clips[i].startFrame = minFrame;
                    controller->clips[i].endFrame = maxFrame;
                }
            }
            
            controller->currentFrameInJSON = controller->clips[i].startFrame;
            
            {
                int j;
                for (j = 0; j < controller->clips[i].eventCount; j++) {
                    controller->clips[i].events[j].processed = false;
                }
            }
            
            return true;
        }
    }
    return false;
}

void AnimController_Pause(AnimationController* controller) {
    if (controller) controller->playing = false;
}

void AnimController_Resume(AnimationController* controller) {
    if (controller) controller->playing = true;
}

int AnimController_GetCurrentFrame(const AnimationController* controller) {
    if (!controller || controller->currentClipIndex < 0) return 0;
    return controller->currentFrameInJSON;
}

void AnimController_Update(AnimationController* controller, float deltaTime) {
    AnimationClipMetadata* clip;
    float clipDuration;
    int totalFrames;
    int i;
    AnimationEvent* event;
    
    if (!controller || !controller->playing || controller->currentClipIndex < 0) return;
    
    clip = &controller->clips[controller->currentClipIndex];
    if (!clip->valid) return;
    
    controller->localTime += deltaTime;
    
    totalFrames = clip->endFrame - clip->startFrame + 1;
    clipDuration = (float)totalFrames / clip->fps;
    
    if (controller->localTime >= clipDuration) {
        if (clip->loop) {
            controller->localTime = fmodf(controller->localTime, clipDuration);
            for (i = 0; i < clip->eventCount; i++) {
                clip->events[i].processed = false;
            }
        } else {
            controller->localTime = clipDuration - 0.001f;
            controller->playing = false;
        }
    }
    
    controller->currentFrameInJSON = clip->startFrame + 
        (int)roundf(controller->localTime * clip->fps);
    
    if (controller->currentFrameInJSON > clip->endFrame) {
        controller->currentFrameInJSON = clip->endFrame;
    }
    
    for (i = 0; i < clip->eventCount; i++) {
        event = &clip->events[i];
        if (!event->valid || event->processed) continue;
        
        if (controller->localTime >= event->time) {
            if (event->type == ANIM_EVENT_TEXTURE && controller->textureSets) {
                BonesTextureSets_SetVariant(controller->textureSets, 
                                           event->boneName, 
                                           event->variantName);
            }
            
            event->processed = true;
        }
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

const Person* FindPersonByBoneName(const AnimationFrame* frame, const char* boneName) {
    if (!frame || !boneName) return NULL;
    for (int p = 0; p < frame->personCount; p++) {
        const Person* person = &frame->persons[p];
        if (!person->active) continue;
        for (int b = 0; b < person->boneCount; b++) {
            if (strcmp(person->bones[b].name, boneName) == 0) return person;
        }
    }
    return NULL;
}

bool GetBoneConnectionsWithPriority(const char* boneName, char connections[3][32], float priorities[3]) {
    for (int i = 0; BONE_CONNECTIONS[i].boneName[0]; i++) {
        if (strcmp(BONE_CONNECTIONS[i].boneName, boneName) == 0) {
            for (int j = 0; j < 3; j++) {
                strncpy(connections[j], BONE_CONNECTIONS[i].connections[j], 31);
                connections[j][31] = '\0';
                priorities[j] = BONE_CONNECTIONS[i].priority[j];
            }
            return true;
        }
    }
    return false;
}

const char* BonesGetErrorString(BonesError error) {
    static const char* errorStrings[] = {
        [BONES_SUCCESS] = "Operation successful",
        [BONES_ERROR_NULL_POINTER] = "Null pointer received",
        [BONES_ERROR_FILE_NOT_FOUND] = "File not found",
        [BONES_ERROR_INVALID_JSON] = "Invalid or malformed JSON",
        [BONES_ERROR_MEMORY_ALLOCATION] = "Memory allocation error",
        [BONES_ERROR_BONE_NOT_FOUND] = "Bone not found",
        [BONES_ERROR_FRAME_OUT_OF_RANGE] = "Frame out of valid range",
        [BONES_ERROR_PERSON_NOT_FOUND] = "Person not found",
        [BONES_ERROR_INVALID_COORDINATES] = "Invalid coordinates",
        [BONES_ERROR_BUFFER_OVERFLOW] = "Buffer overflow",
        [BONES_ERROR_EMPTY_DATA] = "Empty or no content data"
    };
    return (error < sizeof(errorStrings) / sizeof(errorStrings[0]) && errorStrings[error])
        ? errorStrings[error] : "Unknown error";
}

BonesRenderConfig BonesGetDefaultRenderConfig(void) {
    return g_renderConfig;
}

void BonesSetRenderConfig(const BonesRenderConfig* config) {
    if (config) g_renderConfig = *config;
}

static BonesError ParseJSONFrame(const char* jsonData, int* outFrameNum, Person* outPersons, int* outPersonCount) {
    if (!jsonData || !outFrameNum || !outPersons || !outPersonCount) return BONES_ERROR_NULL_POINTER;

    *outPersonCount = 0;

    const char* frameStart = strstr(jsonData, "\"frame_");
    if (!frameStart || sscanf(frameStart, "\"frame_%d\"", outFrameNum) != 1) {
        return BONES_ERROR_INVALID_JSON;
    }

    static const char* expectedBones[] = {
        "Nose", "LEye", "REye", "LEar", "REar",
        "LShoulder", "RShoulder", "LElbow", "RElbow",
        "LWrist", "RWrist", "LHip", "RHip",
        "LKnee", "RKnee", "LAnkle", "RAnkle", "Neck"
    };

    const char* personStart = strstr(frameStart, "\"person_");
    while (personStart && *outPersonCount < MAX_PERSONS) {
        Person* currentPerson = &outPersons[*outPersonCount];
        memset(currentPerson, 0, sizeof(Person));

        if (sscanf(personStart, "\"person_%15[^\"]\"", currentPerson->personId) != 1) break;

        currentPerson->active = true;
        const char* nextPerson = strstr(personStart + 1, "\"person_");

        for (int b = 0; b < 18 && currentPerson->boneCount < MAX_BONES_PER_PERSON; b++) {
            char searchPattern[64];
            snprintf(searchPattern, sizeof(searchPattern), "\"%s\":", expectedBones[b]);

            const char* bonePos = strstr(personStart, searchPattern);
            if (!bonePos || (nextPerson && bonePos > nextPerson)) continue;

            double x, y, z;
            if (sscanf(strstr(bonePos, "\"x\":"), "\"x\": %lf", &x) == 1 &&
                sscanf(strstr(bonePos, "\"y\":"), "\"y\": %lf", &y) == 1 &&
                sscanf(strstr(bonePos, "\"z\":"), "\"z\": %lf", &z) == 1) {

                Bone* currentBone = &currentPerson->bones[currentPerson->boneCount];
                strncpy(currentBone->name, expectedBones[b], MAX_BONE_NAME_LENGTH - 1);
                currentBone->name[MAX_BONE_NAME_LENGTH - 1] = '\0';

                currentBone->position.position = (Vector3){
                    (float)(x * 2.0 - 1.0),
                    (float)(1.0 - y),
                    (float)(z * 2.0 - 1.0)
                };
                currentBone->position.valid = BonesIsPositionValid(currentBone->position.position);
                currentBone->position.confidence = 1.0f;
                currentBone->size = (Vector2){ g_renderConfig.defaultBoneSize, g_renderConfig.defaultBoneSize };
                currentBone->visible = true;

                currentPerson->boneCount++;
            }
        }

        if (currentPerson->boneCount > 0) (*outPersonCount)++;
        personStart = nextPerson;
    }

    return (*outPersonCount > 0) ? BONES_SUCCESS : BONES_ERROR_EMPTY_DATA;
}

BonesError BonesLoadFromJSON(BonesAnimation* animation, const char* jsonFilePath) {
    if (!animation || !jsonFilePath) return BONES_ERROR_NULL_POINTER;

    char* jsonData = LoadFileText(jsonFilePath);
    if (!jsonData) {
        return BONES_ERROR_FILE_NOT_FOUND;
    }

    BonesError result = BonesLoadFromString(animation, jsonData);
    if (result == BONES_SUCCESS) {
        strncpy(animation->filePath, jsonFilePath, sizeof(animation->filePath) - 1);
        animation->filePath[sizeof(animation->filePath) - 1] = '\0';
    }

    UnloadFileText(jsonData);
    return result;
}

BonesError BonesLoadFromString(BonesAnimation* animation, const char* jsonString) {
    if (!animation || !jsonString || !animation->frames) return BONES_ERROR_NULL_POINTER;

    animation->frameCount = 0;
    animation->currentFrame = -1;
    animation->isLoaded = false;

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
            frame->isOriginalKeyframe = true;
            animation->frameCount++;
        }

        searchPos = nextFrame + 1;
    }

    if (animation->frameCount > 0) {
        animation->currentFrame = 0;
        animation->isLoaded = true;
        return BONES_SUCCESS;
    }

    return BONES_ERROR_EMPTY_DATA;
}

BonesError BonesSetFrame(BonesAnimation* animation, int frameNumber) {
    if (!animation) return BONES_ERROR_NULL_POINTER;
    if (!animation->isLoaded) return BONES_ERROR_EMPTY_DATA;
    if (frameNumber < 0 || frameNumber >= animation->frameCount) return BONES_ERROR_FRAME_OUT_OF_RANGE;

    animation->currentFrame = frameNumber;
    return BONES_SUCCESS;
}

int BonesGetCurrentFrame(const BonesAnimation* animation) {
    return animation ? animation->currentFrame : -1;
}

int BonesGetFrameCount(const BonesAnimation* animation) {
    return animation ? animation->frameCount : 0;
}

bool BonesIsValidFrame(const BonesAnimation* animation, int frameNumber) {
    return animation && animation->isLoaded && frameNumber >= 0 &&
        frameNumber < animation->frameCount && animation->frames[frameNumber].valid;
}

bool BonesIsPositionValid(Vector3 position) {
    return !isnan(position.x) && !isnan(position.y) && !isnan(position.z) &&
        isfinite(position.x) && isfinite(position.y) && isfinite(position.z);
}

// ============================================================================
// TEXTURE MANAGEMENT
// ============================================================================

bool LoadSimpleTextureConfig(SimpleTextureSystem* system, const char* filename) {
    char* buffer = LoadFileText(filename);
    if (!buffer) return false;

    int lineCount = 0;
    for (const char* ptr = buffer; *ptr; ptr++) {
        if (*ptr == '\n') {
            const char* lineStart = ptr;
            while (lineStart > buffer && *(lineStart - 1) != '\n') lineStart--;
            int lineLen = ptr - lineStart;
            if (lineLen > 5 && *lineStart != '#' && *lineStart != '\n') lineCount++;
        }
    }

    if (lineCount == 0) {
        UnloadFileText(buffer);
        return false;
    }

    free(system->configs);
    system->configs = calloc(lineCount, sizeof(BoneTextureConfig));
    if (!system->configs) {
        UnloadFileText(buffer);
        return false;
    }

    system->configCapacity = lineCount;
    system->configCount = 0;

    char lineBuffer[512];
    const char* lineStart = buffer;

    for (const char* ptr = buffer; *ptr && system->configCount < system->configCapacity; ptr++) {
        if (*ptr == '\n') {
            int lineLen = ptr - lineStart;
            if (lineLen > 5 && lineLen < 511 && *lineStart != '#' && *lineStart != '\n') {
                memcpy(lineBuffer, lineStart, lineLen);
                lineBuffer[lineLen] = '\0';

                char boneName[MAX_BONE_NAME_LENGTH], texturePath[MAX_FILE_PATH_LENGTH];
                int visible;
                float size;

                if (sscanf(lineBuffer, "%31s %255s %d %f", boneName, texturePath, &visible, &size) == 4) {
                    BoneTextureConfig* config = &system->configs[system->configCount];
                    strncpy(config->boneName, boneName, MAX_BONE_NAME_LENGTH - 1);
                    config->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
                    strncpy(config->texturePath, texturePath, MAX_FILE_PATH_LENGTH - 1);
                    config->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
                    config->visible = (visible != 0);
                    config->size = size;
                    system->configCount++;
                }
            }
            lineStart = ptr + 1;
        }
    }

    UnloadFileText(buffer);

    if (system->configCount > 0) {
        system->loaded = true;
        return true;
    }
    return false;
}

void LoadBoneConfigurations(SimpleTextureSystem* textureSystem, BoneConfig** boneConfigs, int* boneConfigCount) {
    free(*boneConfigs);
    *boneConfigs = NULL;
    *boneConfigCount = 0;

    if (!textureSystem->loaded || textureSystem->configCount == 0) return;

    *boneConfigCount = textureSystem->configCount;
    *boneConfigs = calloc(*boneConfigCount, sizeof(BoneConfig));
    if (!*boneConfigs) return;

    for (int i = 0; i < *boneConfigCount; i++) {
        const BoneTextureConfig* src = &textureSystem->configs[i];
        BoneConfig* dst = &(*boneConfigs)[i];

        strncpy(dst->boneName, src->boneName, MAX_BONE_NAME_LENGTH - 1);
        dst->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
        strncpy(dst->texturePath, src->texturePath, MAX_FILE_PATH_LENGTH - 1);
        dst->texturePath[MAX_FILE_PATH_LENGTH - 1] = '\0';
        dst->visible = src->visible;
        dst->size = src->size;
        dst->valid = true;
    }
}

BoneConfig* FindBoneConfig(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    for (int i = 0; i < boneConfigCount; i++) {
        if (strcmp(boneConfigs[i].boneName, boneName) == 0) {
            return &boneConfigs[i];
        }
    }
    return NULL;
}

const char* GetTexturePathForBone(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName,
    TextureSetCollection* textureSets) {  // <-- AÑADIDO PARÁMETRO
    
    // MODIFICADO: Usar textureSets pasado como parámetro
    if (textureSets && textureSets->loaded) {
        const char* activeTexture = BonesTextureSets_GetActiveTexture(textureSets, boneName);
        if (activeTexture) {
            return activeTexture;
        }
    }
    
    BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return config ? config->texturePath : "default.png";
}

bool IsBoneVisible(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return config ? config->visible : true;
}

float GetBoneSize(BoneConfig* boneConfigs, int boneConfigCount, const char* boneName) {
    BoneConfig* config = FindBoneConfig(boneConfigs, boneConfigCount, boneName);
    return config ? config->size : 0.35f;
}

BoneTextureSet* BonesTextureSets_FindSet(TextureSetCollection* collection, const char* boneName) {
    int i;
    if (!collection || !boneName) return NULL;
    for (i = 0; i < collection->setCount; i++) {
        if (strcmp(collection->sets[i].boneName, boneName) == 0) {
            return &collection->sets[i];
        }
    }
    return NULL;
}

bool BonesTextureSets_LoadFromFile(TextureSetCollection* collection, const char* filePath) {
    FILE* file;
    char line[512];
    int lineNum;
    char boneName[BONES_AE_MAX_NAME];
    char variantName[BONES_AE_MAX_NAME];
    char texturePath[BONES_AE_MAX_PATH];
    BoneTextureSet* set;
    BoneTextureVariant* variant;
    
    if (!collection || !filePath) return false;
    
    file = fopen(filePath, "r");
    if (!file) {
        return false;
    }
    
    collection->setCount = 0;
    collection->loaded = false;
    lineNum = 0;
    
    while (fgets(line, sizeof(line), file) && collection->setCount < BONES_MAX_TEXTURE_SETS) {
        lineNum++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        if (sscanf(line, "%63s %63s %255s", boneName, variantName, texturePath) != 3) {
            continue;
        }
        
        set = BonesTextureSets_FindSet(collection, boneName);
        
        if (!set) {
            if (collection->setCount >= BONES_MAX_TEXTURE_SETS) break;
            set = &collection->sets[collection->setCount];
            memset(set, 0, sizeof(BoneTextureSet));
            strncpy(set->boneName, boneName, BONES_AE_MAX_NAME - 1);
            set->boneName[BONES_AE_MAX_NAME - 1] = '\0';
            set->variantCount = 0;
            set->activeVariantIndex = 0;
            set->valid = true;
            collection->setCount++;
        }
        
        if (set->variantCount >= BONES_MAX_TEXTURE_VARIANTS) continue;
        
        variant = &set->variants[set->variantCount];
        strncpy(variant->variantName, variantName, BONES_AE_MAX_NAME - 1);
        variant->variantName[BONES_AE_MAX_NAME - 1] = '\0';
        strncpy(variant->texturePath, texturePath, BONES_AE_MAX_PATH - 1);
        variant->texturePath[BONES_AE_MAX_PATH - 1] = '\0';
        variant->valid = true;
        set->variantCount++;
    }
    
    fclose(file);
    
    if (collection->setCount > 0) {
        collection->loaded = true;
        return true;
    }
    return false;
}

const char* BonesTextureSets_GetActiveTexture(const TextureSetCollection* collection, const char* boneName) {
    int i;
    const BoneTextureSet* set;
    if (!collection || !boneName || !collection->loaded) return NULL;
    for (i = 0; i < collection->setCount; i++) {
        set = &collection->sets[i];
        if (strcmp(set->boneName, boneName) == 0 && set->valid) {
            if (set->activeVariantIndex >= 0 && set->activeVariantIndex < set->variantCount) {
                return set->variants[set->activeVariantIndex].texturePath;
            }
        }
    }
    return NULL;
}

bool BonesTextureSets_SetVariant(TextureSetCollection* collection, const char* boneName, const char* variantName) {
    BoneTextureSet* set;
    int i;
    if (!collection || !boneName || !variantName) return false;
    set = BonesTextureSets_FindSet(collection, boneName);
    if (!set) return false;
    for (i = 0; i < set->variantCount; i++) {
        if (strcmp(set->variants[i].variantName, variantName) == 0) {
            set->activeVariantIndex = i;
            return true;
        }
    }
    return false;
}

const char* BonesTextureSets_GetActiveVariantName(const TextureSetCollection* collection, const char* boneName) {
    int i;
    const BoneTextureSet* set;
    if (!collection || !boneName || !collection->loaded) return NULL;
    for (i = 0; i < collection->setCount; i++) {
        set = &collection->sets[i];
        if (strcmp(set->boneName, boneName) == 0 && set->valid) {
            if (set->activeVariantIndex >= 0 && set->activeVariantIndex < set->variantCount) {
                return set->variants[set->activeVariantIndex].variantName;
            }
        }
    }
    return NULL;
}

void BonesTextureSets_ResetAll(TextureSetCollection* collection) {
    int i;
    if (!collection) return;
    for (i = 0; i < collection->setCount; i++) {
        collection->sets[i].activeVariantIndex = 0;
    }
}