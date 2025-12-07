#include "bones_core.h"

typedef struct {
	AnimatedCharacter* character;
	Vector3 position;
	float rotation;
	int id;
	bool wasMoving;
	bool isPlayingAction;
	int actionStartFrame;
	int actionEndFrame;
} GameCharacter;

typedef struct {
	GameCharacter* characters;
	int count;
	int max;
	int controlled;
	Camera camera;
	float moveSpeed;
	float cameraSpeed;
	float camAngleH;
	float camAngleV;
	float camDistance;
	Vector3 camLateralOffset;
} GameWorld;

GameWorld* CreateWorld(int maxCharacters) {
	GameWorld* w = (GameWorld*)calloc(1, sizeof(GameWorld));
	w->max = maxCharacters;
	w->characters = (GameCharacter*)calloc(maxCharacters, sizeof(GameCharacter));
	w->count = 0;
	w->controlled = -1;

	w->camera.position = (Vector3){0.0f, 2.5f, 5.0f};
	w->camera.target = (Vector3){0.0f, 0.6f, 0.0f};
	w->camera.up = (Vector3){0.0f, 1.0f, 0.0f};
	w->camera.fovy = 45.0f;
	w->camera.projection = CAMERA_PERSPECTIVE;

	w->moveSpeed = 1.8f;
	w->cameraSpeed = 3.0f;
	w->camAngleH = 0.0f;
	w->camAngleV = 20.0f;
	w->camDistance = 2.5f;
	w->camLateralOffset = (Vector3){0, 0, 0};

	return w;
}

void DestroyWorld(GameWorld* w) {
	if (!w) return;
	for (int i = 0; i < w->count; i++) {
		DestroyAnimatedCharacter(w->characters[i].character);
	}
	free(w->characters);
	free(w);
}

int AddCharacter(GameWorld* w, const char* texCfg, const char* texSets,
		const char* animJson, const char* animMeta, Vector3 pos) {
	if (!w || w->count >= w->max) return -1;

	int idx = w->count;
	GameCharacter* gc = &w->characters[idx];

	gc->character = CreateAnimatedCharacter(texCfg, texSets);
	if (!gc->character) {
		printf("[ERROR] Failed to create character %d\n", idx);
		return -1;
	}

	if (!LoadAnimation(gc->character, animJson, animMeta)) {
		DestroyAnimatedCharacter(gc->character);
		printf("[ERROR] Failed to load animation for character %d\n", idx);
		return -1;
	}

	SetCharacterAutoPlay(gc->character, true);
	gc->position = pos;
	gc->rotation = 0.0f;
	gc->id = idx;
	gc->wasMoving = false;
	gc->isPlayingAction = false;
	gc->actionStartFrame = 0;
	gc->actionEndFrame = 0;

	w->count++;
	return idx;
}

void GetAnimationInfo(AnimatedCharacter* character, int* currentFrame, int* totalFrames) {
	if (!character || !character->animController) {
		*currentFrame = 0;
		*totalFrames = 0;
		return;
	}

	AnimationController* ctrl = character->animController;

	if (ctrl->currentClipIndex >= 0 && ctrl->currentClipIndex < ctrl->clipCount) {
		AnimationClipMetadata* clip = &ctrl->clips[ctrl->currentClipIndex];
		*currentFrame = ctrl->currentFrameInJSON;
		*totalFrames = clip->endFrame;
	} else {
		*currentFrame = character->currentFrame;
		*totalFrames = character->maxFrames;
	}
}

void UpdateGameCamera(GameWorld* w, float dt) {
	if (w->controlled < 0 || w->controlled >= w->count) return;

	GameCharacter* c = &w->characters[w->controlled];

	if (IsKeyDown(KEY_Q)) w->camAngleH += 90.0f * dt;
	if (IsKeyDown(KEY_E)) w->camAngleH -= 90.0f * dt;
	if (IsKeyDown(KEY_R)) w->camAngleV += 60.0f * dt;
	if (IsKeyDown(KEY_F)) w->camAngleV -= 60.0f * dt;

	w->camAngleV = Clamp(w->camAngleV, -35.0f, 35.0f);

	if (IsKeyDown(KEY_W)) w->camDistance -= 3.0f * dt;
	if (IsKeyDown(KEY_S)) w->camDistance += 3.0f * dt;
	w->camDistance = Clamp(w->camDistance, 1.0f, 6.0f);

	float radH = w->camAngleH * DEG2RAD;
	float radV = w->camAngleV * DEG2RAD;

	Vector3 forward = {
		cosf(radV) * sinf(radH),
		sinf(radV),
		cosf(radV) * cosf(radH)
	};

	Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, (Vector3){0, 1, 0}));
	float lateralSpeed = 2.0f;

	if (IsKeyDown(KEY_A)) w->camLateralOffset = Vector3Add(w->camLateralOffset, Vector3Scale(right, lateralSpeed * dt));
	if (IsKeyDown(KEY_D)) w->camLateralOffset = Vector3Add(w->camLateralOffset, Vector3Scale(right, -lateralSpeed * dt));

	float lateralDist = Vector3Length(w->camLateralOffset);
	if (lateralDist > 3.0f) w->camLateralOffset = Vector3Scale(Vector3Normalize(w->camLateralOffset), 3.0f);

	w->camera.target = Vector3Add(c->position, (Vector3){0, 0.6f, 0});
	w->camera.target = Vector3Add(w->camera.target, w->camLateralOffset);

	w->camera.position.x = w->camera.target.x + w->camDistance * cosf(radV) * sinf(radH);
	w->camera.position.y = w->camera.target.y + w->camDistance * sinf(radV);
	w->camera.position.z = w->camera.target.z + w->camDistance * cosf(radV) * cosf(radH);
}

