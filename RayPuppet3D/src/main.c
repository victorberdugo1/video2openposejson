#include "bones_core.h"
#include <dirent.h>

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
	UNDO_FRAME_PROMOTE
} UndoActionType;

typedef struct {
	UndoActionType type;
	char boneName[64];
	int frameNumber;
	Vector3 oldPosition;
	Vector3 newPosition;
	int oldFrameNumber;
	int newFrameNumber;
	int promotedFrameNumber;
} UndoAction;

typedef struct {
	UndoAction actions[MAX_UNDO_STACK];
	int count;
	int currentIndex;
} UndoHistory;

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
} EditorState;

typedef struct {
	bool showBoneNames;
	bool showDebugSpheres;
	bool showConnections;
	bool showOrientation;
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
static CharacterManager g_characterManager = {0};
static void RecalculateAffectedInterpolations(AppState* app, int movedKeyframe);
static void MoveBoneInFrame(AppState* app, int frameNumber, const char* boneName, Vector3 newPosition);
static void MoveKeyframeInTimeline(AppState* app, int fromFrameNumber, int toFrameNumber);
static int FindFrameIndexByNumber(AppState* app, int frameNumber);
static int FindMaxFrameNumber(AppState* app);
static bool Button(Rectangle bounds, const char* text, Color color);
static void DrawTextField(Rectangle bounds, const char* text, bool active);
static void AddMultipleFramesAtEnd(AppState* app, int numFramesToAdd, BonesAnimation* animation);
static void DrawAddFramesDialog(AppState* app);
bool BonesDeleteFrame(AppState* app, BonesAnimation* animation, int frameIndex);
bool BonesDuplicateFrame(AppState* app, BonesAnimation* animation, int frameIndex);

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
	DIR* dir = opendir(animationsPath);
	if (!dir) {
		TraceLog(LOG_WARNING, "Cannot open animations directory: %s", animationsPath);
		if (manager) manager->animationCount = 0;
		return false;
	}

	manager->animationCount = 0;
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL && manager->animationCount < MAX_ANIMATIONS) {
		const char* name = entry->d_name;
		if (!name) continue;

		if (HasExtension(name, ".anim")) {
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
	}
	closedir(dir);

	if (manager->animationCount > 0) {
		manager->currentAnimationIndex = 0;
		TraceLog(LOG_INFO, "Loaded %d animations from %s", manager->animationCount, animationsPath);
		return true;
	} else {
		manager->currentAnimationIndex = -1;
		return false;
	}
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

static void InitUndoHistory(UndoHistory* history) {
	memset(history, 0, sizeof(UndoHistory));
	history->count = 0;
	history->currentIndex = -1;
}

static void PushUndoAction(UndoHistory* history, UndoAction action) {
	if (history->currentIndex < history->count - 1) {
		history->count = history->currentIndex + 1;
	}
	if (history->count >= MAX_UNDO_STACK) {
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

	int startDeleteIdx = frameIndex;
	int endDeleteIdx = frameIndex;

	if (prevKeyframeIdx != -1) {
		startDeleteIdx = prevKeyframeIdx + 1;
	}

	int framesToDelete = endDeleteIdx - startDeleteIdx + 1;

	TraceLog(LOG_INFO, "Deleting keyframe at %d and %d interpolated frames (total: %d)",
			frameIndex, framesToDelete - 1, framesToDelete);

	for (int i = startDeleteIdx; i < animation->frameCount - framesToDelete; i++) {
		animation->frames[i] = animation->frames[i + framesToDelete];
	}

	animation->frameCount -= framesToDelete;

	if (prevKeyframeIdx != -1 && nextKeyframeIdx != -1) {
		int newPrevIdx = prevKeyframeIdx;
		int newNextIdx = nextKeyframeIdx - framesToDelete;

		if (newNextIdx > newPrevIdx + 1) {
			BonesInterpolateFrames(animation, newPrevIdx, newNextIdx,
					newNextIdx - newPrevIdx - 1);
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
		AddMultipleFramesAtEnd(app, app->editor.framesToAdd, &app->character->animation);
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

static void LoadAnimationForCurrentProfile(AppState* app, const char* animName) {
	if (g_characterManager.currentProfileIndex < 0 ||
			g_characterManager.currentProfileIndex >= g_characterManager.profileCount) {
		return;
	}
	CharacterProfile* profile = &g_characterManager.profiles[g_characterManager.currentProfileIndex];

	char animPath[512];
	char metaPath[512];
	snprintf(animPath, sizeof(animPath), "%s%s.json", profile->animationsPath, animName);
	snprintf(metaPath, sizeof(metaPath), "%s%s.anim", profile->animationsPath, animName);

	bool wasPlaying = app->editor.isPlaying;

	if (LoadAnimation(app->character, animPath, metaPath)) {
		strcpy(app->currentAnimation, animName);
		InitUndoHistory(&app->editor.undoHistory);

		app->editor.isPlaying = wasPlaying;
		SetCharacterAutoPlay(app->character, wasPlaying);

		if (app->character->animation.frameCount > 0) {
			SetCharacterFrame(app->character, 0);
		}

		TraceLog(LOG_INFO, "Loaded animation: %s (playing: %d)", animName, wasPlaying);
	} else {
		TraceLog(LOG_WARNING, "Could not load animation: %s", animName);
	}
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
	{"FRONT", 0.0f * PI / 180.0f, 0.0f, (Color){255, 0, 127, 255}},
	{"DIAG45", 45.0f * PI / 180.0f, 0.0f, (Color){255, 127, 0, 255}},
	{"SIDE90", 90.0f * PI / 180.0f, 0.0f, (Color){255, 255, 0, 255}},
	{"BACK135", 135.0f * PI / 180.0f, 0.0f, (Color){127, 255, 0, 255}},
	{"BACK", 180.0f * PI / 180.0f, 0.0f, (Color){0, 255, 127, 255}},
	{"BACK315", -45.0f * PI / 180.0f, 0.0f, (Color){127, 0, 255, 255}},
	{"SIDE270", -90.0f * PI / 180.0f, 0.0f, (Color){0, 127, 255, 255}},
	{"DIAG225", -135.0f * PI / 180.0f, 0.0f, (Color){0, 255, 255, 255}},

	{"HIGH-0", 0.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){255, 182, 193, 255}},
	{"HIGH-45", 45.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){255, 218, 185, 255}},
	{"HIGH-90", 90.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){255, 255, 153, 255}},
	{"HIGH-135", 135.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){204, 255, 153, 255}},
	{"HIGH-180", 180.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){153, 255, 204, 255}},
	{"HIGH-225", -135.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){153, 204, 255, 255}},
	{"HIGH-270", -90.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){204, 153, 255, 255}},
	{"HIGH-315", -45.0f * PI / 180.0f, 30.0f * PI / 180.0f, (Color){255, 153, 255, 255}},

	{"LOW-0", 0.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){220, 20, 60, 255}},
	{"LOW-45", 45.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){255, 140, 0, 255}},
	{"LOW-90", 90.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){218, 165, 32, 255}},
	{"LOW-135", 135.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){50, 205, 50, 255}},
	{"LOW-180", 180.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){0, 206, 209, 255}},
	{"LOW-225", -135.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){30, 144, 255, 255}},
	{"LOW-270", -90.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){138, 43, 226, 255}},
	{"LOW-315", -45.0f * PI / 180.0f, -30.0f * PI / 180.0f, (Color){255, 20, 147, 255}},

	{"TOP", 0.0f, MAX_PITCH, (Color){255, 255, 255, 255}},

	{"BOTTOM", 0.0f, MIN_PITCH, (Color){0, 0, 0, 255}},
};

