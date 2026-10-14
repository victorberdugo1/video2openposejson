#include "bones_core.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOUSER
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
#endif

#define BASE_WIDTH 1920
#define BASE_HEIGHT 1080
#define UI_HEIGHT 200
#define TIMELINE_HEIGHT 100
#define BUTTON_SIZE 35
#define TIMELINE_MARGIN 60
#define MAX_UNDO_STACK 50
#define GIZMO_SIZE 120
#define GIZMO_MARGIN 20
#define GIZMO_CIRCLE_RADIUS 50
#define GIZMO_DOT_RADIUS 8
#define GIZMO_SNAP_THRESHOLD 0.15f

static const float ORBIT_SENSITIVITY = 0.01f;
static const float FPS_SENSITIVITY = 0.003f;
static const float ZOOM_SENSITIVITY = 0.5f;
static const float TARGET_MOVE_SENSITIVITY = 0.02f;
static const float MAX_TARGET_OFFSET = 0.6f;
static const float MIN_ORBIT_RADIUS = 0.5f;
static const float MAX_ORBIT_RADIUS = 20.0f;
static const float MIN_PITCH = -85.0f * PI / 180.0f;
static const float MAX_PITCH = 81.0f * PI / 180.0f;
static const float BASE_SPEED = 5.0f;
static const float MOVEMENT_SPEED = 3.0f;
static const float AXIS_LENGTH = 0.05f;

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

typedef enum {
	TOOL_NONE,
	TOOL_SELECT,
	TOOL_DELETE,
	TOOL_DUPLICATE,
	TOOL_INTERPOLATE
} EditorTool;

typedef enum {
	UNDO_BONE_MOVE,
	UNDO_KEYFRAME_MOVE,
	UNDO_FRAME_PROMOTE,
	UNDO_FRAME_DELETE,
	UNDO_FRAME_DUPLICATE,
	UNDO_FRAMES_ADD,
	UNDO_PASTE_POSE,
	UNDO_BONE_ROTATE,
	UNDO_GROUP_TRANSFORM,
	UNDO_ORIENT_OFFSET
} UndoActionType;

#define MAX_GROUP_BONES 32
#define ROTATION_HANDLE_RADIUS 40.0f

typedef struct {
	char boneName[MAX_BONE_NAME_LENGTH];
	Vector3 oldPosition;
	Vector3 newPosition;
	float oldRotation;
	float newRotation;
} GroupBoneSnapshot;

typedef struct {
	UndoActionType type;
	char boneName[64];
	int frameNumber;
	Vector3 oldPosition;
	Vector3 newPosition;
	float oldRotation;
	float newRotation;
	int oldFrameNumber;
	int newFrameNumber;
	int promotedFrameNumber;
	AnimationFrame* snapshotFrames;
	int snapshotStartIndex;
	int snapshotCount;
	int oldFrameCount;
	int opFrameIndex;
	int opCount;
	AnimationFrame* redoFrame;
	GroupBoneSnapshot groupSnapshots[MAX_GROUP_BONES];
	int groupSnapshotCount;
} UndoAction;

typedef struct {
	UndoAction actions[MAX_UNDO_STACK];
	int count;
	int currentIndex;
} UndoHistory;

// ============================================================================
// CLIPBOARD SYSTEM
// ============================================================================

typedef struct {
	char boneName[MAX_BONE_NAME_LENGTH];
	Vector3 position;
	bool valid;
} ClipboardBone;

typedef struct {
	ClipboardBone bones[MAX_BONES_PER_PERSON];
	int boneCount;
	bool hasCopiedData;
	char sourceAnimation[64];
	int sourceFrame;
} PoseClipboard;

static PoseClipboard g_clipboard = {0};

typedef struct {
    char  boneName[MAX_BONE_NAME_LENGTH];
    float yaw;
    float pitch;
} BoneOrientOffset;

#define MAX_ORIENT_OFFSETS 64
static BoneOrientOffset g_orientOffsets[MAX_ORIENT_OFFSETS];
static int              g_orientOffsetCount = 0;

// ============================================================================
// EDITOR STATE
// ============================================================================

typedef struct {
	bool isPlaying;
	bool showTimeline;
	int selectedFrame;
	int selectionStart;
	int selectionEnd;
	bool isDraggingSlider;
	bool isSelecting;
	EditorTool currentTool;
	int interpolationCount;
	char exportPath[256];
	bool showExportDialog;
	bool needsSave;
	float playbackSpeed;
	bool hasBoneSelected;
	char selectedBoneName[64];
	int selectedBonePersonIndex;
	Vector3 selectedBonePosition;
	bool isDraggingBone;
	Vector3 dragStartPos;
	Vector2 dragStartMouse;
	bool isDraggingKeyframe;
	int draggedKeyframeNumber;
	Vector3 keyframeDragOffset;
	AnimationFrame draggedKeyframeData;
	UndoHistory undoHistory;
	Vector2 lastClickPos;
	float lastClickTime;
	int cycleIndex;
	bool isDraggingGizmo;
	bool showAddFramesDialog;
	int framesToAdd;
	char saveStatusMessage[160];
	float saveStatusTimer;
	bool saveStatusIsError;
	bool isDraggingRotationHandle;
	float rotationHandleStartAngle;
	float rotationHandleStartBoneRotation;
	char groupSelectedBones[MAX_GROUP_BONES][MAX_BONE_NAME_LENGTH];
	int groupSelectedCount;
	Vector3 groupDragStartPositions[MAX_GROUP_BONES];
	float groupDragStartRotations[MAX_GROUP_BONES];
	Vector3 groupDragAnchorStart;
	Vector2 groupDragStartMouse;
	bool isDraggingGroup;
	bool groupDragWasPlaying;
	int  orientDragAxis;
	float orientDragStartAngle;
	float orientDragStartValue;
} EditorState;

typedef struct {
	bool showBoneNames;
	bool showDebugSpheres;
	bool showConnections;
	bool showOrientation;
	bool showOnionSkin;
} DebugOptions;

#define MAX_ANIMATIONS 100

typedef struct {
	char name[64];
	char jsonPath[256];
	char metaPath[256];
} AnimationInfo;

typedef struct {
	AnimationInfo animations[MAX_ANIMATIONS];
	int animationCount;
	int currentAnimationIndex;
} AnimationManager;

typedef struct {
	AnimatedCharacter* character;
	int camMode;
	float orbitYaw, orbitPitch, orbitRadius;
	Vector3 targetOffset; // Offset del punto objetivo en relación a la cámara
	bool showUI;
	EditorState editor;
	DebugOptions debug;
	int screenWidth;
	int screenHeight;
	AnimationManager animManager;
	char currentAnimation[64];
} AppState;

typedef struct {
	char boneName[64];
	int personIndex;
	Vector3 bonePos;
	float distance;
} BoneCandidate;

typedef struct {
	CharacterProfile profiles[10];
	int profileCount;
	int currentProfileIndex;
} CharacterManager;

static void InitUndoHistory(UndoHistory* history);
static void PushUndoAction(UndoHistory* history, UndoAction action);
void AnimController_UpdateFrameBounds(AnimationController* controller, int clipIndex, BonesAnimation* animation);
static CharacterManager g_characterManager = {0};
static void RecalculateAffectedInterpolations(AppState* app, int movedKeyframe);
static void RecalculateInterpolatedFrames(AppState* app, int keyframeA, int keyframeB);
static void MoveBoneInFrame(AppState* app, int frameNumber, const char* boneName, Vector3 newPosition);
static void RotateBoneInFrame(AppState* app, int frameNumber, const char* boneName, float newRotation);
static void MoveKeyframeInTimeline(AppState* app, int fromFrameNumber, int toFrameNumber);
static int FindFrameIndexByNumber(AppState* app, int frameNumber);
static int FindMaxFrameNumber(AppState* app);
static bool Button(Rectangle bounds, const char* text, Color color);
static void DrawTextField(Rectangle bounds, const char* text, bool active);
static void AddMultipleFramesAtEnd(AppState* app, int numFramesToAdd, BonesAnimation* animation);
static void DrawAddFramesDialog(AppState* app);
bool BonesDeleteFrame(AppState* app, BonesAnimation* animation, int frameIndex);
bool BonesDuplicateFrame(AppState* app, BonesAnimation* animation, int frameIndex);
bool BonesExportToJSON(BonesAnimation* animation, const char* filepath, int startIdx, int endIdx);
static void PerformSave(AppState* app);
static int GetCurrentFrameNumber(AppState* app);
static void UpdateBoneRotationHandle(AppState* app);
static void DrawBoneRotationHandle(AppState* app);
static void UpdateBoneOrientGizmo(AppState* app);
static void DrawBoneOrientGizmo(AppState* app);
static void InjectOrientOffsets(AppState* app);
static BoneOrientOffset* GetOrientOffset(const char* boneName);
static void DrawOnionSkin(AppState* app);
static bool IsInGroup(AppState* app, const char* boneName);
static void UpdateGroupDrag(AppState* app);

// ============================================================================
// CLIPBOARD FUNCTIONS
// ============================================================================

static void CopyPoseToClipboard(AppState* app) {
	if (!app->character->animation.isLoaded) {
		TraceLog(LOG_WARNING, "No animation loaded to copy from");
		return;
	}

	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) {
		TraceLog(LOG_WARNING, "Invalid current frame");
		return;
	}

	const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	
	// Clear clipboard
	memset(&g_clipboard, 0, sizeof(PoseClipboard));
	
	// Find the first active person
	const Person* person = NULL;
	for (int p = 0; p < frame->personCount; p++) {
		if (frame->persons[p].active) {
			person = &frame->persons[p];
			break;
		}
	}
	
	if (!person) {
		TraceLog(LOG_WARNING, "No active person found in frame");
		return;
	}

	// Copy all bones
	g_clipboard.boneCount = 0;
	for (int b = 0; b < person->boneCount && g_clipboard.boneCount < MAX_BONES_PER_PERSON; b++) {
		const Bone* bone = &person->bones[b];
		if (bone->position.valid) {
			ClipboardBone* clipBone = &g_clipboard.bones[g_clipboard.boneCount];
			strncpy(clipBone->boneName, bone->name, MAX_BONE_NAME_LENGTH - 1);
			clipBone->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
			clipBone->position = bone->position.position;
			clipBone->valid = true;
			g_clipboard.boneCount++;
		}
	}

	g_clipboard.hasCopiedData = true;
	strncpy(g_clipboard.sourceAnimation, app->currentAnimation, 63);
	g_clipboard.sourceAnimation[63] = '\0';
	g_clipboard.sourceFrame = frame->frameNumber;

	TraceLog(LOG_INFO, "Copied %d bones from %s frame %d", 
		g_clipboard.boneCount, 
		g_clipboard.sourceAnimation, 
		g_clipboard.sourceFrame);
}

static void PastePoseFromClipboard(AppState* app) {
	if (!g_clipboard.hasCopiedData) {
		TraceLog(LOG_WARNING, "No pose data in clipboard");
		return;
	}

	if (!app->character->animation.isLoaded) {
		TraceLog(LOG_WARNING, "No animation loaded to paste to");
		return;
	}

	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) {
		TraceLog(LOG_WARNING, "Invalid current frame");
		return;
	}

	AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	
	// Convert to keyframe if not already
	if (!frame->isOriginalKeyframe) {
		frame->isOriginalKeyframe = true;
		TraceLog(LOG_INFO, "Converted frame to keyframe for paste operation");
	}

	// Find the first active person
	Person* person = NULL;
	for (int p = 0; p < frame->personCount; p++) {
		if (frame->persons[p].active) {
			person = &frame->persons[p];
			break;
		}
	}
	
	if (!person) {
		TraceLog(LOG_WARNING, "No active person found in target frame");
		return;
	}

	// Paste all bones
	int pastedCount = 0;
	for (int c = 0; c < g_clipboard.boneCount; c++) {
		const ClipboardBone* clipBone = &g_clipboard.bones[c];
		if (!clipBone->valid) continue;

		// Find matching bone in current frame
		bool found = false;
		for (int b = 0; b < person->boneCount; b++) {
			Bone* bone = &person->bones[b];
			if (strcmp(bone->name, clipBone->boneName) == 0) {
				bone->position.position = clipBone->position;
				bone->position.valid = true;
				bone->position.confidence = 1.0f;
				found = true;
				pastedCount++;
				break;
			}
		}

		if (!found) {
			TraceLog(LOG_DEBUG, "Bone %s not found in target frame", clipBone->boneName);
		}
	}

	if (pastedCount > 0) {
		// Force visual update
		app->character->forceUpdate = true;
		app->editor.needsSave = true;

		// Recalculate interpolations if this is a keyframe
		int currentFrameNumber = frame->frameNumber;
		RecalculateAffectedInterpolations(app, currentFrameNumber);

		TraceLog(LOG_INFO, "Pasted %d bones from %s frame %d to %s frame %d", 
			pastedCount,
			g_clipboard.sourceAnimation,
			g_clipboard.sourceFrame,
			app->currentAnimation,
			frame->frameNumber);
	} else {
		TraceLog(LOG_WARNING, "No bones were pasted");
	}
}

static void PastePoseFromClipboardWithUndo(AppState* app) {
	if (!g_clipboard.hasCopiedData || !app->character->animation.isLoaded) {
		PastePoseFromClipboard(app);
		return;
	}

	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) {
		PastePoseFromClipboard(app);
		return;
	}

	AnimationFrame beforeFrame = app->character->animation.frames[currentFrame];

	PastePoseFromClipboard(app);

	if (currentFrame >= app->character->animation.frameCount) return;
	if (memcmp(&beforeFrame, &app->character->animation.frames[currentFrame], sizeof(AnimationFrame)) == 0) return;

	AnimationFrame* beforeCopy = (AnimationFrame*)malloc(sizeof(AnimationFrame));
	AnimationFrame* afterCopy = (AnimationFrame*)malloc(sizeof(AnimationFrame));
	if (!beforeCopy || !afterCopy) {
		free(beforeCopy);
		free(afterCopy);
		return;
	}
	*beforeCopy = beforeFrame;
	*afterCopy = app->character->animation.frames[currentFrame];

	UndoAction action = {0};
	action.type = UNDO_PASTE_POSE;
	action.opFrameIndex = currentFrame;
	action.snapshotFrames = beforeCopy;
	action.snapshotStartIndex = currentFrame;
	action.snapshotCount = 1;
	action.oldFrameCount = app->character->animation.frameCount;
	action.redoFrame = afterCopy;
	PushUndoAction(&app->editor.undoHistory, action);
}

// ============================================================================
// ANIM SYSTEM
// ============================================================================

static bool HasExtension(const char* filename, const char* ext) {
	size_t len = strlen(filename);
	size_t extLen = strlen(ext);
	if (len < extLen) return false;
	return strcmp(filename + len - extLen, ext) == 0;
}

static bool LoadAnimationsFromDirectory(AnimationManager* manager, const char* animationsPath) {
	manager->animationCount = 0;

#ifdef _WIN32
	char pattern[512];
	snprintf(pattern, sizeof(pattern), "%s*.anim", animationsPath);
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(pattern, &findData);
	if (hFind == INVALID_HANDLE_VALUE) {
		TraceLog(LOG_WARNING, "Cannot open animations directory: %s", animationsPath);
		return false;
	}
	do {
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		const char* name = findData.cFileName;
#else
	DIR* dir = opendir(animationsPath);
	if (!dir) {
		TraceLog(LOG_WARNING, "Cannot open animations directory: %s", animationsPath);
		return false;
	}
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL && manager->animationCount < MAX_ANIMATIONS) {
		const char* name = entry->d_name;
		if (!name) continue;
#endif

		if (HasExtension(name, ".anim") && manager->animationCount < MAX_ANIMATIONS) {
			char baseName[64];
			strncpy(baseName, name, sizeof(baseName) - 1);
			baseName[sizeof(baseName) - 1] = '\0';
			char* dot = strrchr(baseName, '.');
			if (dot) *dot = '\0';

			char jsonPath[512];
			char animPath[512];
			snprintf(jsonPath, sizeof(jsonPath), "%s%s.json", animationsPath, baseName);
			snprintf(animPath, sizeof(animPath), "%s%s.anim", animationsPath, baseName);

			FILE* testFile = fopen(jsonPath, "r");
			if (testFile) {
				fclose(testFile);
				AnimationInfo* info = &manager->animations[manager->animationCount];
				strncpy(info->name, baseName, sizeof(info->name) - 1);
				info->name[sizeof(info->name) - 1] = '\0';
				strncpy(info->jsonPath, jsonPath, sizeof(info->jsonPath) - 1);
				info->jsonPath[sizeof(info->jsonPath) - 1] = '\0';
				strncpy(info->metaPath, animPath, sizeof(info->metaPath) - 1);
				info->metaPath[sizeof(info->metaPath) - 1] = '\0';
				manager->animationCount++;
				TraceLog(LOG_INFO, "Found animation: %s", baseName);
			} else {
				TraceLog(LOG_DEBUG, "Skipping anim without json: %s", name);
			}
		}

#ifdef _WIN32
	} while (FindNextFileA(hFind, &findData));
	FindClose(hFind);
#else
	}
	closedir(dir);
#endif

	if (manager->animationCount > 0) {
		manager->currentAnimationIndex = 0;
		TraceLog(LOG_INFO, "Loaded %d animations from %s", manager->animationCount, animationsPath);
		return true;
	}
	manager->currentAnimationIndex = -1;
	return false;
}

static void LoadAnimationByIndex(AppState* app, int animIndex) {
	if (animIndex < 0 || animIndex >= app->animManager.animationCount) return;

	AnimationInfo* info = &app->animManager.animations[animIndex];

	bool wasPlaying = app->editor.isPlaying;

	if (LoadAnimation(app->character, info->jsonPath, info->metaPath)) {
		strncpy(app->currentAnimation, info->name, sizeof(app->currentAnimation) - 1);
		app->currentAnimation[sizeof(app->currentAnimation) - 1] = '\0';
		app->animManager.currentAnimationIndex = animIndex;
		InitUndoHistory(&app->editor.undoHistory);

		app->editor.isPlaying = wasPlaying;
		SetCharacterAutoPlay(app->character, wasPlaying);

		if (app->character->animation.frameCount > 0) {
			SetCharacterFrame(app->character, 0);
		}

		TraceLog(LOG_INFO, "Loaded animation %d/%d: %s",
				animIndex + 1, app->animManager.animationCount, info->name);
	} else {
		TraceLog(LOG_WARNING, "Could not load animation: %s", info->name);
	}
}

static void LoadNextAnimation(AppState* app) {
	if (app->animManager.animationCount == 0) return;
	int nextIndex = (app->animManager.currentAnimationIndex + 1) % app->animManager.animationCount;
	LoadAnimationByIndex(app, nextIndex);
}

static void LoadPreviousAnimation(AppState* app) {
	if (app->animManager.animationCount == 0) return;
	int prevIndex = app->animManager.currentAnimationIndex - 1;
	if (prevIndex < 0) prevIndex = app->animManager.animationCount - 1;
	LoadAnimationByIndex(app, prevIndex);
}

// ============================================================================
// UNDO/REDO SYSTEM
// ============================================================================

static void FreeUndoActionData(UndoAction* action) {
	if (action->snapshotFrames) {
		free(action->snapshotFrames);
		action->snapshotFrames = NULL;
	}
	if (action->redoFrame) {
		free(action->redoFrame);
		action->redoFrame = NULL;
	}
}

static bool SnapshotFrameRange(UndoAction* action, const BonesAnimation* animation, int startIndex, int count) {
	action->snapshotStartIndex = startIndex;
	action->snapshotCount = count;
	action->oldFrameCount = animation->frameCount;
	if (count <= 0) {
		action->snapshotFrames = NULL;
		return true;
	}
	action->snapshotFrames = (AnimationFrame*)malloc(sizeof(AnimationFrame) * (size_t)count);
	if (!action->snapshotFrames) return false;
	memcpy(action->snapshotFrames, &animation->frames[startIndex], sizeof(AnimationFrame) * (size_t)count);
	return true;
}

static void RestoreFrameRange(BonesAnimation* animation, const UndoAction* action) {
	if (action->snapshotCount > 0 && action->snapshotFrames) {
		memcpy(&animation->frames[action->snapshotStartIndex], action->snapshotFrames,
				sizeof(AnimationFrame) * (size_t)action->snapshotCount);
	}
	animation->frameCount = action->oldFrameCount;
}

static void InitUndoHistory(UndoHistory* history) {
	for (int i = 0; i < history->count; i++) {
		FreeUndoActionData(&history->actions[i]);
	}
	memset(history, 0, sizeof(UndoHistory));
	history->count = 0;
	history->currentIndex = -1;
}

static void PushUndoAction(UndoHistory* history, UndoAction action) {
	if (history->currentIndex < history->count - 1) {
		for (int i = history->currentIndex + 1; i < history->count; i++) {
			FreeUndoActionData(&history->actions[i]);
		}
		history->count = history->currentIndex + 1;
	}
	if (history->count >= MAX_UNDO_STACK) {
		FreeUndoActionData(&history->actions[0]);
		for (int i = 0; i < MAX_UNDO_STACK - 1; i++) {
			history->actions[i] = history->actions[i + 1];
		}
		history->count = MAX_UNDO_STACK - 1;
	}
	history->actions[history->count] = action;
	history->currentIndex = history->count;
	history->count++;
}

static bool PerformUndo(AppState* app) {
	UndoHistory* history = &app->editor.undoHistory;
	if (history->currentIndex < 0) return false;
	UndoAction* action = &history->actions[history->currentIndex];
	switch (action->type) {
		case UNDO_BONE_MOVE: {
								 MoveBoneInFrame(app, action->frameNumber, action->boneName, action->oldPosition);
								 RecalculateAffectedInterpolations(app, action->frameNumber);
								 int frameIndex = FindFrameIndexByNumber(app, action->frameNumber);
								 if (frameIndex != -1) {
									 SetCharacterFrame(app->character, frameIndex);
								 }
								 break;
							 }
		case UNDO_KEYFRAME_MOVE: {
									 MoveKeyframeInTimeline(app, action->newFrameNumber, action->oldFrameNumber);
									 break;
								 }
		case UNDO_FRAME_PROMOTE: {
									 int frameIndex = FindFrameIndexByNumber(app, action->promotedFrameNumber);
									 if (frameIndex != -1) {
										 app->character->animation.frames[frameIndex].isOriginalKeyframe = false;
										 RecalculateAffectedInterpolations(app, action->promotedFrameNumber);
									 }
									 break;
								 }
		case UNDO_FRAME_DELETE:
		case UNDO_FRAME_DUPLICATE:
		case UNDO_FRAMES_ADD: {
									 RestoreFrameRange(&app->character->animation, action);
									 app->character->maxFrames = app->character->animation.frameCount;
									 if (app->character->animController && app->character->animController->currentClipIndex >= 0) {
										 AnimController_UpdateFrameBounds(app->character->animController,
												 app->character->animController->currentClipIndex,
												 &app->character->animation);
									 }
									 int clampedFrame = app->character->currentFrame;
									 if (clampedFrame >= app->character->animation.frameCount) {
										 clampedFrame = app->character->animation.frameCount - 1;
									 }
									 if (clampedFrame >= 0) {
										 SetCharacterFrame(app->character, clampedFrame);
									 }
									 app->character->forceUpdate = true;
									 break;
								 }
		case UNDO_PASTE_POSE: {
								  RestoreFrameRange(&app->character->animation, action);
								  RecalculateAffectedInterpolations(app, app->character->animation.frames[action->opFrameIndex].frameNumber);
								  int frameIndex = FindFrameIndexByNumber(app, app->character->animation.frames[action->opFrameIndex].frameNumber);
								  if (frameIndex != -1) {
									  SetCharacterFrame(app->character, frameIndex);
								  }
								  app->character->forceUpdate = true;
								  break;
							  }
		case UNDO_BONE_ROTATE: {
								   RotateBoneInFrame(app, action->frameNumber, action->boneName, action->oldRotation);
								   int frameIndex = FindFrameIndexByNumber(app, action->frameNumber);
								   if (frameIndex != -1) SetCharacterFrame(app->character, frameIndex);
								   break;
							   }
		case UNDO_GROUP_TRANSFORM: {
									   int currentFrameNumber = GetCurrentFrameNumber(app);
									   for (int i = 0; i < action->groupSnapshotCount; i++) {
										   MoveBoneInFrame(app, currentFrameNumber, action->groupSnapshots[i].boneName, action->groupSnapshots[i].oldPosition);
										   RotateBoneInFrame(app, currentFrameNumber, action->groupSnapshots[i].boneName, action->groupSnapshots[i].oldRotation);
									   }
									   RecalculateAffectedInterpolations(app, currentFrameNumber);
									   app->character->forceUpdate = true;
									   break;
								   }
		case UNDO_ORIENT_OFFSET: {
									 BoneOrientOffset* off = GetOrientOffset(action->boneName);
									 if (off) {
										 if (action->opFrameIndex == 1) off->yaw   = action->oldRotation;
										 else                            off->pitch = action->oldRotation;
									 }
									 break;
								 }
	}
	history->currentIndex--;
	app->editor.needsSave = true;
	return true;
}

static bool PerformRedo(AppState* app) {
	UndoHistory* history = &app->editor.undoHistory;
	if (history->currentIndex >= history->count - 1) return false;
	history->currentIndex++;
	UndoAction* action = &history->actions[history->currentIndex];
	switch (action->type) {
		case UNDO_BONE_MOVE: {
								 MoveBoneInFrame(app, action->frameNumber, action->boneName, action->newPosition);
								 RecalculateAffectedInterpolations(app, action->frameNumber);
								 int frameIndex = FindFrameIndexByNumber(app, action->frameNumber);
								 if (frameIndex != -1) {
									 SetCharacterFrame(app->character, frameIndex);
								 }
								 break;
							 }
		case UNDO_KEYFRAME_MOVE: {
									 MoveKeyframeInTimeline(app, action->oldFrameNumber, action->newFrameNumber);
									 break;
								 }
		case UNDO_FRAME_PROMOTE: {
									 int frameIndex = FindFrameIndexByNumber(app, action->promotedFrameNumber);
									 if (frameIndex != -1) {
										 app->character->animation.frames[frameIndex].isOriginalKeyframe = true;
									 }
									 break;
								 }
		case UNDO_FRAME_DELETE: {
									 BonesDeleteFrame(app, &app->character->animation, action->opFrameIndex);
									 app->character->maxFrames = app->character->animation.frameCount;
									 app->character->forceUpdate = true;
									 break;
								 }
		case UNDO_FRAME_DUPLICATE: {
										BonesDuplicateFrame(app, &app->character->animation, action->opFrameIndex);
										app->character->forceUpdate = true;
										break;
									}
		case UNDO_FRAMES_ADD: {
								  AddMultipleFramesAtEnd(app, action->opCount, &app->character->animation);
								  app->character->forceUpdate = true;
								  break;
							  }
		case UNDO_PASTE_POSE: {
								  if (action->redoFrame) {
									  app->character->animation.frames[action->opFrameIndex] = *action->redoFrame;
									  RecalculateAffectedInterpolations(app, app->character->animation.frames[action->opFrameIndex].frameNumber);
									  int frameIndex = FindFrameIndexByNumber(app, app->character->animation.frames[action->opFrameIndex].frameNumber);
									  if (frameIndex != -1) {
										  SetCharacterFrame(app->character, frameIndex);
									  }
									  app->character->forceUpdate = true;
								  }
								  break;
							  }
		case UNDO_BONE_ROTATE: {
								   RotateBoneInFrame(app, action->frameNumber, action->boneName, action->newRotation);
								   int frameIndex = FindFrameIndexByNumber(app, action->frameNumber);
								   if (frameIndex != -1) SetCharacterFrame(app->character, frameIndex);
								   break;
							   }
		case UNDO_GROUP_TRANSFORM: {
									   int currentFrameNumber = GetCurrentFrameNumber(app);
									   for (int i = 0; i < action->groupSnapshotCount; i++) {
										   MoveBoneInFrame(app, currentFrameNumber, action->groupSnapshots[i].boneName, action->groupSnapshots[i].newPosition);
										   RotateBoneInFrame(app, currentFrameNumber, action->groupSnapshots[i].boneName, action->groupSnapshots[i].newRotation);
									   }
									   RecalculateAffectedInterpolations(app, currentFrameNumber);
									   app->character->forceUpdate = true;
									   break;
								   }
		case UNDO_ORIENT_OFFSET: {
									 BoneOrientOffset* off = GetOrientOffset(action->boneName);
									 if (off) {
										 if (action->opFrameIndex == 1) off->yaw   = action->newRotation;
										 else                            off->pitch = action->newRotation;
									 }
									 break;
								 }
	}
	app->editor.needsSave = true;
	return true;
}

void AnimController_UpdateFrameBounds(AnimationController* controller, int clipIndex, BonesAnimation* animation) {
	if (!controller || !controller->bonesAnimation || clipIndex < 0 ||
			clipIndex >= controller->clipCount) return;

	if (!animation) return;

	for (int i = 0; i < animation->frameCount; i++) {
		animation->frames[i].frameNumber = i;
	}

	BonesAnimation* anim = (BonesAnimation*)controller->bonesAnimation;
	if (!anim->isLoaded || anim->frameCount == 0) return;

	AnimationClipMetadata* clip = &controller->clips[clipIndex];

	int minFrame = anim->frames[0].frameNumber;
	int maxFrame = anim->frames[0].frameNumber;

	for (int i = 1; i < anim->frameCount; i++) {
		if (anim->frames[i].frameNumber < minFrame) {
			minFrame = anim->frames[i].frameNumber;
		}
		if (anim->frames[i].frameNumber > maxFrame) {
			maxFrame = anim->frames[i].frameNumber;
		}
	}

	clip->startFrame = minFrame;
	clip->endFrame = maxFrame;
}

static void NormalizeFrameNumbers(BonesAnimation* animation) {
	for (int i = 0; i < animation->frameCount; i++) {
		animation->frames[i].frameNumber = i;
	}
}

bool BonesDuplicateFrame(AppState* app, BonesAnimation* animation, int frameIndex) {
	if (!animation || frameIndex < 0 || frameIndex >= animation->frameCount ||
			animation->frameCount >= animation->maxFrames) {
		TraceLog(LOG_WARNING, "Cannot duplicate frame: invalid params or no space");
		return false;
	}

	AnimationFrame* sourceFrame = &animation->frames[frameIndex];

	for (int i = animation->frameCount; i > frameIndex + 1; i--) {
		animation->frames[i] = animation->frames[i - 1];
	}

	animation->frames[frameIndex + 1] = *sourceFrame;
	animation->frames[frameIndex + 1].isOriginalKeyframe = true;
	animation->frameCount++;

	NormalizeFrameNumbers(animation);

	if (app->character->animController &&
			app->character->animController->currentClipIndex >= 0) {
		AnimController_UpdateFrameBounds(app->character->animController,
				app->character->animController->currentClipIndex,
				animation);
	}

	TraceLog(LOG_INFO, "Duplicated frame at index %d (new total: %d)",
			frameIndex, animation->frameCount);

	return true;
}

