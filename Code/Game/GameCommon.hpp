#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

class App;
	
	extern App* g_theApp;
	extern Renderer* g_theRenderer;
	extern InputSystem* g_theInputSystem;
	extern AudioSystem* g_theAudioSystem;
	extern RandomNumberGenerator* g_theRandom;

	void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color);
	void DebugDrawLine(Vec2 const& start, Vec2 const& end, float thickness, Rgba8 const& color);

	

	static constexpr int NUM_STARTING_ASTEROIDS = 6;
	static constexpr int MAX_ASTEROIDS = 12;
	static constexpr int MAX_BEETLES = 12;
	static constexpr int MAX_WASPS = 12;
	static constexpr int MAX_BULLETS = 20;
	static constexpr int MAX_DEBRIS = 100;
	static constexpr float WORLD_SIZE_X = 200.f;
	static constexpr float WORLD_SIZE_Y = 100.f;
	static constexpr float WORLD_CENTER_X = WORLD_SIZE_X / 2.f;
	static constexpr float WORLD_CENTER_Y = WORLD_SIZE_Y / 2.f;
	static constexpr float ASTEROID_SPEED = 10.f;
	static constexpr float ASTEROID_PHYSICS_RADIUS = 1.6f;
	static constexpr float ASTEROID_COSMETIC_RADIUS = 2.0f;
	static constexpr float DEBRIS_PHYSICS_RADIUS = 1.6f;
	static constexpr float DEBRIS_COSMETIC_RADIUS = 2.0f;
	static constexpr float BULLET_LIFETIME_SECONDS = 2.0f;
	static constexpr float BULLET_SPEED = 50.f;
	static constexpr float BULLET_PHYSICS_RADIUS = 0.5f;
	static constexpr float BULLET_COSMETIC_RADIUS = 2.0f;

	static constexpr float BEETLE_SPEED = 10.f;

	static constexpr float PLAYER_SHIP_ACCELERATION = 30.f;
	static constexpr float PLAYER_SHIP_TURN_SPEED = 300.f;
	static constexpr float PLAYER_SHIP_PHYSICS_RADIUS = 1.75f;
	static constexpr float PLAYER_SHIP_COSMETIC_RADIUS = 2.25f;

	