static const int SNAP_POINT_COUNT = 26;

static Rectangle GetGizmoRect(AppState* app) {
	float scale = fminf(app->screenWidth / 1920.0f, app->screenHeight / 1080.0f);

	int gizmoSize = (int)(280 * scale);
	if (gizmoSize < 200) gizmoSize = 200;
	if (gizmoSize > 400) gizmoSize = 400;

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
	const float SELECTION_RADIUS = 50.0f;
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
			!IsKeyDown(KEY_LEFT_CONTROL)) {
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
		}
	}
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
				Color highlightColor;
				const char* modeText;
				if (app->editor.isDraggingKeyframe) {
					highlightColor = PURPLE;
					modeText = "[DRAGGING KEYFRAME]";
				} else if (app->editor.isDraggingBone) {
					if (hasMultiSelection) {
						highlightColor = ORANGE;
						int frameCount = app->editor.selectionEnd - app->editor.selectionStart + 1;
						static char multiText[64];
						snprintf(multiText, sizeof(multiText), "[MOVING %d FRAMES]", frameCount);
						modeText = multiText;
					} else {
						highlightColor = RED;
						modeText = "[DRAGGING BONE]";
					}
				} else if (IsCurrentFrameKeyframe(app)) {
					highlightColor = GREEN;
					modeText = "[KEYFRAME]";
				} else {
					highlightColor = ORANGE;
					modeText = "[INTERPOLATED]";
				}
				DrawSphere(bone->position.position, 0.04f, highlightColor);
				DrawSphereWires(bone->position.position, 0.042f, 8, 8, WHITE);
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
			3, RED);
	DrawCircle((int)markerX, (int)(timeline.y + timeline.height + 5), 6, RED);
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
				DrawRectangle((int)mousePos.x - textW / 2 - 5, (int)mousePos.y - 30,
						textW + 10, 20, (Color){128, 0, 128, 200});
				DrawText(dragText, (int)mousePos.x - textW / 2, (int)mousePos.y - 27, dragTextSize, WHITE);
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
		app->editor.isPlaying = !app->editor.isPlaying;
		SetCharacterAutoPlay(app->character, app->editor.isPlaying);
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
				BonesDeleteFrame(app, &app->character->animation, frameIndex);
				app->character->maxFrames = app->character->animation.frameCount;
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
					if (BonesDuplicateFrame(app, &app->character->animation, frameIndex)) {
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
				"EXPORT", app->editor.needsSave ? ORANGE : DARKGREEN)) {
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
	int panelHeight = (int)(200 * scale);
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
}