bool BonesDeleteFrame(AppState* app, BonesAnimation* animation, int frameIndex) {
	if (!animation || frameIndex < 0 || frameIndex >= animation->frameCount) {
		TraceLog(LOG_WARNING, "Cannot delete frame: invalid index");
		return false;
	}

	AnimationFrame* frameToDelete = &animation->frames[frameIndex];

	if (!frameToDelete->isOriginalKeyframe) {
		TraceLog(LOG_WARNING, "Cannot delete interpolated frame. Delete the keyframe instead.");
		return false;
	}

	int keyframeCount = 0;
	for (int i = 0; i < animation->frameCount; i++) {
		if (animation->frames[i].isOriginalKeyframe) {
			keyframeCount++;
		}
	}

	if (keyframeCount <= 1) {
		TraceLog(LOG_WARNING, "Cannot delete the last keyframe");
		return false;
	}

	int prevKeyframeIdx = -1;
	for (int i = frameIndex - 1; i >= 0; i--) {
		if (animation->frames[i].isOriginalKeyframe) {
			prevKeyframeIdx = i;
			break;
		}
	}

	int nextKeyframeIdx = -1;
	for (int i = frameIndex + 1; i < animation->frameCount; i++) {
		if (animation->frames[i].isOriginalKeyframe) {
			nextKeyframeIdx = i;
			break;
		}
	}

	bool isClustered = (prevKeyframeIdx == frameIndex - 1) || (nextKeyframeIdx == frameIndex + 1);

	int startDeleteIdx = frameIndex;
	int endDeleteIdx = frameIndex;

	if (!isClustered && prevKeyframeIdx != -1) {
		startDeleteIdx = prevKeyframeIdx + 1;
	}

	int framesToDelete = endDeleteIdx - startDeleteIdx + 1;

	TraceLog(LOG_INFO, "Deleting keyframe at %d and %d interpolated frames (total: %d)",
			frameIndex, framesToDelete - 1, framesToDelete);

	for (int i = startDeleteIdx; i < animation->frameCount - framesToDelete; i++) {
		animation->frames[i] = animation->frames[i + framesToDelete];
	}

	animation->frameCount -= framesToDelete;
	NormalizeFrameNumbers(animation);

	if (!isClustered && prevKeyframeIdx != -1 && nextKeyframeIdx != -1) {
		int newPrevIdx = prevKeyframeIdx;
		int newNextIdx = nextKeyframeIdx - framesToDelete;

		if (newNextIdx > newPrevIdx + 1) {
			RecalculateInterpolatedFrames(app, newPrevIdx, newNextIdx);
		}
	}

	if (app->character->animController &&
			app->character->animController->currentClipIndex >= 0) {
		AnimController_UpdateFrameBounds(app->character->animController,
				app->character->animController->currentClipIndex,
				animation);
	}

	return true;
}

static bool DeleteFrameWithUndo(AppState* app, int frameIndex) {
	BonesAnimation* animation = &app->character->animation;
	UndoAction action = {0};
	action.type = UNDO_FRAME_DELETE;
	action.opFrameIndex = frameIndex;
	if (!SnapshotFrameRange(&action, animation, 0, animation->frameCount)) return false;

	if (!BonesDeleteFrame(app, animation, frameIndex)) {
		FreeUndoActionData(&action);
		return false;
	}

	app->character->maxFrames = animation->frameCount;
	PushUndoAction(&app->editor.undoHistory, action);
	return true;
}

static bool DuplicateFrameWithUndo(AppState* app, int frameIndex) {
	BonesAnimation* animation = &app->character->animation;
	UndoAction action = {0};
	action.type = UNDO_FRAME_DUPLICATE;
	action.opFrameIndex = frameIndex;
	if (!SnapshotFrameRange(&action, animation, frameIndex + 1, animation->frameCount - (frameIndex + 1))) return false;

	if (!BonesDuplicateFrame(app, animation, frameIndex)) {
		FreeUndoActionData(&action);
		return false;
	}

	PushUndoAction(&app->editor.undoHistory, action);
	return true;
}

static void AddMultipleFramesAtEnd(AppState* app, int numFramesToAdd, BonesAnimation* animation) {
	if (numFramesToAdd < 1 || !animation) {
		TraceLog(LOG_WARNING, "Invalid number of frames to add: %d", numFramesToAdd);
		return;
	}

	if (animation->frameCount + numFramesToAdd > animation->maxFrames) {
		TraceLog(LOG_WARNING, "Not enough space to add %d frames (current: %d, max: %d)",
				numFramesToAdd, animation->frameCount, animation->maxFrames);
		return;
	}

	int lastKeyframeIndex = -1;
	int lastFrameNumber = 0;

	for (int i = animation->frameCount - 1; i >= 0; i--) {
		if (animation->frames[i].isOriginalKeyframe) {
			lastKeyframeIndex = i;
			lastFrameNumber = animation->frames[i].frameNumber;
			break;
		}
	}

	if (lastKeyframeIndex == -1) {
		TraceLog(LOG_WARNING, "No keyframe found to extend from");
		return;
	}

	AnimationFrame* sourceFrame = &animation->frames[lastKeyframeIndex];

	TraceLog(LOG_INFO, "Adding %d frames after frame %d", numFramesToAdd, lastFrameNumber);

	for (int i = 1; i <= numFramesToAdd; i++) {
		int newIndex = animation->frameCount;
		AnimationFrame* newFrame = &animation->frames[newIndex];

		*newFrame = *sourceFrame;
		newFrame->valid = true;
		newFrame->frameNumber = lastFrameNumber + i;

		newFrame->isOriginalKeyframe = (i == numFramesToAdd);

		animation->frameCount++;
	}

	if (numFramesToAdd > 1) {
		int startKeyframeIdx = lastKeyframeIndex;
		int endKeyframeIdx = animation->frameCount - 1;

		AnimationFrame* startFrame = &animation->frames[startKeyframeIdx];
		AnimationFrame* endFrame = &animation->frames[endKeyframeIdx];

		for (int frameOffset = 1; frameOffset < numFramesToAdd; frameOffset++) {
			int interpFrameIdx = startKeyframeIdx + frameOffset;
			AnimationFrame* interpFrame = &animation->frames[interpFrameIdx];

			float t = (float)frameOffset / (float)numFramesToAdd;

			for (int p = 0; p < startFrame->personCount && p < endFrame->personCount; p++) {
				Person* startPerson = &startFrame->persons[p];
				Person* endPerson = &endFrame->persons[p];
				Person* interpPerson = &interpFrame->persons[p];

				if (!startPerson->active || !endPerson->active) continue;

				for (int b = 0; b < startPerson->boneCount; b++) {
					Bone* startBone = &startPerson->bones[b];
					if (!startBone->position.valid) continue;

					for (int eb = 0; eb < endPerson->boneCount; eb++) {
						Bone* endBone = &endPerson->bones[eb];
						if (strcmp(startBone->name, endBone->name) == 0 && endBone->position.valid) {
							for (int ib = 0; ib < interpPerson->boneCount; ib++) {
								Bone* interpBone = &interpPerson->bones[ib];
								if (strcmp(interpBone->name, startBone->name) == 0) {
									interpBone->position.position = Vector3Lerp(
											startBone->position.position,
											endBone->position.position,
											t);
									interpBone->position.valid = true;
									interpBone->position.confidence =
										startBone->position.confidence * (1.0f - t) +
										endBone->position.confidence * t;
									break;
								}
							}
							break;
						}
					}
				}
			}
		}
	}

	if (app->character->animController) {
		AnimController_UpdateFrameBounds(app->character->animController,
				app->character->animController->currentClipIndex,
				animation);
	}

	bool wasPlaying = app->editor.isPlaying;
	if (wasPlaying) {
		SetCharacterAutoPlay(app->character, false);
	}

	app->character->forceUpdate = true;
	app->editor.needsSave = true;

	int newFrameIndex = animation->frameCount - 1;
	SetCharacterFrame(app->character, newFrameIndex);

	if (wasPlaying) {
		SetCharacterAutoPlay(app->character, true);
		app->editor.isPlaying = true;
	}

	int newFrameNumber = animation->frames[newFrameIndex].frameNumber;
	app->editor.selectionStart = newFrameNumber;
	app->editor.selectionEnd = newFrameNumber;

	TraceLog(LOG_INFO, "Added %d frames (1 keyframe + %d interpolated). Total frames: %d",
			numFramesToAdd, numFramesToAdd - 1, animation->frameCount);
}

static bool AddMultipleFramesAtEndWithUndo(AppState* app, int numFramesToAdd, BonesAnimation* animation) {
	UndoAction action = {0};
	action.type = UNDO_FRAMES_ADD;
	action.opCount = numFramesToAdd;
	if (!SnapshotFrameRange(&action, animation, animation->frameCount, 0)) return false;

	int frameCountBefore = animation->frameCount;
	AddMultipleFramesAtEnd(app, numFramesToAdd, animation);
	if (animation->frameCount == frameCountBefore) {
		FreeUndoActionData(&action);
		return false;
	}

	PushUndoAction(&app->editor.undoHistory, action);
	return true;
}

static void DrawAddFramesDialog(AppState* app) {
	if (!app->editor.showAddFramesDialog) return;

	float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
	int dialogW = (int)(400 * scale);
	int dialogH = (int)(180 * scale);
	int dialogX = (app->screenWidth - dialogW) / 2;
	int dialogY = (app->screenHeight - dialogH) / 2;
	int padding = (int)(20 * scale);
	int buttonH = (int)(35 * scale);
	int fontSize = (int)(16 * scale);

	DrawRectangle(0, 0, app->screenWidth, app->screenHeight, (Color){0, 0, 0, 150});
	DrawRectangle(dialogX, dialogY, dialogW, dialogH, RAYWHITE);
	DrawRectangleLinesEx((Rectangle){(float)dialogX, (float)dialogY, (float)dialogW, (float)dialogH}, 3, BLACK);

	DrawText("Add Frames at End", dialogX + padding, dialogY + padding, fontSize + 4, BLACK);
	DrawText("Number of frames to add:", dialogX + padding, dialogY + (int)(60 * scale), fontSize, DARKGRAY);

	char framesText[32];
	snprintf(framesText, sizeof(framesText), "%d", app->editor.framesToAdd);
	DrawTextField((Rectangle){(float)(dialogX + padding), (float)(dialogY + (int)(85 * scale)),
			(float)(dialogW - padding * 2), (float)(30 * scale)},
			framesText, true);

	int btnSize = (int)(30 * scale);
	if (Button((Rectangle){(float)(dialogX + dialogW - (int)(180 * scale)),
				(float)(dialogY + (int)(85 * scale)),
				(float)btnSize, (float)btnSize}, "+", GREEN)) {
		app->editor.framesToAdd++;
	}
	if (Button((Rectangle){(float)(dialogX + dialogW - (int)(145 * scale)),
				(float)(dialogY + (int)(85 * scale)),
				(float)btnSize, (float)btnSize}, "-", RED)) {
		if (app->editor.framesToAdd > 1) app->editor.framesToAdd--;
	}

	int btnW = (int)(100 * scale);
	if (Button((Rectangle){(float)(dialogX + padding), (float)(dialogY + dialogH - buttonH - padding),
				(float)btnW, (float)buttonH}, "ADD", GREEN)) {
		AddMultipleFramesAtEndWithUndo(app, app->editor.framesToAdd, &app->character->animation);
		app->editor.showAddFramesDialog = false;
	}

	if (Button((Rectangle){(float)(dialogX + padding * 2 + btnW), (float)(dialogY + dialogH - buttonH - padding),
				(float)btnW, (float)buttonH}, "CANCEL", RED)) {
		app->editor.showAddFramesDialog = false;
	}
}

// ============================================================================
// CHARACTER PROFILE MANAGEMENT
// ============================================================================

static bool LoadCharacterProfiles(CharacterManager* manager, const char* configPath) {
	char* buffer = LoadFileText(configPath);
	if (!buffer) {
		TraceLog(LOG_WARNING, "Could not load character profiles from %s", configPath);
		return false;
	}

	manager->profileCount = 0;

	char lineBuffer[512];
	const char* lineStart = buffer;

	for (const char* ptr = buffer; *ptr && manager->profileCount < 10; ptr++) {
		if (*ptr == '\n') {
			int lineLen = ptr - lineStart;
			if (lineLen > 5 && lineLen < 511 && *lineStart != '#' && *lineStart != '\n') {
				memcpy(lineBuffer, lineStart, lineLen);
				lineBuffer[lineLen] = '\0';

				CharacterProfile* profile = &manager->profiles[manager->profileCount];

				if (sscanf(lineBuffer, "%63s %255s %255s %255s",
							profile->name,
							profile->texturesConfigPath,
							profile->textureSetsPath,
							profile->animationsPath) == 4) {
					manager->profileCount++;
					TraceLog(LOG_INFO, "Loaded character profile: %s", profile->name);
				}
			}
			lineStart = ptr + 1;
		}
	}

	UnloadFileText(buffer);

	if (manager->profileCount > 0) {
		manager->currentProfileIndex = 0;
		return true;
	}
	return false;
}

static bool SwitchCharacterProfile(AppState* app, int profileIndex) {
	if (profileIndex < 0 || profileIndex >= g_characterManager.profileCount) {
		return false;
	}

	CharacterProfile* profile = &g_characterManager.profiles[profileIndex];

	TraceLog(LOG_INFO, "Switching to character profile: %s", profile->name);

	bool wasPlaying = app->editor.isPlaying;

	if (app->character) {
		DestroyAnimatedCharacter(app->character);
		app->character = NULL;
	}

	app->character = CreateAnimatedCharacter(profile->texturesConfigPath,
			profile->textureSetsPath);
	if (!app->character) {
		TraceLog(LOG_ERROR, "Failed to create character with profile: %s", profile->name);
		return false;
	}

	if (!LoadAnimationsFromDirectory(&app->animManager, profile->animationsPath)) {
		TraceLog(LOG_WARNING, "No animations found in %s", profile->animationsPath);
	}

	if (app->animManager.animationCount > 0) {
		LoadAnimationByIndex(app, 0);
	}
	char idleAnimPath[512];
	char idleMetaPath[512];
	snprintf(idleAnimPath, sizeof(idleAnimPath), "%sidle.json", profile->animationsPath);
	snprintf(idleMetaPath, sizeof(idleMetaPath), "%sidle.anim", profile->animationsPath);

	if (LoadAnimation(app->character, idleAnimPath, idleMetaPath)) {
		strcpy(app->currentAnimation, "idle");
		TraceLog(LOG_INFO, "Loaded idle animation for %s", profile->name);
	} else {
		TraceLog(LOG_WARNING, "Could not load idle animation for %s", profile->name);
	}

	InitUndoHistory(&app->editor.undoHistory);

	app->editor.needsSave = false;
	app->character->forceUpdate = true;

	app->editor.isPlaying = wasPlaying;
	SetCharacterAutoPlay(app->character, wasPlaying);

	if (app->character->animation.frameCount > 0) {
		SetCharacterFrame(app->character, 0);
	}

	g_characterManager.currentProfileIndex = profileIndex;

	return true;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static int GetCurrentFrameNumber(AppState* app) {
	if (app->character->currentFrame < 0 ||
			app->character->currentFrame >= app->character->animation.frameCount) {
		return 0;
	}
	return app->character->animation.frames[app->character->currentFrame].frameNumber;
}

static int FindFrameIndexByNumber(AppState* app, int frameNumber) {
	for (int i = 0; i < app->character->animation.frameCount; i++) {
		if (app->character->animation.frames[i].frameNumber == frameNumber) {
			return i;
		}
	}
	return -1;
}

static int FindMaxFrameNumber(AppState* app) {
	int maxFrame = 0;
	for (int i = 0; i < app->character->animation.frameCount; i++) {
		if (app->character->animation.frames[i].frameNumber > maxFrame) {
			maxFrame = app->character->animation.frames[i].frameNumber;
		}
	}
	return maxFrame;
}

static bool FrameExists(AppState* app, int frameNumber) {
	for (int i = 0; i < app->character->animation.frameCount; i++) {
		if (app->character->animation.frames[i].frameNumber == frameNumber) {
			return true;
		}
	}
	return false;
}

static int FindPreviousKeyframe(AppState* app, int fromFrame) {
	for (int i = fromFrame - 1; i >= 0; i--) {
		int frameIndex = FindFrameIndexByNumber(app, i);
		if (frameIndex != -1 && app->character->animation.frames[frameIndex].isOriginalKeyframe) {
			return i;
		}
	}
	return -1;
}

static int FindNextKeyframe(AppState* app, int fromFrame) {
	int maxFrameNumber = FindMaxFrameNumber(app);
	for (int i = fromFrame + 1; i <= maxFrameNumber; i++) {
		int frameIndex = FindFrameIndexByNumber(app, i);
		if (frameIndex != -1 && app->character->animation.frames[frameIndex].isOriginalKeyframe) {
			return i;
		}
	}
	return -1;
}

static bool IsCurrentFrameKeyframe(AppState* app) {
	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return false;
	return app->character->animation.frames[currentFrame].isOriginalKeyframe;
}

static Rectangle GetTimelineRect(AppState* app) {
	float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
	int margin = (int)(TIMELINE_MARGIN * scale);
	int uiHeight = (int)(UI_HEIGHT * scale);
	return (Rectangle){
		(float)margin,
			(float)(app->screenHeight - uiHeight + (int)(50 * scale)),
			(float)(app->screenWidth - margin * 2),
			(float)(int)(40 * scale)};
}

// ============================================================================
// CAMERA GIZMO
// ============================================================================

typedef struct {
	const char* name;
	float yaw;
	float pitch;
	Color color;
} SnapPoint;

static const SnapPoint SNAP_POINTS[] = {
	{"FRONT",    0.0f   * PI / 180.0f,  0.0f, (Color){255,  80, 120, 255}},
	{"DIAG45",   45.0f  * PI / 180.0f,  0.0f, (Color){255, 160,  40, 255}},
	{"SIDE90",   90.0f  * PI / 180.0f,  0.0f, (Color){240, 220,  30, 255}},
	{"BACK135",  135.0f * PI / 180.0f,  0.0f, (Color){ 80, 220,  80, 255}},
	{"BACK",     180.0f * PI / 180.0f,  0.0f, (Color){ 30, 200, 160, 255}},
	{"BACK",    -180.0f * PI / 180.0f,  0.0f, (Color){ 30, 200, 160, 255}},
	{"BACK315",  -45.0f * PI / 180.0f,  0.0f, (Color){160,  60, 240, 255}},
	{"SIDE270",  -90.0f * PI / 180.0f,  0.0f, (Color){ 40, 140, 255, 255}},
	{"DIAG225", -135.0f * PI / 180.0f,  0.0f, (Color){ 30, 220, 210, 255}},

	{"HIGH-0",    0.0f   * PI / 180.0f, 30.0f * PI / 180.0f, (Color){255, 140, 160, 255}},
	{"HIGH-45",   45.0f  * PI / 180.0f, 30.0f * PI / 180.0f, (Color){255, 190, 120, 255}},
	{"HIGH-90",   90.0f  * PI / 180.0f, 30.0f * PI / 180.0f, (Color){230, 220,  80, 255}},
	{"HIGH-135",  135.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){130, 230, 120, 255}},
	{"HIGH-180",  180.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){ 80, 220, 180, 255}},
	{"HIGH-180", -180.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){ 80, 220, 180, 255}},
	{"HIGH-225", -135.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){130, 150, 240, 255}},
	{"HIGH-270",  -90.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){100, 180, 255, 255}},
	{"HIGH-315",  -45.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){210, 130, 255, 255}},

	{"LOW-0",    0.0f   * PI / 180.0f, -30.0f * PI / 180.0f, (Color){240,  60,  90, 255}},
	{"LOW-45",   45.0f  * PI / 180.0f, -30.0f * PI / 180.0f, (Color){255, 130,  30, 255}},
	{"LOW-90",   90.0f  * PI / 180.0f, -30.0f * PI / 180.0f, (Color){210, 170,  20, 255}},
	{"LOW-135",  135.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){ 40, 190,  60, 255}},
	{"LOW-180",  180.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){ 20, 190, 180, 255}},
	{"LOW-180", -180.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){ 20, 190, 180, 255}},
	{"LOW-225", -135.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){ 80, 100, 220, 255}},
	{"LOW-270",  -90.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){ 40, 120, 240, 255}},
	{"LOW-315",  -45.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){220,  60, 180, 255}},

	{"TOP",    0.0f, MAX_PITCH, (Color){240, 240, 255, 255}},
	{"BOTTOM", 0.0f, MIN_PITCH, (Color){ 60,  60,  80, 255}},
};

static const int SNAP_POINT_COUNT = 29;

static Rectangle GetGizmoRect(AppState* app) {
	float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);

	int gizmoSize = (int)(280 * scale);

	Rectangle timeline = GetTimelineRect(app);
	int margin = (int)(10 * scale);
	int topMargin = (int)(30 * scale);

	int gizmoX = margin;
	int gizmoY = (int)(timeline.y - gizmoSize - topMargin);

	return (Rectangle){
		(float)gizmoX,
			(float)gizmoY,
			(float)gizmoSize,
			(float)gizmoSize};
}

static void SnapToNearestPoint(AppState* app) {
	float bestDistance = GIZMO_SNAP_THRESHOLD;
	int bestSnapIndex = -1;

	for (int i = 0; i < SNAP_POINT_COUNT; i++) {
		float yawDiff = fabsf(app->orbitYaw - SNAP_POINTS[i].yaw);
		float pitchDiff = fabsf(app->orbitPitch - SNAP_POINTS[i].pitch);

		if (yawDiff > PI) yawDiff = 2 * PI - yawDiff;

		float normalizedYaw = yawDiff / PI;
		float normalizedPitch = pitchDiff / (MAX_PITCH * 2);

		float distance = sqrtf(normalizedYaw * normalizedYaw + normalizedPitch * normalizedPitch);

		if (distance < bestDistance) {
			bestDistance = distance;
			bestSnapIndex = i;
		}
	}

	if (bestSnapIndex != -1) {
		app->orbitYaw = SNAP_POINTS[bestSnapIndex].yaw;
		app->orbitPitch = SNAP_POINTS[bestSnapIndex].pitch;
	}
}

static void DrawCameraGizmo(AppState* app) {
	if (!app->showUI || app->camMode != 1) return;

	Rectangle gizmoRect = GetGizmoRect(app);
	float scale = gizmoRect.width / 280.0f;

	Vector2 center = {
		gizmoRect.x + gizmoRect.width / 2,
		gizmoRect.y + gizmoRect.height / 2};

	float circleRadius = gizmoRect.width * 0.42f;
	float dotRadius = gizmoRect.width * 0.035f;
	float snapPointRadius = gizmoRect.width * 0.025f;

	DrawRectangleRec(gizmoRect, (Color){40, 40, 40, 200});
	DrawRectangleLinesEx(gizmoRect, 2, BLACK);

	DrawCircleV(center, circleRadius, (Color){60, 60, 60, 255});
	DrawCircleLines((int)center.x, (int)center.y, circleRadius, LIGHTGRAY);

	float highRing = circleRadius * 0.6f;
	float lowRing = circleRadius * 0.6f;

	DrawCircleLines((int)center.x, (int)(center.y - circleRadius * 0.3f), highRing * 0.3f, Fade(GREEN, 0.5f));
	DrawCircleLines((int)center.x, (int)(center.y + circleRadius * 0.3f), lowRing * 0.3f, Fade(YELLOW, 0.5f));

	for (int i = 0; i < SNAP_POINT_COUNT; i++) {
		float normalizedYaw = SNAP_POINTS[i].yaw / PI;
		float normalizedPitch = SNAP_POINTS[i].pitch / (MAX_PITCH * 1.2f);

		float snapX = center.x + normalizedYaw * (circleRadius - snapPointRadius * 3);
		float snapY = center.y - normalizedPitch * (circleRadius - snapPointRadius * 3);

		float yawDiff = fabsf(app->orbitYaw - SNAP_POINTS[i].yaw);
		float pitchDiff = fabsf(app->orbitPitch - SNAP_POINTS[i].pitch);
		if (yawDiff > PI) yawDiff = 2 * PI - yawDiff;

		bool isNear = (yawDiff < GIZMO_SNAP_THRESHOLD * 2) &&
			(pitchDiff < GIZMO_SNAP_THRESHOLD * 2);

		float currentSnapRadius = isNear ? snapPointRadius * 1.5f : snapPointRadius;

		DrawCircleV((Vector2){snapX, snapY}, currentSnapRadius,
				isNear ? ColorBrightness(SNAP_POINTS[i].color, 0.3f) : SNAP_POINTS[i].color);
		DrawCircleLines((int)snapX, (int)snapY, currentSnapRadius, WHITE);

		if (isNear) {
			int textSize = (int)(10 * scale);
			if (textSize < 8) textSize = 8;
			if (textSize > 14) textSize = 14;
			int textW = MeasureText(SNAP_POINTS[i].name, textSize);
			DrawRectangle((int)snapX - textW / 2 - 2, (int)snapY - (int)(25 * scale),
					textW + 4, textSize + 2, (Color){0, 0, 0, 180});
			DrawText(SNAP_POINTS[i].name, (int)snapX - textW / 2, (int)snapY - (int)(23 * scale), textSize, WHITE);
		}
	}

	DrawLineEx(
			(Vector2){center.x - circleRadius, center.y},
			(Vector2){center.x + circleRadius, center.y},
			2, (Color){100, 100, 100, 255});
	DrawLineEx(
			(Vector2){center.x, center.y - circleRadius},
			(Vector2){center.x, center.y + circleRadius},
			2, (Color){100, 100, 100, 255});

	float normalizedYaw = app->orbitYaw / PI;
	float normalizedPitch = app->orbitPitch / (MAX_PITCH * 1.2f);
	float dotX = center.x + normalizedYaw * (circleRadius - dotRadius * 2);
	float dotY = center.y - normalizedPitch * (circleRadius - dotRadius * 2);
	Vector2 dotPos = {dotX, dotY};

	float distFromCenter = Vector2Distance(dotPos, center);
	if (distFromCenter > circleRadius - dotRadius) {
		Vector2 direction = Vector2Normalize(Vector2Subtract(dotPos, center));
		dotPos = Vector2Add(center, Vector2Scale(direction, circleRadius - dotRadius));
	}

	Color dotColor = app->editor.isDraggingGizmo ? ORANGE : SKYBLUE;
	DrawCircleV(dotPos, dotRadius, dotColor);
	DrawCircleLines((int)dotPos.x, (int)dotPos.y, dotRadius, WHITE);
	DrawCircleV(dotPos, dotRadius * 0.3f, WHITE);

	int titleSize = (int)(16 * scale);
	if (titleSize < 12) titleSize = 12;
	if (titleSize > 20) titleSize = 20;
	DrawText("CAMERA VIEW", (int)(gizmoRect.x + 10 * scale), (int)(gizmoRect.y + 8 * scale), titleSize, YELLOW);

	int angleTextSize = (int)(11 * scale);
	if (angleTextSize < 9) angleTextSize = 9;
	if (angleTextSize > 14) angleTextSize = 14;

	char yawText[32], pitchText[32];
	snprintf(yawText, sizeof(yawText), "Yaw: %.0f°", app->orbitYaw * 180.0f / PI);
	snprintf(pitchText, sizeof(pitchText), "Pitch: %.0f°", app->orbitPitch * 180.0f / PI);
	DrawText(yawText, (int)(gizmoRect.x + 10 * scale), (int)(gizmoRect.y + gizmoRect.height - 40 * scale), angleTextSize, WHITE);
	DrawText(pitchText, (int)(gizmoRect.x + 10 * scale), (int)(gizmoRect.y + gizmoRect.height - 25 * scale), angleTextSize, WHITE);
}

static void UpdateCameraGizmo(AppState* app) {
	if (!app->showUI || app->camMode != 1) {
		app->editor.isDraggingGizmo = false;
		return;
	}

	Rectangle gizmoRect = GetGizmoRect(app);
	Vector2 mousePos = GetMousePosition();
	float circleRadius = gizmoRect.width * 0.42f;
	float snapPointRadius = gizmoRect.width * 0.025f;

	Vector2 center = {
		gizmoRect.x + gizmoRect.width / 2,
		gizmoRect.y + gizmoRect.height / 2};

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, gizmoRect)) {
		for (int i = 0; i < SNAP_POINT_COUNT; i++) {
			float normalizedYaw = SNAP_POINTS[i].yaw / PI;
			float normalizedPitch = SNAP_POINTS[i].pitch / (MAX_PITCH * 1.2f);
			float snapX = center.x + normalizedYaw * (circleRadius - snapPointRadius * 3);
			float snapY = center.y - normalizedPitch * (circleRadius - snapPointRadius * 3);
			Vector2 snapPos = {snapX, snapY};

			float distToSnap = Vector2Distance(mousePos, snapPos);
			float detectionRadius = snapPointRadius * 3.0f;

			if (distToSnap < detectionRadius) {
				app->orbitYaw = SNAP_POINTS[i].yaw;
				app->orbitPitch = SNAP_POINTS[i].pitch;
				return;
			}
		}
		app->editor.isDraggingGizmo = true;
	}

	if (app->editor.isDraggingGizmo && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
		Vector2 offset = Vector2Subtract(mousePos, center);
		float dist = Vector2Length(offset);
		if (dist > circleRadius - snapPointRadius) {
			offset = Vector2Scale(Vector2Normalize(offset), circleRadius - snapPointRadius);
		}

		float normalizedX = offset.x / (circleRadius - snapPointRadius);
		float normalizedY = -offset.y / (circleRadius - snapPointRadius);

		app->orbitYaw = normalizedX * PI;
		app->orbitPitch = Clamp(normalizedY * MAX_PITCH * 1.2f, MIN_PITCH, MAX_PITCH);

		SnapToNearestPoint(app);
	}

	if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
		if (app->editor.isDraggingGizmo) {
			SnapToNearestPoint(app);
		}
		app->editor.isDraggingGizmo = false;
	}
}

// ============================================================================
// KEYFRAME MANAGEMENT FUNCTIONS
// ============================================================================

