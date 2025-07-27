#include "GameCommon.hpp"
#include <Engine/Core/Vertex_PCU.hpp>
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/VertexUtils.hpp"



//extern Renderer*  g_theRenderer;

/*void DebugDrawLine(Vec2 const& start, Vec2 const& end, float thickness, Rgba8 const& color)
{
	//for (int sideNum = 0; sideNum < NUM_SEGMENTS; ++sideNum)
	{
		//Vec3 topLeft = Vec3(start.x - halfThickness, start.y + halfThickness, 0.f);
		//Vec3 bottomLeft = Vec3(start.x - halfThickness, start.y - halfThickness, 0.f);
		//Vec3 topRight = Vec3(end.x + halfThickness, end.y + halfThickness, 0.f);
		//Vec3 bottomRight = Vec3(end.x + halfThickness, end.y - halfThickness, 0.f);
//	}
//}
*/
void DebugDrawLine(Vec2 const& start, Vec2 const& end, float thickness, Rgba8 const& color)
{
	//take the Displacement of end - start then normalize it.
	//radius = thickness * .5f
	// forward = r * normalized displacement
	// left = forward.get rotated 90
	//a = end + forward + left
	//b = start + forward + left
	//c = start -forward - left
	//d = end + forward - left
	//(x, y) becomes (-y, x) rotates things by 90 degrees
	//app owns game which owns many asteroids and bullets and a playership which all stem from entity
	
	Vec2 displacement = end - start;
	Vec2 displaceNormal = displacement.GetNormalized();



	float halfThickness = 0.5f * thickness;
	//float innerRadius = halfThickness;
	//float outerRadius = -halfThickness;
	//constexpr int NUM_SEGMENTS = 20;
	constexpr int NUM_TRIS = 2;
	constexpr int NUM_VERTS = 3 * NUM_TRIS;
	Vertex_PCU verts[NUM_VERTS];

	Vec2 fwd = halfThickness * displaceNormal;

	Vec2 left = fwd.GetRotated90Degrees();
	//Vec2 right = fwd.GetRotatedMinus90Degrees();
	Vec2 A = end + fwd + left;
	Vec2 B = start - fwd + left;
	Vec2 C = start - fwd - left;
	Vec2 D = end + fwd - left;



	verts[0].m_position = Vec3(A.x, A.y, 0.f);
	verts[1].m_position = Vec3(B.x, B.y, 0.f);
	verts[2].m_position = Vec3(D.x, D.y, 0.f);

	verts[0].m_color = color;
	verts[1].m_color = color;
	verts[2].m_color = color;

	verts[3].m_position = Vec3(B.x, B.y, 0.f);
	verts[4].m_position = Vec3(C.x, C.y, 0.f);
	verts[5].m_position = Vec3(D.x, D.y, 0.f);
	//int i = 0;
	verts[3].m_color = color;
	verts[4].m_color = color;
	verts[5].m_color = color;

	//constexpr float DEGREES_PER_SIDE = 360.f / static_cast<float>(NUM_SEGMENTS);
	//for(int sideNum = 0; sideNum < NUM_SEGMENTS; ++sideNum)
	//{
		//GetDistance2D()
		/*
		//float startDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum);
		//float endDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum + 1);
		//float cosStart = CosDegrees( startDegrees);
		//float sinStart = SinDegrees(startDegrees);
		//float cosEnd = CosDegrees( endDegrees );
		//float sinEnd = SinDegrees( endDegrees );
		Vec3 topLeft = Vec3(start.x - halfThickness, start.y + halfThickness, 0.f);
		Vec3 bottomLeft = Vec3(start.x - halfThickness, start.y - halfThickness, 0.f);
		Vec3 topRight = Vec3(end.x + halfThickness, end.y + halfThickness, 0.f);
		Vec3 bottomRight = Vec3(end.x + halfThickness, end.y - halfThickness, 0.f);
		//Vec3 innerStartPos = Vec3(start.x + innerRadius, start.y + innerRadius, 0.f);
		//Vec3 outerStartPos = Vec3(end.x + outerRadius, end.y + outerRadius, 0.f);
		//Vec3 outerEndPos = Vec3(center.x + innerRadius * cosEnd, center.y + innerRadius * sinEnd, 0.f); for spikey
		//Vec3 outerEndPos = Vec3(center.x + outerRadius * cosEnd, center.y + outerRadius * sinEnd, 0.f); for beveled
		//Vec3 outerEndPos = Vec3(end.x + outerRadius, end.y + outerRadius, 0.f);
		//Vec3 innerEndPos = Vec3(start.x + innerRadius, start.y + innerRadius, 0.f);

		int vertIndexA = (6 * sideNum) + 0;
		int vertIndexB = (6 * sideNum) + 1;
		int vertIndexC = (6 * sideNum) + 2;
		int vertIndexD = (6 * sideNum) + 3;
		int vertIndexE = (6 * sideNum) + 4;
		int vertIndexF = (6 * sideNum) + 5;

		verts[vertIndexA].m_position = topLeft;
		verts[vertIndexB].m_position = bottomLeft;
		verts[vertIndexC].m_position = topRight;

		verts[vertIndexA].m_color = color;
		verts[vertIndexB].m_color = color;
		verts[vertIndexC].m_color = color;

		verts[vertIndexD].m_position = topRight;
		verts[vertIndexE].m_position = bottomLeft;
		verts[vertIndexF].m_position = bottomRight;
		int i = 0;
		verts[vertIndexD].m_color = color;
		verts[vertIndexE].m_color = color;
		verts[vertIndexF].m_color = color;
		//TransformVertexArrayXY3D(NUM_SEGMENTS, verts, 1.f, Atan2Degrees(end.y - start.y, end.x - start.x), Vec2(0, 0));
		g_theRenderer -> DrawVertexArray(NUM_VERTS, verts);
		*/
		g_theRenderer -> DrawVertexArray(NUM_VERTS, verts);
}



