// main.c - Test simplificado y reducido (misma funcionalidad)
#include "bones_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// ESTRUCTURAS
// ============================================================================
typedef struct {
	AnimatedCharacter* character;
	Vector3 position;
	float rotation;
	int id;
} GameCharacter;

typedef struct {
	GameCharacter* characters;
	int count;
	int max;
	int controlled;
	Camera camera;
	float moveSpeed;
	float cameraSpeed;
	// Estado de cámara antes en statics (ahora en el mundo)
	float camAngleH;
	float camAngleV;
	float camDistance;
	Vector3 camLateralOffset;
} GameWorld;

// ============================================================================
// GESTIÓN DEL MUNDO
// ============================================================================
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

	// Inicializar estado de cámara
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

	w->count++;
	return idx;
}

// ============================================================================
// INPUT Y UPDATE
// ============================================================================
void ProcessInput(GameWorld* w, float dt) {
	if (!w) return;

	// ===== CÁMARA ORBITAL CON DESPLAZAMIENTO LATERAL =====
	if (w->controlled >= 0 && w->controlled < w->count) {
		GameCharacter* c = &w->characters[w->controlled];

		// Rotar horizontalmente con Q/E
		if (IsKeyDown(KEY_Q)) w->camAngleH += 90.0f * dt;
		if (IsKeyDown(KEY_E)) w->camAngleH -= 90.0f * dt;

		// Rotar verticalmente con R/F
		if (IsKeyDown(KEY_R)) w->camAngleV += 60.0f * dt;
		if (IsKeyDown(KEY_F)) w->camAngleV -= 60.0f * dt;

		// Limitar pitch vertical
		if (w->camAngleV > 60.0f) w->camAngleV = 60.0f;
		if (w->camAngleV < 5.0f) w->camAngleV = 5.0f;

		// Zoom con W/S
		if (IsKeyDown(KEY_W)) w->camDistance -= 3.0f * dt;
		if (IsKeyDown(KEY_S)) w->camDistance += 3.0f * dt;
		if (w->camDistance < 1.0f) w->camDistance = 1.0f;
		if (w->camDistance > 6.0f) w->camDistance = 6.0f;

		// Calcular vectores de la cámara
		float radH = w->camAngleH * DEG2RAD;
		float radV = w->camAngleV * DEG2RAD;

		Vector3 forward = {
			cosf(radV) * sinf(radH),
			sinf(radV),
			cosf(radV) * cosf(radH)
		};

		Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, (Vector3){0, 1, 0}));

		// Desplazamiento lateral con A/D
		float lateralSpeed = 2.0f;
		if (IsKeyDown(KEY_A)) w->camLateralOffset = Vector3Add(w->camLateralOffset, Vector3Scale(right, lateralSpeed * dt));
		if (IsKeyDown(KEY_D)) w->camLateralOffset = Vector3Add(w->camLateralOffset, Vector3Scale(right, -lateralSpeed * dt));

		// Limitar desplazamiento lateral
		float lateralDist = Vector3Length(w->camLateralOffset);
		if (lateralDist > 3.0f) w->camLateralOffset = Vector3Scale(Vector3Normalize(w->camLateralOffset), 3.0f);

		// Target = posición del personaje + offset lateral
		w->camera.target = Vector3Add(c->position, (Vector3){0, 0.6f, 0});
		w->camera.target = Vector3Add(w->camera.target, w->camLateralOffset);

		// Posición de cámara = target + distancia orbital
		w->camera.position.x = w->camera.target.x + w->camDistance * cosf(radV) * sinf(radH);
		w->camera.position.y = w->camera.target.y + w->camDistance * sinf(radV);
		w->camera.position.z = w->camera.target.z + w->camDistance * cosf(radV) * cosf(radH);
	}

	// ===== CONTROL DE PERSONAJE (FLECHAS) =====
	if (w->controlled >= 0 && w->controlled < w->count) {
		GameCharacter* c = &w->characters[w->controlled];
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

		// Cambiar animación según movimiento (comportamiento original: variable estática compartida)
		static bool wasMoving = false;
		if (moving != wasMoving) {
			const char* animPath = moving ? "data/animations/walk.json" : "data/animations/idle.json";
			const char* metaPath = moving ? "data/animations/walk.anim" : "data/animations/idle.anim";
			LoadAnimation(c->character, animPath, metaPath);
			wasMoving = moving;
		}
	}
}

void UpdateWorld(GameWorld* w, float dt) {
	if (!w) return;
	for (int i = 0; i < w->count; i++) UpdateAnimatedCharacter(w->characters[i].character, dt);
}

// ============================================================================
// RENDER
// ============================================================================
void RenderWorld(GameWorld* w) {
	if (!w) return;
	BeginMode3D(w->camera);
	DrawGrid(20, 1.0f);

	// Orden simple por distancia (más lejos primero) - selection sort
	int n = w->count;
	if (n > 0) {
		int idxs[64];
		float dists[64];
		if (n > 64) n = 64;
		for (int i = 0; i < n; i++) {
			idxs[i] = i;
			dists[i] = Vector3Distance(w->camera.position, w->characters[i].position);
		}
		for (int i = 0; i < n - 1; i++) {
			int sel = i;
			for (int j = i + 1; j < n; j++) if (dists[j] > dists[sel]) sel = j;
			if (sel != i) {
				float td = dists[i]; dists[i] = dists[sel]; dists[sel] = td;
				int ti = idxs[i]; idxs[i] = idxs[sel]; idxs[sel] = ti;
			}
		}
		for (int i = 0; i < n; i++) {
			GameCharacter* gc = &w->characters[idxs[i]];
			DrawAnimatedCharacterTransformed(gc->character, w->camera, gc->position, gc->rotation);
		}
	}

	EndMode3D();
}

void DrawUI(GameWorld* w) {
	DrawText("Bones3D - Test Simplificado", 10, 10, 20, DARKGRAY);
	DrawFPS(10, 40);
	if (w->controlled >= 0) DrawText(TextFormat("Controlando personaje: %d", w->controlled), 10, 70, 16, DARKGRAY);
	DrawText("=== CONTROLES ===", 10, 100, 14, DARKGRAY);
	DrawText("Q/E: Rotar camara horizontal", 10, 120, 14, DARKGRAY);
	DrawText("R/F: Rotar camara vertical", 10, 140, 14, DARKGRAY);
	DrawText("W/S: Zoom acercar/alejar", 10, 160, 14, DARKGRAY);
	DrawText("A/D: Desplazar camara lateralmente", 10, 180, 14, DARKGRAY);
	DrawText("Flechas: Mover/rotar personaje", 10, 200, 14, DARKGRAY);
	DrawText("TAB: Cambiar personaje controlado", 10, 220, 14, DARKGRAY);
}

// ============================================================================
// MAIN
// ============================================================================
int main(void) {
	InitWindow(1280, 720, "Bones3D - Test Simplificado");
	SetTargetFPS(60);

	GameWorld* world = CreateWorld(4);

	AddCharacter(world,
		"data/textures/zeta/bone_textures.txt",
		"data/textures/zeta/texture_sets.txt",
		"data/animations/idle.json",
		"data/animations/idle.anim",
		(Vector3){0.0f, 0.0f, 0.0f});

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
			int next = (world->controlled + 1) % world->count;
			world->controlled = next;
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