static void PromoteFrameToKeyframe(AppState* app) {
	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
	AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	if (frame->isOriginalKeyframe) return;
	UndoAction action = {0};
	action.type = UNDO_FRAME_PROMOTE;
	action.promotedFrameNumber = frame->frameNumber;
	PushUndoAction(&app->editor.undoHistory, action);
	frame->isOriginalKeyframe = true;
	app->editor.needsSave = true;
}

static void RecalculateInterpolatedFrames(AppState* app, int keyframeA, int keyframeB) {
	if (keyframeA < 0 || keyframeB < 0 || keyframeB <= keyframeA) return;
	int indexA = FindFrameIndexByNumber(app, keyframeA);
	int indexB = FindFrameIndexByNumber(app, keyframeB);
	if (indexA == -1 || indexB == -1) return;
	AnimationFrame* frameA = &app->character->animation.frames[indexA];
	AnimationFrame* frameB = &app->character->animation.frames[indexB];
	for (int frameNum = keyframeA + 1; frameNum < keyframeB; frameNum++) {
		int interpIndex = FindFrameIndexByNumber(app, frameNum);
		if (interpIndex == -1) continue;
		AnimationFrame* interpFrame = &app->character->animation.frames[interpIndex];
		if (!interpFrame->isOriginalKeyframe) {
			float t = (float)(frameNum - keyframeA) / (float)(keyframeB - keyframeA);
			for (int p = 0; p < frameA->personCount && p < frameB->personCount; p++) {
				Person* personA = &frameA->persons[p];
				Person* personB = &frameB->persons[p];
				Person* interpPerson = &interpFrame->persons[p];
				if (!personA->active || !personB->active) continue;
				for (int bA = 0; bA < personA->boneCount; bA++) {
					Bone* boneA = &personA->bones[bA];
					if (!boneA->position.valid) continue;
					for (int bB = 0; bB < personB->boneCount; bB++) {
						Bone* boneB = &personB->bones[bB];
						if (strcmp(boneA->name, boneB->name) == 0 && boneB->position.valid) {
							for (int bI = 0; bI < interpPerson->boneCount; bI++) {
								Bone* boneInterp = &interpPerson->bones[bI];
								if (strcmp(boneInterp->name, boneA->name) == 0) {
									boneInterp->position.position = Vector3Lerp(
											boneA->position.position,
											boneB->position.position,
											t);
									boneInterp->position.valid = true;
									boneInterp->position.confidence =
										boneA->position.confidence * (1.0f - t) +
										boneB->position.confidence * t;
									break;
								}
							}
							break;
						}
					}
				}
			}
		}
	}
}

static void RecalculateAffectedInterpolations(AppState* app, int movedKeyframe) {
	int prevKeyframe = FindPreviousKeyframe(app, movedKeyframe);
	int nextKeyframe = FindNextKeyframe(app, movedKeyframe);
	if (prevKeyframe != -1) {
		RecalculateInterpolatedFrames(app, prevKeyframe, movedKeyframe);
	}
	if (nextKeyframe != -1) {
		RecalculateInterpolatedFrames(app, movedKeyframe, nextKeyframe);
	}
}

// ============================================================================
// GEOMETRY HELPER FUNCTIONS
// ============================================================================

static bool RayPlaneIntersection(Ray ray, Vector3 planePoint, Vector3 planeNormal, Vector3* outPoint) {
	float denom = Vector3DotProduct(planeNormal, ray.direction);
	if (fabsf(denom) < 0.0001f) return false;
	Vector3 diff = Vector3Subtract(planePoint, ray.position);
	float t = Vector3DotProduct(diff, planeNormal) / denom;
	if (t < 0) return false;
	*outPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
	return true;
}

// ============================================================================
// TIMELINE KEYFRAME DRAGGING
// ============================================================================

static void EnsureFrameExists(AppState* app, int frameNumber) {
	int existingIndex = FindFrameIndexByNumber(app, frameNumber);
	if (existingIndex != -1) return;
	int prevKeyframe = -1;
	int nextKeyframe = -1;
	for (int i = frameNumber - 1; i >= 0; i--) {
		int idx = FindFrameIndexByNumber(app, i);
		if (idx != -1 && app->character->animation.frames[idx].isOriginalKeyframe) {
			prevKeyframe = i;
			break;
		}
	}
	int maxFrame = FindMaxFrameNumber(app);
	for (int i = frameNumber + 1; i <= maxFrame; i++) {
		int idx = FindFrameIndexByNumber(app, i);
		if (idx != -1 && app->character->animation.frames[idx].isOriginalKeyframe) {
			nextKeyframe = i;
			break;
		}
	}
	if (prevKeyframe == -1 || nextKeyframe == -1) return;
	int prevIdx = FindFrameIndexByNumber(app, prevKeyframe);
	int nextIdx = FindFrameIndexByNumber(app, nextKeyframe);
	if (prevIdx == -1 || nextIdx == -1) return;
	if (app->character->animation.frameCount >= app->character->animation.maxFrames) return;
	AnimationFrame* prevFrame = &app->character->animation.frames[prevIdx];
	AnimationFrame* nextFrame = &app->character->animation.frames[nextIdx];
	int insertPos = app->character->animation.frameCount;
	for (int i = 0; i < app->character->animation.frameCount; i++) {
		if (app->character->animation.frames[i].frameNumber > frameNumber) {
			insertPos = i;
			break;
		}
	}
	for (int i = app->character->animation.frameCount; i > insertPos; i--) {
		app->character->animation.frames[i] = app->character->animation.frames[i - 1];
	}
	AnimationFrame* newFrame = &app->character->animation.frames[insertPos];
	memset(newFrame, 0, sizeof(AnimationFrame));
	newFrame->frameNumber = frameNumber;
	newFrame->valid = true;
	newFrame->isOriginalKeyframe = false;
	newFrame->personCount = prevFrame->personCount;
	float t = (float)(frameNumber - prevKeyframe) / (float)(nextKeyframe - prevKeyframe);
	for (int p = 0; p < prevFrame->personCount && p < nextFrame->personCount; p++) {
		Person* prevPerson = &prevFrame->persons[p];
		Person* nextPerson = &nextFrame->persons[p];
		Person* newPerson = &newFrame->persons[p];
		newPerson->active = prevPerson->active && nextPerson->active;
		newPerson->boneCount = prevPerson->boneCount;
		for (int b = 0; b < prevPerson->boneCount; b++) {
			Bone* prevBone = &prevPerson->bones[b];
			strncpy(newPerson->bones[b].name, prevBone->name, MAX_BONE_NAME_LENGTH - 1);
			for (int nb = 0; nb < nextPerson->boneCount; nb++) {
				Bone* nextBone = &nextPerson->bones[nb];
				if (strcmp(prevBone->name, nextBone->name) == 0 &&
						prevBone->position.valid && nextBone->position.valid) {
					newPerson->bones[b].position.position = Vector3Lerp(
							prevBone->position.position,
							nextBone->position.position,
							t);
					newPerson->bones[b].position.valid = true;
					newPerson->bones[b].position.confidence =
						prevBone->position.confidence * (1.0f - t) +
						nextBone->position.confidence * t;
					break;
				}
			}
		}
	}
	app->character->animation.frameCount++;
	app->character->maxFrames = app->character->animation.frameCount;
}

static void MoveKeyframeInTimeline(AppState* app, int fromFrameNumber, int toFrameNumber) {
	if (fromFrameNumber == toFrameNumber) return;
	int fromIndex = FindFrameIndexByNumber(app, fromFrameNumber);
	if (fromIndex == -1) return;
	AnimationFrame* sourceFrame = &app->character->animation.frames[fromIndex];
	if (!sourceFrame->isOriginalKeyframe) return;
	EnsureFrameExists(app, toFrameNumber);
	int toIndex = FindFrameIndexByNumber(app, toFrameNumber);
	if (toIndex == -1) return;
	AnimationFrame* targetFrame = &app->character->animation.frames[toIndex];
	if (targetFrame->isOriginalKeyframe && toIndex != fromIndex) return;
	targetFrame->isOriginalKeyframe = true;
	targetFrame->frameNumber = toFrameNumber;
	for (int p = 0; p < sourceFrame->personCount && p < MAX_PERSONS; p++) {
		Person* sourcePerson = &sourceFrame->persons[p];
		Person* targetPerson = &targetFrame->persons[p];
		targetPerson->active = sourcePerson->active;
		targetPerson->boneCount = sourcePerson->boneCount;
		for (int b = 0; b < sourcePerson->boneCount && b < MAX_BONES_PER_PERSON; b++) {
			targetPerson->bones[b] = sourcePerson->bones[b];
		}
	}
	if (fromIndex != toIndex) {
		sourceFrame->isOriginalKeyframe = false;
		int prevKeyframe = FindPreviousKeyframe(app, fromFrameNumber);
		int nextKeyframe = FindNextKeyframe(app, fromFrameNumber);
		if (prevKeyframe != -1 && nextKeyframe != -1) {
			RecalculateInterpolatedFrames(app, prevKeyframe, nextKeyframe);
		} else {
			BonesDeleteFrame(app, &app->character->animation, fromIndex);
			app->character->maxFrames = app->character->animation.frameCount;
		}
	}
	app->character->forceUpdate = true;
}

static void UpdateTimelineKeyframeDragging(AppState* app) {
	if (!app->editor.showTimeline || !app->character->animation.isLoaded) return;

	Rectangle timeline = GetTimelineRect(app);
	Vector2 mousePos = GetMousePosition();
	int maxFrameNumber = FindMaxFrameNumber(app);
	float frameWidth = timeline.width / (float)(maxFrameNumber + 1);

	if (IsKeyDown(KEY_LEFT_CONTROL) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		if (CheckCollisionPointRec(mousePos, timeline)) {
			int clickedFrameNumber = (int)((mousePos.x - timeline.x) / frameWidth);
			if (clickedFrameNumber >= 0 && clickedFrameNumber <= maxFrameNumber) {
				int frameIndex = FindFrameIndexByNumber(app, clickedFrameNumber);
				if (frameIndex != -1) {
					AnimationFrame* frame = &app->character->animation.frames[frameIndex];
					if (frame->isOriginalKeyframe) {
						app->editor.isDraggingKeyframe = true;
						app->editor.draggedKeyframeNumber = clickedFrameNumber;
						app->editor.draggedKeyframeData = *frame;
					}
				}
			}
		}
	}

	if (app->editor.isDraggingKeyframe && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
		if (mousePos.x >= timeline.x && mousePos.x <= timeline.x + timeline.width) {
			int targetFrameNumber = (int)((mousePos.x - timeline.x) / frameWidth);
			if (targetFrameNumber >= 0 && targetFrameNumber <= maxFrameNumber) {
				if (targetFrameNumber != app->editor.draggedKeyframeNumber) {
					int targetIndex = FindFrameIndexByNumber(app, targetFrameNumber);
					bool canMove = true;

					if (targetIndex != -1) {
						AnimationFrame* targetFrame = &app->character->animation.frames[targetIndex];
						if (targetFrame->isOriginalKeyframe) {
							canMove = false;
						}
					} else {
						EnsureFrameExists(app, targetFrameNumber);
						targetIndex = FindFrameIndexByNumber(app, targetFrameNumber);
						if (targetIndex == -1) {
							canMove = false;
						}
					}

					if (canMove) {
						MoveKeyframeInTimeline(app, app->editor.draggedKeyframeNumber, targetFrameNumber);
						app->editor.draggedKeyframeNumber = targetFrameNumber;
					}
				}
			}
		}
	}

	if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && app->editor.isDraggingKeyframe) {
		int oldFrameNumber = app->editor.draggedKeyframeData.frameNumber;
		int newFrameNumber = app->editor.draggedKeyframeNumber;

		if (oldFrameNumber != newFrameNumber) {
			UndoAction action = {0};
			action.type = UNDO_KEYFRAME_MOVE;
			action.oldFrameNumber = oldFrameNumber;
			action.newFrameNumber = newFrameNumber;
			PushUndoAction(&app->editor.undoHistory, action);
		}

		app->editor.isDraggingKeyframe = false;

		int prevKeyframe = FindPreviousKeyframe(app, app->editor.draggedKeyframeNumber);
		int nextKeyframe = FindNextKeyframe(app, app->editor.draggedKeyframeNumber);
		if (prevKeyframe != -1) {
			RecalculateInterpolatedFrames(app, prevKeyframe, app->editor.draggedKeyframeNumber);
		}
		if (nextKeyframe != -1) {
			RecalculateInterpolatedFrames(app, app->editor.draggedKeyframeNumber, nextKeyframe);
		}

		int frameIndex = FindFrameIndexByNumber(app, app->editor.draggedKeyframeNumber);
		if (frameIndex != -1) {
			SetCharacterFrame(app->character, frameIndex);
		}

		app->editor.needsSave = true;
	}
}

// ============================================================================
// BONE MANIPULATION FUNCTIONS
// ============================================================================

static void MoveBoneInFrame(AppState* app, int frameNumber, const char* boneName, Vector3 newPosition) {
	int frameIndex = FindFrameIndexByNumber(app, frameNumber);
	if (frameIndex == -1) return;
	AnimationFrame* frame = &app->character->animation.frames[frameIndex];
	for (int p = 0; p < frame->personCount; p++) {
		Person* person = &frame->persons[p];
		if (!person->active) continue;
		for (int b = 0; b < person->boneCount; b++) {
			Bone* bone = &person->bones[b];
			if (strcmp(bone->name, boneName) == 0 && bone->position.valid) {
				bone->position.position = newPosition;
				app->editor.needsSave = true;
				app->character->forceUpdate = true;
				return;
			}
		}
	}
}

static void RotateBoneInFrame(AppState* app, int frameNumber, const char* boneName, float newRotation) {
	int frameIndex = FindFrameIndexByNumber(app, frameNumber);
	if (frameIndex == -1) return;
	AnimationFrame* frame = &app->character->animation.frames[frameIndex];
	for (int p = 0; p < frame->personCount; p++) {
		Person* person = &frame->persons[p];
		if (!person->active) continue;
		for (int b = 0; b < person->boneCount; b++) {
			Bone* bone = &person->bones[b];
			if (strcmp(bone->name, boneName) == 0 && bone->position.valid) {
				bone->rotation = newRotation;
				app->editor.needsSave = true;
				app->character->forceUpdate = true;
				return;
			}
		}
	}
}

static float GetBoneRotation(AppState* app, int frameNumber, const char* boneName) {
	int frameIndex = FindFrameIndexByNumber(app, frameNumber);
	if (frameIndex == -1) return 0.0f;
	const AnimationFrame* frame = &app->character->animation.frames[frameIndex];
	for (int p = 0; p < frame->personCount; p++) {
		const Person* person = &frame->persons[p];
		if (!person->active) continue;
		for (int b = 0; b < person->boneCount; b++) {
			const Bone* bone = &person->bones[b];
			if (strcmp(bone->name, boneName) == 0 && bone->position.valid) {
				return bone->rotation;
			}
		}
	}
	return 0.0f;
}

static void MoveBoneWithMouse(AppState* app) {
	if (!app->editor.hasBoneSelected || !app->editor.isDraggingBone) return;
	if (!app->character->animation.isLoaded) return;
	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
	Camera camera = app->character->renderer->camera;
	Vector2 mousePos = GetMousePosition();
	Ray mouseRay = GetMouseRay(mousePos, camera);
	Vector3 camToPoint = Vector3Subtract(app->editor.selectedBonePosition, camera.position);
	Vector3 planeNormal = Vector3Normalize(camToPoint);
	Vector3 newPosition;
	if (RayPlaneIntersection(mouseRay, app->editor.selectedBonePosition, planeNormal, &newPosition)) {
		int currentFrameNumber = GetCurrentFrameNumber(app);
		bool hasMultiSelection = (app->editor.selectionStart != -1 && 
				app->editor.selectionEnd != -1 && 
				app->editor.selectionStart != app->editor.selectionEnd);

		if (hasMultiSelection) {
			for (int frameNum = app->editor.selectionStart; frameNum <= app->editor.selectionEnd; frameNum++) {
				if (FrameExists(app, frameNum)) {
					MoveBoneInFrame(app, frameNum, app->editor.selectedBoneName, newPosition);
					int frameIndex = FindFrameIndexByNumber(app, frameNum);
					if (frameIndex != -1 && app->character->animation.frames[frameIndex].isOriginalKeyframe) {
						RecalculateAffectedInterpolations(app, frameNum);
					}
				}
			}
		} else {
			MoveBoneInFrame(app, currentFrameNumber, app->editor.selectedBoneName, newPosition);
			if (IsCurrentFrameKeyframe(app)) {
				RecalculateAffectedInterpolations(app, currentFrameNumber);
			}
		}

		app->editor.selectedBonePosition = newPosition;
	}
}

static int FindAllBonesUnderMouse(AppState* app, BoneCandidate* candidates, int maxCandidates) {
	if (!app->character->animation.isLoaded) return 0;
	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return 0;
	const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	Camera camera = app->character->renderer->camera;
	Vector2 mousePos = GetMousePosition();
	int candidateCount = 0;
	float scale = fminf((float)app->screenWidth / 1920.0f, (float)app->screenHeight / 1080.0f);
	const float SELECTION_RADIUS = 50.0f * scale;
	for (int p = 0; p < frame->personCount; p++) {
		const Person* person = &frame->persons[p];
		if (!person->active) continue;
		for (int b = 0; b < person->boneCount; b++) {
			const Bone* bone = &person->bones[b];
			if (!bone->position.valid) continue;
			Vector2 screenPos = GetWorldToScreen(bone->position.position, camera);
			float dist = Vector2Distance(mousePos, screenPos);
			if (dist < SELECTION_RADIUS && candidateCount < maxCandidates) {
				candidates[candidateCount].distance = dist;
				strncpy(candidates[candidateCount].boneName, bone->name, 63);
				candidates[candidateCount].boneName[63] = '\0';
				candidates[candidateCount].personIndex = p;
				candidates[candidateCount].bonePos = bone->position.position;
				candidateCount++;
			}
		}
	}
	for (int i = 0; i < candidateCount - 1; i++) {
		for (int j = 0; j < candidateCount - i - 1; j++) {
			if (candidates[j].distance > candidates[j + 1].distance) {
				BoneCandidate temp = candidates[j];
				candidates[j] = candidates[j + 1];
				candidates[j + 1] = temp;
			}
		}
	}
	return candidateCount;
}

static bool IsInGroup(AppState* app, const char* boneName) {
	for (int i = 0; i < app->editor.groupSelectedCount; i++) {
		if (strcmp(app->editor.groupSelectedBones[i], boneName) == 0) return true;
	}
	return false;
}

static void UpdateBoneRotationHandle(AppState* app) {
	if (!app->editor.hasBoneSelected || !app->character->animation.isLoaded) return;
	if (app->editor.isDraggingBone || app->editor.isDraggingGroup) return;
	if (app->debug.showOrientation) return;

	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;

	const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	Camera camera = app->character->renderer->camera;
	Vector2 mousePos = GetMousePosition();

	Vector3 boneWorldPos = {0};
	bool found = false;
	for (int p = 0; p < frame->personCount && !found; p++) {
		const Person* person = &frame->persons[p];
		if (!person->active) continue;
		for (int b = 0; b < person->boneCount; b++) {
			const Bone* bone = &person->bones[b];
			if (strcmp(bone->name, app->editor.selectedBoneName) == 0 && bone->position.valid) {
				boneWorldPos = bone->position.position;
				found = true;
				break;
			}
		}
	}
	if (!found) return;

	float scale = fminf((float)app->screenWidth / 1920.0f, (float)app->screenHeight / 1080.0f);
	float radius = ROTATION_HANDLE_RADIUS * scale;
	Vector2 boneScreen = GetWorldToScreen(boneWorldPos, camera);
	Vector2 handleOffset = {mousePos.x - boneScreen.x, mousePos.y - boneScreen.y};
	if (app->editor.isDraggingRotationHandle) {
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			float angle = atan2f(handleOffset.x, -handleOffset.y) * RAD2DEG;
			float delta = angle - app->editor.rotationHandleStartAngle;
			float newRotation = app->editor.rotationHandleStartBoneRotation + delta;

			int currentFrameNumber = GetCurrentFrameNumber(app);
			if (IsInGroup(app, app->editor.selectedBoneName) && app->editor.groupSelectedCount > 1) {
				for (int i = 0; i < app->editor.groupSelectedCount; i++) {
					float startRot = app->editor.groupDragStartRotations[i];
					RotateBoneInFrame(app, currentFrameNumber, app->editor.groupSelectedBones[i], startRot + delta);
				}
			} else {
				RotateBoneInFrame(app, currentFrameNumber, app->editor.selectedBoneName, newRotation);
			}
		} else {
			int currentFrameNumber = GetCurrentFrameNumber(app);
			if (IsInGroup(app, app->editor.selectedBoneName) && app->editor.groupSelectedCount > 1) {
				UndoAction action = {0};
				action.type = UNDO_GROUP_TRANSFORM;
				action.frameNumber = currentFrameNumber;
				action.groupSnapshotCount = app->editor.groupSelectedCount;
				for (int i = 0; i < app->editor.groupSelectedCount; i++) {
					strncpy(action.groupSnapshots[i].boneName, app->editor.groupSelectedBones[i], MAX_BONE_NAME_LENGTH - 1);
					action.groupSnapshots[i].oldPosition = app->editor.groupDragStartPositions[i];
					action.groupSnapshots[i].newPosition = app->editor.groupDragStartPositions[i];
					action.groupSnapshots[i].oldRotation = app->editor.groupDragStartRotations[i];
					action.groupSnapshots[i].newRotation = GetBoneRotation(app, currentFrameNumber, app->editor.groupSelectedBones[i]);
				}
				PushUndoAction(&app->editor.undoHistory, action);
			} else {
				UndoAction action = {0};
				action.type = UNDO_BONE_ROTATE;
				strncpy(action.boneName, app->editor.selectedBoneName, 63);
				action.frameNumber = currentFrameNumber;
				action.oldRotation = app->editor.rotationHandleStartBoneRotation;
				action.newRotation = GetBoneRotation(app, currentFrameNumber, app->editor.selectedBoneName);
				PushUndoAction(&app->editor.undoHistory, action);
			}
			app->editor.isDraggingRotationHandle = false;
		}
		return;
	}

	float dotRadius = 8.0f * scale;
	float currentRotation = GetBoneRotation(app, GetCurrentFrameNumber(app), app->editor.selectedBoneName);
	float handleAngleRad = currentRotation * DEG2RAD;
	Vector2 handlePos = {
		boneScreen.x + sinf(handleAngleRad) * radius,
		boneScreen.y - cosf(handleAngleRad) * radius
	};

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
			Vector2Distance(mousePos, handlePos) < dotRadius * 2.0f &&
			!CheckCollisionPointRec(mousePos, GetTimelineRect(app)) &&
			!CheckCollisionPointRec(mousePos, GetGizmoRect(app))) {
		if (!IsCurrentFrameKeyframe(app)) PromoteFrameToKeyframe(app);
		app->editor.isDraggingRotationHandle = true;
		app->editor.rotationHandleStartAngle = atan2f(handleOffset.x, -handleOffset.y) * RAD2DEG;
		app->editor.rotationHandleStartBoneRotation = currentRotation;
		if (IsInGroup(app, app->editor.selectedBoneName) && app->editor.groupSelectedCount > 1) {
			int cfn = GetCurrentFrameNumber(app);
			for (int i = 0; i < app->editor.groupSelectedCount; i++) {
				app->editor.groupDragStartRotations[i] = GetBoneRotation(app, cfn, app->editor.groupSelectedBones[i]);
			}
		}
	}
}

static void DrawBoneRotationHandle(AppState* app) {
	(void)app;
}

static void DrawOnionSkin(AppState* app) {
	if (!app->debug.showOnionSkin || !app->character->animation.isLoaded) return;

	int currentFrameIndex = app->character->currentFrame;
	if (currentFrameIndex < 0 || currentFrameIndex >= app->character->animation.frameCount) return;
	int currentFrameNumber = app->character->animation.frames[currentFrameIndex].frameNumber;

	AnimatedCharacter* ch = app->character;
	int savedFrame = ch->currentFrame;

	for (int delta = -2; delta <= 2; delta++) {
		if (delta == 0) continue;
		int targetFrameNumber = currentFrameNumber + delta;
		if (targetFrameNumber < 0) continue;
		int targetIdx = FindFrameIndexByNumber(app, targetFrameNumber);
		if (targetIdx == -1) continue;
		if (!ch->animation.frames[targetIdx].valid) continue;

		ch->currentFrame             = targetIdx;
		ch->animation.currentFrame   = targetIdx;
		ch->renderBonesCount  = 0;
		ch->renderHeadsCount  = 0;
		ch->renderTorsosCount = 0;

		CollectBonesForRendering(&ch->animation, ch->renderer->camera,
			&ch->renderBones, &ch->renderBonesCount, &ch->renderBonesCapacity,
			ch->boneConfigs, ch->boneConfigCount, ch->textureSets, ch->ornaments);

		if (ch->renderHeadBillboards)
			CollectHeadsForRendering(&ch->animation, &ch->renderHeads,
				&ch->renderHeadsCount, &ch->renderHeadsCapacity,
				ch->boneConfigs, ch->boneConfigCount, ch->textureSets);

		if (ch->renderTorsoBillboards)
			CollectTorsosForRendering(&ch->animation, &ch->renderTorsos,
				&ch->renderTorsosCount, &ch->renderTorsosCapacity,
				ch->boneConfigs, ch->boneConfigCount, ch->textureSets);

		float alpha = 1.0f - (float)(abs(delta) - 1) * 0.35f;
		float a = alpha * 0.65f;
		unsigned char ar = (unsigned char)(a * (delta < 0 ? 100 : 255));
		unsigned char ag = (unsigned char)(a * (delta < 0 ? 200 : 160));
		unsigned char ab = (unsigned char)(a * (delta < 0 ? 255 :  60));
		unsigned char aa = (unsigned char)(a * 255.0f);
		g_quadRenderTint        = (Color){ar, ag, ab, aa};
		g_quadRenderNoDepthTest = true;

		BonesRenderer_RenderFrame(ch->renderer,
			ch->renderBones,  ch->renderBonesCount,
			ch->renderHeads,  ch->renderHeadsCount,
			ch->renderTorsos, ch->renderTorsosCount,
			ch->autoCenter, ch->autoCenterCalculated,
			NULL, 0);
	}

	g_quadRenderTint        = (Color){255, 255, 255, 255};
	g_quadRenderNoDepthTest = false;

	ch->currentFrame             = savedFrame;
	ch->animation.currentFrame   = savedFrame;
	ch->renderBonesCount  = 0;
	ch->renderHeadsCount  = 0;
	ch->renderTorsosCount = 0;

	CollectBonesForRendering(&ch->animation, ch->renderer->camera,
		&ch->renderBones, &ch->renderBonesCount, &ch->renderBonesCapacity,
		ch->boneConfigs, ch->boneConfigCount, ch->textureSets, ch->ornaments);

	if (ch->renderHeadBillboards)
		CollectHeadsForRendering(&ch->animation, &ch->renderHeads,
			&ch->renderHeadsCount, &ch->renderHeadsCapacity,
			ch->boneConfigs, ch->boneConfigCount, ch->textureSets);

	if (ch->renderTorsoBillboards)
		CollectTorsosForRendering(&ch->animation, &ch->renderTorsos,
			&ch->renderTorsosCount, &ch->renderTorsosCapacity,
			ch->boneConfigs, ch->boneConfigCount, ch->textureSets);
}

static void UpdateGroupDrag(AppState* app) {
	if (app->editor.groupSelectedCount < 2) return;
	if (!app->editor.hasBoneSelected) return;
	if (!app->character->animation.isLoaded) return;

	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;

	Camera camera = app->character->renderer->camera;
	Vector2 mousePos = GetMousePosition();
	Rectangle timeline = GetTimelineRect(app);
	Rectangle gizmoRect = GetGizmoRect(app);

	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) &&
			!CheckCollisionPointRec(mousePos, timeline) &&
			!CheckCollisionPointRec(mousePos, gizmoRect) &&
			IsInGroup(app, app->editor.selectedBoneName)) {
		if (!IsCurrentFrameKeyframe(app)) PromoteFrameToKeyframe(app);
		app->editor.isDraggingGroup = true;
		app->editor.groupDragWasPlaying = app->character->autoPlay;
		SetCharacterAutoPlay(app->character, false);
		app->editor.groupDragStartMouse = mousePos;

		int cfn = GetCurrentFrameNumber(app);
		const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
		for (int i = 0; i < app->editor.groupSelectedCount; i++) {
			app->editor.groupDragStartPositions[i] = (Vector3){0};
			app->editor.groupDragStartRotations[i] = 0.0f;
			for (int p = 0; p < frame->personCount; p++) {
				const Person* person = &frame->persons[p];
				if (!person->active) continue;
				for (int b = 0; b < person->boneCount; b++) {
					const Bone* bone = &person->bones[b];
					if (strcmp(bone->name, app->editor.groupSelectedBones[i]) == 0 && bone->position.valid) {
						app->editor.groupDragStartPositions[i] = bone->position.position;
						app->editor.groupDragStartRotations[i] = bone->rotation;
						break;
					}
				}
			}
		}
		int anchorIdx = -1;
		const AnimationFrame* frame2 = &app->character->animation.frames[currentFrame];
		for (int p = 0; p < frame2->personCount; p++) {
			const Person* person = &frame2->persons[p];
			if (!person->active) continue;
			for (int b = 0; b < person->boneCount; b++) {
				const Bone* bone = &person->bones[b];
				if (strcmp(bone->name, app->editor.selectedBoneName) == 0 && bone->position.valid) {
					app->editor.groupDragAnchorStart = bone->position.position;
					anchorIdx = 0;
					break;
				}
			}
			if (anchorIdx >= 0) break;
		}
		(void)cfn;
	}

	if (app->editor.isDraggingGroup && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
		Vector3 anchorWorld = app->editor.groupDragAnchorStart;
		Ray anchorRay = GetMouseRay(app->editor.groupDragStartMouse, camera);
		Vector3 camToAnchor = Vector3Subtract(anchorWorld, camera.position);
		Vector3 planeNormal = Vector3Normalize(camToAnchor);
		Vector3 anchorPlanePoint;
		if (!RayPlaneIntersection(anchorRay, anchorWorld, planeNormal, &anchorPlanePoint)) return;

		Ray currentRay = GetMouseRay(mousePos, camera);
		Vector3 currentPlanePoint;
		if (!RayPlaneIntersection(currentRay, anchorWorld, planeNormal, &currentPlanePoint)) return;

		Vector3 delta = Vector3Subtract(currentPlanePoint, anchorPlanePoint);
		int cfn = GetCurrentFrameNumber(app);
		for (int i = 0; i < app->editor.groupSelectedCount; i++) {
			Vector3 newPos = Vector3Add(app->editor.groupDragStartPositions[i], delta);
			MoveBoneInFrame(app, cfn, app->editor.groupSelectedBones[i], newPos);
		}
		if (IsCurrentFrameKeyframe(app)) RecalculateAffectedInterpolations(app, cfn);
		app->character->forceUpdate = true;
	}

	if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && app->editor.isDraggingGroup) {
		int cfn = GetCurrentFrameNumber(app);
		UndoAction action = {0};
		action.type = UNDO_GROUP_TRANSFORM;
		action.frameNumber = cfn;
		action.groupSnapshotCount = app->editor.groupSelectedCount;
		for (int i = 0; i < app->editor.groupSelectedCount; i++) {
			strncpy(action.groupSnapshots[i].boneName, app->editor.groupSelectedBones[i], MAX_BONE_NAME_LENGTH - 1);
			action.groupSnapshots[i].oldPosition = app->editor.groupDragStartPositions[i];
			action.groupSnapshots[i].newPosition = (Vector3){0};
			const AnimationFrame* frame3 = &app->character->animation.frames[app->character->currentFrame];
			for (int p = 0; p < frame3->personCount; p++) {
				const Person* person = &frame3->persons[p];
				if (!person->active) continue;
				for (int b = 0; b < person->boneCount; b++) {
					const Bone* bone = &person->bones[b];
					if (strcmp(bone->name, app->editor.groupSelectedBones[i]) == 0 && bone->position.valid) {
						action.groupSnapshots[i].newPosition = bone->position.position;
						break;
					}
				}
			}
			action.groupSnapshots[i].oldRotation = app->editor.groupDragStartRotations[i];
			action.groupSnapshots[i].newRotation = GetBoneRotation(app, cfn, app->editor.groupSelectedBones[i]);
		}
		PushUndoAction(&app->editor.undoHistory, action);
		app->editor.isDraggingGroup = false;
		if (app->editor.groupDragWasPlaying)
			SetCharacterAutoPlay(app->character, true);
	}
}