void PlayActionAnimation(GameCharacter* c, const char* animJson, const char* animMeta, const char* name) {
	if (LoadAnimation(c->character, animJson, animMeta)) {
		printf("[INFO] Playing animation: %s\n", name);
		c->isPlayingAction = true;

		if (c->character->animController && c->character->animController->currentClipIndex >= 0) {
			AnimationClipMetadata* clip = &c->character->animController->clips[c->character->animController->currentClipIndex];
			c->actionStartFrame = clip->startFrame;
			c->actionEndFrame = clip->endFrame;
			printf("[INFO] %s: frames %d-%d (total: %d frames)\n", 
					name, c->actionStartFrame, c->actionEndFrame, 
					c->actionEndFrame - c->actionStartFrame + 1);
		}
	}
}

void HandleActionInput(GameWorld* w) {
	if (w->controlled < 0 || w->controlled >= w->count) return;

	GameCharacter* c = &w->characters[w->controlled];
	if (c->isPlayingAction) return;

	if (IsKeyPressed(KEY_J)) {
		PlayActionAnimation(c, "data/animations/jump.json", "data/animations/jump.anim", "Jump");
	} else if (IsKeyPressed(KEY_K)) {
		PlayActionAnimation(c, "data/animations/kick.json", "data/animations/kick.anim", "Kick");
	} else if (IsKeyPressed(KEY_P)) {
		PlayActionAnimation(c, "data/animations/punch.json", "data/animations/punch.anim", "Punch");
	}
}

void HandleCharacterMovement(GameWorld* w, float dt) {
	if (w->controlled < 0 || w->controlled >= w->count) return;

	GameCharacter* c = &w->characters[w->controlled];
	if (c->isPlayingAction) return;

	bool moving = false;

	if (IsKeyDown(KEY_UP)) {
		Vector3 dir = {sinf(c->rotation), 0, cosf(c->rotation)};
		c->position = Vector3Add(c->position, Vector3Scale(dir, w->moveSpeed * dt));
		moving = true;
	}
	if (IsKeyDown(KEY_DOWN)) {
		Vector3 dir = {sinf(c->rotation), 0, cosf(c->rotation)};
		c->position = Vector3Subtract(c->position, Vector3Scale(dir, w->moveSpeed * dt));
		moving = true;
	}
	if (IsKeyDown(KEY_LEFT)) c->rotation += 2.5f * dt;
	if (IsKeyDown(KEY_RIGHT)) c->rotation -= 2.5f * dt;

	while (c->rotation > PI) c->rotation -= 2.0f * PI;
	while (c->rotation < -PI) c->rotation += 2.0f * PI;

	if (moving != c->wasMoving) {
		const char* animPath = moving ? "data/animations/walk.json" : "data/animations/idle.json";
		const char* metaPath = moving ? "data/animations/walk.anim" : "data/animations/idle.anim";
		LoadAnimation(c->character, animPath, metaPath);
		c->wasMoving = moving;
	}
}

void ProcessInput(GameWorld* w, float dt) {
	if (!w) return;
	UpdateGameCamera(w, dt);
	HandleActionInput(w);
	HandleCharacterMovement(w, dt);
}

void UpdateActionAnimation(GameCharacter* gc) {
	if (!gc->isPlayingAction) return;

	int currentFrame, totalFrames;
	GetAnimationInfo(gc->character, &currentFrame, &totalFrames);

	bool animationComplete = false;

	if (gc->character->animController) {
		AnimationController* ctrl = gc->character->animController;
		if (currentFrame >= gc->actionEndFrame) {
			if (!ctrl->playing || (ctrl->currentClipIndex >= 0 && !ctrl->clips[ctrl->currentClipIndex].loop)) {
				animationComplete = true;
			}
		}
	} else {
		if (currentFrame >= totalFrames - 1) {
			animationComplete = true;
		}
	}

	if (animationComplete) {
		printf("[INFO] Action animation completed. Final frame: %d/%d\n", currentFrame, totalFrames);

		gc->isPlayingAction = false;
		gc->actionStartFrame = 0;
		gc->actionEndFrame = 0;

		const char* animPath = gc->wasMoving ? "data/animations/walk.json" : "data/animations/idle.json";
		const char* metaPath = gc->wasMoving ? "data/animations/walk.anim" : "data/animations/idle.anim";
		LoadAnimation(gc->character, animPath, metaPath);
	}
}