// ============================================================================
// DEBUG VISUALIZATION FUNCTIONS
// ============================================================================

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
				uniqueBones[uniqueCount].orientation = CalculateBoneOrientation(bone->name, person, bone->position.position);
				uniqueCount++;
			}
		}
	}
	BeginMode3D(app->character->renderer->camera);
	for (int i = 0; i < uniqueCount; i++) {
		if (!uniqueBones[i].orientation.valid) continue;
		Vector3 pos = uniqueBones[i].position;
		BoneOrientation orient = uniqueBones[i].orientation;
		Vector3 forward = Vector3Scale(orient.forward, AXIS_LENGTH);
		Vector3 right = Vector3Scale(orient.right, AXIS_LENGTH);
		Vector3 up = Vector3Scale(orient.up, AXIS_LENGTH);
		DrawLine3D(pos, Vector3Add(pos, right), RED);
		DrawSphere(Vector3Add(pos, right), 0.0035f, RED);
		DrawLine3D(pos, Vector3Add(pos, up), GREEN);
		DrawSphere(Vector3Add(pos, up), 0.0035f, GREEN);
		DrawLine3D(pos, Vector3Add(pos, forward), BLUE);
		DrawSphere(Vector3Add(pos, forward), 0.0035f, BLUE);
		DrawSphere(pos, 0.005f, YELLOW);
	}
	EndMode3D();
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

	DrawText("BONES3D ANIMATION EDITOR", 10, 10, titleSize, BLUE);

	if (g_characterManager.currentProfileIndex >= 0 &&
			g_characterManager.currentProfileIndex < g_characterManager.profileCount) {
		char profileText[128];
		snprintf(profileText, sizeof(profileText), "Character: %s",
				g_characterManager.profiles[g_characterManager.currentProfileIndex].name);
		DrawText(profileText, 10, 10 + titleSize + 5, textSize, ORANGE);
	}

	int yPos = 10 + titleSize + textSize + 10;
	DrawText("1/2/3/4: Switch Character | 5-8: Load Anims | H/T: Billboards", 10, yPos, smallSize, DARKGRAY);
	yPos += smallSize + 3;
	DrawText("LEFT CLICK: Select | RIGHT CLICK: Move | CTRL+LEFT (timeline): Drag keyframe", 10, yPos, smallSize, DARKGRAY);

	yPos += smallSize + 5;
	char frameText[128];
	snprintf(frameText, sizeof(frameText), "Animation: %s | Frame: %d/%d (%d existing) %s %s",
			app->currentAnimation,
			currentFrameNumber,
			maxFrameNumber,
			existingFrames,
			app->editor.isPlaying ? "[PLAYING]" : "[PAUSED]",
			app->editor.needsSave ? "[*]" : "");
	DrawText(frameText, 10, yPos, textSize, app->editor.needsSave ? ORANGE : DARKGRAY);

	yPos += textSize + 3;
	if (app->editor.isDraggingKeyframe) {
		DrawText("[DRAGGING KEYFRAME] Release to confirm", 10, yPos, textSize, PURPLE);
	} else if (app->editor.hasBoneSelected) {
		bool isKeyframe = IsCurrentFrameKeyframe(app);
		char selectionText[256];
		if (app->editor.isDraggingBone) {
			snprintf(selectionText, sizeof(selectionText), "[DRAGGING BONE] %s | Live preview",
					app->editor.selectedBoneName);
			DrawText(selectionText, 10, yPos, textSize, RED);
		} else if (isKeyframe) {
			snprintf(selectionText, sizeof(selectionText), "[KEYFRAME] %s | Right-click: move",
					app->editor.selectedBoneName);
			DrawText(selectionText, 10, yPos, textSize, GREEN);
		} else {
			snprintf(selectionText, sizeof(selectionText), "[INTERPOLATED] %s | Right-click: convert & move",
					app->editor.selectedBoneName);
			DrawText(selectionText, 10, yPos, textSize, ORANGE);
		}
	}
}