static void UpdateBoneSelection(AppState* app) {
	if (app->camMode != 1) {
		app->editor.hasBoneSelected = false;
		app->editor.isDraggingBone = false;
		return;
	}
	if (app->editor.isDraggingSlider || app->editor.isDraggingKeyframe) return;
	Vector2 mousePos = GetMousePosition();
	Rectangle timeline = GetTimelineRect(app);
	Rectangle gizmoRect = GetGizmoRect(app);
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
			!CheckCollisionPointRec(mousePos, timeline) &&
			!CheckCollisionPointRec(mousePos, gizmoRect) &&
			!IsKeyDown(KEY_LEFT_CONTROL) &&
			!app->editor.isDraggingRotationHandle) {
		BoneCandidate candidates[100];
		int candidateCount = FindAllBonesUnderMouse(app, candidates, 100);
		if (candidateCount > 0) {
			float currentTime = GetTime();
			float timeSinceLastClick = currentTime - app->editor.lastClickTime;
			float distanceFromLastClick = Vector2Distance(mousePos, app->editor.lastClickPos);
			const float DOUBLE_CLICK_TIME = 0.5f;
			const float DOUBLE_CLICK_DISTANCE = 10.0f;
			if (timeSinceLastClick < DOUBLE_CLICK_TIME &&
					distanceFromLastClick < DOUBLE_CLICK_DISTANCE &&
					candidateCount > 1) {
				app->editor.cycleIndex = (app->editor.cycleIndex + 1) % candidateCount;
			} else {
				app->editor.cycleIndex = 0;
			}
			BoneCandidate* selected = &candidates[app->editor.cycleIndex];

			if (IsKeyDown(KEY_LEFT_SHIFT)) {
				bool alreadyInGroup = false;
				for (int i = 0; i < app->editor.groupSelectedCount; i++) {
					if (strcmp(app->editor.groupSelectedBones[i], selected->boneName) == 0) {
						for (int j = i; j < app->editor.groupSelectedCount - 1; j++) {
							strncpy(app->editor.groupSelectedBones[j], app->editor.groupSelectedBones[j+1], MAX_BONE_NAME_LENGTH);
						}
						app->editor.groupSelectedCount--;
						alreadyInGroup = true;
						break;
					}
				}
				if (!alreadyInGroup && app->editor.groupSelectedCount < MAX_GROUP_BONES) {
					if (app->editor.hasBoneSelected && !IsInGroup(app, app->editor.selectedBoneName)) {
						strncpy(app->editor.groupSelectedBones[app->editor.groupSelectedCount], app->editor.selectedBoneName, MAX_BONE_NAME_LENGTH - 1);
						app->editor.groupSelectedCount++;
					}
					strncpy(app->editor.groupSelectedBones[app->editor.groupSelectedCount], selected->boneName, MAX_BONE_NAME_LENGTH - 1);
					app->editor.groupSelectedBones[app->editor.groupSelectedCount][MAX_BONE_NAME_LENGTH - 1] = '\0';
					app->editor.groupSelectedCount++;
				}
			} else {
				app->editor.groupSelectedCount = 0;
			}

			app->editor.hasBoneSelected = true;
			strncpy(app->editor.selectedBoneName, selected->boneName, 63);
			app->editor.selectedBoneName[63] = '\0';
			app->editor.selectedBonePersonIndex = selected->personIndex;
			app->editor.selectedBonePosition = selected->bonePos;
			app->editor.lastClickPos = mousePos;
			app->editor.lastClickTime = currentTime;
		} else {
			app->editor.hasBoneSelected = false;
			app->editor.cycleIndex = 0;
			app->editor.groupSelectedCount = 0;
		}
	}

	UpdateGroupDrag(app);

	if (app->editor.isDraggingGroup) return;

	if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && app->editor.hasBoneSelected) {
		if (!CheckCollisionPointRec(mousePos, timeline) && !CheckCollisionPointRec(mousePos, gizmoRect)) {
			BoneCandidate candidates[100];
			int candidateCount = FindAllBonesUnderMouse(app, candidates, 100);
			for (int i = 0; i < candidateCount; i++) {
				if (strcmp(candidates[i].boneName, app->editor.selectedBoneName) == 0) {
					if (!IsCurrentFrameKeyframe(app)) {
						PromoteFrameToKeyframe(app);
					}
					app->editor.isDraggingBone = true;
					app->editor.dragStartPos = candidates[i].bonePos;
					app->editor.dragStartMouse = mousePos;
					break;
				}
			}
		}
	}
	if (app->editor.isDraggingBone && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
		MoveBoneWithMouse(app);
	}
	if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT) && app->editor.isDraggingBone) {
		int currentFrameNumber = GetCurrentFrameNumber(app);
		UndoAction action = {0};
		action.type = UNDO_BONE_MOVE;
		strncpy(action.boneName, app->editor.selectedBoneName, 63);
		action.frameNumber = currentFrameNumber;
		action.oldPosition = app->editor.dragStartPos;
		action.newPosition = app->editor.selectedBonePosition;
		PushUndoAction(&app->editor.undoHistory, action);
		app->editor.isDraggingBone = false;
		RecalculateAffectedInterpolations(app, currentFrameNumber);
	}
}

// ============================================================================
// UI DRAWING FUNCTIONS
// ============================================================================

static bool Button(Rectangle bounds, const char* text, Color color) {
	bool isHovered = CheckCollisionPointRec(GetMousePosition(), bounds);
	bool isPressed = isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
	Color bgColor = isPressed ? ColorBrightness(color, -0.3f) :
		isHovered ? ColorBrightness(color, -0.1f) : color;
	DrawRectangleRec(bounds, bgColor);
	DrawRectangleLinesEx(bounds, 2, BLACK);
	int fontSize = (int)(bounds.height * 0.4f);
	if (fontSize < 10) fontSize = 10;
	if (fontSize > 20) fontSize = 20;

	int textWidth = MeasureText(text, fontSize);
	DrawText(text,
			(int)(bounds.x + (bounds.width - textWidth) / 2),
			(int)(bounds.y + (bounds.height - fontSize) / 2),
			fontSize, WHITE);
	return isPressed;
}

static bool ToggleButton(Rectangle bounds, const char* text, bool isActive, Color activeColor, Color inactiveColor) {
	bool isHovered = CheckCollisionPointRec(GetMousePosition(), bounds);
	bool isPressed = isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
	Color baseColor = isActive ? activeColor : inactiveColor;
	Color bgColor = isPressed ? ColorBrightness(baseColor, -0.3f) :
		isHovered ? ColorBrightness(baseColor, -0.1f) : baseColor;
	DrawRectangleRec(bounds, bgColor);
	DrawRectangleLinesEx(bounds, 2, isActive ? ColorBrightness(activeColor, -0.2f) : BLACK);
	int fontSize = (int)(bounds.height * 0.35f);
	if (fontSize < 10) fontSize = 10;
	if (fontSize > 18) fontSize = 18;

	int textWidth = MeasureText(text, fontSize);
	DrawText(text,
			(int)(bounds.x + (bounds.width - textWidth) / 2),
			(int)(bounds.y + (bounds.height - fontSize) / 2),
			fontSize, WHITE);
	return isPressed;
}

static bool IconButton(Rectangle bounds, const char* icon, Color color) {
	bool isHovered = CheckCollisionPointRec(GetMousePosition(), bounds);
	bool isPressed = isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
	Color bgColor = isPressed ? ColorBrightness(color, -0.3f) :
		isHovered ? ColorBrightness(color, -0.1f) : color;
	DrawRectangleRec(bounds, bgColor);
	DrawRectangleLinesEx(bounds, 2, BLACK);
	int fontSize = (int)(bounds.height * 0.5f);
	if (fontSize < 14) fontSize = 14;
	if (fontSize > 24) fontSize = 24;

	int textWidth = MeasureText(icon, fontSize);
	DrawText(icon,
			(int)(bounds.x + (bounds.width - textWidth) / 2),
			(int)(bounds.y + (bounds.height - fontSize) / 2),
			fontSize, WHITE);
	return isPressed;
}

static void DrawTextField(Rectangle bounds, const char* text, bool active) {
	DrawRectangleRec(bounds, active ? (Color){240, 240, 255, 255} : LIGHTGRAY);
	DrawRectangleLinesEx(bounds, 2, active ? BLUE : DARKGRAY);
	int fontSize = (int)(bounds.height * 0.5f);
	if (fontSize < 12) fontSize = 12;
	if (fontSize > 18) fontSize = 18;

	DrawText(text, (int)(bounds.x + 5), (int)(bounds.y + (bounds.height - fontSize) / 2), fontSize, BLACK);
}

static void DrawBoneSelectionFeedback(AppState* app) {
	if (!app->editor.hasBoneSelected) return;
	if (!app->character->animation.isLoaded) return;
	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
	const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	Camera camera = app->character->renderer->camera;

	if (app->editor.groupSelectedCount > 1) {
		char drawnNames[MAX_GROUP_BONES][MAX_BONE_NAME_LENGTH];
		int drawnCount = 0;
		BeginMode3D(camera);
		rlDisableDepthTest();
		for (int p = 0; p < frame->personCount; p++) {
			const Person* person = &frame->persons[p];
			if (!person->active) continue;
			for (int b = 0; b < person->boneCount; b++) {
				const Bone* bone = &person->bones[b];
				if (!bone->position.valid) continue;
				if (!IsInGroup(app, bone->name) || strcmp(bone->name, app->editor.selectedBoneName) == 0) continue;
				bool alreadyDrawn = false;
				for (int i = 0; i < drawnCount; i++) {
					if (strcmp(drawnNames[i], bone->name) == 0) { alreadyDrawn = true; break; }
				}
				if (alreadyDrawn) continue;
				strncpy(drawnNames[drawnCount], bone->name, MAX_BONE_NAME_LENGTH - 1);
				drawnNames[drawnCount][MAX_BONE_NAME_LENGTH - 1] = '\0';
				drawnCount++;
				DrawSphere(bone->position.position, 0.028f, (Color){255, 200, 0, 160});
				DrawSphereWires(bone->position.position, 0.031f, 8, 8, (Color){255, 230, 100, 255});
			}
		}
		rlEnableDepthTest();
		EndMode3D();
	}

	bool hasMultiSelection = (app->editor.selectionStart != -1 && 
			app->editor.selectionEnd != -1 && 
			app->editor.selectionStart != app->editor.selectionEnd);

	for (int p = 0; p < frame->personCount; p++) {
		const Person* person = &frame->persons[p];
		if (!person->active) continue;
		for (int b = 0; b < person->boneCount; b++) {
			const Bone* bone = &person->bones[b];
			if (strcmp(bone->name, app->editor.selectedBoneName) == 0 && bone->position.valid) {
				Vector2 screenPos = GetWorldToScreen(bone->position.position, camera);
				BeginMode3D(camera);
				rlDisableDepthTest();
				Color highlightColor;
				const char* modeText;
				if (app->editor.isDraggingKeyframe) {
					highlightColor = (Color){128, 0, 128, 140};
					modeText = "[DRAGGING KEYFRAME]";
				} else if (app->editor.isDraggingBone) {
					if (hasMultiSelection) {
						highlightColor = (Color){255, 165, 0, 140};
						int frameCount = app->editor.selectionEnd - app->editor.selectionStart + 1;
						static char multiText[64];
						snprintf(multiText, sizeof(multiText), "[MOVING %d FRAMES]", frameCount);
						modeText = multiText;
					} else {
						highlightColor = (Color){255, 0, 0, 140};
						modeText = "[DRAGGING BONE]";
					}
				} else if (IsCurrentFrameKeyframe(app)) {
					highlightColor = (Color){0, 255, 0, 140};
					modeText = "[KEYFRAME]";
				} else {
					highlightColor = (Color){255, 165, 0, 140};
					modeText = "[INTERPOLATED]";
				}
				DrawSphere(bone->position.position, 0.030f, highlightColor);
				DrawSphereWires(bone->position.position, 0.033f, 8, 8, WHITE);
				rlEnableDepthTest();
				EndMode3D();
				float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
				int fontSize = (int)(12 * scale);
				if (fontSize < 10) fontSize = 10;

				int textWidth = MeasureText(bone->name, fontSize);
				DrawRectangle((int)screenPos.x - 2, (int)screenPos.y - (int)(40 * scale),
						textWidth + 4, fontSize + 4, highlightColor);
				DrawText(bone->name, (int)screenPos.x, (int)screenPos.y - (int)(38 * scale), fontSize, WHITE);

				int modeSize = (int)(10 * scale);
				if (modeSize < 8) modeSize = 8;
				int modeWidth = MeasureText(modeText, modeSize);
				DrawRectangle((int)screenPos.x - 2, (int)screenPos.y - (int)(23 * scale),
						modeWidth + 4, modeSize + 3, Fade(highlightColor, 0.8f));
				DrawText(modeText, (int)screenPos.x, (int)screenPos.y - (int)(21 * scale), modeSize, WHITE);
				return;
			}
		}
	}
}

static void DrawTimeline(AppState* app) {
	if (!app->editor.showTimeline || !app->character->animation.isLoaded) return;
	Rectangle timeline = GetTimelineRect(app);
	int maxFrameNumber = FindMaxFrameNumber(app);
	if (maxFrameNumber == 0 && app->character->animation.frameCount == 0) return;
	DrawRectangleRec(timeline, (Color){40, 40, 40, 255});
	DrawRectangleLinesEx(timeline, 2, BLACK);
	float frameWidth = timeline.width / (float)(maxFrameNumber + 1);
	int currentFrameNumber = GetCurrentFrameNumber(app);
	float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
	int textSize = (int)(10 * scale);
	if (textSize < 8) textSize = 8;

	for (int i = 0; i <= maxFrameNumber; i++) {
		float x = timeline.x + i * frameWidth;
		Rectangle frameRect = {x, timeline.y, frameWidth, timeline.height};
		bool frameExists = FrameExists(app, i);
		bool isCurrentFrame = (i == currentFrameNumber);
		bool isSelected = (i >= app->editor.selectionStart && i <= app->editor.selectionEnd);
		bool isDraggedKeyframe = (app->editor.isDraggingKeyframe && i == app->editor.draggedKeyframeNumber);
		Color frameColor;
		if (!frameExists) {
			frameColor = (Color){30, 30, 30, 255};
		} else if (isDraggedKeyframe) {
			frameColor = PURPLE;
		} else if (isCurrentFrame) {
			frameColor = ORANGE;
		} else if (isSelected) {
			frameColor = SKYBLUE;
		} else {
			frameColor = (i % 5 == 0) ? (Color){60, 60, 60, 255} : (Color){50, 50, 50, 255};
		}
		DrawRectangleRec(frameRect, frameColor);
		DrawRectangleLinesEx(frameRect, 1, BLACK);
		if (i % 5 == 0) {
			char frameNum[16];
			snprintf(frameNum, sizeof(frameNum), "%d", i);
			DrawText(frameNum, (int)(x + 2), (int)(timeline.y - 15 * scale), textSize,
					!frameExists ? DARKGRAY : LIGHTGRAY);
		}
		if (frameExists) {
			int frameIndex = FindFrameIndexByNumber(app, i);
			bool isInterpolated = false;
			if (frameIndex != -1 && frameIndex < app->character->animation.frameCount) {
				isInterpolated = !app->character->animation.frames[frameIndex].isOriginalKeyframe;
			}
			Color indicatorColor = isInterpolated ? BLUE : GREEN;
			if (isDraggedKeyframe) {
				indicatorColor = PURPLE;
				DrawRectangle((int)(x + 1), (int)(timeline.y + timeline.height - 10),
						(int)(frameWidth - 2), 6, indicatorColor);
			} else {
				DrawRectangle((int)(x + 2), (int)(timeline.y + timeline.height - 8),
						(int)(frameWidth - 4), 4, indicatorColor);
			}
		}
	}
	float markerX = timeline.x + currentFrameNumber * frameWidth + frameWidth / 2;
	DrawLineEx((Vector2){markerX, timeline.y},
			(Vector2){markerX, timeline.y + timeline.height},
			3.0f * scale, RED);
	DrawCircle((int)markerX, (int)(timeline.y + timeline.height + 5 * scale), (int)(6 * scale), RED);
	if (app->editor.isDraggingKeyframe) {
		Vector2 mousePos = GetMousePosition();
		if (CheckCollisionPointRec(mousePos, timeline)) {
			int targetFrame = (int)((mousePos.x - timeline.x) / frameWidth);
			if (targetFrame >= 0 && targetFrame <= maxFrameNumber) {
				float targetX = timeline.x + targetFrame * frameWidth;
				DrawRectangleLinesEx(
						(Rectangle){targetX, timeline.y, frameWidth, timeline.height},
						3, PURPLE);
				char dragText[64];
				snprintf(dragText, sizeof(dragText), "Move to frame %d", targetFrame);
				int dragTextSize = (int)(12 * scale);
				if (dragTextSize < 10) dragTextSize = 10;
				int textW = MeasureText(dragText, dragTextSize);
				int tooltipH = (int)(20 * scale);
				int tooltipOffY = (int)(30 * scale);
				DrawRectangle((int)mousePos.x - textW / 2 - 5, (int)mousePos.y - tooltipOffY,
						textW + 10, tooltipH, (Color){128, 0, 128, 200});
				DrawText(dragText, (int)mousePos.x - textW / 2, (int)(mousePos.y - tooltipOffY + (tooltipH - dragTextSize) / 2), dragTextSize, WHITE);
			}
		}
	}
	if (!app->editor.isDraggingKeyframe) {
		if (CheckCollisionPointRec(GetMousePosition(), timeline)) {
			if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsKeyDown(KEY_LEFT_CONTROL)) {
				app->editor.isDraggingSlider = true;
			}
			float wheel = GetMouseWheelMove();
			if (wheel != 0) {
				int newFrameNumber = currentFrameNumber - (int)wheel;
				if (newFrameNumber >= 0 && newFrameNumber <= maxFrameNumber) {
					if (FrameExists(app, newFrameNumber)) {
						int frameIndex = FindFrameIndexByNumber(app, newFrameNumber);
						if (frameIndex != -1) {
							SetCharacterFrame(app->character, frameIndex);
						}
					}
				}
			}
		}
		if (app->editor.isDraggingSlider) {
			if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
				Vector2 mousePos = GetMousePosition();
				if (mousePos.x >= timeline.x && mousePos.x <= timeline.x + timeline.width) {
					int clickedFrameNumber = (int)((mousePos.x - timeline.x) / frameWidth);
					if (clickedFrameNumber >= 0 && clickedFrameNumber <= maxFrameNumber) {
						if (FrameExists(app, clickedFrameNumber)) {
							int frameIndex = FindFrameIndexByNumber(app, clickedFrameNumber);
							if (frameIndex != -1) {
								SetCharacterFrame(app->character, frameIndex);
								if (IsKeyDown(KEY_LEFT_SHIFT)) {
									if (app->editor.selectionStart == -1) {
										app->editor.selectionStart = clickedFrameNumber;
									}
									app->editor.selectionEnd = clickedFrameNumber;
									if (app->editor.selectionStart > app->editor.selectionEnd) {
										int temp = app->editor.selectionStart;
										app->editor.selectionStart = app->editor.selectionEnd;
										app->editor.selectionEnd = temp;
									}
								} else {
									app->editor.selectionStart = clickedFrameNumber;
									app->editor.selectionEnd = clickedFrameNumber;
								}
							}
						}
					}
				}
			} else {
				app->editor.isDraggingSlider = false;
			}
		}
	}
}

static void DrawControlPanel(AppState* app) {
	if (!app->editor.showTimeline) return;
	float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
	int uiHeight = (int)(UI_HEIGHT * scale);
	int panelY = app->screenHeight - uiHeight + (int)(100 * scale);
	int buttonSize = (int)(BUTTON_SIZE * scale);
	if (buttonSize < 25) buttonSize = 25;
	int buttonX = (int)(10 * scale);
	int spacing = (int)(5 * scale);

	int maxFrameNumber = FindMaxFrameNumber(app);
	int currentFrameNumber = GetCurrentFrameNumber(app);

	if (IconButton((Rectangle){(float)buttonX, (float)panelY, (float)buttonSize, (float)buttonSize},
				"|<", DARKBLUE)) {
		for (int i = 0; i <= maxFrameNumber; i++) {
			if (FrameExists(app, i)) {
				int frameIndex = FindFrameIndexByNumber(app, i);
				if (frameIndex != -1) {
					SetCharacterFrame(app->character, frameIndex);
				}
				break;
			}
		}
	}
	buttonX += buttonSize + spacing;

	if (IconButton((Rectangle){(float)buttonX, (float)panelY, (float)buttonSize, (float)buttonSize},
				"<", DARKBLUE)) {
		for (int i = currentFrameNumber - 1; i >= 0; i--) {
			if (FrameExists(app, i)) {
				int frameIndex = FindFrameIndexByNumber(app, i);
				if (frameIndex != -1) {
					SetCharacterFrame(app->character, frameIndex);
				}
				break;
			}
		}
	}
	buttonX += buttonSize + spacing;

	const char* playIcon = app->editor.isPlaying ? "||" : ">";
	if (IconButton((Rectangle){(float)buttonX, (float)panelY, (float)buttonSize, (float)buttonSize},
				playIcon, app->editor.isPlaying ? DARKGREEN : GREEN)) {
		// Detectar si la animación terminó automáticamente (sin loop)
		bool animationFinished = false;
		if (app->character && app->character->animController && 
		    app->character->animController->currentClipIndex >= 0) {
			AnimationClipMetadata* clip = &app->character->animController->clips[app->character->animController->currentClipIndex];
			// La animación terminó si: no tiene loop, isPlaying está true pero controller->playing está false
			if (!clip->loop && app->editor.isPlaying && !app->character->animController->playing) {
				animationFinished = true;
			}
		}
		
		// Si la animación terminó automáticamente, no cambiar isPlaying, solo reiniciar
		if (animationFinished) {
			TraceLog(LOG_INFO, "Animation finished, restarting...");
			RestartAnimation(app->character);
			app->editor.isPlaying = true;
			SetCharacterAutoPlay(app->character, true);
		} else {
			// Comportamiento normal: toggle play/pause
			app->editor.isPlaying = !app->editor.isPlaying;
			SetCharacterAutoPlay(app->character, app->editor.isPlaying);
		}
	}
	buttonX += buttonSize + spacing;

	if (IconButton((Rectangle){(float)buttonX, (float)panelY, (float)buttonSize, (float)buttonSize},
				">", DARKBLUE)) {
		for (int i = currentFrameNumber + 1; i <= maxFrameNumber; i++) {
			if (FrameExists(app, i)) {
				int frameIndex = FindFrameIndexByNumber(app, i);
				if (frameIndex != -1) {
					SetCharacterFrame(app->character, frameIndex);
				}
				break;
			}
		}
	}
	buttonX += buttonSize + spacing;

	if (IconButton((Rectangle){(float)buttonX, (float)panelY, (float)buttonSize, (float)buttonSize},
				">|", DARKBLUE)) {
		for (int i = maxFrameNumber; i >= 0; i--) {
			if (FrameExists(app, i)) {
				int frameIndex = FindFrameIndexByNumber(app, i);
				if (frameIndex != -1) {
					SetCharacterFrame(app->character, frameIndex);
				}
				break;
			}
		}
	}
	buttonX += buttonSize + (int)(15 * scale);

	bool canUndo = app->editor.undoHistory.currentIndex >= 0;
	if (IconButton((Rectangle){(float)buttonX, (float)panelY, (float)buttonSize, (float)buttonSize},
				"<-", canUndo ? PURPLE : DARKGRAY)) {
		if (canUndo) PerformUndo(app);
	}
	buttonX += buttonSize + spacing;

	bool canRedo = app->editor.undoHistory.currentIndex < app->editor.undoHistory.count - 1;
	if (IconButton((Rectangle){(float)buttonX, (float)panelY, (float)buttonSize, (float)buttonSize},
				"->", canRedo ? PURPLE : DARKGRAY)) {
		if (canRedo) PerformRedo(app);
	}
	buttonX += buttonSize + (int)(15 * scale);

	int btnWidth = (int)(80 * scale);
	if (btnWidth < 60) btnWidth = 60;

	if (Button((Rectangle){(float)buttonX, (float)panelY, (float)btnWidth, (float)buttonSize},
				"DELETE", RED)) {
		if (app->editor.selectionStart != -1 && FrameExists(app, app->editor.selectionStart)) {
			int frameIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
			if (frameIndex != -1) {
				int deletedFrameNumber = app->editor.selectionStart;
				DeleteFrameWithUndo(app, frameIndex);
				app->editor.needsSave = true;
				int nextValidFrame = -1;
				for (int i = deletedFrameNumber; i <= maxFrameNumber; i++) {
					if (FrameExists(app, i)) {
						nextValidFrame = i;
						break;
					}
				}
				if (nextValidFrame == -1) {
					for (int i = deletedFrameNumber - 1; i >= 0; i--) {
						if (FrameExists(app, i)) {
							nextValidFrame = i;
							break;
						}
					}
				}
				if (nextValidFrame != -1) {
					int newIndex = FindFrameIndexByNumber(app, nextValidFrame);
					if (newIndex != -1) {
						SetCharacterFrame(app->character, newIndex);
						app->editor.selectionStart = nextValidFrame;
						app->editor.selectionEnd = nextValidFrame;
					}
				} else {
					app->editor.selectionStart = -1;
					app->editor.selectionEnd = -1;
				}
			}
		}
	}
	buttonX += btnWidth + spacing;

	if (Button((Rectangle){(float)buttonX, (float)panelY, (float)btnWidth, (float)buttonSize},
				"DUPLICATE", BLUE)) {
		if (app->editor.selectionStart != -1 && FrameExists(app, app->editor.selectionStart)) {
			int frameIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
			if (frameIndex != -1) {
				bool isLastKeyframe = false;
				int lastKeyframeIdx = -1;
				for (int i = app->character->animation.frameCount - 1; i >= 0; i--) {
					if (app->character->animation.frames[i].isOriginalKeyframe) {
						lastKeyframeIdx = i;
						break;
					}
				}

				isLastKeyframe = (frameIndex == lastKeyframeIdx);

				if (isLastKeyframe) {
					app->editor.showAddFramesDialog = true;
				} else {
					if (DuplicateFrameWithUndo(app, frameIndex)) {
						app->editor.needsSave = true;

						int newFrameNumber = app->editor.selectionStart + 1;
						app->editor.selectionStart = newFrameNumber;
						app->editor.selectionEnd = newFrameNumber;

						int newIndex = FindFrameIndexByNumber(app, newFrameNumber);
						if (newIndex != -1) {
							SetCharacterFrame(app->character, newIndex);
						}
					}
				}
			}
		}
	}
	buttonX += btnWidth + spacing;

	int interpBtnWidth = (int)(100 * scale);
	if (interpBtnWidth < 80) interpBtnWidth = 80;

	if (Button((Rectangle){(float)buttonX, (float)panelY, (float)interpBtnWidth, (float)buttonSize},
				"INTERPOLATE", PURPLE)) {
		if (app->editor.selectionStart != -1 && app->editor.selectionEnd != -1 &&
				app->editor.selectionEnd > app->editor.selectionStart) {
			int startIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
			int endIndex = FindFrameIndexByNumber(app, app->editor.selectionEnd);
			if (startIndex != -1 && endIndex != -1) {
				BonesInterpolateFrames(&app->character->animation,
						startIndex,
						endIndex,
						app->editor.interpolationCount);
				app->character->maxFrames = app->character->animation.frameCount;
				app->editor.needsSave = true;
			}
		}
	}
	buttonX += interpBtnWidth + spacing;
	char interpText[32];
	snprintf(interpText, sizeof(interpText), "Frames: %d", app->editor.interpolationCount);
	DrawTextField((Rectangle){(float)buttonX, (float)panelY, (float)interpBtnWidth, (float)buttonSize}, interpText, false);

	if (IsKeyPressed(KEY_KP_ADD) || (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_EQUAL))) {
		app->editor.interpolationCount++;
	}
	if (IsKeyPressed(KEY_KP_SUBTRACT) || (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_MINUS))) {
		if (app->editor.interpolationCount > 1) app->editor.interpolationCount--;
	}
	buttonX += interpBtnWidth + spacing;

	if (Button((Rectangle){(float)buttonX, (float)panelY, (float)btnWidth, (float)buttonSize},
				"SAVE", app->editor.needsSave ? ORANGE : DARKGREEN)) {
		PerformSave(app);
	}
	buttonX += btnWidth + spacing;

	if (Button((Rectangle){(float)buttonX, (float)panelY, (float)btnWidth, (float)buttonSize},
				"EXPORT", GRAY)) {
		app->editor.showExportDialog = !app->editor.showExportDialog;
	}
	buttonX += btnWidth + spacing;

	int totalFrames = maxFrameNumber + 1;
	int existingFrames = app->character->animation.frameCount;
	char infoText[128];
	snprintf(infoText, sizeof(infoText), "Frame: %d/%d | Undo: %d Redo: %d | Frames: %d/%d",
			currentFrameNumber,
			maxFrameNumber,
			app->editor.undoHistory.currentIndex + 1,
			app->editor.undoHistory.count - app->editor.undoHistory.currentIndex - 1,
			existingFrames,
			totalFrames);

	int infoSize = (int)(14 * scale);
	if (infoSize < 10) infoSize = 10;
	DrawText(infoText, buttonX + (int)(10 * scale), panelY + (int)(10 * scale), infoSize, WHITE);
}

