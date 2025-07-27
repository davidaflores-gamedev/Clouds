#include "Game/Player.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"


Player::Player(Game* owner, Vec3 const& startPos) : Entity(owner, startPos)
{
	m_game = owner;
	m_position = startPos;
	m_playerCam = Camera();
	m_playerCam.SetTransform(startPos, EulerAngles(0, 0, 0));
	InitializeLocalVerts();
}

Player::Player(Game* owner, Vec3 const& startPos, float orientation, float rotation) : Entity(owner, startPos)

{
	m_game = owner;
	m_position = startPos;
	m_playerCam = Camera();
	m_playerCam.SetTransform(startPos, EulerAngles(0, 0, 0));
	InitializeLocalVerts();
	UNUSED(orientation);
	UNUSED(rotation);
	//m_orientationDegrees = orientation;
}

Player::~Player()
{
}

void Player::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);
}

void Player::HandleInput(float deltaSeconds)
{
	UNUSED(deltaSeconds);
	Mat44 orientationMatrix = m_orientationDegrees.GetMatrix_XFwd_YLeft_ZUp();

	float moveSpeed = 7.f;

	float mouseSpeed = 5.f;

	if (g_theInputSystem->IsKeyDown(KEYCODE_SHIFT))
	{
		moveSpeed = 10.f * moveSpeed;
	}

	if (g_theInputSystem->IsKeyDown(KEYCODE_RIGHT_MOUSE))
	{
		mouseSpeed *= .2f;
	}

	if (g_theInputSystem->IsKeyDown('6'))
	{
		m_position = Vec3(25.4f, 1.3f, 151.61f);
		m_orientationDegrees = EulerAngles(34.6f, -18.3f, 0.f);
	}

	if (g_theInputSystem->IsKeyDown('7'))
	{
		m_position = Vec3(115.7f, 414.5f, 293.9f);
		m_orientationDegrees = EulerAngles(129.7f, 39.1f, 0.f);
	}

	if (g_theInputSystem->IsKeyDown('8'))
	{
		m_position = Vec3(103.2f, 120.1f, 330.7f);
		m_orientationDegrees = EulerAngles(210.9f, 51.8f, 0.f);
	}

	if (g_theInputSystem->IsKeyDown('9'))
	{
		m_position = Vec3(-121.2f, -18.3f, 173.82f);
		m_orientationDegrees = EulerAngles(353.6f, 3.0f, 0.f);
	}

	if (g_theInputSystem->IsKeyDown('W'))
	{
		m_velocity = moveSpeed * orientationMatrix.GetIBasis3D();
		m_position += m_velocity * deltaSeconds;
	}

	if (g_theInputSystem->IsKeyDown('A'))
	{
		m_velocity = moveSpeed * orientationMatrix.GetJBasis3D();
		m_position += m_velocity * deltaSeconds;
	}

	if (g_theInputSystem->IsKeyDown('C'))
	{
		m_velocity = moveSpeed * orientationMatrix.GetKBasis3D();
		m_position += m_velocity * deltaSeconds;
	}

	if (g_theInputSystem->IsKeyDown('S'))
	{
		m_velocity = moveSpeed * orientationMatrix.GetIBasis3D();
		m_position -= m_velocity * deltaSeconds;
	}

	if (g_theInputSystem->IsKeyDown('D'))
	{
		m_velocity = moveSpeed * orientationMatrix.GetJBasis3D();
		m_position -= m_velocity * deltaSeconds;
	}

	if (g_theInputSystem->IsKeyDown('Z'))
	{
		m_velocity = moveSpeed * orientationMatrix.GetKBasis3D();
		m_position -= m_velocity * deltaSeconds;
	}

	if (g_theInputSystem->IsKeyDown('Q'))
	{
		m_orientationDegrees.m_rollDegrees += -100.f * deltaSeconds;
	}
	if (g_theInputSystem->IsKeyDown('E'))
	{
		m_orientationDegrees.m_rollDegrees += 100.f * deltaSeconds;
	}

	if (g_theInputSystem->GetCursorClientDelta().y < -0.1f)
	{
		m_orientationDegrees.m_pitchDegrees += g_theInputSystem->GetCursorClientDelta().y * mouseSpeed * .01f;
	}
	
	if (g_theInputSystem->GetCursorClientDelta().y > 0.01f)
	{
		m_orientationDegrees.m_pitchDegrees += g_theInputSystem->GetCursorClientDelta().y * mouseSpeed * .01f;
	}
	
	if (g_theInputSystem->GetCursorClientDelta().x < -0.01f)
	{
		m_orientationDegrees.m_yawDegrees += -g_theInputSystem->GetCursorClientDelta().x * mouseSpeed * .01f;
	}
	
	if (g_theInputSystem->GetCursorClientDelta().x > 0.f)
	{
		m_orientationDegrees.m_yawDegrees += -g_theInputSystem->GetCursorClientDelta().x * mouseSpeed * .01f;
	}

	m_orientationDegrees.m_pitchDegrees = GetClamped(m_orientationDegrees.m_pitchDegrees, -85.f, 85.f);
	m_orientationDegrees.m_rollDegrees = GetClamped(m_orientationDegrees.m_rollDegrees, -45.f, 45.f);

	if (g_theInputSystem->WasKeyJustPressed('H'))
	{
		m_position = Vec3(0.f, 0.f, 0.f);
		m_orientationDegrees = EulerAngles();

	}

	if (g_theInputSystem->WasKeyJustPressed('I'))
	{
		DebugRenderToggle();
	}

	if (g_theInputSystem->WasKeyJustPressed('C'))
	{
		DebugRenderClear();
	}

	m_playerCam.SetTransform(m_position, m_orientationDegrees);
}

void Player::RenderShadow() const
{

}	

void Player::Render() const
{
// 	Vertex_PCU tempLocalVerts[NUM_PLAYER_VERTS];
// 
// 
// 
// 	for (int vertIndex = 0; vertIndex < NUM_PLAYER_VERTS; vertIndex++)
// 	{
// 		tempLocalVerts[vertIndex] = m_localVerts[vertIndex];
// 	}
// 
// 	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
// 	g_theRenderer->BindTexture(nullptr);
// 
// 	TransformVertexArrayXY3D(NUM_PLAYER_VERTS, tempLocalVerts, 80.f, 0.f, Vec2(m_position.x, m_position.y));
// 	g_theRenderer->DrawVertexArray(NUM_PLAYER_VERTS, tempLocalVerts);
}

void Player::InitializeLocalVerts()
{
	m_localVerts[0] = Vertex_PCU(-1.f, -1.f);
	m_localVerts[1] = Vertex_PCU(1.f, -1.f);
	m_localVerts[2] = Vertex_PCU(1.f, 1.f);
	m_localVerts[3] = Vertex_PCU(-1.f, -1.f);
	m_localVerts[4] = Vertex_PCU(1.f, 1.f);
	m_localVerts[5] = Vertex_PCU(-1.f, 1.f);

	for (int vertIndex = 0; vertIndex < NUM_PLAYER_VERTS; ++vertIndex)
	{
		m_localVerts[vertIndex].m_color = Rgba8(20, 250, 5);
	}
}