static void App_Draw(AppState* app) {
	if (!app) return;

	BeginDrawing();
	ClearBackground(RAYWHITE);

	BeginMode3D(app->character->renderer->camera);
	DrawGrid(10, 1.0f);
	EndMode3D();

	DrawAnimatedCharacter(app->character, app->character->renderer->camera);
	DrawBoneOrientation(app);
	DrawDebugVisuals(app);
	DrawBoneSelectionFeedback(app);

	DrawCameraGizmo(app);
	App_DrawUI(app);
	DrawDebugPanel(app);
	DrawTimeline(app);
	DrawControlPanel(app);
	DrawExportDialog(app);
	DrawAddFramesDialog(app);

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
			app->editor.showExportDialog = true;
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

	if (IsKeyPressed(KEY_SPACE)) {
		app->editor.isPlaying = !app->editor.isPlaying;
		SetCharacterAutoPlay(app->character, app->editor.isPlaying);
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

	if (IsKeyPressed(KEY_FIVE)) {
		LoadAnimationForCurrentProfile(app, "idle");
	}
	if (IsKeyPressed(KEY_SIX)) {
		LoadAnimationForCurrentProfile(app, "talk");
	}
	if (IsKeyPressed(KEY_SEVEN)) {
		LoadAnimationForCurrentProfile(app, "walk");
	}
	if (IsKeyPressed(KEY_EIGHT)) {
		LoadAnimationForCurrentProfile(app, "jump");
	}

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
	if (IsKeyPressed(KEY_DELETE) && app->editor.selectionStart != -1 && FrameExists(app, app->editor.selectionStart)) {
		int frameIndex = FindFrameIndexByNumber(app, app->editor.selectionStart);
		if (frameIndex != -1) {
			int deletedFrameNumber = app->editor.selectionStart;
			BonesDeleteFrame(app, &app->character->animation, frameIndex);
			app->character->maxFrames = app->character->animation.frameCount;
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

	Vector3 cameraTarget = app->character->autoCenterCalculated ?
		app->character->autoCenter : (Vector3){0, 0.6f, 0};

	if (app->camMode == 1) {
		UpdateCameraGizmo(app);

		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
				!app->editor.isDraggingSlider &&
				!app->editor.isDraggingKeyframe &&
				!app->editor.isDraggingGizmo &&
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
	if (!app->editor.isDraggingKeyframe) {
		UpdateBoneSelection(app);
	}
}

// ============================================================================
// INITIALIZATION AND SHUTDOWN
// ============================================================================

static bool App_Init(AppState* app) {
	if (!app) return false;
	memset(app, 0, sizeof(*app));
	InitWindow(BASE_WIDTH, BASE_HEIGHT, "Bones3D - Animation Editor");
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
		App_HandleInput(&app);
		App_UpdateCamera(&app, dt);
		DrawGrid(10, 1.0f);
		UpdateAnimatedCharacter(app.character, dt);
		App_Draw(&app);
	}
	App_Shutdown(&app);
	return 0;
}
