#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Game/Game.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/EulerAngles.hpp"

//class Game;
//extern Game* m_theGame = nullptr;

class Entity
{
public:
	Entity(Game* owner, Vec3 const& startPos);
	virtual ~Entity();
	virtual void BeginFrame() const;
	virtual void Update(float deltaSeconds) = 0;
	virtual void RenderShadow() const = 0;
	virtual void Render() const = 0;
	bool IsOffscreen();
//	Vec2 GetForwardNormal();

//	virtual void Die() = 0;
//	virtual void DebugRender() const;
//	bool IsAlive(){ return !m_isDead;}

	virtual Mat44 GetModelMatrix();

public:
	Game* m_game = nullptr;

	Vec3 m_position;
	Vec3 m_velocity;

	EulerAngles m_orientationDegrees;
	EulerAngles m_angularVelocity;

	Rgba8 m_color = Rgba8::WHITE;

// 	float m_physicsRadius = 5.f;
// 	float m_cosmeticRadius = 10.f;
// 	int health = 1;
// 	bool m_isDead = false;
// 	bool m_isGarbage = false;
};