static void DrawDebugPanel(AppState* app) {
	if (!app->showUI) return;
	float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
	int panelWidth = (int)(210 * scale);
	int panelHeight = (int)(245 * scale);
	int panelX = app->screenWidth - panelWidth - (int)(10 * scale);
	int panelY = (int)(10 * scale);
	int buttonWidth = panelWidth - (int)(20 * scale);
	int buttonHeight = (int)(30 * scale);
	int buttonY = panelY + (int)(40 * scale);
	int spacing = (int)(5 * scale);

	DrawRectangle(panelX, panelY, panelWidth, panelHeight, (Color){40, 40, 40, 220});
	DrawRectangleLinesEx((Rectangle){(float)panelX, (float)panelY, (float)panelWidth, (float)panelHeight}, 2, BLACK);

	int titleSize = (int)(16 * scale);
	if (titleSize < 12) titleSize = 12;
	DrawText("DEBUG OPTIONS", panelX + (int)(10 * scale), panelY + (int)(10 * scale), titleSize, YELLOW);

	if (ToggleButton((Rectangle){(float)(panelX + 10 * scale), (float)buttonY, (float)buttonWidth, (float)buttonHeight},
				"Bone Names", app->debug.showBoneNames, GREEN, DARKGRAY)) {
		app->debug.showBoneNames = !app->debug.showBoneNames;
	}
	buttonY += buttonHeight + spacing;

	if (ToggleButton((Rectangle){(float)(panelX + 10 * scale), (float)buttonY, (float)buttonWidth, (float)buttonHeight},
				"Debug Spheres", app->debug.showDebugSpheres, GREEN, DARKGRAY)) {
		app->debug.showDebugSpheres = !app->debug.showDebugSpheres;
	}
	buttonY += buttonHeight + spacing;

	if (ToggleButton((Rectangle){(float)(panelX + 10 * scale), (float)buttonY, (float)buttonWidth, (float)buttonHeight},
				"Connections", app->debug.showConnections, GREEN, DARKGRAY)) {
		app->debug.showConnections = !app->debug.showConnections;
	}
	buttonY += buttonHeight + spacing;

	if (ToggleButton((Rectangle){(float)(panelX + 10 * scale), (float)buttonY, (float)buttonWidth, (float)buttonHeight},
				"Orientation", app->debug.showOrientation, GREEN, DARKGRAY)) {
		app->debug.showOrientation = !app->debug.showOrientation;
	}
	buttonY += buttonHeight + spacing;

	if (ToggleButton((Rectangle){(float)(panelX + 10 * scale), (float)buttonY, (float)buttonWidth, (float)buttonHeight},
				"Onion Skin [F6]", app->debug.showOnionSkin, (Color){180, 100, 0, 255}, DARKGRAY)) {
		app->debug.showOnionSkin = !app->debug.showOnionSkin;
	}
}

// ============================================================================
// DEBUG VISUALIZATION FUNCTIONS
// ============================================================================

static BoneOrientOffset* GetOrientOffset(const char* boneName) {
	for (int i = 0; i < g_orientOffsetCount; i++)
		if (strcmp(g_orientOffsets[i].boneName, boneName) == 0) return &g_orientOffsets[i];
	if (g_orientOffsetCount < MAX_ORIENT_OFFSETS) {
		BoneOrientOffset* e = &g_orientOffsets[g_orientOffsetCount++];
		strncpy(e->boneName, boneName, MAX_BONE_NAME_LENGTH - 1);
		e->boneName[MAX_BONE_NAME_LENGTH - 1] = '\0';
		e->yaw = 0.0f; e->pitch = 0.0f;
		return e;
	}
	return NULL;
}

static void InjectOrientOffsets(AppState* app) {
	for (int i = 0; i < app->character->renderBonesCount; i++) {
		BoneRenderData* rd = &app->character->renderBones[i];
		rd->yawOffset = 0.0f; rd->pitchOffset = 0.0f;
		for (int j = 0; j < g_orientOffsetCount; j++) {
			if (strcmp(g_orientOffsets[j].boneName, rd->boneName) == 0) {
				rd->yawOffset   = g_orientOffsets[j].yaw;
				rd->pitchOffset = g_orientOffsets[j].pitch;
				break;
			}
		}
	}
}

static BoneOrientation GetOrientWithOffsets(const char* boneName, const Person* person, Vector3 pos) {
	BoneOrientation orient = CalculateBoneOrientation(boneName, person, pos);
	if (!orient.valid) return orient;
	BoneOrientOffset* off = GetOrientOffset(boneName);
	if (!off || (off->yaw == 0.0f && off->pitch == 0.0f)) return orient;
	float yawRad   = off->yaw   * (PI / 180.0f);
	float pitchRad = off->pitch * (PI / 180.0f);
	Vector3 fw = orient.forward, up = orient.up, rt = orient.right;
	Vector3 axisUp = up, axisRt = rt;
	fw = Vector3RotateByAxisAngle(fw, axisUp, yawRad);
	rt = Vector3RotateByAxisAngle(rt, axisUp, yawRad);
	fw = Vector3RotateByAxisAngle(fw, axisRt, pitchRad);
	up = Vector3RotateByAxisAngle(up, axisRt, pitchRad);
	orient.forward = Vector3Normalize(fw);
	orient.up      = Vector3Normalize(up);
	orient.right   = Vector3Normalize(rt);
	return orient;
}

static void UpdateBoneOrientGizmo(AppState* app) {
	if (!app->debug.showOrientation || !app->editor.hasBoneSelected) return;
	if (!app->character->animation.isLoaded) return;
	if (app->editor.isDraggingRotationHandle) return;
	if (app->editor.isDraggingBone || app->editor.isDraggingGroup) return;

	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
	const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	Camera camera = app->character->renderer->camera;
	Vector2 mousePos = GetMousePosition();

	const Person* person = NULL;
	Vector3 bonePos = {0};
	bool found = false;
	for (int p = 0; p < frame->personCount && !found; p++) {
		const Person* pr = &frame->persons[p];
		if (!pr->active) continue;
		for (int b = 0; b < pr->boneCount; b++) {
			if (strcmp(pr->bones[b].name, app->editor.selectedBoneName) == 0 && pr->bones[b].position.valid) {
				bonePos = pr->bones[b].position.position;
				person  = pr;
				found   = true;
				break;
			}
		}
	}
	if (!found || !person) return;

	BoneOrientation orient = GetOrientWithOffsets(app->editor.selectedBoneName, person, bonePos);
	if (!orient.valid) return;

	if (app->editor.orientDragAxis != 0) {
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			Vector2 boneScreen = GetWorldToScreen(bonePos, camera);
			Vector2 off = {mousePos.x - boneScreen.x, mousePos.y - boneScreen.y};
			float angle = atan2f(off.x, -off.y) * RAD2DEG;
			float delta = angle - app->editor.orientDragStartAngle;
			BoneOrientOffset* offsets = GetOrientOffset(app->editor.selectedBoneName);
			if (offsets) {
				if (app->editor.orientDragAxis == 1)
					offsets->yaw = app->editor.orientDragStartValue + delta;
				else if (app->editor.orientDragAxis == 2)
					offsets->pitch = app->editor.orientDragStartValue + delta;
				else if (app->editor.orientDragAxis == 3) {
					float newRot = app->editor.orientDragStartValue + delta;
					RotateBoneInFrame(app, GetCurrentFrameNumber(app), app->editor.selectedBoneName, newRot);
				}
			}
		} else {
			if (app->editor.orientDragAxis == 3) {
				UndoAction action = {0};
				action.type = UNDO_BONE_ROTATE;
				strncpy(action.boneName, app->editor.selectedBoneName, 63);
				action.frameNumber = GetCurrentFrameNumber(app);
				action.oldRotation = app->editor.orientDragStartValue;
				action.newRotation = GetBoneRotation(app, action.frameNumber, app->editor.selectedBoneName);
				PushUndoAction(&app->editor.undoHistory, action);
			} else if (app->editor.orientDragAxis == 1 || app->editor.orientDragAxis == 2) {
				BoneOrientOffset* off = GetOrientOffset(app->editor.selectedBoneName);
				if (off) {
					float newVal = (app->editor.orientDragAxis == 1) ? off->yaw : off->pitch;
					if (newVal != app->editor.orientDragStartValue) {
						UndoAction action = {0};
						action.type = UNDO_ORIENT_OFFSET;
						strncpy(action.boneName, app->editor.selectedBoneName, 63);
						action.opFrameIndex = app->editor.orientDragAxis;
						action.oldRotation = app->editor.orientDragStartValue;
						action.newRotation = newVal;
						PushUndoAction(&app->editor.undoHistory, action);
					}
				}
			}
			app->editor.orientDragAxis = 0;
		}
		return;
	}

	if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;
	if (CheckCollisionPointRec(mousePos, GetTimelineRect(app))) return;
	if (CheckCollisionPointRec(mousePos, GetGizmoRect(app))) return;

	float sphereScreenRadius = 15.0f;

	Vector3 tips[3] = {
		Vector3Add(bonePos, Vector3Scale(orient.right,   AXIS_LENGTH * 1.4f)),
		Vector3Add(bonePos, Vector3Scale(orient.up,      AXIS_LENGTH * 1.4f)),
		Vector3Add(bonePos, Vector3Scale(orient.forward, AXIS_LENGTH * 1.4f))
	};

	for (int axis = 1; axis <= 3; axis++) {
		Vector2 tipScreen = GetWorldToScreen(tips[axis - 1], camera);
		if (Vector2Distance(mousePos, tipScreen) < sphereScreenRadius * 2.0f) {
			Vector2 boneScreen = GetWorldToScreen(bonePos, camera);
			Vector2 off = {mousePos.x - boneScreen.x, mousePos.y - boneScreen.y};
			app->editor.orientDragAxis = axis;
			app->editor.orientDragStartAngle = atan2f(off.x, -off.y) * RAD2DEG;
			BoneOrientOffset* offsets = GetOrientOffset(app->editor.selectedBoneName);
			if (offsets) {
				if (axis == 1) app->editor.orientDragStartValue = offsets->yaw;
				else if (axis == 2) app->editor.orientDragStartValue = offsets->pitch;
				else app->editor.orientDragStartValue = GetBoneRotation(app, GetCurrentFrameNumber(app), app->editor.selectedBoneName);
			}
			return;
		}
	}
}

static void DrawBoneOrientGizmo(AppState* app) {
	if (!app->debug.showOrientation || !app->editor.hasBoneSelected) return;
	if (app->editor.orientDragAxis == 0) return;
	if (!app->character->animation.isLoaded) return;

	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
	const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	Camera camera = app->character->renderer->camera;

	Vector3 bonePos = {0};
	bool found = false;
	for (int p = 0; p < frame->personCount && !found; p++) {
		const Person* pr = &frame->persons[p];
		if (!pr->active) continue;
		for (int b = 0; b < pr->boneCount; b++) {
			if (strcmp(pr->bones[b].name, app->editor.selectedBoneName) == 0 && pr->bones[b].position.valid) {
				bonePos = pr->bones[b].position.position;
				found   = true;
				break;
			}
		}
	}
	if (!found) return;

	float scale = fminf((float)app->screenWidth / 1920.0f, (float)app->screenHeight / 1080.0f);
	float radius = ROTATION_HANDLE_RADIUS * scale;
	float dotRadius = 8.0f * scale;
	Vector2 boneScreen = GetWorldToScreen(bonePos, camera);

	Color axisColor = (app->editor.orientDragAxis == 1) ? RED :
	                  (app->editor.orientDragAxis == 2) ? GREEN : BLUE;

	DrawCircleLines((int)boneScreen.x, (int)boneScreen.y, radius, axisColor);

	float currentValue = 0.0f;
	if (app->editor.orientDragAxis == 3) {
		currentValue = GetBoneRotation(app, GetCurrentFrameNumber(app), app->editor.selectedBoneName);
	} else {
		BoneOrientOffset* off = GetOrientOffset(app->editor.selectedBoneName);
		if (off) currentValue = (app->editor.orientDragAxis == 1) ? off->yaw : off->pitch;
	}

	float handleAngleRad = currentValue * DEG2RAD;
	Vector2 handlePos = {
		boneScreen.x + sinf(handleAngleRad) * radius,
		boneScreen.y - cosf(handleAngleRad) * radius
	};
	DrawCircleV(handlePos, dotRadius, ORANGE);
	DrawCircleLines((int)handlePos.x, (int)handlePos.y, dotRadius, WHITE);

	float scale2 = fminf((float)app->screenWidth / 1920.0f, (float)app->screenHeight / 1080.0f);
	int fs = (int)(11 * scale2);
	if (fs < 9) fs = 9;
	const char* axisName = (app->editor.orientDragAxis == 1) ? "yaw" :
	                       (app->editor.orientDragAxis == 2) ? "pitch" : "roll";
	char label[32];
	snprintf(label, sizeof(label), "%s %.1f", axisName, currentValue);
	DrawText(label, (int)boneScreen.x + (int)(dotRadius * 2), (int)boneScreen.y - (int)(dotRadius * 2), fs, ORANGE);
}

static void DrawBoneOrientation(AppState* app) {
	if (!app->debug.showOrientation || !app->character->animation.isLoaded) return;
	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
	const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	typedef struct {
		char name[MAX_BONE_NAME_LENGTH];
		Vector3 position;
		BoneOrientation orientation;
	} UniqueBone;
	UniqueBone uniqueBones[MAX_BONES_PER_PERSON * MAX_PERSONS];
	int uniqueCount = 0;
	for (int p = 0; p < frame->personCount; p++) {
		const Person* person = &frame->persons[p];
		if (!person->active) continue;
		for (int b = 0; b < person->boneCount; b++) {
			const Bone* bone = &person->bones[b];
			if (!bone->position.valid) continue;
			bool exists = false;
			for (int u = 0; u < uniqueCount; u++) {
				if (strncmp(uniqueBones[u].name, bone->name, MAX_BONE_NAME_LENGTH) == 0) {
					exists = true;
					break;
				}
			}
			if (!exists && uniqueCount < (MAX_BONES_PER_PERSON * MAX_PERSONS)) {
				strncpy(uniqueBones[uniqueCount].name, bone->name, MAX_BONE_NAME_LENGTH - 1);
				uniqueBones[uniqueCount].name[MAX_BONE_NAME_LENGTH - 1] = '\0';
				uniqueBones[uniqueCount].position = bone->position.position;
				uniqueBones[uniqueCount].orientation = GetOrientWithOffsets(bone->name, person, bone->position.position);
				uniqueCount++;
			}
		}
	}

	Camera camera = app->character->renderer->camera;
	Vector2 mousePos = GetMousePosition();

	int hoveredAxis = 0;
	if (app->editor.hasBoneSelected && app->editor.orientDragAxis == 0) {
		for (int i = 0; i < uniqueCount; i++) {
			if (!uniqueBones[i].orientation.valid) continue;
			if (strcmp(uniqueBones[i].name, app->editor.selectedBoneName) != 0) continue;
			Vector3 pos = uniqueBones[i].position;
			BoneOrientation orient = uniqueBones[i].orientation;
			Vector3 tips[3] = {
				Vector3Add(pos, Vector3Scale(orient.right,   AXIS_LENGTH)),
				Vector3Add(pos, Vector3Scale(orient.up,      AXIS_LENGTH)),
				Vector3Add(pos, Vector3Scale(orient.forward, AXIS_LENGTH))
			};
			for (int a = 0; a < 3; a++) {
				Vector2 tipScreen = GetWorldToScreen(tips[a], camera);
				if (Vector2Distance(mousePos, tipScreen) < 15.0f) {
					hoveredAxis = a + 1;
					break;
				}
			}
			break;
		}
	}

	BeginMode3D(camera);
	for (int i = 0; i < uniqueCount; i++) {
		if (!uniqueBones[i].orientation.valid) continue;
		Vector3 pos = uniqueBones[i].position;
		BoneOrientation orient = uniqueBones[i].orientation;
		bool isSelected = app->editor.hasBoneSelected &&
			strcmp(uniqueBones[i].name, app->editor.selectedBoneName) == 0;

		float tipSize = isSelected ? 0.007f : 0.0035f;
		float axisLen = isSelected ? AXIS_LENGTH * 1.4f : AXIS_LENGTH;

		Color rightColor = RED, upColor = GREEN, fwdColor = BLUE;
		if (isSelected) {
			if (app->editor.orientDragAxis == 1 || hoveredAxis == 1) rightColor = ORANGE;
			if (app->editor.orientDragAxis == 2 || hoveredAxis == 2) upColor    = ORANGE;
			if (app->editor.orientDragAxis == 3 || hoveredAxis == 3) fwdColor   = ORANGE;
		}

		Vector3 right   = Vector3Scale(orient.right,   axisLen);
		Vector3 up      = Vector3Scale(orient.up,      axisLen);
		Vector3 forward = Vector3Scale(orient.forward, axisLen);

		DrawLine3D(pos, Vector3Add(pos, right),   rightColor);
		DrawSphere(Vector3Add(pos, right),   tipSize, rightColor);
		DrawLine3D(pos, Vector3Add(pos, up),      upColor);
		DrawSphere(Vector3Add(pos, up),      tipSize, upColor);
		DrawLine3D(pos, Vector3Add(pos, forward), fwdColor);
		DrawSphere(Vector3Add(pos, forward), tipSize, fwdColor);
		DrawSphere(pos, 0.005f, YELLOW);
	}
	EndMode3D();

	float scale = fminf((float)app->screenWidth / 1920.0f, (float)app->screenHeight / 1080.0f);
	int fs = (int)(10 * scale); if (fs < 8) fs = 8;
	for (int i = 0; i < uniqueCount; i++) {
		if (!uniqueBones[i].orientation.valid) continue;
		if (!app->editor.hasBoneSelected) continue;
		if (strcmp(uniqueBones[i].name, app->editor.selectedBoneName) != 0) continue;
		Vector3 pos = uniqueBones[i].position;
		BoneOrientation orient = uniqueBones[i].orientation;
		float axisLen = AXIS_LENGTH * 1.4f;
		Vector2 rScreen = GetWorldToScreen(Vector3Add(pos, Vector3Scale(orient.right,   axisLen)), camera);
		Vector2 uScreen = GetWorldToScreen(Vector3Add(pos, Vector3Scale(orient.up,      axisLen)), camera);
		Vector2 fScreen = GetWorldToScreen(Vector3Add(pos, Vector3Scale(orient.forward, axisLen)), camera);
		DrawText("yaw",   (int)rScreen.x + 6, (int)rScreen.y - fs/2, fs, RED);
		DrawText("pitch", (int)uScreen.x + 6, (int)uScreen.y - fs/2, fs, GREEN);
		DrawText("roll",  (int)fScreen.x + 6, (int)fScreen.y - fs/2, fs, BLUE);
	}
}

static void DrawDebugVisuals(AppState* app) {
	if (!app->character->animation.isLoaded) return;
	int currentFrame = app->character->currentFrame;
	if (currentFrame < 0 || currentFrame >= app->character->animation.frameCount) return;
	const AnimationFrame* frame = &app->character->animation.frames[currentFrame];
	typedef struct {
		char name[64];
		Vector3 worldPos;
		Vector2 screenPos;
	} DrawnBone;
	static DrawnBone drawnBones[MAX_BONES_PER_PERSON * MAX_PERSONS];
	int drawnCount = 0;
	for (int p = 0; p < frame->personCount; p++) {
		const Person* person = &frame->persons[p];
		if (!person->active) continue;
		for (int b = 0; b < person->boneCount; b++) {
			const Bone* bone = &person->bones[b];
			if (!bone->position.valid) continue;
			bool alreadyDrawn = false;
			for (int d = 0; d < drawnCount; d++) {
				if (strcmp(drawnBones[d].name, bone->name) == 0) {
					alreadyDrawn = true;
					break;
				}
			}
			if (!alreadyDrawn && drawnCount < MAX_BONES_PER_PERSON * MAX_PERSONS) {
				Vector2 screenPos = GetWorldToScreen(bone->position.position,
						app->character->renderer->camera);
				if (screenPos.x >= 0 && screenPos.x < app->screenWidth &&
						screenPos.y >= 0 && screenPos.y < app->screenHeight) {
					strncpy(drawnBones[drawnCount].name, bone->name, 63);
					drawnBones[drawnCount].name[63] = '\0';
					drawnBones[drawnCount].worldPos = bone->position.position;
					drawnBones[drawnCount].screenPos = screenPos;
					drawnCount++;
				}
			}
		}
	}
	if (app->debug.showDebugSpheres && drawnCount > 0) {
		BeginMode3D(app->character->renderer->camera);
		for (int i = 0; i < drawnCount; i++) {
			DrawSphere(drawnBones[i].worldPos, 0.028f, (Color){80, 160, 255, 140});
			DrawSphereWires(drawnBones[i].worldPos, 0.031f, 8, 8, (Color){235, 235, 235, 255});
		}
		EndMode3D();
	}
	if (app->debug.showConnections && drawnCount > 0) {
		const char* connections[][2] = {
			{"Neck", "LShoulder"}, {"Neck", "RShoulder"},
			{"LShoulder", "LElbow"}, {"LElbow", "LWrist"},
			{"RShoulder", "RElbow"}, {"RElbow", "RWrist"},
			{"Neck", "LHip"}, {"Neck", "RHip"},
			{"LHip", "LKnee"}, {"LKnee", "LAnkle"},
			{"RHip", "RKnee"}, {"RKnee", "RAnkle"},
			{"LHip", "RHip"}, {"LShoulder", "RShoulder"},
			{NULL, NULL}};
		BeginMode3D(app->character->renderer->camera);
		for (int c = 0; connections[c][0] != NULL; c++) {
			Vector3 pos1 = {0};
			Vector3 pos2 = {0};
			bool found1 = false, found2 = false;
			for (int i = 0; i < drawnCount; i++) {
				if (strcmp(drawnBones[i].name, connections[c][0]) == 0) {
					pos1 = drawnBones[i].worldPos;
					found1 = true;
				}
				if (strcmp(drawnBones[i].name, connections[c][1]) == 0) {
					pos2 = drawnBones[i].worldPos;
					found2 = true;
				}
				if (found1 && found2) break;
			}
			if (found1 && found2 && Vector3Length(pos1) > 0.01f && Vector3Length(pos2) > 0.01f) {
				DrawLine3D(pos1, pos2, LIME);
			}
		}
		EndMode3D();
	}
	if (app->debug.showBoneNames && drawnCount > 0) {
		float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
		int fontSize = (int)(10 * scale);
		if (fontSize < 8) fontSize = 8;
		for (int i = 0; i < drawnCount; i++) {
			int textWidth = MeasureText(drawnBones[i].name, fontSize);
			DrawRectangle((int)drawnBones[i].screenPos.x - 2,
					(int)drawnBones[i].screenPos.y - (int)(22 * scale),
					textWidth + 4, fontSize + 4,
					(Color){0, 0, 0, 180});
			DrawText(drawnBones[i].name,
					(int)drawnBones[i].screenPos.x,
					(int)drawnBones[i].screenPos.y - (int)(20 * scale),
					fontSize, YELLOW);
		}
	}
}

// ============================================================================
// EXPORT FUNCTIONS
// ============================================================================

static const Person* GetPrimaryPerson(const AnimationFrame* frame) {
	if (!frame) return NULL;
	for (int p = 0; p < frame->personCount; p++) {
		if (frame->persons[p].active) return &frame->persons[p];
	}
	return NULL;
}

bool BonesExportToJSON(BonesAnimation* animation, const char* filepath, int startIdx, int endIdx) {
	if (!animation || !filepath || startIdx < 0 || endIdx < 0 || startIdx >= animation->frameCount) {
		return false;
	}

	if (endIdx >= animation->frameCount) endIdx = animation->frameCount - 1;
	if (startIdx > endIdx) {
		return false;
	}

	FILE* file = fopen(filepath, "w");
	if (!file) {
		return false;
	}

	fprintf(file, "{\n");

	bool firstPrinted = true;
	int exportedCount = 0;

	for (int idx = startIdx; idx <= endIdx; idx++) {
		AnimationFrame* frame = &animation->frames[idx];
		if (!frame->valid) continue;
		if (!frame->isOriginalKeyframe) continue;

		const Person* person = GetPrimaryPerson(frame);
		if (!person) continue;

		if (!firstPrinted) fprintf(file, ",\n");
		firstPrinted = false;

		fprintf(file, "  \"frame_%04d\": {\n", frame->frameNumber);
		fprintf(file, "    \"person_0\": {\n");

		bool firstBone = true;
		for (int b = 0; b < person->boneCount; b++) {
			const Bone* bone = &person->bones[b];
			if (!bone->position.valid) continue;

			if (!firstBone) fprintf(file, ",\n");
			firstBone = false;

			double x_transformed = (double)bone->position.position.x;
			double y_transformed = (double)bone->position.position.y;
			double z_transformed = (double)bone->position.position.z;

			double x_original = (x_transformed + 1.0) * 0.5;
			double y_original = 1.0 - y_transformed;
			double z_original = (z_transformed + 1.0) * 0.5;

			fprintf(file, "      \"%s\": {\"x\": %.17g, \"y\": %.17g, \"z\": %.17g}",
					bone->name, x_original, y_original, z_original);
		}

		fprintf(file, "\n    }\n  }");
		exportedCount++;
	}

	fprintf(file, "\n}\n");
	fclose(file);

	return exportedCount > 0;
}

// ============================================================================
// SAFE SAVE TO SOURCE FILE (backup + atomic replace)
// ============================================================================

typedef enum {
	SAVE_RESULT_OK,
	SAVE_RESULT_NO_ANIMATION,
	SAVE_RESULT_NO_PATH,
	SAVE_RESULT_BACKUP_FAILED,
	SAVE_RESULT_WRITE_FAILED,
	SAVE_RESULT_REPLACE_FAILED
} SaveResult;

#define MAX_ANIMATION_BACKUPS 10

static void SetSaveStatus(AppState* app, const char* message, bool isError) {
	strncpy(app->editor.saveStatusMessage, message, sizeof(app->editor.saveStatusMessage) - 1);
	app->editor.saveStatusMessage[sizeof(app->editor.saveStatusMessage) - 1] = '\0';
	app->editor.saveStatusIsError = isError;
	app->editor.saveStatusTimer = isError ? 6.0f : 3.0f;
}

static bool CopyFileContents(const char* srcPath, const char* dstPath) {
	FILE* src = fopen(srcPath, "rb");
	if (!src) return false;
	FILE* dst = fopen(dstPath, "wb");
	if (!dst) {
		fclose(src);
		return false;
	}

	char buffer[8192];
	size_t bytesRead;
	bool ok = true;
	while ((bytesRead = fread(buffer, 1, sizeof(buffer), src)) > 0) {
		if (fwrite(buffer, 1, bytesRead, dst) != bytesRead) {
			ok = false;
			break;
		}
	}

	fclose(src);
	fclose(dst);
	if (!ok) remove(dstPath);
	return ok;
}

static void GetBackupDirectory(const char* animationFilePath, char* outDir, size_t outDirSize) {
	const char* dir = GetDirectoryPath(animationFilePath);
	snprintf(outDir, outDirSize, "%s/.bones_backups", (dir && dir[0]) ? dir : ".");
}

static int CompareBackupPaths(const void* a, const void* b) {
	return strcmp((const char*)a, (const char*)b);
}

static void RotateAnimationBackups(const char* backupDir, const char* baseName) {
	FilePathList files = LoadDirectoryFilesEx(backupDir, ".bak", false);
	if (files.count <= MAX_ANIMATION_BACKUPS) {
		UnloadDirectoryFiles(files);
		return;
	}

	size_t baseLen = strlen(baseName);
	char (*matches)[MAX_FILE_PATH_LENGTH] = (char(*)[MAX_FILE_PATH_LENGTH])malloc(sizeof(char[MAX_FILE_PATH_LENGTH]) * files.count);
	if (matches) {
		int matchCount = 0;
		for (unsigned int i = 0; i < files.count; i++) {
			const char* name = GetFileNameWithoutExt(files.paths[i]);
			if (strncmp(name, baseName, baseLen) == 0) {
				strncpy(matches[matchCount], files.paths[i], MAX_FILE_PATH_LENGTH - 1);
				matches[matchCount][MAX_FILE_PATH_LENGTH - 1] = '\0';
				matchCount++;
			}
		}

		qsort(matches, matchCount, sizeof(char[MAX_FILE_PATH_LENGTH]), CompareBackupPaths);

		int toDelete = matchCount - MAX_ANIMATION_BACKUPS;
		for (int i = 0; i < toDelete; i++) {
			remove(matches[i]);
		}
		free(matches);
	}

	UnloadDirectoryFiles(files);
}

static SaveResult ReplaceFileAtomically(const char* tmpPath, const char* finalPath) {
	if (rename(tmpPath, finalPath) == 0) {
		return SAVE_RESULT_OK;
	}

	char oldPath[MAX_FILE_PATH_LENGTH];
	snprintf(oldPath, sizeof(oldPath), "%s.old", finalPath);
	remove(oldPath);

	bool hadOriginal = (rename(finalPath, oldPath) == 0);

	if (rename(tmpPath, finalPath) == 0) {
		if (hadOriginal) remove(oldPath);
		return SAVE_RESULT_OK;
	}

	if (hadOriginal) {
		rename(oldPath, finalPath);
	}
	return SAVE_RESULT_REPLACE_FAILED;
}