void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color)
{
	float halfThickness = thickness;
	float innerRadius = radius - halfThickness;
	float outerRadius = radius + halfThickness;
	constexpr int NUM_SIDES = 32;
	constexpr int NUM_TRIS = 2 * NUM_SIDES;
	constexpr int NUM_VERTS = 3 * NUM_TRIS;
	Vertex_PCU verts[NUM_VERTS];
	constexpr float DEGREES_PER_SIDE = 360.f / static_cast<float>(NUM_SIDES);

	for (int sideNum = 0; sideNum < NUM_SIDES; ++sideNum)
	{
		float startDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum);
		float endDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum + 1);
		float cosStart = CosDegrees( startDegrees);
		float sinStart = SinDegrees(startDegrees);
		float cosEnd = CosDegrees( endDegrees );
		float sinEnd = SinDegrees( endDegrees );
		Vec3 innerStartPos = Vec3(center.x + innerRadius * cosStart, center.y + innerRadius * sinStart, 0.f);
		Vec3 outerStartPos = Vec3(center.x + outerRadius * cosStart, center.y + outerRadius * sinStart, 0.f);
		//Vec3 outerEndPos = Vec3(center.x + innerRadius * cosEnd, center.y + innerRadius * sinEnd, 0.f); for spikey
		//Vec3 outerEndPos = Vec3(center.x + outerRadius * cosEnd, center.y + outerRadius * sinEnd, 0.f); for beveled
		Vec3 outerEndPos = Vec3(center.x + outerRadius * cosEnd, center.y + outerRadius * sinEnd, 0.f);
		Vec3 innerEndPos = Vec3(center.x + innerRadius * cosEnd, center.y + innerRadius * sinEnd, 0.f);
		
		int vertIndexA = (6 * sideNum) + 0;
		int vertIndexB = (6 * sideNum) + 1;
		int vertIndexC = (6 * sideNum) + 2;
		int vertIndexD = (6 * sideNum) + 3;
		int vertIndexE = (6 * sideNum) + 4;
		int vertIndexF = (6 * sideNum) + 5;

		verts[vertIndexA].m_position = innerEndPos;
		verts[vertIndexB].m_position = innerStartPos;
		verts[vertIndexC].m_position = outerStartPos;

		verts[vertIndexA].m_color = color;
		verts[vertIndexB].m_color = color;
		verts[vertIndexC].m_color = color;

		verts[vertIndexD].m_position = innerEndPos;
		verts[vertIndexE].m_position = outerStartPos;
		//inner start pos instead of outer start pos to create beveled
		verts[vertIndexF].m_position = outerEndPos;

		verts[vertIndexD].m_color = color;
		verts[vertIndexE].m_color = color;
		verts[vertIndexF].m_color = color;
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer -> DrawVertexArray(NUM_VERTS, verts);
	}
}

