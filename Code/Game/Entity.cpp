#include "Entity.hpp"
#include "Engine/Math/EulerAngles.hpp"


extern Game* m_theGame = nullptr;
Entity::Entity(Game* owner, Vec3 const& startPos)
{	
	m_position = startPos;
	m_theGame = owner;
}

Entity::~Entity()
{
}

void Entity::BeginFrame() const
{
}

// void Entity::DebugRender() const
// {
// }

bool Entity::IsOffscreen()
{
	return false;
}

Mat44 Entity::GetModelMatrix()
{
	Mat44 modelMatrix = Mat44();

	modelMatrix = m_orientationDegrees.GetMatrix_XFwd_YLeft_ZUp();

	modelMatrix.SetTranslation3D(m_position);

	return modelMatrix;
}
// 
// Vec2 Entity::GetForwardNormal()
// {
// 	return Vec2(CosDegrees(m_orientationDegrees), SinDegrees((m_orientationDegrees))).GetNormalized();
// }


