#pragma once
#include "Game/Entity.hpp"

enum ObjectType
{
	Cube,
	UniformColorCube,
	Sphere,
	Cylinder,
	Cone,
	Count
};

constexpr int NUM_SQUARE_TRIS = 2;

constexpr int NUM_SQUARE_SIDES = 6;

constexpr int NUM_SQUARE_VERTS = 3 * NUM_SQUARE_TRIS * NUM_SQUARE_SIDES;

constexpr int NUM_SPHERE_TRIS = 2;

constexpr int NUM_SPHERE_SIDES = 128;

constexpr int NUM_SPHERE_VERTS = 3 * NUM_SPHERE_TRIS * NUM_SPHERE_SIDES;

class Prop : public Entity
{
public:
	Prop(Game* owner, Vec3 const& startPos, ObjectType type = ObjectType::Cube);
	Prop(Game* owner, Vec3 const& startPos, float orientation, float rotation);
	~Prop();

	virtual void Update(float deltaSeconds) override;
	virtual void RenderShadow() const override;
	virtual void Render() const override;


	Mat44					m_modelMatrix;

	float xScale = 1.f;
	float yScale = 1.f;
	float zScale = 1.f;

private:
	void InitializeLocalVerts();

private:
	std::vector<Vertex_PCU> m_vertexes;
	Texture*				m_texture = nullptr;
	ObjectType				m_type;
};