static SaveResult SaveAnimationToSourceFile(BonesAnimation* animation) {
	if (!animation || !animation->isLoaded) return SAVE_RESULT_NO_ANIMATION;
	if (animation->filePath[0] == '\0') return SAVE_RESULT_NO_PATH;
	if (animation->frameCount <= 0) return SAVE_RESULT_NO_ANIMATION;

	char backupDir[MAX_FILE_PATH_LENGTH];
	GetBackupDirectory(animation->filePath, backupDir, sizeof(backupDir));
	if (!DirectoryExists(backupDir)) {
#ifdef _WIN32
		CreateDirectoryA(backupDir, NULL);
#else
		mkdir(backupDir, 0755);
#endif
	}

	const char* baseName = GetFileNameWithoutExt(animation->filePath);

	FILE* probe = fopen(animation->filePath, "rb");
	bool originalExists = (probe != NULL);
	if (probe) fclose(probe);

	if (originalExists) {
		time_t now = time(NULL);
		struct tm* tmInfo = localtime(&now);
		int yr  = ((tmInfo->tm_year + 1900) & 0x7FFF) % 10000;
		int mon = (tmInfo->tm_mon  & 0xFF) % 12 + 1;
		int day = (tmInfo->tm_mday & 0xFF) % 31 + 1;
		int hr  = (tmInfo->tm_hour & 0xFF) % 24;
		int min = (tmInfo->tm_min  & 0xFF) % 60;
		int sec = (tmInfo->tm_sec  & 0xFF) % 61;
		char backupPath[MAX_FILE_PATH_LENGTH * 2];
		snprintf(backupPath, sizeof(backupPath), "%s/%s_%04d%02d%02d_%02d%02d%02d.bak",
				backupDir, baseName, yr, mon, day, hr, min, sec);

		if (!CopyFileContents(animation->filePath, backupPath)) {
			return SAVE_RESULT_BACKUP_FAILED;
		}
	}

	char tmpPath[MAX_FILE_PATH_LENGTH];
	snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", animation->filePath);

	if (!BonesExportToJSON(animation, tmpPath, 0, animation->frameCount - 1)) {
		remove(tmpPath);
		return SAVE_RESULT_WRITE_FAILED;
	}

	FILE* verify = fopen(tmpPath, "rb");
	if (!verify) {
		return SAVE_RESULT_WRITE_FAILED;
	}
	fseek(verify, 0, SEEK_END);
	long tmpSize = ftell(verify);
	fclose(verify);
	if (tmpSize <= 0) {
		remove(tmpPath);
		return SAVE_RESULT_WRITE_FAILED;
	}

	SaveResult replaceResult = ReplaceFileAtomically(tmpPath, animation->filePath);
	if (replaceResult != SAVE_RESULT_OK) {
		remove(tmpPath);
		return replaceResult;
	}

	RotateAnimationBackups(backupDir, baseName);
	return SAVE_RESULT_OK;
}

static void PerformSave(AppState* app) {
	if (!app->character) {
		SetSaveStatus(app, "No hay personaje cargado", true);
		return;
	}

	SaveResult result = SaveAnimationToSourceFile(&app->character->animation);
	switch (result) {
		case SAVE_RESULT_OK:
			app->editor.needsSave = false;
			SetSaveStatus(app, "Guardado correctamente", false);
			break;
		case SAVE_RESULT_NO_ANIMATION:
			SetSaveStatus(app, "No hay animacion cargada para guardar", true);
			break;
		case SAVE_RESULT_NO_PATH:
			SetSaveStatus(app, "No se pudo guardar: ruta de origen desconocida. Usa EXPORT AS", true);
			break;
		case SAVE_RESULT_BACKUP_FAILED:
			SetSaveStatus(app, "NO SE GUARDO: fallo el backup. El archivo original sigue intacto", true);
			break;
		case SAVE_RESULT_WRITE_FAILED:
			SetSaveStatus(app, "NO SE GUARDO: fallo al escribir. El archivo original sigue intacto", true);
			break;
		case SAVE_RESULT_REPLACE_FAILED:
			SetSaveStatus(app, "NO SE GUARDO: el archivo original sigue intacto (revisa .bones_backups)", true);
			break;
	}
}

static void DrawExportDialog(AppState* app) {
	if (!app->editor.showExportDialog) return;
	float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
	int dialogW = (int)(500 * scale);
	int dialogH = (int)(200 * scale);
	int dialogX = (app->screenWidth - dialogW) / 2;
	int dialogY = (app->screenHeight - dialogH) / 2;
	int padding = (int)(20 * scale);
	int buttonH = (int)(40 * scale);
	int fontSize = (int)(16 * scale);

	DrawRectangle(0, 0, app->screenWidth, app->screenHeight, (Color){0, 0, 0, 150});
	DrawRectangle(dialogX, dialogY, dialogW, dialogH, RAYWHITE);
	DrawRectangleLinesEx((Rectangle){(float)dialogX, (float)dialogY, (float)dialogW, (float)dialogH}, 3, BLACK);

	DrawText("Export Animation", dialogX + padding, dialogY + padding, fontSize + 4, BLACK);
	DrawText("Export Path:", dialogX + padding, dialogY + (int)(60 * scale), fontSize, DARKGRAY);
	DrawTextField((Rectangle){(float)(dialogX + padding), (float)(dialogY + (int)(80 * scale)), (float)(dialogW - padding * 2), (float)(30 * scale)},
			app->editor.exportPath, true);

	int btnW = (int)(100 * scale);
	if (Button((Rectangle){(float)(dialogX + padding), (float)(dialogY + dialogH - buttonH - padding), (float)btnW, (float)buttonH}, "EXPORT", GREEN)) {
		if (strlen(app->editor.exportPath) > 0) {
			bool hasSelection = (app->editor.selectionStart != -1 && app->editor.selectionEnd != -1 &&
					app->editor.selectionStart != app->editor.selectionEnd);
			int startFrame = hasSelection ? app->editor.selectionStart : 0;
			int endFrame = hasSelection ? app->editor.selectionEnd :
				(app->character && app->character->animation.frameCount > 0 ? app->character->animation.frameCount - 1 : 0);
			if (startFrame < 0) startFrame = 0;
			if (endFrame < 0) endFrame = 0;
			if (app->character && app->character->animation.frameCount > 0) {
				if (startFrame >= app->character->animation.frameCount) startFrame = app->character->animation.frameCount - 1;
				if (endFrame >= app->character->animation.frameCount) endFrame = app->character->animation.frameCount - 1;
			}
			if (BonesExportToJSON(&app->character->animation,
						app->editor.exportPath,
						startFrame,
						endFrame)) {
				app->editor.needsSave = false;
			}
			app->editor.showExportDialog = false;
		}
	}

	if (Button((Rectangle){(float)(dialogX + padding * 2 + btnW), (float)(dialogY + dialogH - buttonH - padding), (float)btnW, (float)buttonH}, "CANCEL", RED)) {
		app->editor.showExportDialog = false;
	}

	if (Button((Rectangle){(float)(dialogX + dialogW - btnW - padding), (float)(dialogY + dialogH - buttonH - padding), (float)btnW, (float)buttonH}, "BROWSE", BLUE)) {
		strcpy(app->editor.exportPath, "data/poses/exported.json");
	}
}

#define TEXED_MAX_PATHS 32
#define TEXED_MAX_CELLS 25
#define TEXED_SCALE_MIN 0.25f
#define TEXED_SCALE_MAX 4.0f
#define TEXED_ROT_SNAP 15.0f

typedef struct {
	float x[4];
	float y[4];
} TexCellXform;

typedef enum {
	TEXED_DRAG_NONE,
	TEXED_DRAG_MOVE,
	TEXED_DRAG_VERTEX,
	TEXED_DRAG_ROTATE,
	TEXED_DRAG_BRUSH
} TexDragMode;

typedef enum {
	TEXED_TOOL_MOVE,
	TEXED_TOOL_ERASE,
	TEXED_TOOL_SMUDGE
} TexTool;

#define TEXED_UNDO_MAX 24
#define TEXED_BRUSH_MIN 1.0f
#define TEXED_BRUSH_MAX 300.0f
#define TEXED_CELL_ZOOM_STEP 1.05f
#define TEXED_CELL_ZOOM_FINE 1.01f
#define TEXED_TILE_SHIFT 6
#define TEXED_TILE (1 << TEXED_TILE_SHIFT)

typedef struct {
	int x;
	int y;
	int w;
	int h;
	Color* pixels;
} TexUndoPatch;

typedef struct {
	bool open;
	AnimatedCharacter* owner;
	char paths[TEXED_MAX_PATHS][MAX_FILE_PATH_LENGTH];
	int gridCols[TEXED_MAX_PATHS];
	int gridRows[TEXED_MAX_PATHS];
	int pathCount;
	int current;
	bool loaded;
	Image original;
	Image working;
	Texture2D preview;
	TexCellXform xform[TEXED_MAX_CELLS];
	int selected;
	TexDragMode dragMode;
	int dragVertex;
	Vector2 dragStartMouse;
	TexCellXform dragStartXform;
	Vector2 dragPivot;
	float dragStartAngle;
	Image disk;
	TexTool tool;
	float brushSize;
	float brushHardness;
	float brushOpacity;
	bool edited;
	int sliderActive;
	float* mask;
	Image strokeBase;
	bool strokeActive;
	Vector2 strokePrevPt;
	Vector2 strokePrevMid;
	int strokeMinX;
	int strokeMinY;
	int strokeMaxX;
	int strokeMaxY;
	int frameMinX;
	int frameMinY;
	int frameMaxX;
	int frameMaxY;
	TexUndoPatch undo[TEXED_UNDO_MAX];
	int undoCount;
	bool wasPlaying;
	float smudgeStrength;
	float** smTiles;
	int smTilesX;
	int smTilesY;
	float* smAccum;
	int smDim;
	bool smPrimed;
	float smCarry;
} TexEditor;

static TexEditor g_texEd = {0};

static float TexEd_UiScale(AppState* app) {
	return fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
}

static float TexEd_HitRadius(float s) {
	float r = 10.0f * s;
	return r < 8.0f ? 8.0f : r;
}

static Rectangle TexEd_PanelRect(AppState* app) {
	float s = TexEd_UiScale(app);
	float w = 640.0f * s;
	return (Rectangle){app->screenWidth - w - 10.0f * s, 10.0f * s, w, app->screenHeight - 20.0f * s};
}

static Rectangle TexEd_ToggleRect(AppState* app) {
	float s = TexEd_UiScale(app);
	float w = 190.0f * s;
	return (Rectangle){app->screenWidth - w - 10.0f * s, 265.0f * s, w, 32.0f * s};
}

static Rectangle TexEd_AtlasRect(AppState* app) {
	Rectangle p = TexEd_PanelRect(app);
	float s = TexEd_UiScale(app);
	float pad = 12.0f * s;
	float availW = p.width - 2.0f * pad;
	float availH = p.height - 290.0f * s;
	float k = fminf(availW / (float)g_texEd.working.width, availH / (float)g_texEd.working.height);
	float w = g_texEd.working.width * k;
	float h = g_texEd.working.height * k;
	return (Rectangle){p.x + (p.width - w) * 0.5f, p.y + 100.0f * s, w, h};
}

static void TexEd_CellSize(float* cw, float* ch) {
	*cw = (float)(g_texEd.original.width / g_texEd.gridCols[g_texEd.current]);
	*ch = (float)(g_texEd.original.height / g_texEd.gridRows[g_texEd.current]);
}

static TexCellXform TexEd_IdentityXform(void) {
	float cw, ch;
	TexEd_CellSize(&cw, &ch);
	float hw = cw * 0.5f;
	float hh = ch * 0.5f;
	TexCellXform xf = {{-hw, hw, hw, -hw}, {-hh, -hh, hh, hh}};
	return xf;
}

static bool TexEd_XformIsIdentity(const TexCellXform* xf) {
	TexCellXform id = TexEd_IdentityXform();
	for (int i = 0; i < 4; i++) {
		if (fabsf(xf->x[i] - id.x[i]) > 0.01f || fabsf(xf->y[i] - id.y[i]) > 0.01f) return false;
	}
	return true;
}

static Vector2 TexEd_Centroid(const TexCellXform* xf) {
	Vector2 c = {0.0f, 0.0f};
	for (int i = 0; i < 4; i++) {
		c.x += xf->x[i];
		c.y += xf->y[i];
	}
	return (Vector2){c.x * 0.25f, c.y * 0.25f};
}

static float TexEd_ScaleOf(const TexCellXform* xf) {
	float cw, ch;
	TexEd_CellSize(&cw, &ch);
	Vector2 c = TexEd_Centroid(xf);
	float r = 0.0f;
	for (int i = 0; i < 4; i++) r += hypotf(xf->x[i] - c.x, xf->y[i] - c.y);
	return (r * 0.25f) / (hypotf(cw, ch) * 0.5f);
}

static float TexEd_RotationOf(const TexCellXform* xf) {
	return atan2f(xf->y[1] - xf->y[0], xf->x[1] - xf->x[0]) * RAD2DEG;
}

static void TexEd_Translate(TexCellXform* xf, const TexCellXform* base, float dx, float dy) {
	for (int i = 0; i < 4; i++) {
		xf->x[i] = base->x[i] + dx;
		xf->y[i] = base->y[i] + dy;
	}
}

static void TexEd_RotateAbout(TexCellXform* xf, const TexCellXform* base, Vector2 pivot, float degrees) {
	float c = cosf(degrees * DEG2RAD);
	float s = sinf(degrees * DEG2RAD);
	for (int i = 0; i < 4; i++) {
		float dx = base->x[i] - pivot.x;
		float dy = base->y[i] - pivot.y;
		xf->x[i] = pivot.x + dx * c - dy * s;
		xf->y[i] = pivot.y + dx * s + dy * c;
	}
}

static void TexEd_ScaleAbout(TexCellXform* xf, float factor) {
	float cur = TexEd_ScaleOf(xf);
	if (cur <= 0.0001f) return;
	float f = Clamp(cur * factor, TEXED_SCALE_MIN, TEXED_SCALE_MAX) / cur;
	Vector2 c = TexEd_Centroid(xf);
	for (int i = 0; i < 4; i++) {
		xf->x[i] = c.x + (xf->x[i] - c.x) * f;
		xf->y[i] = c.y + (xf->y[i] - c.y) * f;
	}
}

static bool TexEd_IsDirty(void) {
	if (!g_texEd.loaded) return false;
	if (g_texEd.edited) return true;
	int cells = g_texEd.gridCols[g_texEd.current] * g_texEd.gridRows[g_texEd.current];
	for (int i = 0; i < cells; i++) {
		if (!TexEd_XformIsIdentity(&g_texEd.xform[i])) return true;
	}
	return false;
}

static void TexEd_ResetXforms(void) {
	TexCellXform id = TexEd_IdentityXform();
	for (int i = 0; i < TEXED_MAX_CELLS; i++) g_texEd.xform[i] = id;
}

static void TexEd_AddPath(const char* path, int cols, int rows) {
	if (!path || path[0] == '\0' || strcmp(path, "NULL") == 0) return;
	if (g_texEd.pathCount >= TEXED_MAX_PATHS) return;
	for (int i = 0; i < g_texEd.pathCount; i++) {
		if (strcmp(g_texEd.paths[i], path) == 0) return;
	}
	if (!FileExists(path)) return;
	int n = g_texEd.pathCount++;
	strncpy(g_texEd.paths[n], path, MAX_FILE_PATH_LENGTH - 1);
	g_texEd.paths[n][MAX_FILE_PATH_LENGTH - 1] = '\0';
	g_texEd.gridCols[n] = cols;
	g_texEd.gridRows[n] = rows;
}

static void TexEd_CollectPaths(AppState* app) {
	AnimatedCharacter* ch = app->character;
	int physCols = ch->renderer->physCols;
	int physRows = ch->renderer->physRows;
	g_texEd.pathCount = 0;
	for (int i = 0; i < ch->boneConfigCount; i++) {
		BoneConfig* cfg = &ch->boneConfigs[i];
		if (!cfg->valid) continue;
		bool asym = cfg->visibleMode == 2 || IsWristBone(cfg->boneName);
		TexEd_AddPath(cfg->texturePath, asym ? ASYM_ATLAS_COLS : physCols, asym ? ASYM_ATLAS_ROWS : physRows);
	}
	if (ch->textureSets) {
		for (int i = 0; i < ch->textureSets->setCount; i++) {
			BoneTextureSet* set = &ch->textureSets->sets[i];
			bool asym = IsWristBone(set->boneName);
			for (int j = 0; j < set->variantCount; j++) {
				TexEd_AddPath(set->variants[j].texturePath, asym ? ASYM_ATLAS_COLS : physCols, asym ? ASYM_ATLAS_ROWS : physRows);
			}
		}
	}
	g_texEd.owner = ch;
}

static void TexEd_SmudgeRelease(void);

static void TexEd_UndoClear(void) {
	for (int i = 0; i < g_texEd.undoCount; i++) {
		MemFree(g_texEd.undo[i].pixels);
		g_texEd.undo[i].pixels = NULL;
	}
	g_texEd.undoCount = 0;
}

static void TexEd_UndoPush(int x0, int y0, int x1, int y1) {
	int w = x1 - x0;
	int h = y1 - y0;
	if (w <= 0 || h <= 0) return;
	Color* pixels = (Color*)MemAlloc((unsigned int)(w * h * (int)sizeof(Color)));
	if (!pixels) return;
	const Color* src = (const Color*)g_texEd.strokeBase.data;
	int stride = g_texEd.strokeBase.width;
	for (int y = 0; y < h; y++) {
		memcpy(pixels + y * w, src + (y0 + y) * stride + x0, (size_t)w * sizeof(Color));
	}
	if (g_texEd.undoCount == TEXED_UNDO_MAX) {
		MemFree(g_texEd.undo[0].pixels);
		memmove(&g_texEd.undo[0], &g_texEd.undo[1], (TEXED_UNDO_MAX - 1) * sizeof(TexUndoPatch));
		g_texEd.undoCount--;
	}
	g_texEd.undo[g_texEd.undoCount++] = (TexUndoPatch){x0, y0, w, h, pixels};
}

static void TexEd_Unload(void) {
	if (!g_texEd.loaded) return;
	if (g_texEd.strokeActive) {
		UnloadImage(g_texEd.strokeBase);
		g_texEd.strokeActive = false;
	}
	TexEd_SmudgeRelease();
	TexEd_UndoClear();
	if (g_texEd.mask) {
		MemFree(g_texEd.mask);
		g_texEd.mask = NULL;
	}
	UnloadImage(g_texEd.original);
	UnloadImage(g_texEd.working);
	UnloadImage(g_texEd.disk);
	UnloadTexture(g_texEd.preview);
	g_texEd.loaded = false;
	g_texEd.edited = false;
	g_texEd.sliderActive = 0;
	g_texEd.dragMode = TEXED_DRAG_NONE;
}

static bool TexEd_Load(int index) {
	TexEd_Unload();
	Image img = LoadImage(g_texEd.paths[index]);
	if (img.data == NULL) return false;
	ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
	g_texEd.original = img;
	g_texEd.working = ImageCopy(img);
	g_texEd.disk = ImageCopy(img);
	g_texEd.mask = (float*)MemAlloc((unsigned int)(img.width * img.height * (int)sizeof(float)));
	g_texEd.edited = false;
	g_texEd.sliderActive = 0;
	g_texEd.preview = LoadTextureFromImage(g_texEd.working);
	SetTextureFilter(g_texEd.preview, TEXTURE_FILTER_BILINEAR);
	g_texEd.current = index;
	TexEd_ResetXforms();
	g_texEd.selected = 0;
	g_texEd.loaded = true;
	return true;
}

static bool TexEd_SolveHomography(const float* fx, const float* fy, const float* tu, const float* tv, double h[8]) {
	double m[8][9];
	for (int i = 0; i < 4; i++) {
		double x = fx[i];
		double y = fy[i];
		double u = tu[i];
		double v = tv[i];
		double* r0 = m[i * 2];
		double* r1 = m[i * 2 + 1];
		r0[0] = x; r0[1] = y; r0[2] = 1.0; r0[3] = 0.0; r0[4] = 0.0; r0[5] = 0.0; r0[6] = -u * x; r0[7] = -u * y; r0[8] = u;
		r1[0] = 0.0; r1[1] = 0.0; r1[2] = 0.0; r1[3] = x; r1[4] = y; r1[5] = 1.0; r1[6] = -v * x; r1[7] = -v * y; r1[8] = v;
	}
	for (int c = 0; c < 8; c++) {
		int p = c;
		for (int r = c + 1; r < 8; r++) {
			if (fabs(m[r][c]) > fabs(m[p][c])) p = r;
		}
		if (fabs(m[p][c]) < 1e-9) return false;
		if (p != c) {
			for (int k = 0; k < 9; k++) {
				double t = m[c][k];
				m[c][k] = m[p][k];
				m[p][k] = t;
			}
		}
		for (int r = 0; r < 8; r++) {
			if (r == c) continue;
			double f = m[r][c] / m[c][c];
			for (int k = c; k < 9; k++) m[r][k] -= f * m[c][k];
		}
	}
	for (int i = 0; i < 8; i++) h[i] = m[i][8] / m[i][i];
	return true;
}

static void TexEd_ComposeCell(int cell) {
	int cols = g_texEd.gridCols[g_texEd.current];
	int rows = g_texEd.gridRows[g_texEd.current];
	int cw = g_texEd.original.width / cols;
	int chh = g_texEd.original.height / rows;
	int x0 = (cell % cols) * cw;
	int y0 = (cell / cols) * chh;
	int stride = g_texEd.original.width;
	const TexCellXform* xf = &g_texEd.xform[cell];
	const Color* src = (const Color*)g_texEd.original.data;
	Color* dst = (Color*)g_texEd.working.data;
	float midX = cw * 0.5f;
	float midY = chh * 0.5f;

	float tu[4] = {-midX, midX, midX, -midX};
	float tv[4] = {-midY, -midY, midY, midY};
	double h[8];
	if (!TexEd_SolveHomography(xf->x, xf->y, tu, tv, h)) return;

	for (int y = 0; y < chh; y++) {
		for (int x = 0; x < cw; x++) {
			double px = x + 0.5 - midX;
			double py = y + 0.5 - midY;
			double den = h[6] * px + h[7] * py + 1.0;
			Color out = {0, 0, 0, 0};
			if (den > 1e-6) {
				float sx = (float)((h[0] * px + h[1] * py + h[2]) / den) + midX - 0.5f;
				float sy = (float)((h[3] * px + h[4] * py + h[5]) / den) + midY - 0.5f;
				int ix = (int)floorf(sx);
				int iy = (int)floorf(sy);
				float fx = sx - (float)ix;
				float fy = sy - (float)iy;
				float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
				for (int j = 0; j < 2; j++) {
					for (int i = 0; i < 2; i++) {
						int qx = ix + i;
						int qy = iy + j;
						if (qx < 0 || qy < 0 || qx >= cw || qy >= chh) continue;
						float w = (i ? fx : 1.0f - fx) * (j ? fy : 1.0f - fy);
						if (w <= 0.0f) continue;
						Color c = src[(y0 + qy) * stride + x0 + qx];
						float ca = (c.a / 255.0f) * w;
						r += c.r * ca;
						g += c.g * ca;
						b += c.b * ca;
						a += ca;
					}
				}
				if (a > 0.0001f) {
					out.r = (unsigned char)fminf(255.0f, r / a + 0.5f);
					out.g = (unsigned char)fminf(255.0f, g / a + 0.5f);
					out.b = (unsigned char)fminf(255.0f, b / a + 0.5f);
					out.a = (unsigned char)fminf(255.0f, a * 255.0f + 0.5f);
				}
			}
			dst[(y0 + y) * stride + x0 + x] = out;
		}
	}
}

static void TexEd_SyncTextures(AppState* app) {
	UpdateTexture(g_texEd.preview, g_texEd.working.data);
	BonesRenderer* r = app->character->renderer;
	for (int i = 0; i < r->textureCount; i++) {
		if (strcmp(r->texturePaths[i], g_texEd.paths[g_texEd.current]) != 0) continue;
		Texture2D t = r->textures[i];
		if (t.width != g_texEd.working.width || t.height != g_texEd.working.height ||
				t.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) return;
		Image pm = ImageCopy(g_texEd.working);
		ImageAlphaPremultiply(&pm);
		UpdateTexture(t, pm.data);
		UnloadImage(pm);
		return;
	}
}

static void TexEd_SyncRect(AppState* app, int x0, int y0, int x1, int y1) {
	int w = x1 - x0;
	int h = y1 - y0;
	if (w <= 0 || h <= 0) return;
	Color* buf = (Color*)MemAlloc((unsigned int)(w * h * (int)sizeof(Color)));
	if (!buf) return;
	const Color* src = (const Color*)g_texEd.working.data;
	int stride = g_texEd.working.width;
	for (int y = 0; y < h; y++) {
		memcpy(buf + y * w, src + (y0 + y) * stride + x0, (size_t)w * sizeof(Color));
	}
	Rectangle rect = {(float)x0, (float)y0, (float)w, (float)h};
	UpdateTextureRec(g_texEd.preview, rect, buf);
	BonesRenderer* r = app->character->renderer;
	for (int i = 0; i < r->textureCount; i++) {
		if (strcmp(r->texturePaths[i], g_texEd.paths[g_texEd.current]) != 0) continue;
		Texture2D t = r->textures[i];
		if (t.width != g_texEd.working.width || t.height != g_texEd.working.height ||
				t.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) break;
		for (int j = 0; j < w * h; j++) {
			unsigned int a = buf[j].a;
			buf[j].r = (unsigned char)((buf[j].r * a + 127) / 255);
			buf[j].g = (unsigned char)((buf[j].g * a + 127) / 255);
			buf[j].b = (unsigned char)((buf[j].b * a + 127) / 255);
		}
		UpdateTextureRec(t, rect, buf);
		break;
	}
	MemFree(buf);
}

static bool TexEd_UndoPop(AppState* app) {
	if (g_texEd.undoCount == 0) return false;
	TexUndoPatch patch = g_texEd.undo[--g_texEd.undoCount];
	Color* work = (Color*)g_texEd.working.data;
	Color* orig = (Color*)g_texEd.original.data;
	int stride = g_texEd.working.width;
	for (int y = 0; y < patch.h; y++) {
		size_t off = (size_t)(patch.y + y) * stride + patch.x;
		memcpy(work + off, patch.pixels + y * patch.w, (size_t)patch.w * sizeof(Color));
		memcpy(orig + off, patch.pixels + y * patch.w, (size_t)patch.w * sizeof(Color));
	}
	TexEd_SyncRect(app, patch.x, patch.y, patch.x + patch.w, patch.y + patch.h);
	MemFree(patch.pixels);
	return true;
}

static void TexEd_BakeXforms(void) {
	int cells = g_texEd.gridCols[g_texEd.current] * g_texEd.gridRows[g_texEd.current];
	bool pending = false;
	for (int i = 0; i < cells; i++) {
		if (!TexEd_XformIsIdentity(&g_texEd.xform[i])) {
			pending = true;
			break;
		}
	}
	if (!pending) return;
	UnloadImage(g_texEd.original);
	g_texEd.original = ImageCopy(g_texEd.working);
	TexEd_ResetXforms();
	TexEd_UndoClear();
	g_texEd.edited = true;
}

static void TexEd_BrushShape(float* radius, float* inner, float* soft) {
	float R = g_texEd.brushSize * 0.5f;
	float hard = Clamp(g_texEd.brushHardness, 0.0f, 1.0f);
	float sf = fminf(fmaxf(R * (1.0f - hard), 1.25f), R);
	*radius = R;
	*soft = sf;
	*inner = R - sf;
}

static float TexEd_BrushCoverage(float d, float R, float inner, float soft) {
	if (d >= R) return 0.0f;
	float u = (d - inner) / soft;
	return u <= 0.0f ? 1.0f : 1.0f - u * u * (3.0f - 2.0f * u);
}

static void TexEd_TouchBounds(int x0, int y0, int x1, int y1) {
	if (x0 < g_texEd.strokeMinX) g_texEd.strokeMinX = x0;
	if (y0 < g_texEd.strokeMinY) g_texEd.strokeMinY = y0;
	if (x1 > g_texEd.strokeMaxX) g_texEd.strokeMaxX = x1;
	if (y1 > g_texEd.strokeMaxY) g_texEd.strokeMaxY = y1;
	if (x0 < g_texEd.frameMinX) g_texEd.frameMinX = x0;
	if (y0 < g_texEd.frameMinY) g_texEd.frameMinY = y0;
	if (x1 > g_texEd.frameMaxX) g_texEd.frameMaxX = x1;
	if (y1 > g_texEd.frameMaxY) g_texEd.frameMaxY = y1;
}

static void TexEd_EraseSegment(Vector2 a, Vector2 b) {
	int W = g_texEd.working.width;
	int H = g_texEd.working.height;
	float R, inner, soft;
	TexEd_BrushShape(&R, &inner, &soft);
	float opacity = Clamp(g_texEd.brushOpacity, 0.0f, 1.0f);
	int x0 = (int)floorf(fminf(a.x, b.x) - R - 1.0f);
	int x1 = (int)ceilf(fmaxf(a.x, b.x) + R + 1.0f);
	int y0 = (int)floorf(fminf(a.y, b.y) - R - 1.0f);
	int y1 = (int)ceilf(fmaxf(a.y, b.y) + R + 1.0f);
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > W) x1 = W;
	if (y1 > H) y1 = H;
	if (x1 <= x0 || y1 <= y0) return;

	float dx = b.x - a.x;
	float dy = b.y - a.y;
	float len2 = dx * dx + dy * dy;
	const Color* base = (const Color*)g_texEd.strokeBase.data;
	Color* dst = (Color*)g_texEd.working.data;
	float* mask = g_texEd.mask;

	for (int y = y0; y < y1; y++) {
		float py = y + 0.5f;
		for (int x = x0; x < x1; x++) {
			float px = x + 0.5f;
			float t = 0.0f;
			if (len2 > 1e-6f) t = Clamp(((px - a.x) * dx + (py - a.y) * dy) / len2, 0.0f, 1.0f);
			float d = hypotf(a.x + dx * t - px, a.y + dy * t - py);
			float cov = TexEd_BrushCoverage(d, R, inner, soft);
			if (cov <= 0.0f) continue;
			int idx = y * W + x;
			if (cov <= mask[idx]) continue;
			mask[idx] = cov;
			dst[idx].a = (unsigned char)(base[idx].a * (1.0f - opacity * cov) + 0.5f);
		}
	}

	TexEd_TouchBounds(x0, y0, x1, y1);
}

static float TexEd_SmudgeSpacing(void) {
	return fmaxf(0.75f, g_texEd.brushSize * 0.08f);
}

