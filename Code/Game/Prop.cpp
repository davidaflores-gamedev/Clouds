#include "Game/Prop.hpp"


Prop::Prop(Game* owner, Vec3 const& startPos, ObjectType type) : Entity(owner, startPos)
{
	m_game = owner;
	m_position = startPos;

	m_modelMatrix = Mat44();

	m_type = type;

	m_modelMatrix.SetTranslation3D(m_position);

	m_texture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/TestUV.png");

	InitializeLocalVerts();
}

Prop::Prop(Game* owner, Vec3 const& startPos, float orientation, float rotation) : Entity(owner, startPos)

{
	m_game = owner;
	m_position = startPos;
	InitializeLocalVerts();
	UNUSED(orientation);
	UNUSED(rotation);
	//m_orientationDegrees = orientation;
}

Prop::~Prop()
{
}

void Prop::Update(float deltaSeconds)
{
	m_orientationDegrees.m_pitchDegrees += m_angularVelocity.m_pitchDegrees *  deltaSeconds;
	m_orientationDegrees.m_rollDegrees+= m_angularVelocity.m_rollDegrees *  deltaSeconds;
	m_orientationDegrees.m_yawDegrees += m_angularVelocity.m_yawDegrees *  deltaSeconds;

	Vec3 x;
	Vec3 y;
	Vec3 z;

	m_orientationDegrees.GetVectors_XFwd_YLeft_ZUp(x, y, z);

	m_modelMatrix.SetIJK3D(x * xScale, y * yScale, z * zScale);
}

void Prop::RenderShadow() const
{
	g_theRenderer->SetDepthMode(DepthMode::ENABLED);

	if (m_type == ObjectType::UniformColorCube)
	{
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->BindTexture(nullptr);

		g_theRenderer->SetModelConstants(m_modelMatrix, m_color);

		g_theRenderer->DrawVertexArray((int)m_vertexes.size(), m_vertexes.data());
		return;
	}
}

void Prop::Render() const
{
	g_theRenderer->SetDepthMode(DepthMode::ENABLED);

	if(m_type == ObjectType::Cube)
	{
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->BindTexture(nullptr);
		
		g_theRenderer->SetModelConstants(m_modelMatrix, m_color);

		g_theRenderer->DrawVertexArray((int)m_vertexes.size(), m_vertexes.data());
	}

	else if(m_type == ObjectType::Sphere)
	{
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->BindTexture(m_texture);

		g_theRenderer->SetModelConstants(m_modelMatrix, m_color);

		g_theRenderer->DrawVertexArray((int)m_vertexes.size(), m_vertexes.data());
	}

	else if (m_type == ObjectType::Cylinder)
	{
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->BindTexture(m_texture);

		g_theRenderer->SetModelConstants(m_modelMatrix, m_color);

		g_theRenderer->DrawVertexArray((int)m_vertexes.size(), m_vertexes.data());
	}

	else if (m_type == ObjectType::Cone)
	{
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->BindTexture(m_texture);

		g_theRenderer->SetModelConstants(m_modelMatrix, m_color);

		g_theRenderer->DrawVertexArray((int)m_vertexes.size(), m_vertexes.data());
	}

	else if (m_type == ObjectType::UniformColorCube)
	{
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->BindTexture(nullptr);

		g_theRenderer->SetModelConstants(m_modelMatrix, m_color);

		g_theRenderer->DrawVertexArray((int)m_vertexes.size(), m_vertexes.data());
	}
}

void Prop::InitializeLocalVerts()
{
	//all are from the cube's pov
	//left +x 2

	if (m_type == ObjectType::Cylinder)
	{
		AddVertsForCylinder3D(m_vertexes, Vec3(0.f, 0.f, 0.f), Vec3(0.f, 0.f, 1.f), 1.f, m_color, AABB2::ZERO_TO_ONE, 8);
		return;
	}

	if (m_type == ObjectType::Cone)
	{
		AddVertsForCone3D(m_vertexes, Vec3(0.f, 0.f, 0.f), Vec3(0.f, 0.f, 1.f), 1.f, m_color, AABB2::ZERO_TO_ONE, 8);
		return;
	}

	if (m_type == ObjectType::Sphere)
	{
		
		AddVertsForSphere3D(m_vertexes, Vec3(0.f, 0.f, 0.f), 1.f, m_color, AABB2::ZERO_TO_ONE, 16, 8);
		return;
	}

	if (m_type == ObjectType::UniformColorCube)
	{
		AddVertsForAABB3D(m_vertexes, AABB3(Vec3(-.5, -.5, -.5), Vec3(.5, .5, .5)), m_color);
		return;
	}

	AddVertsForAABB3D(m_vertexes, AABB3( Vec3(-.5, -.5, -.5), Vec3(.5, .5, .5)));
}
