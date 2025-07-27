#pragma once
#include "Game/Entity.hpp"

constexpr int NUM_PLAYER_TRIS = 2;

constexpr int NUM_PLAYER_VERTS = 3 * NUM_PLAYER_TRIS;

class Player : public Entity
{
public:
	Player(Game* owner, Vec3 const& startPos);
	Player(Game* owner, Vec3 const& startPos, float orientation, float rotation);
	~Player();

	virtual void Update(float deltaSeconds) override;

	void HandleInput(float deltaSeconds);

	virtual void RenderShadow() const override;
	virtual void Render() const override;

public:
	Camera m_playerCam;

private:
	void InitializeLocalVerts();

private:
	std::vector<Vertex_PCU> m_vertexes;
	Rgba8					m_color = Rgba8::WHITE;
	Texture* m_texture = nullptr;

	
	Vertex_PCU m_localVerts[NUM_PLAYER_VERTS];
};