static float* TexEd_SmudgeTile(int tx, int ty) {
	int idx = ty * g_texEd.smTilesX + tx;
	float* tile = g_texEd.smTiles[idx];
	if (tile) return tile;
	tile = (float*)MemAlloc((unsigned int)(TEXED_TILE * TEXED_TILE * 4 * (int)sizeof(float)));
	if (!tile) return NULL;
	int W = g_texEd.strokeBase.width;
	int H = g_texEd.strokeBase.height;
	const Color* base = (const Color*)g_texEd.strokeBase.data;
	for (int y = 0; y < TEXED_TILE; y++) {
		int iy = ty * TEXED_TILE + y;
		if (iy >= H) break;
		for (int x = 0; x < TEXED_TILE; x++) {
			int ix = tx * TEXED_TILE + x;
			if (ix >= W) break;
			Color c = base[iy * W + ix];
			float a = c.a / 255.0f;
			float* px = tile + (y * TEXED_TILE + x) * 4;
			px[0] = c.r / 255.0f * a;
			px[1] = c.g / 255.0f * a;
			px[2] = c.b / 255.0f * a;
			px[3] = a;
		}
	}
	g_texEd.smTiles[idx] = tile;
	return tile;
}

static float* TexEd_SmudgePixel(int x, int y) {
	float* tile = TexEd_SmudgeTile(x >> TEXED_TILE_SHIFT, y >> TEXED_TILE_SHIFT);
	if (!tile) return NULL;
	return tile + (((y & (TEXED_TILE - 1)) << TEXED_TILE_SHIFT) + (x & (TEXED_TILE - 1))) * 4;
}

static void TexEd_SmudgeStore(int x, int y, const float* px) {
	Color* dst = (Color*)g_texEd.working.data + (size_t)y * g_texEd.working.width + x;
	float a = Clamp(px[3], 0.0f, 1.0f);
	dst->a = (unsigned char)(a * 255.0f + 0.5f);
	if (a > 1e-4f) {
		float inv = 255.0f / a;
		dst->r = (unsigned char)Clamp(px[0] * inv + 0.5f, 0.0f, 255.0f);
		dst->g = (unsigned char)Clamp(px[1] * inv + 0.5f, 0.0f, 255.0f);
		dst->b = (unsigned char)Clamp(px[2] * inv + 0.5f, 0.0f, 255.0f);
	}
}

static void TexEd_SmudgeRelease(void) {
	if (g_texEd.smTiles) {
		int n = g_texEd.smTilesX * g_texEd.smTilesY;
		for (int i = 0; i < n; i++) {
			if (g_texEd.smTiles[i]) MemFree(g_texEd.smTiles[i]);
		}
		MemFree(g_texEd.smTiles);
		g_texEd.smTiles = NULL;
	}
	if (g_texEd.smAccum) {
		MemFree(g_texEd.smAccum);
		g_texEd.smAccum = NULL;
	}
	g_texEd.smPrimed = false;
	g_texEd.smCarry = 0.0f;
}

static void TexEd_SmudgeDab(Vector2 c) {
	int W = g_texEd.working.width;
	int H = g_texEd.working.height;
	float R, inner, soft;
	TexEd_BrushShape(&R, &inner, &soft);
	float strength = Clamp(g_texEd.smudgeStrength, 0.0f, 1.0f);
	float ratio = TexEd_SmudgeSpacing() / R;
	float rateDab = 1.0f - powf(1.0f - strength, ratio);
	float rateAcc = 1.0f - powf(strength, ratio);
	int D = g_texEd.smDim;
	int ox = (int)floorf(c.x - R) - 1;
	int oy = (int)floorf(c.y - R) - 1;
	int x0 = ox < 0 ? 0 : ox;
	int y0 = oy < 0 ? 0 : oy;
	int x1 = ox + D > W ? W : ox + D;
	int y1 = oy + D > H ? H : oy + D;
	if (x1 <= x0 || y1 <= y0) return;
	float* acc = g_texEd.smAccum;

	if (!g_texEd.smPrimed) {
		for (int y = y0; y < y1; y++) {
			for (int x = x0; x < x1; x++) {
				float* px = TexEd_SmudgePixel(x, y);
				if (!px) continue;
				memcpy(acc + ((y - oy) * D + (x - ox)) * 4, px, 4 * sizeof(float));
			}
		}
		g_texEd.smPrimed = true;
		return;
	}

	for (int y = y0; y < y1; y++) {
		float py = y + 0.5f - c.y;
		for (int x = x0; x < x1; x++) {
			float px0 = x + 0.5f - c.x;
			float cov = TexEd_BrushCoverage(hypotf(px0, py), R, inner, soft);
			if (cov <= 0.0f) continue;
			float* px = TexEd_SmudgePixel(x, y);
			if (!px) continue;
			float* a = acc + ((y - oy) * D + (x - ox)) * 4;
			float w = rateDab * cov;
			for (int ch = 0; ch < 4; ch++) {
				float before = px[ch];
				px[ch] = before + (a[ch] - before) * w;
				a[ch] += (before - a[ch]) * rateAcc;
			}
			TexEd_SmudgeStore(x, y, px);
		}
	}
	TexEd_TouchBounds(x0, y0, x1, y1);
}

static bool TexEd_SmudgeBegin(Vector2 pos) {
	TexEd_SmudgeRelease();
	g_texEd.smTilesX = (g_texEd.working.width + TEXED_TILE - 1) >> TEXED_TILE_SHIFT;
	g_texEd.smTilesY = (g_texEd.working.height + TEXED_TILE - 1) >> TEXED_TILE_SHIFT;
	g_texEd.smTiles = (float**)MemAlloc((unsigned int)(g_texEd.smTilesX * g_texEd.smTilesY * (int)sizeof(float*)));
	g_texEd.smDim = (int)ceilf(g_texEd.brushSize) + 4;
	g_texEd.smAccum = (float*)MemAlloc((unsigned int)(g_texEd.smDim * g_texEd.smDim * 4 * (int)sizeof(float)));
	if (!g_texEd.smTiles || !g_texEd.smAccum) {
		TexEd_SmudgeRelease();
		return false;
	}
	g_texEd.smCarry = 0.0f;
	g_texEd.smPrimed = false;
	TexEd_SmudgeDab(pos);
	return true;
}

static void TexEd_SmudgeWalk(Vector2 a, Vector2 b) {
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	float len = hypotf(dx, dy);
	if (len < 1e-4f) return;
	float spacing = TexEd_SmudgeSpacing();
	float t = spacing - g_texEd.smCarry;
	float lastT = 0.0f;
	bool placed = false;
	while (t <= len) {
		float f = t / len;
		TexEd_SmudgeDab((Vector2){a.x + dx * f, a.y + dy * f});
		lastT = t;
		placed = true;
		t += spacing;
	}
	g_texEd.smCarry = placed ? len - lastT : g_texEd.smCarry + len;
}

static void TexEd_StrokeSegment(Vector2 a, Vector2 b) {
	if (g_texEd.tool == TEXED_TOOL_SMUDGE) {
		TexEd_SmudgeWalk(a, b);
	} else {
		TexEd_EraseSegment(a, b);
	}
}

static void TexEd_ResetFrameBounds(void) {
	g_texEd.frameMinX = g_texEd.working.width;
	g_texEd.frameMinY = g_texEd.working.height;
	g_texEd.frameMaxX = 0;
	g_texEd.frameMaxY = 0;
}

static void TexEd_FlushFrame(AppState* app) {
	TexEd_SyncRect(app, g_texEd.frameMinX, g_texEd.frameMinY, g_texEd.frameMaxX, g_texEd.frameMaxY);
	TexEd_ResetFrameBounds();
}

static void TexEd_BeginStroke(AppState* app, Vector2 pos) {
	if (!g_texEd.mask) return;
	TexEd_BakeXforms();
	g_texEd.strokeBase = ImageCopy(g_texEd.working);
	g_texEd.strokeActive = true;
	g_texEd.dragMode = TEXED_DRAG_BRUSH;
	g_texEd.strokeMinX = g_texEd.working.width;
	g_texEd.strokeMinY = g_texEd.working.height;
	g_texEd.strokeMaxX = 0;
	g_texEd.strokeMaxY = 0;
	TexEd_ResetFrameBounds();
	g_texEd.strokePrevPt = pos;
	g_texEd.strokePrevMid = pos;
	if (g_texEd.tool == TEXED_TOOL_SMUDGE) {
		if (!TexEd_SmudgeBegin(pos)) {
			UnloadImage(g_texEd.strokeBase);
			g_texEd.strokeActive = false;
			g_texEd.dragMode = TEXED_DRAG_NONE;
			return;
		}
	} else {
		TexEd_EraseSegment(pos, pos);
	}
	TexEd_FlushFrame(app);
}

static void TexEd_StrokeMove(Vector2 pos) {
	Vector2 prev = g_texEd.strokePrevPt;
	if (pos.x == prev.x && pos.y == prev.y) return;
	Vector2 mid = {(prev.x + pos.x) * 0.5f, (prev.y + pos.y) * 0.5f};
	Vector2 p0 = g_texEd.strokePrevMid;
	float length = hypotf(prev.x - p0.x, prev.y - p0.y) + hypotf(mid.x - prev.x, mid.y - prev.y);
	float spacing = fmaxf(1.0f, g_texEd.brushSize * 0.5f * 0.2f);
	int n = (int)ceilf(length / spacing);
	if (n < 1) n = 1;
	if (n > 48) n = 48;
	Vector2 last = p0;
	for (int i = 1; i <= n; i++) {
		float t = (float)i / (float)n;
		float u = 1.0f - t;
		Vector2 q = {u * u * p0.x + 2.0f * u * t * prev.x + t * t * mid.x,
				u * u * p0.y + 2.0f * u * t * prev.y + t * t * mid.y};
		TexEd_StrokeSegment(last, q);
		last = q;
	}
	g_texEd.strokePrevMid = mid;
	g_texEd.strokePrevPt = pos;
}

static void TexEd_EndStroke(AppState* app) {
	TexEd_StrokeSegment(g_texEd.strokePrevMid, g_texEd.strokePrevPt);
	TexEd_FlushFrame(app);
	int x0 = g_texEd.strokeMinX;
	int y0 = g_texEd.strokeMinY;
	int x1 = g_texEd.strokeMaxX;
	int y1 = g_texEd.strokeMaxY;
	int W = g_texEd.working.width;
	if (x1 > x0 && y1 > y0) {
		TexEd_UndoPush(x0, y0, x1, y1);
		const Color* work = (const Color*)g_texEd.working.data;
		Color* orig = (Color*)g_texEd.original.data;
		for (int y = y0; y < y1; y++) {
			memcpy(orig + (size_t)y * W + x0, work + (size_t)y * W + x0, (size_t)(x1 - x0) * sizeof(Color));
			memset(g_texEd.mask + (size_t)y * W + x0, 0, (size_t)(x1 - x0) * sizeof(float));
		}
		g_texEd.edited = true;
	}
	UnloadImage(g_texEd.strokeBase);
	g_texEd.strokeActive = false;
	g_texEd.dragMode = TEXED_DRAG_NONE;
	TexEd_SmudgeRelease();
}

static void TexEd_ResetAll(AppState* app) {
	if (g_texEd.strokeActive) return;
	TexEd_ResetXforms();
	UnloadImage(g_texEd.original);
	UnloadImage(g_texEd.working);
	g_texEd.original = ImageCopy(g_texEd.disk);
	g_texEd.working = ImageCopy(g_texEd.disk);
	g_texEd.edited = false;
	TexEd_UndoClear();
	TexEd_SyncTextures(app);
}

static void TexEd_CommitSave(void) {
	UnloadImage(g_texEd.original);
	g_texEd.original = ImageCopy(g_texEd.working);
	UnloadImage(g_texEd.disk);
	g_texEd.disk = ImageCopy(g_texEd.working);
	g_texEd.edited = false;
	TexEd_ResetXforms();
}

static void TexEd_Slider(int id, Rectangle r, const char* label, float* value, float lo, float hi, bool logScale) {
	Vector2 m = GetMousePosition();
	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && g_texEd.sliderActive == 0 && CheckCollisionPointRec(m, r)) {
		g_texEd.sliderActive = id;
	}
	if (g_texEd.sliderActive == id) {
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			float t = Clamp((m.x - r.x) / r.width, 0.0f, 1.0f);
			*value = logScale ? lo * powf(hi / lo, t) : lo + t * (hi - lo);
		} else {
			g_texEd.sliderActive = 0;
		}
	}
	float t = logScale ? logf(*value / lo) / logf(hi / lo) : (*value - lo) / (hi - lo);
	t = Clamp(t, 0.0f, 1.0f);
	DrawRectangleRec(r, (Color){50, 50, 50, 255});
	DrawRectangleRec((Rectangle){r.x, r.y, r.width * t, r.height}, (Color){70, 130, 200, 255});
	DrawRectangleLinesEx(r, 2, BLACK);
	int fs = (int)(r.height * 0.45f);
	if (fs < 10) fs = 10;
	int tw = MeasureText(label, fs);
	DrawText(label, (int)(r.x + (r.width - tw) * 0.5f), (int)(r.y + (r.height - fs) * 0.5f), fs, WHITE);
}

static void TexEd_DrawChecker(Rectangle ar, float cell) {
	for (float y = 0.0f; y < ar.height; y += cell) {
		float h = fminf(cell, ar.height - y);
		for (float x = 0.0f; x < ar.width; x += cell) {
			float w = fminf(cell, ar.width - x);
			bool odd = (((int)(x / cell)) + ((int)(y / cell))) & 1;
			DrawRectangleRec((Rectangle){ar.x + x, ar.y + y, w, h}, odd ? (Color){120, 120, 120, 255} : (Color){165, 165, 165, 255});
		}
	}
}

static void TexEd_DrawFitText(const char* text, int x, int y, int fs, int maxWidth, Color color) {
	while (fs > 8 && MeasureText(text, fs) > maxWidth) fs--;
	DrawText(text, x, y, fs, color);
}

static void TexEd_CellChanged(AppState* app, int cell) {
	TexEd_UndoClear();
	TexEd_ComposeCell(cell);
	TexEd_SyncTextures(app);
}

static Vector2 TexEd_CellCenterScreen(Rectangle ar, int cell) {
	int cols = g_texEd.gridCols[g_texEd.current];
	int rows = g_texEd.gridRows[g_texEd.current];
	float cellW = ar.width / cols;
	float cellH = ar.height / rows;
	return (Vector2){ar.x + (cell % cols + 0.5f) * cellW, ar.y + (cell / cols + 0.5f) * cellH};
}

static Vector2 TexEd_VertexScreen(const TexCellXform* xf, int i, Vector2 center, float k) {
	return (Vector2){center.x + xf->x[i] * k, center.y + xf->y[i] * k};
}

static Vector2 TexEd_RotHandleScreen(const TexCellXform* xf, Vector2 center, float k, float dist, Vector2* anchor) {
	Vector2 a = {center.x + (xf->x[0] + xf->x[1]) * 0.5f * k, center.y + (xf->y[0] + xf->y[1]) * 0.5f * k};
	Vector2 cen = TexEd_Centroid(xf);
	Vector2 d = {a.x - (center.x + cen.x * k), a.y - (center.y + cen.y * k)};
	float len = hypotf(d.x, d.y);
	if (len < 0.001f) {
		d = (Vector2){0.0f, -1.0f};
	} else {
		d = (Vector2){d.x / len, d.y / len};
	}
	*anchor = a;
	return (Vector2){a.x + d.x * dist, a.y + d.y * dist};
}

static bool TexEd_RotHandleBlocksUi(AppState* app) {
	if (g_texEd.dragMode != TEXED_DRAG_NONE) return true;
	if (g_texEd.tool != TEXED_TOOL_MOVE) return false;
	Rectangle ar = TexEd_AtlasRect(app);
	float k = ar.width / (float)g_texEd.working.width;
	float s = TexEd_UiScale(app);
	TexCellXform* xf = &g_texEd.xform[g_texEd.selected];
	Vector2 center = TexEd_CellCenterScreen(ar, g_texEd.selected);
	Vector2 anchor;
	Vector2 rot = TexEd_RotHandleScreen(xf, center, k, 30.0f * s, &anchor);
	Vector2 m = GetMousePosition();
	return hypotf(m.x - rot.x, m.y - rot.y) <= TexEd_HitRadius(s);
}

static int TexEd_HitVertex(const TexCellXform* xf, Vector2 center, float k, Vector2 m, float radius) {
	for (int i = 0; i < 4; i++) {
		Vector2 v = TexEd_VertexScreen(xf, i, center, k);
		if (hypotf(m.x - v.x, m.y - v.y) <= radius) return i;
	}
	return -1;
}

static void TexEd_EnforceFrameLock(AppState* app) {
	if (!g_texEd.open || !app->character) return;
	app->editor.isPlaying = false;
	SetCharacterAutoPlay(app->character, false);
	if (app->character->animation.frameCount > 0 && app->character->currentFrame != 0) {
		SetCharacterFrame(app->character, 0);
	}
}

static void TexEd_Close(AppState* app) {
	if (!g_texEd.open) return;
	g_texEd.open = false;
	if (g_texEd.wasPlaying && app->character) {
		app->editor.isPlaying = true;
		SetCharacterAutoPlay(app->character, true);
	}
	g_texEd.wasPlaying = false;
}

static void TexEd_Open(AppState* app) {
	if (g_texEd.owner != app->character) {
		TexEd_Unload();
		TexEd_CollectPaths(app);
	}
	if (!g_texEd.open) g_texEd.wasPlaying = app->editor.isPlaying;
	g_texEd.open = true;
	TexEd_EnforceFrameLock(app);
	if (g_texEd.brushSize <= 0.0f) {
		g_texEd.brushSize = 40.0f;
		g_texEd.brushHardness = 0.5f;
		g_texEd.brushOpacity = 1.0f;
		g_texEd.smudgeStrength = 0.6f;
	}
	if (!g_texEd.loaded && g_texEd.pathCount > 0) TexEd_Load(0);
}

static bool TexEd_UpdateBrush(AppState* app, Rectangle ar, float k, bool hover, bool inAtlas) {
	Vector2 m = GetMousePosition();
	Vector2 pos = {(m.x - ar.x) / k, (m.y - ar.y) / k};
	bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

	if (g_texEd.dragMode == TEXED_DRAG_NONE && g_texEd.sliderActive == 0 && inAtlas && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		TexEd_BeginStroke(app, pos);
	}

	if (g_texEd.dragMode == TEXED_DRAG_BRUSH) {
		TexEd_StrokeMove(pos);
		TexEd_FlushFrame(app);
		if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) TexEd_EndStroke(app);
	} else {
		if (hover && ctrl && IsKeyPressed(KEY_Z)) TexEd_UndoPop(app);
		float wheel = GetMouseWheelMove();
		if (inAtlas && wheel != 0.0f) {
			g_texEd.brushSize = Clamp(g_texEd.brushSize * powf(1.1f, wheel), TEXED_BRUSH_MIN, TEXED_BRUSH_MAX);
		}
	}
	return hover || g_texEd.dragMode != TEXED_DRAG_NONE || g_texEd.sliderActive != 0;
}

static bool TexEd_Update(AppState* app) {
	if (IsKeyPressed(KEY_F7)) {
		if (g_texEd.open) TexEd_Close(app);
		else TexEd_Open(app);
	}
	Vector2 m = GetMousePosition();
	if (!g_texEd.open) {
		return app->showUI && CheckCollisionPointRec(m, TexEd_ToggleRect(app));
	}

	bool hover = CheckCollisionPointRec(m, TexEd_PanelRect(app));
	if (!g_texEd.loaded) return hover;

	int cols = g_texEd.gridCols[g_texEd.current];
	int rows = g_texEd.gridRows[g_texEd.current];
	Rectangle ar = TexEd_AtlasRect(app);
	float k = ar.width / (float)g_texEd.working.width;
	bool inAtlas = CheckCollisionPointRec(m, ar);
	float s = TexEd_UiScale(app);
	float hitR = TexEd_HitRadius(s);
	bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

	if (hover && IsKeyPressed(KEY_B) && g_texEd.dragMode == TEXED_DRAG_NONE) {
		g_texEd.tool = (TexTool)((g_texEd.tool + 1) % 3);
	}
	if (g_texEd.tool != TEXED_TOOL_MOVE) return TexEd_UpdateBrush(app, ar, k, hover, inAtlas);

	if (hover && g_texEd.dragMode == TEXED_DRAG_NONE && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		TexCellXform* sel = &g_texEd.xform[g_texEd.selected];
		Vector2 center = TexEd_CellCenterScreen(ar, g_texEd.selected);
		int hitV = TexEd_HitVertex(sel, center, k, m, hitR);
		Vector2 anchor;
		Vector2 rot = TexEd_RotHandleScreen(sel, center, k, 30.0f * s, &anchor);
		if (hitV >= 0) {
			g_texEd.dragMode = TEXED_DRAG_VERTEX;
			g_texEd.dragVertex = hitV;
			g_texEd.dragStartMouse = m;
			g_texEd.dragStartXform = *sel;
		} else if (hypotf(m.x - rot.x, m.y - rot.y) <= hitR) {
			Vector2 cen = TexEd_Centroid(sel);
			Vector2 pv = {center.x + cen.x * k, center.y + cen.y * k};
			g_texEd.dragMode = TEXED_DRAG_ROTATE;
			g_texEd.dragPivot = cen;
			g_texEd.dragStartAngle = atan2f(m.y - pv.y, m.x - pv.x) * RAD2DEG;
			g_texEd.dragStartMouse = m;
			g_texEd.dragStartXform = *sel;
		} else if (inAtlas) {
			int c = (int)((m.x - ar.x) / (ar.width / cols));
			int r = (int)((m.y - ar.y) / (ar.height / rows));
			if (c >= cols) c = cols - 1;
			if (r >= rows) r = rows - 1;
			g_texEd.selected = r * cols + c;
			g_texEd.dragMode = TEXED_DRAG_MOVE;
			g_texEd.dragStartMouse = m;
			g_texEd.dragStartXform = g_texEd.xform[g_texEd.selected];
		}
	}

	TexCellXform* xf = &g_texEd.xform[g_texEd.selected];
	TexCellXform before = *xf;

	if (g_texEd.dragMode != TEXED_DRAG_NONE) {
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
			float dx = roundf((m.x - g_texEd.dragStartMouse.x) / k);
			float dy = roundf((m.y - g_texEd.dragStartMouse.y) / k);
			if (g_texEd.dragMode == TEXED_DRAG_MOVE) {
				TexEd_Translate(xf, &g_texEd.dragStartXform, dx, dy);
			} else if (g_texEd.dragMode == TEXED_DRAG_VERTEX) {
				*xf = g_texEd.dragStartXform;
				xf->x[g_texEd.dragVertex] += dx;
				xf->y[g_texEd.dragVertex] += dy;
			} else {
				Vector2 center = TexEd_CellCenterScreen(ar, g_texEd.selected);
				Vector2 pv = {center.x + g_texEd.dragPivot.x * k, center.y + g_texEd.dragPivot.y * k};
				float d = atan2f(m.y - pv.y, m.x - pv.x) * RAD2DEG - g_texEd.dragStartAngle;
				if (shift) d = roundf(d / TEXED_ROT_SNAP) * TEXED_ROT_SNAP;
				TexEd_RotateAbout(xf, &g_texEd.dragStartXform, g_texEd.dragPivot, d);
			}
		} else {
			g_texEd.dragMode = TEXED_DRAG_NONE;
		}
	}

	if (inAtlas) {
		float wheel = GetMouseWheelMove();
		if (wheel != 0.0f) TexEd_ScaleAbout(xf, powf(1.03f, wheel));
	}

	if (hover) {
		float step = shift ? 8.0f : 1.0f;
		float rotStep = shift ? TEXED_ROT_SNAP : 1.0f;
		if (IsKeyPressed(KEY_LEFT))  TexEd_Translate(xf, xf, -step, 0.0f);
		if (IsKeyPressed(KEY_RIGHT)) TexEd_Translate(xf, xf, step, 0.0f);
		if (IsKeyPressed(KEY_UP))    TexEd_Translate(xf, xf, 0.0f, -step);
		if (IsKeyPressed(KEY_DOWN))  TexEd_Translate(xf, xf, 0.0f, step);
		if (IsKeyPressed(KEY_Q)) TexEd_RotateAbout(xf, xf, TexEd_Centroid(xf), -rotStep);
		if (IsKeyPressed(KEY_E)) TexEd_RotateAbout(xf, xf, TexEd_Centroid(xf), rotStep);
		if (IsKeyPressed(KEY_R)) *xf = TexEd_IdentityXform();
	}

	if (memcmp(&before, xf, sizeof(before)) != 0) TexEd_CellChanged(app, g_texEd.selected);
	return hover || g_texEd.dragMode != TEXED_DRAG_NONE;
}