void UpdateWorld(GameWorld* w, float dt) {
	if (!w) return;

	for (int i = 0; i < w->count; i++) {
		GameCharacter* gc = &w->characters[i];
		UpdateAnimatedCharacter(gc->character, dt);
		UpdateActionAnimation(gc);
	}
}

void SortCharactersByDepth(GameWorld* w, int* indices, int count) {
	float distances[64];

	for (int i = 0; i < count; i++) {
		indices[i] = i;
		distances[i] = Vector3Distance(w->camera.position, w->characters[i].position);
	}

	for (int i = 0; i < count - 1; i++) {
		int maxIdx = i;
		for (int j = i + 1; j < count; j++) {
			if (distances[j] > distances[maxIdx]) maxIdx = j;
		}
		if (maxIdx != i) {
			float tempDist = distances[i];
			distances[i] = distances[maxIdx];
			distances[maxIdx] = tempDist;

			int tempIdx = indices[i];
			indices[i] = indices[maxIdx];
			indices[maxIdx] = tempIdx;
		}
	}
}

void RenderWorld(GameWorld* w) {
	if (!w) return;

	BeginMode3D(w->camera);
	DrawGrid(20, 1.0f);

	int count = w->count > 64 ? 64 : w->count;
	if (count > 0) {
		int indices[64];
		SortCharactersByDepth(w, indices, count);

		for (int i = 0; i < count; i++) {
			GameCharacter* gc = &w->characters[indices[i]];
			DrawAnimatedCharacterTransformed(gc->character, w->camera, gc->position, gc->rotation);
		}
	}

	EndMode3D();
}

void DrawUI(GameWorld* w) {
	DrawText("Bones3D Engine", 10, 10, 20, DARKGRAY);
	DrawFPS(10, 40);

	if (w->controlled >= 0) {
		GameCharacter* c = &w->characters[w->controlled];
		DrawText(TextFormat("Controlled Character: %d", w->controlled), 10, 70, 16, DARKGRAY);

		if (c->isPlayingAction) {
			int currentFrame, totalFrames;
			GetAnimationInfo(c->character, &currentFrame, &totalFrames);
			DrawText("[ACTION IN PROGRESS]", 10, 90, 16, RED);
			DrawText(TextFormat("Frame: %d/%d", currentFrame, totalFrames), 10, 110, 14, RED);

			float progress = (float)currentFrame / (float)totalFrames;
			DrawRectangle(10, 130, 200, 10, LIGHTGRAY);
			DrawRectangle(10, 130, (int)(200 * progress), 10, RED);
		}
	}

	DrawText("=== CAMERA ===", 10, 150, 14, DARKGRAY);
	DrawText("Q/E: Rotate horizontal", 10, 170, 14, DARKGRAY);
	DrawText("R/F: Rotate vertical", 10, 190, 14, DARKGRAY);
	DrawText("W/S: Zoom in/out", 10, 210, 14, DARKGRAY);
	DrawText("A/D: Lateral movement", 10, 230, 14, DARKGRAY);

	DrawText("=== CHARACTER ===", 10, 250, 14, DARKGRAY);
	DrawText("Arrows: Move/rotate", 10, 270, 14, DARKGRAY);
	DrawText("TAB: Switch character", 10, 290, 14, DARKGRAY);

	DrawText("=== ACTIONS ===", 10, 310, 14, DARKGREEN);
	DrawText("J: Jump", 10, 330, 14, DARKGREEN);
	DrawText("K: Kick", 10, 350, 14, DARKGREEN);
	DrawText("P: Punch", 10, 370, 14, DARKGREEN);
}

int main(void) {
	InitWindow(1280, 720, "Bones3D - Animation Engine");
	SetTargetFPS(60);

	GameWorld* world = CreateWorld(4);

    int char0 = AddCharacter(world,
            "data/textures/zeta/bone_textures.txt",
            "data/textures/zeta/texture_sets.txt",
            "data/animations/idle.json",
            "data/animations/idle.anim",
            (Vector3){0.0f, 0.5f, 0.0f});

    // ¡CENTRAR EL PERSONAJE!
    if (char0 >= 0) {
        CenterCharacterAtOrigin(world->characters[char0].character);
    }

	AddCharacter(world,
			"data/textures/hil/bone_textures.txt",
			"data/textures/hil/texture_sets.txt",
			"data/animations/idle.json",
			"data/animations/idle.anim",
			(Vector3){1.0f, 0.0f, 0.0f});

	AddCharacter(world,
			"data/textures/eld/bone_textures.txt",
			"data/textures/eld/texture_sets.txt",
			"data/animations/idle.json",
			"data/animations/idle.anim",
			(Vector3){-1.0f, 0.0f, 0.0f});

	world->controlled = 0;

	while (!WindowShouldClose()) {
		float dt = GetFrameTime();

		if (IsKeyPressed(KEY_TAB) && world->count > 0) {
			world->controlled = (world->controlled + 1) % world->count;
		}

		ProcessInput(world, dt);
		UpdateWorld(world, dt);

		BeginDrawing();
		ClearBackground(RAYWHITE);
		RenderWorld(world);
		DrawUI(world);
		EndDrawing();
	}

	DestroyWorld(world);
	CloseWindow();
	return 0;
}