static void TexEd_Draw(AppState* app) {
	if (!g_texEd.open) {
		if (app->showUI && Button(TexEd_ToggleRect(app), "TEXTURAS (F7)", DARKBLUE)) TexEd_Open(app);
		return;
	}

	float s = TexEd_UiScale(app);
	Rectangle p = TexEd_PanelRect(app);
	int pad = (int)(12 * s);
	int fs = (int)(16 * s);
	if (fs < 12) fs = 12;

	DrawRectangleRec(p, (Color){30, 30, 30, 240});
	DrawRectangleLinesEx(p, 2, BLACK);
	DrawText("TEXTURE EDITOR", (int)p.x + pad, (int)p.y + pad, fs + 4, YELLOW);

	Rectangle closeBtn = {p.x + p.width - 100 * s - pad, p.y + 8 * s, 100 * s, 32 * s};
	if (Button(closeBtn, "CLOSE", RED)) TexEd_Close(app);

	if (!g_texEd.loaded) {
		DrawText("No hay texturas para editar", (int)p.x + pad, (int)p.y + (int)(60 * s), fs, WHITE);
		return;
	}

	int cols = g_texEd.gridCols[g_texEd.current];
	int rows = g_texEd.gridRows[g_texEd.current];
	bool dirty = TexEd_IsDirty();
	bool moveTool = g_texEd.tool == TEXED_TOOL_MOVE;

	float tbW = 72.0f * s;
	Rectangle undoBtn = {closeBtn.x - 8.0f * s - tbW, p.y + 8.0f * s, tbW, 32.0f * s};
	Rectangle smudgeBtn = {undoBtn.x - 6.0f * s - tbW, p.y + 8.0f * s, tbW, 32.0f * s};
	Rectangle eraseBtn = {smudgeBtn.x - 6.0f * s - tbW, p.y + 8.0f * s, tbW, 32.0f * s};
	Rectangle moveBtn = {eraseBtn.x - 6.0f * s - tbW, p.y + 8.0f * s, tbW, 32.0f * s};
	bool idle = g_texEd.dragMode == TEXED_DRAG_NONE && g_texEd.sliderActive == 0;
	if (ToggleButton(moveBtn, "MOVER", g_texEd.tool == TEXED_TOOL_MOVE, BLUE, GRAY) && idle) g_texEd.tool = TEXED_TOOL_MOVE;
	if (ToggleButton(eraseBtn, "BORRAR", g_texEd.tool == TEXED_TOOL_ERASE, BLUE, GRAY) && idle) g_texEd.tool = TEXED_TOOL_ERASE;
	if (ToggleButton(smudgeBtn, "SMUDGE", g_texEd.tool == TEXED_TOOL_SMUDGE, BLUE, GRAY) && idle) g_texEd.tool = TEXED_TOOL_SMUDGE;
	if (Button(undoBtn, "DESHACER", DARKGRAY) && idle) TexEd_UndoPop(app);
	moveTool = g_texEd.tool == TEXED_TOOL_MOVE;

	if (moveTool) {
		TexCellXform* zxf = &g_texEd.xform[g_texEd.selected];
		Rectangle zoomInBtn = {p.x + p.width - pad - 30.0f * s, p.y + 52.0f * s, 30.0f * s, 32.0f * s};
		Rectangle zoomOutBtn = {zoomInBtn.x - 4.0f * s - 30.0f * s, p.y + 52.0f * s, 30.0f * s, 32.0f * s};
		float step = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? TEXED_CELL_ZOOM_FINE : TEXED_CELL_ZOOM_STEP;
		float scaleBefore = TexEd_ScaleOf(zxf);
		if (Button(zoomOutBtn, "-", GRAY) && idle) TexEd_ScaleAbout(zxf, 1.0f / step);
		if (Button(zoomInBtn, "+", GRAY) && idle) TexEd_ScaleAbout(zxf, step);
		if (fabsf(TexEd_ScaleOf(zxf) - scaleBefore) > 1e-6f) TexEd_CellChanged(app, g_texEd.selected);
		const char* zoomText = TextFormat("Zoom celda %d%%", (int)roundf(TexEd_ScaleOf(zxf) * 100.0f));
		DrawText(zoomText, (int)(zoomOutBtn.x - 8.0f * s) - MeasureText(zoomText, fs), (int)(p.y + 60 * s), fs, LIGHTGRAY);
	}

	Rectangle prevBtn = {p.x + pad, p.y + 52 * s, 40 * s, 32 * s};
	Rectangle nextBtn = {p.x + pad + 48 * s, p.y + 52 * s, 40 * s, 32 * s};
	int delta = 0;
	bool uiBlocked = TexEd_RotHandleBlocksUi(app);
	if (Button(prevBtn, "<", GRAY) && !uiBlocked) delta = -1;
	if (Button(nextBtn, ">", GRAY) && !uiBlocked) delta = 1;
	if (delta != 0) {
		if (dirty) {
			SetSaveStatus(app, "Guarda o resetea los cambios antes de cambiar de textura", true);
		} else {
			int next = (g_texEd.current + delta + g_texEd.pathCount) % g_texEd.pathCount;
			TexEd_Load(next);
			return;
		}
	}
	DrawText(TextFormat("%s  [%dx%d]  %d/%d%s", GetFileName(g_texEd.paths[g_texEd.current]), cols, rows,
				g_texEd.current + 1, g_texEd.pathCount, dirty ? "  *" : ""),
			(int)(p.x + pad + 100 * s), (int)(p.y + 60 * s), fs, WHITE);

	Rectangle ar = TexEd_AtlasRect(app);
	float k = ar.width / (float)g_texEd.working.width;
	if (moveTool) {
		DrawRectangleRec(ar, (Color){90, 90, 90, 255});
	} else {
		TexEd_DrawChecker(ar, 8.0f * s < 4.0f ? 4.0f : 8.0f * s);
	}
	DrawTexturePro(g_texEd.preview, (Rectangle){0, 0, (float)g_texEd.working.width, (float)g_texEd.working.height},
			ar, (Vector2){0, 0}, 0.0f, WHITE);

	float cellW = ar.width / cols;
	float cellH = ar.height / rows;
	for (int i = 1; i < cols; i++) {
		DrawLineV((Vector2){ar.x + i * cellW, ar.y}, (Vector2){ar.x + i * cellW, ar.y + ar.height}, (Color){255, 255, 255, 110});
	}
	for (int i = 1; i < rows; i++) {
		DrawLineV((Vector2){ar.x, ar.y + i * cellH}, (Vector2){ar.x + ar.width, ar.y + i * cellH}, (Color){255, 255, 255, 110});
	}
	DrawRectangleLinesEx(ar, 1, WHITE);
	int idxSize = fs - 2;
	for (int i = 0; i < cols * rows; i++) {
		DrawText(TextFormat("%d", i), (int)(ar.x + (i % cols) * cellW + 3), (int)(ar.y + (i / cols) * cellH + 2), idxSize, YELLOW);
	}

	TexCellXform* xf = &g_texEd.xform[g_texEd.selected];
	Vector2 m = GetMousePosition();
	float footerY = p.y + p.height - 140 * s;

	if (moveTool) {
		Rectangle sel = {ar.x + (g_texEd.selected % cols) * cellW, ar.y + (g_texEd.selected / cols) * cellH, cellW, cellH};
		DrawRectangleLinesEx(sel, 3, RED);

		Vector2 center = TexEd_CellCenterScreen(ar, g_texEd.selected);
		float hitR = TexEd_HitRadius(s);
		int hoverV = TexEd_HitVertex(xf, center, k, m, hitR);
		Color quadColor = {0, 255, 255, 220};
		for (int i = 0; i < 4; i++) {
			Vector2 a = TexEd_VertexScreen(xf, i, center, k);
			Vector2 b = TexEd_VertexScreen(xf, (i + 1) % 4, center, k);
			DrawLineEx(a, b, 2.0f, quadColor);
		}
		Vector2 anchor;
		Vector2 rot = TexEd_RotHandleScreen(xf, center, k, 30.0f * s, &anchor);
		DrawLineEx(anchor, rot, 2.0f, LIME);
		bool rotActive = g_texEd.dragMode == TEXED_DRAG_ROTATE || hypotf(m.x - rot.x, m.y - rot.y) <= hitR;
		DrawCircleV(rot, hitR * 0.8f, rotActive ? ORANGE : LIME);
		DrawCircleLinesV(rot, hitR * 0.8f, BLACK);
		for (int i = 0; i < 4; i++) {
			Vector2 v = TexEd_VertexScreen(xf, i, center, k);
			bool active = (g_texEd.dragMode == TEXED_DRAG_VERTEX && g_texEd.dragVertex == i) || (g_texEd.dragMode == TEXED_DRAG_NONE && hoverV == i);
			DrawCircleV(v, hitR * 0.7f, active ? ORANGE : WHITE);
			DrawCircleLinesV(v, hitR * 0.7f, BLACK);
		}

	} else {
		bool overView = CheckCollisionPointRec(m, ar);
		if (g_texEd.sliderActive == 0 && (overView || g_texEd.dragMode == TEXED_DRAG_BRUSH)) {
			float rad = g_texEd.brushSize * 0.5f * k;
			DrawCircleLinesV(m, rad + 1.0f, BLACK);
			DrawCircleLinesV(m, rad, WHITE);
			if (g_texEd.brushHardness < 0.99f) DrawCircleLinesV(m, rad * g_texEd.brushHardness, (Color){255, 255, 255, 110});
		}
	}

	int textMaxW = (int)p.width - 2 * pad;
	if (moveTool) {
		Vector2 cen = TexEd_Centroid(xf);
		DrawText(TextFormat("Celda %d (col %d, fila %d)   offset %d, %d px   giro %.1f   escala %.2f", g_texEd.selected,
					g_texEd.selected % cols, g_texEd.selected / cols, (int)roundf(cen.x), (int)roundf(cen.y),
					TexEd_RotationOf(xf), TexEd_ScaleOf(xf)),
				(int)p.x + pad, (int)footerY, fs, WHITE);
		DrawText("Arrastrar: mover  |  Vertices: estirar  |  Circulo verde: girar (Shift 15)  |  Rueda: escala",
				(int)p.x + pad, (int)(footerY + 22 * s), fs - 3, LIGHTGRAY);
		TexEd_DrawFitText("Flechas: 1px (Shift 8)  |  Q/E: girar  |  R: reset  |  Rueda o botones +/-: zoom celda (Shift fino)  |  B: herramienta",
				(int)p.x + pad, (int)(footerY + 40 * s), fs - 3, textMaxW, LIGHTGRAY);
	} else {
		bool smudge = g_texEd.tool == TEXED_TOOL_SMUDGE;
		float* thirdValue = smudge ? &g_texEd.smudgeStrength : &g_texEd.brushOpacity;
		float sliderY = footerY - 40 * s;
		float gap = 10 * s;
		float sw = (p.width - 2.0f * pad - 2.0f * gap) / 3.0f;
		float sh = 30 * s;
		TexEd_Slider(1, (Rectangle){p.x + pad, sliderY, sw, sh},
				TextFormat("Tamano: %d px", (int)roundf(g_texEd.brushSize)), &g_texEd.brushSize,
				TEXED_BRUSH_MIN, TEXED_BRUSH_MAX, true);
		TexEd_Slider(2, (Rectangle){p.x + pad + sw + gap, sliderY, sw, sh},
				TextFormat("Dureza: %d%%", (int)roundf(g_texEd.brushHardness * 100.0f)), &g_texEd.brushHardness,
				0.0f, 1.0f, false);
		TexEd_Slider(3, (Rectangle){p.x + pad + 2.0f * (sw + gap), sliderY, sw, sh},
				TextFormat("%s: %d%%", smudge ? "Fuerza" : "Opacidad", (int)roundf(*thirdValue * 100.0f)), thirdValue,
				0.0f, 1.0f, false);

		DrawText(TextFormat("%s   tamano %d px   dureza %d%%   %s %d%%", smudge ? "Smudge" : "Borrador",
					(int)roundf(g_texEd.brushSize), (int)roundf(g_texEd.brushHardness * 100.0f),
					smudge ? "fuerza" : "opacidad", (int)roundf(*thirdValue * 100.0f)),
				(int)p.x + pad, (int)footerY, fs, WHITE);
		TexEd_DrawFitText(smudge ? "Arrastrar: empujar pixeles  |  Rueda: tamano  |  Ctrl+Z: deshacer"
				: "Arrastrar: borrar  |  Rueda: tamano  |  Ctrl+Z: deshacer",
				(int)p.x + pad, (int)(footerY + 22 * s), fs - 3, textMaxW, LIGHTGRAY);
		TexEd_DrawFitText("B: cambiar herramienta  |  RESET TODO: vuelve al archivo guardado",
				(int)p.x + pad, (int)(footerY + 40 * s), fs - 3, textMaxW, LIGHTGRAY);
	}

	float btnY = footerY + 62 * s;
	float btnW = (p.width - 4.0f * pad) / 3.0f;
	Rectangle resetCell = {p.x + pad, btnY, btnW, 40 * s};
	Rectangle resetAll = {p.x + 2 * pad + btnW, btnY, btnW, 40 * s};
	Rectangle saveBtn = {p.x + 3 * pad + 2 * btnW, btnY, btnW, 40 * s};

	if (Button(resetCell, "RESET CELDA", GRAY)) {
		*xf = TexEd_IdentityXform();
		TexEd_CellChanged(app, g_texEd.selected);
	}
	if (Button(resetAll, "RESET TODO", ORANGE)) TexEd_ResetAll(app);
	if (Button(saveBtn, "GUARDAR", GREEN)) {
		const char* path = g_texEd.paths[g_texEd.current];
		if (ExportImage(g_texEd.working, path)) {
			TexEd_CommitSave();
			SetSaveStatus(app, TextFormat("Textura guardada: %s", path), false);
		} else {
			SetSaveStatus(app, "No se pudo guardar la textura", true);
		}
	}
}

// ============================================================================
// MAIN UI DRAWING
// ============================================================================

static void App_DrawUI(AppState* app) {
	if (!app->showUI) return;
	float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);
	int titleSize = (int)(20 * scale);
	if (titleSize < 14) titleSize = 14;
	int textSize = (int)(14 * scale);
	if (textSize < 10) textSize = 10;
	int smallSize = (int)(12 * scale);
	if (smallSize < 9) smallSize = 9;

	int maxFrameNumber = FindMaxFrameNumber(app);
	int existingFrames = app->character->animation.frameCount;
	int currentFrameNumber = GetCurrentFrameNumber(app);

	int marginX = (int)(10 * scale);
	int marginY = (int)(10 * scale);
	DrawText("BONES3D ANIMATION EDITOR", marginX, marginY, titleSize, BLUE);

	if (g_characterManager.currentProfileIndex >= 0 &&
			g_characterManager.currentProfileIndex < g_characterManager.profileCount) {
		char profileText[128];
		snprintf(profileText, sizeof(profileText), "Character: %s",
				g_characterManager.profiles[g_characterManager.currentProfileIndex].name);
		DrawText(profileText, marginX, marginY + titleSize + (int)(5 * scale), textSize, ORANGE);
	}

	int yPos = marginY + titleSize + textSize + (int)(10 * scale);
	DrawText("1/2/3/4: Switch Character | 5-8: Load Anims | H/T: Billboards", marginX, yPos, smallSize, DARKGRAY);
	yPos += smallSize + (int)(3 * scale);
	DrawText("LEFT CLICK: Select | RIGHT CLICK: Move | CTRL+LEFT (timeline): Drag keyframe", marginX, yPos, smallSize, DARKGRAY);

	yPos += smallSize + (int)(5 * scale);
	char frameText[128];
	snprintf(frameText, sizeof(frameText), "Animation: %s | Frame: %d/%d (%d existing) %s %s",
			app->currentAnimation,
			currentFrameNumber,
			maxFrameNumber,
			existingFrames,
			app->editor.isPlaying ? "[PLAYING]" : "[PAUSED]",
			app->editor.needsSave ? "[*]" : "");
	DrawText(frameText, marginX, yPos, textSize, app->editor.needsSave ? ORANGE : DARKGRAY);

	yPos += textSize + (int)(3 * scale);
	if (app->editor.isDraggingKeyframe) {
		DrawText("[DRAGGING KEYFRAME] Release to confirm", marginX, yPos, textSize, PURPLE);
	} else if (app->editor.hasBoneSelected) {
		bool isKeyframe = IsCurrentFrameKeyframe(app);
		char selectionText[256];
		if (app->editor.isDraggingBone) {
			snprintf(selectionText, sizeof(selectionText), "[DRAGGING BONE] %s | Live preview",
					app->editor.selectedBoneName);
			DrawText(selectionText, marginX, yPos, textSize, RED);
		} else if (isKeyframe) {
			snprintf(selectionText, sizeof(selectionText), "[KEYFRAME] %s | Right-click: move",
					app->editor.selectedBoneName);
			DrawText(selectionText, marginX, yPos, textSize, GREEN);
		} else {
			snprintf(selectionText, sizeof(selectionText), "[INTERPOLATED] %s | Right-click: convert & move",
					app->editor.selectedBoneName);
			DrawText(selectionText, marginX, yPos, textSize, ORANGE);
		}
	}

	if (app->editor.saveStatusTimer > 0.0f && app->editor.saveStatusMessage[0] != '\0') {
		yPos += textSize + (int)(3 * scale);
		Color statusColor = app->editor.saveStatusIsError ? RED : DARKGREEN;
		DrawText(app->editor.saveStatusMessage, marginX, yPos, textSize + 2, statusColor);
	}
}

static bool TexEd_BlocksMouse(AppState* app) {
	if (!g_texEd.open) return false;
	if (g_texEd.dragMode != TEXED_DRAG_NONE || g_texEd.sliderActive != 0) return true;
	return CheckCollisionPointRec(GetMousePosition(), TexEd_PanelRect(app));
}

static void App_Draw(AppState* app) {
	if (!app) return;

	BeginDrawing();
	ClearBackground(RAYWHITE);
	if (TexEd_BlocksMouse(app)) SetMouseOffset(-100000, -100000);

	BeginMode3D(app->character->renderer->camera);
	DrawGrid(10, 1.0f);
	EndMode3D();

	DrawOnionSkin(app);
	DrawAnimatedCharacter(app->character, app->character->renderer->camera);
	DrawBoneOrientation(app);
	DrawDebugVisuals(app);
	DrawBoneSelectionFeedback(app);
	DrawBoneRotationHandle(app);
	DrawBoneOrientGizmo(app);

	DrawCameraGizmo(app);
	App_DrawUI(app);
	DrawDebugPanel(app);
	DrawTimeline(app);
	DrawControlPanel(app);
	DrawExportDialog(app);
	DrawAddFramesDialog(app);
	SetMouseOffset(0, 0);
	TexEd_Draw(app);

	EndDrawing();
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

static void App_HandleInput(AppState* app) {
	if (!app) return;
	int maxFrameNumber = FindMaxFrameNumber(app);
	int currentFrameNumber = GetCurrentFrameNumber(app);

	if (IsKeyDown(KEY_LEFT_CONTROL)) {
		if (IsKeyPressed(KEY_Z)) {
			PerformUndo(app);
			return;
		}
		if (IsKeyPressed(KEY_Y)) {
			PerformRedo(app);
			return;
		}
		if (IsKeyPressed(KEY_S)) {
			PerformSave(app);
			return;
		}
		if (IsKeyPressed(KEY_C)) {
			CopyPoseToClipboard(app);
			return;
		}
		if (IsKeyPressed(KEY_V)) {
			PastePoseFromClipboardWithUndo(app);
			return;
		}
		return;
	}

	if (IsKeyPressed(KEY_ONE) && g_characterManager.profileCount > 0) {
		SwitchCharacterProfile(app, 0);
		return;
	}
	if (IsKeyPressed(KEY_TWO) && g_characterManager.profileCount > 1) {
		SwitchCharacterProfile(app, 1);
		return;
	}
	if (IsKeyPressed(KEY_THREE) && g_characterManager.profileCount > 2) {
		SwitchCharacterProfile(app, 2);
		return;
	}
	if (IsKeyPressed(KEY_FOUR) && g_characterManager.profileCount > 3) {
		SwitchCharacterProfile(app, 3);
		return;
	}
	if (IsKeyPressed(KEY_FIVE) && g_characterManager.profileCount > 4) {
		SwitchCharacterProfile(app, 4);
		return;
	}
	if (IsKeyPressed(KEY_SIX) && g_characterManager.profileCount > 5) {
		SwitchCharacterProfile(app, 5);
		return;
	}
	if (IsKeyPressed(KEY_SEVEN) && g_characterManager.profileCount > 6) {
		SwitchCharacterProfile(app, 6);
		return;
	}
	if (IsKeyPressed(KEY_EIGHT) && g_characterManager.profileCount > 7) {
		SwitchCharacterProfile(app, 7);
		return;
	}
	if (IsKeyPressed(KEY_NINE) && g_characterManager.profileCount > 8) {
		SwitchCharacterProfile(app, 8);
		return;
	}

	if (IsKeyPressed(KEY_SPACE)) {
		// Detectar si la animación terminó automáticamente (sin loop)
		bool animationFinished = false;
		if (app->character && app->character->animController && 
		    app->character->animController->currentClipIndex >= 0) {
			AnimationClipMetadata* clip = &app->character->animController->clips[app->character->animController->currentClipIndex];
			// La animación terminó si: no tiene loop, isPlaying está true pero controller->playing está false
			if (!clip->loop && app->editor.isPlaying && !app->character->animController->playing) {
				animationFinished = true;
			}
		}
		
		// Si la animación terminó automáticamente, no cambiar isPlaying, solo reiniciar
		if (animationFinished) {
			TraceLog(LOG_INFO, "Animation finished, restarting...");
			RestartAnimation(app->character);
			app->editor.isPlaying = true;
			SetCharacterAutoPlay(app->character, true);
		} else {
			// Comportamiento normal: toggle play/pause
			app->editor.isPlaying = !app->editor.isPlaying;
			SetCharacterAutoPlay(app->character, app->editor.isPlaying);
		}
	}

	if (IsKeyPressed(KEY_LEFT)) {
		for (int i = currentFrameNumber - 1; i >= 0; i--) {
			if (FrameExists(app, i)) {
				int frameIndex = FindFrameIndexByNumber(app, i);
				if (frameIndex != -1) {
					SetCharacterFrame(app->character, frameIndex);
				}
				break;
			}
		}
	}

	if (IsKeyPressed(KEY_RIGHT)) {
		for (int i = currentFrameNumber + 1; i <= maxFrameNumber; i++) {
			if (FrameExists(app, i)) {
				int frameIndex = FindFrameIndexByNumber(app, i);
				if (frameIndex != -1) {
					SetCharacterFrame(app->character, frameIndex);
				}
				break;
			}
		}
	}

	if (IsKeyPressed(KEY_HOME)) {
		for (int i = 0; i <= maxFrameNumber; i++) {
			if (FrameExists(app, i)) {
				int frameIndex = FindFrameIndexByNumber(app, i);
				if (frameIndex != -1) {
					SetCharacterFrame(app->character, frameIndex);
				}
				break;
			}
		}
	}

	if (IsKeyPressed(KEY_END)) {
		for (int i = maxFrameNumber; i >= 0; i--) {
			if (FrameExists(app, i)) {
				int frameIndex = FindFrameIndexByNumber(app, i);
				if (frameIndex != -1) {
					SetCharacterFrame(app->character, frameIndex);
				}
				break;
			}
		}
	}

	if (IsKeyPressed(KEY_C)) {
		Vector3 cameraTarget = app->character->autoCenterCalculated ?
			app->character->autoCenter : (Vector3){0, 0.6f, 0};

		if (app->camMode == 1) {
			app->character->renderer->camera.position = (Vector3){
				cameraTarget.x + 1.5f,
					cameraTarget.y + 0.1f,
					cameraTarget.z + 1.5f};

			Vector3 direction = Vector3Subtract(cameraTarget, app->character->renderer->camera.position);
			direction = Vector3Normalize(direction);

			app->orbitYaw = atan2f(direction.x, direction.z);
			app->orbitPitch = asinf(direction.y);

			app->character->renderer->camera.target = cameraTarget;
			app->camMode = 2;
			DisableCursor();
		} else {
			app->camMode = 1;
			app->orbitRadius = 2.5f;
			app->orbitYaw = 0.0f;
			app->orbitPitch = 0.0f;
			EnableCursor();
		}
	}

	// Note: KEY_FIVE through KEY_NINE are now used for character switching (see above)
	// Use the 'N' key to cycle through animations (next) and 'B' key for previous

	if (IsKeyPressed(KEY_N)) {
		LoadNextAnimation(app);
		return;
	}

	if (IsKeyPressed(KEY_B)) {
		LoadPreviousAnimation(app);
		return;
	}

	if (IsKeyPressed(KEY_H)) {
		SetCharacterBillboards(app->character,
				!app->character->renderHeadBillboards,
				app->character->renderTorsoBillboards);
	}
	if (IsKeyPressed(KEY_T)) {
		SetCharacterBillboards(app->character,
				app->character->renderHeadBillboards,
				!app->character->renderTorsoBillboards);
	}

	if (IsKeyPressed(KEY_F1)) {
		app->showUI = !app->showUI;
		app->editor.showTimeline = app->showUI;
	}
	if (IsKeyPressed(KEY_F2)) {
		app->debug.showDebugSpheres = !app->debug.showDebugSpheres;
	}
	if (IsKeyPressed(KEY_F3)) {
		app->debug.showBoneNames = !app->debug.showBoneNames;
	}
	if (IsKeyPressed(KEY_F4)) {
		app->debug.showConnections = !app->debug.showConnections;
	}
	if (IsKeyPressed(KEY_F5)) {
		app->debug.showOrientation = !app->debug.showOrientation;
	}
	if (IsKeyPressed(KEY_F6)) {
		app->debug.showOnionSkin = !app->debug.showOnionSkin;
	}
	if (IsKeyPressed(KEY_DELETE) && app->editor.selectionStart != -1 && FrameExists(app, app->editor.selectionStart)) {
		int frameIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
		if (frameIndex != -1) {
			int deletedFrameNumber = app->editor.selectionStart;
			DeleteFrameWithUndo(app, frameIndex);
			app->editor.needsSave = true;
			int nextValidFrame = -1;
			for (int i = deletedFrameNumber; i <= maxFrameNumber; i++) {
				if (FrameExists(app, i)) {
					nextValidFrame = i;
					break;
				}
			}
			if (nextValidFrame == -1) {
				for (int i = deletedFrameNumber - 1; i >= 0; i--) {
					if (FrameExists(app, i)) {
						nextValidFrame = i;
						break;
					}
				}
			}
			if (nextValidFrame != -1) {
				int newIndex = FindFrameIndexByNumber(app, nextValidFrame);
				if (newIndex != -1) {
					SetCharacterFrame(app->character, newIndex);
					app->editor.selectionStart = nextValidFrame;
					app->editor.selectionEnd = nextValidFrame;
				}
			} else {
				app->editor.selectionStart = -1;
				app->editor.selectionEnd = -1;
			}
		}
	}
}

// ============================================================================
// CAMERA UPDATE
// ============================================================================

static void App_UpdateCamera(AppState* app, float dt) {
	if (!app || !app->character) return;

	Vector3 baseCameraTarget = app->character->autoCenterCalculated ?
		app->character->autoCenter : (Vector3){0, 0.6f, 0};

	if (app->camMode == 1) {
		UpdateCameraGizmo(app);

		// Mover el target con las flechas del teclado
		float moveSpeed = TARGET_MOVE_SENSITIVITY;
		if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
			moveSpeed *= 3.0f; // Más rápido con Shift
		
		// Calcular vectores de la cámara
		Vector3 cameraPos = app->character->renderer->camera.position;
		Vector3 currentTarget = Vector3Add(baseCameraTarget, app->targetOffset);
		Vector3 forward = Vector3Normalize(Vector3Subtract(currentTarget, cameraPos));
		Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, (Vector3){0, 1, 0}));
		Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

		// Movimiento con flechas en relación a la orientación de la cámara
		if (IsKeyDown(KEY_UP)) {
			app->targetOffset = Vector3Add(app->targetOffset, Vector3Scale(up, moveSpeed));
		}
		if (IsKeyDown(KEY_DOWN)) {
			app->targetOffset = Vector3Subtract(app->targetOffset, Vector3Scale(up, moveSpeed));
		}
		if (IsKeyDown(KEY_LEFT)) {
			app->targetOffset = Vector3Subtract(app->targetOffset, Vector3Scale(right, moveSpeed));
		}
		if (IsKeyDown(KEY_RIGHT)) {
			app->targetOffset = Vector3Add(app->targetOffset, Vector3Scale(right, moveSpeed));
		}

		// Limitar el offset para evitar que la cámara se vuelva inestable
		float offsetLength = Vector3Length(app->targetOffset);
		if (offsetLength > MAX_TARGET_OFFSET) {
			app->targetOffset = Vector3Scale(Vector3Normalize(app->targetOffset), MAX_TARGET_OFFSET);
		}

		// Reset del offset con la tecla R
		if (IsKeyPressed(KEY_R)) {
			app->targetOffset = (Vector3){0, 0, 0};
		}

		// Aplicar el offset al target
		Vector3 cameraTarget = Vector3Add(baseCameraTarget, app->targetOffset);

		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
				!app->editor.isDraggingSlider &&
				!app->editor.isDraggingKeyframe &&
				!app->editor.isDraggingGizmo &&
				!app->editor.isDraggingRotationHandle &&
				app->editor.orientDragAxis == 0 &&
				!CheckCollisionPointRec(GetMousePosition(), GetTimelineRect(app)) &&
				!CheckCollisionPointRec(GetMousePosition(), GetGizmoRect(app))) {
			Vector2 mouseDelta = GetMouseDelta();
			app->orbitYaw += mouseDelta.x * ORBIT_SENSITIVITY;

			while (app->orbitYaw > PI) app->orbitYaw -= 2.0f * PI;
			while (app->orbitYaw < -PI) app->orbitYaw += 2.0f * PI;

			app->orbitPitch = Clamp(app->orbitPitch - mouseDelta.y * ORBIT_SENSITIVITY,
					MIN_PITCH, MAX_PITCH);
		}

		float wheel = GetMouseWheelMove();
		if (wheel != 0.0f && !CheckCollisionPointRec(GetMousePosition(), GetTimelineRect(app))) {
			app->orbitRadius = Clamp(app->orbitRadius - wheel * ZOOM_SENSITIVITY,
					MIN_ORBIT_RADIUS, MAX_ORBIT_RADIUS);
		}

		float cosP = cosf(app->orbitPitch);
		float sinP = sinf(app->orbitPitch);
		float cosY = cosf(app->orbitYaw);
		float sinY = sinf(app->orbitYaw);

		app->character->renderer->camera.position = (Vector3){
			cameraTarget.x + app->orbitRadius * cosP * sinY,
				cameraTarget.y + app->orbitRadius * sinP,
				cameraTarget.z + app->orbitRadius * cosP * cosY};

		app->character->renderer->camera.target = cameraTarget;
	} else {
		Vector2 mouse_delta = GetMouseDelta();
		app->orbitYaw += mouse_delta.x * FPS_SENSITIVITY;

		while (app->orbitYaw > PI) app->orbitYaw -= 2.0f * PI;
		while (app->orbitYaw < -PI) app->orbitYaw += 2.0f * PI;

		app->orbitPitch -= mouse_delta.y * FPS_SENSITIVITY;
		app->orbitPitch = Clamp(app->orbitPitch, MIN_PITCH, MAX_PITCH);

		Vector3 forward;
		forward.x = cosf(app->orbitPitch) * cosf(app->orbitYaw);
		forward.y = sinf(app->orbitPitch);
		forward.z = cosf(app->orbitPitch) * sinf(app->orbitYaw);
		forward = Vector3Normalize(forward);

		Vector3 right_dir = Vector3Normalize(Vector3CrossProduct(forward,
					app->character->renderer->camera.up));

		float speed = BASE_SPEED * dt;
		if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
			speed *= MOVEMENT_SPEED;

		if (IsKeyDown(KEY_W))
			app->character->renderer->camera.position =
				Vector3Add(app->character->renderer->camera.position, Vector3Scale(forward, speed));
		if (IsKeyDown(KEY_S))
			app->character->renderer->camera.position =
				Vector3Subtract(app->character->renderer->camera.position, Vector3Scale(forward, speed));
		if (IsKeyDown(KEY_D))
			app->character->renderer->camera.position =
					Vector3Add(app->character->renderer->camera.position, Vector3Scale(right_dir, speed));
		if (IsKeyDown(KEY_A))
			app->character->renderer->camera.position =
				Vector3Subtract(app->character->renderer->camera.position, Vector3Scale(right_dir, speed));

		app->character->renderer->camera.target =
			Vector3Add(app->character->renderer->camera.position, forward);
	}

	UpdateTimelineKeyframeDragging(app);
	UpdateBoneOrientGizmo(app);
	if (!app->editor.isDraggingKeyframe && app->editor.orientDragAxis == 0) {
		UpdateBoneSelection(app);
		UpdateBoneRotationHandle(app);
	}
}

// ============================================================================
// INITIALIZATION AND SHUTDOWN
// ============================================================================

static bool App_Init(AppState* app) {
	if (!app) return false;
	memset(app, 0, sizeof(*app));
	InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - Animation Editor");
	Bones_SetWriteDepth(false);
	SetWindowState(FLAG_WINDOW_RESIZABLE);
#if defined(linux)
	for (int i = 0; i < 5; i++) PollInputEvents();
#endif
	MaximizeWindow();
	SetTargetFPS(120);
	if (!LoadCharacterProfiles(&g_characterManager, "data/characters.txt")) {
		TraceLog(LOG_WARNING, "Could not load character profiles, using default");
		CharacterProfile* defaultProfile = &g_characterManager.profiles[0];
		strcpy(defaultProfile->name, "default");
		strcpy(defaultProfile->texturesConfigPath, "data/textures/hil/bone_textures.txt");
		strcpy(defaultProfile->textureSetsPath, "data/textures/hil/texture_sets.txt");
		strcpy(defaultProfile->animationsPath, "data/animations/");
		g_characterManager.profileCount = 1;
		g_characterManager.currentProfileIndex = 0;
	}

	app->editor.showAddFramesDialog = false;
	app->editor.framesToAdd = 10;
	app->camMode = 1;
	app->orbitRadius = 2.5f;
	app->orbitPitch = 0.0f;
	app->targetOffset = (Vector3){0, 0, 0};
	app->showUI = true;
	app->editor.showTimeline = true;
	app->editor.isPlaying = true;
	app->editor.selectedFrame = 0;
	app->editor.selectionStart = -1;
	app->editor.selectionEnd = -1;
	app->editor.interpolationCount = 5;
	app->editor.playbackSpeed = 1.0f;
	app->editor.isDraggingKeyframe = false;
	app->editor.draggedKeyframeNumber = -1;
	app->editor.isDraggingGizmo = false;
	strcpy(app->editor.exportPath, "data/poses/exported.json");
	InitUndoHistory(&app->editor.undoHistory);
	app->debug.showBoneNames = false;
	app->debug.showDebugSpheres = false;
	app->debug.showConnections = false;
	app->debug.showOrientation = false;
	app->screenWidth = GetScreenWidth();
	app->screenHeight = GetScreenHeight();

	if (app->character && app->character->renderTorsos) {
		for (int i = 0; i < app->character->renderTorsosCount; i++) {
			app->character->renderTorsos[i].disableCompensation = false;
		}
	}

	if (!SwitchCharacterProfile(app, 0)) {
		CloseWindow();
		return false;
	}

	return true;
}

static void App_Shutdown(AppState* app) {
	if (!app) return;
	TexEd_Unload();
	DestroyAnimatedCharacter(app->character);
	CloseWindow();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

int main(void) {
	AppState app;
	if (!App_Init(&app)) return -1;
	while (!WindowShouldClose()) {
		float dt = GetFrameTime();
		app.screenWidth = GetScreenWidth();
		app.screenHeight = GetScreenHeight();
		if (!TexEd_Update(&app)) {
			App_HandleInput(&app);
			App_UpdateCamera(&app, dt);
		}
		TexEd_EnforceFrameLock(&app);
		if (app.editor.saveStatusTimer > 0.0f) {
			app.editor.saveStatusTimer -= dt;
		}
		DrawGrid(10, 1.0f);
		UpdateAnimatedCharacter(app.character, dt);
		InjectOrientOffsets(&app);
		App_Draw(&app);
	}
	App_Shutdown(&app);
	return 0;
}