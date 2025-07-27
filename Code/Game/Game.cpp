
#include "Game.hpp"
#include "Game/Entity.hpp"
#include "Game/app.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Player.hpp"
#include "Game/Prop.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "ThirdParty/Engine_Code_ThirdParty_Squirrel/SmoothNoise.hpp"
#include "Game/Perlin3D.hpp"

extern InputSystem* g_theInputSystem;
extern AudioSystem* g_theAudioSystem;
bool m_debug = false;
RandomNumberGenerator* g_theRandom = nullptr;
//Camera m_worldCamera;
Camera m_screenCamera;
Camera m_lightCamera;
extern DevConsole* g_theConsole;
extern Window* g_theWindow;

Game::Game()
{
	SubscribeEventCallbackFunction("SetGameScale", Event_SetGameTimeScale);
	SubscribeEventCallbackFunction("Controls", Event_Controls);

	//m_worldCamera

	m_screenCamera.SetOrthoView(Vec2(0.f, 0.f), Vec2(1600.f, 800.f));
}

Game::~Game()
{
	delete[] m_noiseData;
	m_noiseData = nullptr;

	//delete m_cloudManager;
	//m_cloudManager = nullptr;

	delete m_singleCloudManager;
	m_singleCloudManager = nullptr;
	//m_cloudManager;
}

void Game::Startup()
{
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	m_entities[0] = new Player(this, Vec3(-10, -200, 140));
	m_entities[0]->m_orientationDegrees.m_yawDegrees = 90.f;	
	m_entities[0]->m_orientationDegrees.m_pitchDegrees = 20.f;

	m_entities[1] = new Prop(this, Vec3(0, 0, -2.f), ObjectType::UniformColorCube);
	m_entities[1]->m_color = Rgba8::BLUE;
	static_cast<Prop*>(m_entities[1])->xScale = 10.f;
	static_cast<Prop*>(m_entities[1])->yScale = 10.f;
	static_cast<Prop*>(m_entities[1])->zScale = .5f;

	m_entities[2] = new Prop(this, Vec3(0, 0, 1.f), ObjectType::UniformColorCube);
	m_entities[2]->m_color = Rgba8::GREEN;
	static_cast<Prop*>(m_entities[2])->xScale = 1.f;
	static_cast<Prop*>(m_entities[2])->yScale = 3.f;
	static_cast<Prop*>(m_entities[2])->zScale = 3.f;

	m_entities[3] = new Prop(this, Vec3(0, 0, 6.f), ObjectType::UniformColorCube);
	m_entities[3]->m_color = Rgba8::RED;
	static_cast<Prop*>(m_entities[3])->xScale = 3.f;
	static_cast<Prop*>(m_entities[3])->yScale = 1.f;
	static_cast<Prop*>(m_entities[3])->zScale = 3.f;

	for (int i = 0; i < 101; i++)
	{
		m_entities[i + 4] = new Prop(this, Vec3(0.f, (i) - 50.f, 0.f), ObjectType::UniformColorCube);
		m_entities[i + 4]->m_color = Rgba8(100, 100, 100);
		static_cast<Prop*>(m_entities[i + 4])->yScale = .01f;
		static_cast<Prop*>(m_entities[i + 4])->zScale = .01f;

		if (i % 5 == 0)
		{
			m_entities[i + 4]->m_color = Rgba8(150, 10, 10);
			static_cast<Prop*>(m_entities[i + 4])->yScale = .03f;
			static_cast<Prop*>(m_entities[i + 4])->zScale = .03f;
		}

		if (i == 50)
		{
			m_entities[i + 4]->m_color = Rgba8::RED;
			static_cast<Prop*>(m_entities[i + 4])->yScale = .06f;
			static_cast<Prop*>(m_entities[i + 4])->zScale = .06f;
		}

		static_cast<Prop*>(m_entities[i + 4])->xScale = 100.f;
		
	}
	
	for (int i = 0; i < 101; i++)
	{
		m_entities[i + 105] = new Prop(this, Vec3((i)-50.f, 0.f, 0.f), ObjectType::UniformColorCube);
		m_entities[i + 105]->m_color = Rgba8(100, 100, 100);

		static_cast<Prop*>(m_entities[i + 105])->xScale = .01f;
		static_cast<Prop*>(m_entities[i + 105])->zScale = .01f;

		if (i % 5 == 0)
		{
			m_entities[i + 105]->m_color = Rgba8(10, 150, 10);
			static_cast<Prop*>(m_entities[i + 105])->xScale = .03f;
			static_cast<Prop*>(m_entities[i + 105])->zScale = .03f;
		}

		if (i == 50)
		{
			m_entities[i + 105]->m_color = Rgba8::GREEN;
			static_cast<Prop*>(m_entities[i + 105])->xScale = .06f;
			static_cast<Prop*>(m_entities[i + 105])->zScale = .06f;
		}

	
		static_cast<Prop*>(m_entities[i + 105])->yScale = 100.f;
	}

	m_entities[206] = new Prop(this, Vec3(0, 0, -3.0), ObjectType::UniformColorCube);
	m_entities[206]->m_color = Rgba8(77, 153, 51, 255);
	static_cast<Prop*>(m_entities[206])->xScale = 500.f;
	static_cast<Prop*>(m_entities[206])->yScale = 500.f;


	m_player = (Player*)m_entities[0];

	float aspect = g_theWindow->GetAspect();

	m_player->m_playerCam.SetPerspView(aspect, 60.f, 0.1f, 10000.f);
	m_player->m_playerCam.SetRenderBasis(Vec3::DIRECTX11_IBASIS, Vec3::DIRECTX11_JBASIS, Vec3::DIRECTX11_KBASIS);
	m_lightCamera.SetRenderBasis(Vec3::DIRECTX11_IBASIS, Vec3::DIRECTX11_JBASIS, Vec3::DIRECTX11_KBASIS);

	g_theRandom = new RandomNumberGenerator();
	m_gameClock = new Clock();

	FireEvent("Controls");

	m_weather.Initialize(25.0f, 60.0f, 15.0f, Vec3(1.0f, 0.0f, 0.0f));

	//m_cloudManager = new CloudManager(200);
	m_singleCloudManager = new CloudManager(this, 1);

	m_weather = Weather();
	m_weather.SetWeatherConditions(1.0f, 1.0f, 1.0f, Vec3(1.0f, 0.0f, 0.0f));
	
	//int noiseWidth	= 64;
	//int noiseHeight = 64;
	//int noiseDepth	= 64;
	
	//m_singleCloudManager->CreateCloud(IntVec3(25.f, 0.f, 20.f), IntVec3(5), 0.01f, 0.5f);

	//for (int i = 0; i < 10; i++)
	//{
	//	m_cloudManager->CreateCloud(g_theRandom->RollRandomVec3InRange(0.f, 50.f), g_theRandom->RollRandomVec3InRange(32, 64), 0.01f, 0.5f);
	//}

	m_singleCloudManager->CreateTest();
	//m_singleCloudManager->CreateCloud(Vec3(0, 0, 0);
	//cloud1.m_scale = Vec3(5.0f, 3.0f, 5.0f);

	//std::unique_ptr<Cloud> cloud1;
	//
	//cloud1 = m_cloudManager.CreateCloud();
	//auto& cloud2 = m_cloudManager.CreateCloud();

	//m_cloudManager.InitializeNoiseTexture(128, 128, 128, m_noiseData);

	//m_cloudManager.CreateCloud(Vec3(0.0f, 10.0f, 0.0f), Vec3(5.0f, 3.0f, 5.0f));
	//int numClouds = 100;

	//for (int i = 0; i < numClouds; i++)
	//{
	//	Cloud* m_cloud = new Cloud(Vec3(g_theRandom->RollRandomVec3InRange(-50.f, 50.f)), 10.f);
	//	m_clouds.push_back(m_cloud);
	//}
	//
	//for (Cloud* cloud : m_clouds)
	//{
	//	cloud->BuildVerts();
	//}
	m_shadowShader = g_theRenderer->CreateOrGetShader("Data/Shaders/DefaultShadows", VertexType::VERTEX_PCU);

}

void Game::StartGame()
{
	if (!m_attract)
	{
		m_triOffset1 = 800;
		m_triOffset2 = -1000;
		//m_playerShip = new PlayerShip(this, Vec2(WORLD_CENTER_X, WORLD_CENTER_Y));

		float scale = 2.f;
		float length = .2f;

		DebugAddWorldArrow(Vec3::ZERO, Vec3::WORLD_XBASIS * scale, .7f, length, -1.f, Rgba8::RED, Rgba8::RED);
		DebugAddWorldArrow(Vec3::ZERO, Vec3::WORLD_YBASIS * scale, .7f, length, -1.f, Rgba8::GREEN, Rgba8::GREEN);
		DebugAddWorldArrow(Vec3::ZERO, Vec3::WORLD_ZBASIS * scale, .7f, length, -1.f, Rgba8::BLUE, Rgba8::BLUE);

		//DebugAddWorldBillboardText()

		Mat44 xForward = Mat44();
		Mat44 yForward = Mat44();
		Mat44 zForward = Mat44();

		EulerAngles xangle = EulerAngles();
		xangle.m_yawDegrees = 90.f;

		EulerAngles yangle = EulerAngles();
		//xangle.m_Degrees = 90.f; 

		EulerAngles zangle = EulerAngles();
		zangle.m_yawDegrees = 90.f;
		//zangle.m_pitchDegrees = 90.f;
		

		//xForward.SetIJK3D(Vec3::WORLD_XBASIS, Vec3::WORLD_YBASIS, Vec3::WORLD_ZBASIS);
		xForward.Append(xangle.GetMatrix_XFwd_YLeft_ZUp());
		yForward.Append(yangle.GetMatrix_XFwd_YLeft_ZUp());
		zForward.Append(zangle.GetMatrix_XFwd_YLeft_ZUp());

		//yForward.SetIJK3D(Vec3::WORLD_XBASIS, Vec3::WORLD_ZBASIS, Vec3::WORLD_YBASIS * -1.f);
		//zForward.SetIJK3D(Vec3::WORLD_XBASIS, Vec3::WORLD_ZBASIS, Vec3::WORLD_YBASIS * -1.f);
		
		//DebugAddWorldText("X - Forward", 50.f, xForward, Vec2(0.5f, 0.5f), 0.f, Rgba8::RED, Rgba8::RED);
		//DebugAddWorldText("Y - Forward", 50.f, yForward, Vec2(0.5f, 0.5f), 0.f, Rgba8::RED, Rgba8::RED);
		DebugAddWorldText("Z - Forward", 50.f, zForward, Vec2(0.5f, 0.5f), 0.f, Rgba8::RED, Rgba8::RED);

		//DebugAddWorldText("X - Forward", 50.f, Vec2(.5f, .5f), -1.f, Rgba8::RED, Rgba8::RED);
		//DebugAddWorldText("Y - Left", 50.f, Vec2(.5f, .5f), -1.f, Rgba8::GREEN, Rgba8::GREEN);
		//DebugAddWorldText("Z - Up", 50.f, xForward, true, Vec2(.5f, .5f), -1.f, Rgba8::BLUE, Rgba8::BLUE);

	}																 
}

void Game::Shutdown()
{
	//m_cloudManager->Shutdown();
	delete m_singleCloudManager;
	m_singleCloudManager = nullptr;
	shuttingDown = true;	
}

void Game::Update()
{
	float deltaSeconds = m_gameClock->GetDeltaSeoconds();

	if(g_theInputSystem->WasKeyJustPressed('P'))
		Pause();

	if(g_theInputSystem->WasKeyJustPressed('T'))
		SlowMo();

	if(g_theInputSystem->WasKeyJustPressed('O'))
	{
		NextFrame();
	}

	if(m_attract)
	{
		if (g_theInputSystem->WasKeyJustPressed(' ') || g_theInputSystem->m_controllers[0]->WasButtonJustPressed(XBOXBUTTON_START) || g_theInputSystem->m_controllers[0]->WasButtonJustPressed(XBOXBUTTON_A) || g_theInputSystem->WasKeyJustPressed('N'))
		{
			m_attract = false;
			StartGame();
		}
		if (g_theInputSystem->WasKeyJustPressed(27) || g_theInputSystem->m_controllers[0]->GetLeftTrigger() == 1.f)
		{
			g_theApp->HandleQuitRequested();
		}

		m_triOffset1 += m_triangleMoveSpeed * deltaSeconds;
		m_triOffset2 += m_triangleMoveSpeed * deltaSeconds;

		if (m_triOffset1 >= 2400.f)
		{
			m_triOffset1 = -800.f;
		}

		if (m_triOffset2 >= 2400.f)
		{
			m_triOffset2 = -800.f;
		}

	}

	if(g_theInputSystem->WasKeyJustPressed(27) || g_theInputSystem->m_controllers[0]->GetLeftTrigger() == 1.f)
	{
		g_theApp->HandleQuitRequested();
		return;
	}
	  
	if (g_theInputSystem->WasKeyJustPressed('B'))
	{
		m_debug = !m_debug;
	}

	if (g_theInputSystem->IsKeyDown(KEYCODE_UPARROW))
	{
		m_sunOrientation.m_pitchDegrees += 45.f * deltaSeconds;
	}

	if (g_theInputSystem->IsKeyDown(KEYCODE_DOWNARROW))
	{
		m_sunOrientation.m_pitchDegrees -= 45.f * deltaSeconds;
	}

	if (g_theInputSystem->IsKeyDown(KEYCODE_LEFT))
	{
		m_sunOrientation.m_yawDegrees += 45.f * deltaSeconds;
	}

	if (g_theInputSystem->IsKeyDown(KEYCODE_RIGHT))
	{
		m_sunOrientation.m_yawDegrees -= 45.f * deltaSeconds;
	}

	Vec3 sunDirection;
	Vec3 y;
	Vec3 z;
	m_sunOrientation.GetVectors_XFwd_YLeft_ZUp(sunDirection, y, z);

	m_weather.m_lightConstants.SunDirection = sunDirection;

	if (m_victoryTimer <= 0.f)
	{
		Shutdown();
	}

	if (m_victory == true)
	{
		m_victoryTimer -= deltaSeconds;
	}

	if (m_deathTimer <= 0.f)
	{
		Shutdown();
	}

	if (m_restartGame == true)
	{
		m_deathTimer -= deltaSeconds;
	}

	if(m_isSlowMo){deltaSeconds = deltaSeconds/10;}

	if(!m_attract)
	{
		m_sunVerts.clear();
		m_sunWireframeVerts.clear();

		AddVertsForArrow3D(m_sunVerts, Vec3(0.f, 0.f, 2.5f), Vec3(0.f, 0.f, 2.5f) + m_weather.m_lightConstants.SunDirection * 5.f, .7f, .2f, .3f, Rgba8(255, 0, 0, 127));

		AddVertsForArrow3D(m_sunWireframeVerts, Vec3(0.f, 0.f, 2.5f), Vec3(0.f, 0.f, 2.5f) + m_weather.m_lightConstants.SunDirection * 5.f, .7f, .2f, .3f, Rgba8::RED);

		//m_entities

		if (m_isPaused == false)
		{
			m_player->HandleInput(deltaSeconds);

			for (int i = 0; i < maxEntities; i++)
			{
				if(m_entities[i] != nullptr)
				{
					if (i == 1)
					{
						//m_entities[i]->m_angularVelocity.m_rollDegrees = 30;
						//m_entities[i]->m_angularVelocity.m_pitchDegrees = 30;
					}

					if (i == 2)
					{
						//float pulse = (SinDegrees(m_gameClock->GetTotalSeconds() * 45.f) + 1.f) * 0.5f;
						//m_entities[i]->m_color.r = (unsigned char)pulse * 255;
						//m_entities[i]->m_color.g = (unsigned char)pulse * 255;
						//m_entities[i]->m_color.b = (unsigned char)pulse * 255;
					}

					if (i == 3)
					{
						//m_entities[i]->m_angularVelocity.m_yawDegrees = 45;
					}

					m_entities[i]->Update(deltaSeconds);
				}
			}

			//update stuff
			m_rotationoffset +=  10 * deltaSeconds;

			//m_weather.Update(deltaSeconds, *m_cloudManager);
			m_weather.Update(deltaSeconds, *m_singleCloudManager);
			//m_cloudManager.UpdateClouds(deltaSeconds, m_weather);
		}
		if (m_nextFrame)
		{
			m_isPaused = true;
			m_nextFrame = false;
		}
		
	}

	m_screenCamera.SetOrthoView(Vec2(0.f, 0.f), Vec2(1600.f, 800.f));

	float aspect = g_theWindow->GetAspect();

	m_player->m_playerCam.SetPerspView(aspect, 60.f, 0.1f, 1000.f);
		
		//SetOrthoView(Vec2(-1, -1), Vec2(1, 1));

// 	if (m_cameraShakeTimer > 0)
// 	{
// 		m_worldCamera.Translate2D(Vec2(g_theRandom->RollRandomFloatInRange(-0.5f, 0.5f), g_theRandom->RollRandomFloatInRange(-0.5f, 0.5f)));
// 		m_cameraShakeTimer -= deltaSeconds;
// 	}
}

void Game::RunCompute()
{
}

void Game::PrepareForRender()
{
}

// void Game::PlayerUpdate(float deltaSeconds)
// {
// 	m_playerShip->Update(deltaSeconds);
// }


void Game::BeginFrame()
{
	m_singleCloudManager->BeginFrame();
}

void Game::EndFrame()
{
	if (!m_debug)
	{
		return;
	}
	std::string string1 = Stringf("Time: %.1f, FPS: %.1f, TimeDilation: %.1f", m_gameClock->GetTotalSeconds(), 1.f / m_gameClock->GetDeltaSeoconds(), m_gameClock->GetTimeScale());

	DebugAddScreenText(string1, Vec2(1400.f, 600.f), 200.f, Vec2(1.f, 1.f), 0.f, Rgba8::YELLOW, Rgba8::YELLOW);

	Mat44 translation;

	std::string string2 = Stringf("X: %.1f, Y: %.1f, Z:%1f", m_player->m_position.x, m_player->m_position.y, m_player->m_position.z);

	DebugAddMessage(string2, translation, 0.f, Rgba8::RED, Rgba8::RED);

	//I want to print out my camera matrix
	std::string string3 = Stringf("X: %.1f, Y: %.1f, Z:%1f", m_player->m_playerCam.m_position.x, m_player->m_playerCam.m_position.y, m_player->m_playerCam.m_position.z);
	std::string string4 = Stringf("Yaw: %.1f, Pitch: %.1f, Roll:%1f", m_player->m_playerCam.m_orientation.m_yawDegrees, m_player->m_playerCam.m_orientation.m_pitchDegrees, m_player->m_playerCam.m_orientation.m_rollDegrees);

	DebugAddMessage(string3, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string4, translation, 0.f, Rgba8::RED, Rgba8::RED);

	//std::string string5 = Stringf("X: %.1f, Y: %.1f, Z:%1f", m_player->m_playerCam.GetForward().x, m_player->m_playerCam.GetForward().y, m_player->m_playerCam.GetForward().z);

	//use this m_player->m_playerCam.GetPerspectiveMatrix();

	std::string string6 = Stringf("ViewMatrix:Ix, Jx, Kx, Wx: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetViewMatrix().m_values[0], m_player->m_playerCam.GetViewMatrix().m_values[4], m_player->m_playerCam.GetViewMatrix().m_values[8],  m_player->m_playerCam.GetViewMatrix().m_values[12]);
	std::string string7 = Stringf("ViewMatrix:Iy, Jy, Ky, Wy: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetViewMatrix().m_values[1], m_player->m_playerCam.GetViewMatrix().m_values[5], m_player->m_playerCam.GetViewMatrix().m_values[9],  m_player->m_playerCam.GetViewMatrix().m_values[13]);
	std::string string8 = Stringf("ViewMatrix:Iz, Jz, Kz, Wz: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetViewMatrix().m_values[2], m_player->m_playerCam.GetViewMatrix().m_values[6], m_player->m_playerCam.GetViewMatrix().m_values[10], m_player->m_playerCam.GetViewMatrix().m_values[14]);
	std::string string9 = Stringf("ViewMatrix:Iw, Jw, Kw, Ww: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetViewMatrix().m_values[3], m_player->m_playerCam.GetViewMatrix().m_values[7], m_player->m_playerCam.GetViewMatrix().m_values[11], m_player->m_playerCam.GetViewMatrix().m_values[15]);

	//DebugAddMessage(string5, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string6, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string7, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string8, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string9, translation, 0.f, Rgba8::RED, Rgba8::RED);

	std::string string10 = Stringf("InverseViewMatrixIx, Jx, Kx, Wx: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[0], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[4], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[8], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[12]);
	std::string string11 = Stringf("InverseViewMatrixIy, Jy, Ky, Wy: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[1], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[5], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[9], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[13]);
	std::string string12 = Stringf("InverseViewMatrixIz, Jz, Kz, Wz: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[2], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[6], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[10], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[14]);
	std::string string13 = Stringf("InverseViewMatrixIw, Jw, Kw, Ww: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[3], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[7], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[11], m_player->m_playerCam.GetViewMatrix().GetOrthonormalInverse().m_values[15]);

	DebugAddMessage(string10, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string11, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string12, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string13, translation, 0.f, Rgba8::RED, Rgba8::RED);

	//here I want to do projection matrix and then inverse projection matrix
	std::string string14 = Stringf("ProjectionMatrix:Ix, Jx, Kx, Wx: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetProjectionMatrix().m_values[0], m_player->m_playerCam.GetProjectionMatrix().m_values[4], m_player->m_playerCam.GetProjectionMatrix().m_values[8], m_player->m_playerCam.GetProjectionMatrix().m_values[12]);
	std::string string15 = Stringf("ProjectionMatrix:Iy, Jy, Ky, Wy: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetProjectionMatrix().m_values[1], m_player->m_playerCam.GetProjectionMatrix().m_values[5], m_player->m_playerCam.GetProjectionMatrix().m_values[9], m_player->m_playerCam.GetProjectionMatrix().m_values[13]);
	std::string string16 = Stringf("ProjectionMatrix:Iz, Jz, Kz, Wz: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetProjectionMatrix().m_values[2], m_player->m_playerCam.GetProjectionMatrix().m_values[6], m_player->m_playerCam.GetProjectionMatrix().m_values[10], m_player->m_playerCam.GetProjectionMatrix().m_values[14]);
	std::string string17 = Stringf("ProjectionMatrix:Iw, Jw, Kw, Ww: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetProjectionMatrix().m_values[3], m_player->m_playerCam.GetProjectionMatrix().m_values[7], m_player->m_playerCam.GetProjectionMatrix().m_values[11], m_player->m_playerCam.GetProjectionMatrix().m_values[15]);

	DebugAddMessage(string14, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string15, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string16, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string17, translation, 0.f, Rgba8::RED, Rgba8::RED);

	std::string string18 = Stringf("InverseProjectionMatrix:Ix, Jx, Kx, Wx: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[0], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[4], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[8],  m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[12]);
	std::string string19 = Stringf("InverseProjectionMatrix:Iy, Jy, Ky, Wy: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[1], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[5], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[9],  m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[13]);
	std::string string20 = Stringf("InverseProjectionMatrix:Iz, Jz, Kz, Wz: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[2], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[6], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[10], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[14]);
	std::string string21 = Stringf("InverseProjectionMatrix:Iw, Jw, Kw, Ww: %.1f, %.1f, %.1f, %.1f", m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[3], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[7], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[11], m_player->m_playerCam.GetProjectionMatrix().GetInverse().m_values[15]);

	DebugAddMessage(string18, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string19, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string20, translation, 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(string21, translation, 0.f, Rgba8::RED, Rgba8::RED);

	//I want to calculate near screen dimensions
	std::string string22 = Stringf("Near Screen Dimensions: %.1f, %.1f", m_player->m_playerCam.CalculateNearScreenDimensions().x, m_player->m_playerCam.CalculateNearScreenDimensions().y);
	DebugAddMessage(string22, translation, 0.f, Rgba8::RED, Rgba8::RED);
}

void Game::RenderShadowMap()
{
	g_theRenderer->PreRenderShadowMap();

	//Vec3 sunDirection = m_sunOrientation.GetMatrix_XFwd_YLeft_ZUp().GetIBasis3D();

	float distance = 700.f;

	Vec3 iBasis, jBasis, kBasis;
	m_lightCamera.m_orientation.GetVectors_XFwd_YLeft_ZUp(iBasis, jBasis, kBasis);

	Vec3 sunPosition = Vec3(0.f, 0.f, 0.f) - iBasis * distance;
	float orthoWidth = 500.f;
	float orthoHeight = 500.f;

	//sunPosition = sunPosition + jBasis * orthoWidth * 0.375f + kBasis * -orthoHeight * .75f;

	m_lightCamera.m_position = sunPosition;    // or some base offset
	m_lightCamera.m_orientation = m_sunOrientation;

	//float percentDown = 0.25f;   // 25% down
	//float percentLeft = 0.375f;  // 37.5% left


	//Vec3 offsetDown = -percentDown * orthoHeight * kBasis;
	//Vec3 offsetLeft = percentLeft * orthoWidth * jBasis;
	//Vec3 totalOffset = offsetDown + offsetLeft;

	//m_lightCamera.m_position += totalOffset;

	//10, -5

	m_lightCamera.SetOrthoView(AABB2(Vec2(-orthoWidth, -orthoHeight), Vec2(orthoWidth, orthoHeight)), 0.1f, 1000.f);

	//Vec3 sunPosition = Vec3(0.f, 0.f, 0.f) - sunDirection * distance;
	//
	//m_lightCamera.m_position = Vec3(-20.0f, 37.5f, -25.f);
	//
	//m_lightCamera.m_orientation = m_sunOrientation;
	//
	////m_lightCamera.SetTransform(sunPosition, m_sunOrientation);
	//
	//m_lightCamera.SetOrthoView(AABB2(Vec2(100, 100)), 0.f, 200.f);
	//

	

	g_theRenderer->BeginCamera(m_lightCamera);

	g_theRenderer->SetModelConstants();
	//g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->BindTexture(nullptr);
	//g_theRenderer->BindTexture(nullptr);
	//g_theRenderer->BindShader(nullptr);

	g_theRenderer->SetDepthMode(DepthMode::ENABLED);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	m_entities[1]->RenderShadow();

	m_entities[2]->RenderShadow();

	m_entities[3]->RenderShadow();

	m_singleCloudManager->RenderShadows();

	for (int i = 0; i < maxEntities; i++)
	{
		if (m_entities[i] != nullptr)
		{
			
		}
	}

	

	g_theRenderer->EndCamera(m_lightCamera);
}

void Game::Render()
{
	RenderShadowMap();

	g_theRenderer->ClearScreen(Rgba8(135, 206, 235, 1));

	g_theRenderer->PreRenderMain();

	g_theRenderer->BeginCamera(m_player->m_playerCam);

	//g_theRenderer->SetLightConstants(m_sunDirection.GetNormalized(), m_sunIntensity, m_ambientIntensity);
	g_theRenderer->SetModelConstants();

	

	Mat44 ViewProjectionMatrix = m_lightCamera.GetViewProjectionMatrix();

	sc.lightProjection = ViewProjectionMatrix;

	g_theRenderer->SetShadowConstants(sc);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindTexture(g_theRenderer->GetTextureForFileName("ShadowMap"), 1);
	g_theRenderer->BindTexture(m_singleCloudManager->m_outShadowTexture, 2);
	//g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(m_shadowShader);

	//g_theRenderer->BindTexture3D(m_noiseTexture);
	g_theRenderer->SetDepthMode(DepthMode::ENABLED);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

	if (!m_attract)
	{
		g_theRenderer->DrawVertexArray((int)m_sunVerts.size(), m_sunVerts.data());
		g_theRenderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_BACK);
		g_theRenderer->DrawVertexArray((int)m_sunWireframeVerts.size(), m_sunWireframeVerts.data());

		std::vector<Vertex_PCU> sunPointverts;

		AddVertsForSphere3D(sunPointverts, m_lightCamera.m_position, .5f);

		Vec3 direction;
		Vec3 j;
		Vec3 k;

		m_sunOrientation.GetVectors_XFwd_YLeft_ZUp(direction, j, k);

		AddVertsForArrow3D(sunPointverts, m_lightCamera.m_position, m_lightCamera.m_position + (direction * 5.f), .7f, .2f, .3f, Rgba8::RED);

		g_theRenderer->DrawVertexArray((int)sunPointverts.size(), sunPointverts.data());

		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);

		m_singleCloudManager->DebugRenderClouds();

		g_theRenderer->BindTexture();
		for (int i = 0; i < maxEntities; i++)
		{
			if (m_entities[i] != nullptr)
			{
				m_entities[i]->Render();
			}
		}
		//m_cloudManager->RenderClouds();
		
		m_singleCloudManager->RenderClouds();

		DebugRenderWorld(m_player->m_playerCam);
	}

	g_theRenderer->EndCamera(m_player->m_playerCam);


	// Transform sun position from world-space to clip-space

	//Mat44 sunView = m_player->m_playerCam.GetViewProjectionMatrix();
	//
	//Vec4 sunClipSpace = sunView * Vec4(m_lightCamera.m_position, 1.0f);
	//
	//if (sunClipSpace.w != 0.0f) {
	//	sunClipSpace.x /= sunClipSpace.w;
	//	sunClipSpace.y /= sunClipSpace.w;
	//	sunClipSpace.z /= sunClipSpace.w;
	//}
	//
	//
	//// Convert from NDC (-1,1) to UV (0,1)
	//Vec2 sunScreenPos;
	//sunScreenPos.x = (sunClipSpace.x * 0.5f) + 0.5f;
	//sunScreenPos.y = (-sunClipSpace.y * 0.5f) + 0.5f;
	//
	//sunScreenPos *= .5f;
	//
	//if (sunClipSpace.w < 0)
	//	sunScreenPos = Vec2(-1, -1); // Invalid position

	grc.sunPosition = m_lightCamera.m_position;

	grc.viewProj = m_player->m_playerCam.GetViewProjectionMatrix();

	g_theRenderer->SetGodRaysConstants(grc);

	g_theRenderer->BindTexture(m_singleCloudManager->m_outShadowTexture, 1);

	g_theRenderer->RenderGodRays();

	g_theRenderer->PreRenderMain();
#pragma region ScreenCamera

	//create new camera for UI stuff that is bigger than the world camera
	g_theRenderer->BeginCamera(m_screenCamera);


	//render attract screen on UI
	if (m_attract)
	{
		RenderAttractScreen();
		//return;
	}

	//render game if we are not in attract mode
	if (!m_attract)
	{
		DebugRenderScreen(m_screenCamera);
// 		Vertex_PCU tempPlay1Verts[3];
// 
// 		Vertex_PCU m_playButtonVerts[3];
// 
// 		m_playButtonVerts[0] = Vertex_PCU(-1.5f, 1.5f, 102, 153, 204, 255);
// 		m_playButtonVerts[1] = Vertex_PCU(-1.5f, -1.5f, 102, 153, 204, 255);
// 		m_playButtonVerts[2] = Vertex_PCU(1.5, 0.f, 102, 153, 204, 255);
// 
// 		for (int vertIndex = 0; vertIndex < 3; vertIndex++)
// 		{
// 			tempPlay1Verts[vertIndex] = m_playButtonVerts[vertIndex];
// 		}
// 		TransformVertexArrayXY3D(3, tempPlay1Verts, 80.f, m_rotationoffset, Vec2(m_triOffset1, 400.f));
// 		g_theRenderer->DrawVertexArray(3, tempPlay1Verts);
// 
		if (m_debug == true)
		{
			DebugRender();
		}
	}

	

	g_theRenderer->EndCamera(m_screenCamera);

#pragma endregion ScreenCameraStuff
}

void Game::RenderAttractScreen()
{
	DebugDrawRing(Vec2(800, 400), (sinf(m_gameClock->GetTotalSeconds())  * 100.f) + 100.f, 20, Rgba8(255, 0, 0));
}

void Game::DebugRender()
{
	//DebugDrawRing(m_playerShip->m_position, PLAYER_SHIP_COSMETIC_RADIUS, .1f, Rgba8(255, 0, 255));
	//DebugDrawLine(m_playerShip->m_position, Vec2(PLAYER_SHIP_COSMETIC_RADIUS*CosDegrees(m_playerShip->m_orientationDegrees)+ m_playerShip->m_position.x, PLAYER_SHIP_COSMETIC_RADIUS*SinDegrees(m_playerShip->m_orientationDegrees)+ m_playerShip->m_position.y), .5f, Rgba8(0, 255, 0));
	//DebugDrawLine(m_playerShip->m_position, Vec2(m_playerShip->m_velocity.x + m_playerShip->m_position.x, m_playerShip->m_velocity.y + m_playerShip->m_position.y), .5f, Rgba8(255, 255, 0));
}

void Game::ToggleDebug()
{
	m_debug = !m_debug;
}
	    
void Game::Pause()
{
	m_gameClock->TogglePause();
}

void Game::SlowMo()
{
	if (m_gameClock->GetTimeScale() == .1f)
	{
		m_gameClock->SetTimeScale(1.f);
	}
	else
	{
		m_gameClock->SetTimeScale(.1f);
	}
	//m_isSlowMo = !m_isSlowMo;
}

void Game::NextFrame()
{
	m_gameClock->StepSingleFrame();
}

bool Game::IsAlive(Entity* entity) const
{
	entity;
	return false;
}

bool Event_SetGameTimeScale(EventArgs& args)
{
	float scale = args.GetValue("scale", 1.f);
	g_theApp->m_theGame->SetGameTimeScale(scale);

	return true;
}

bool Event_Controls(EventArgs& args)
{
	UNUSED(args);
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "Controls:");
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "E to move Forward");
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "S to rotate counter clockwise");
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "F to rotate clockwise");
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "Spacebar to shoot");
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "F1 to turn on DebguDraw");
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "T to Slow Down Time");
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "O to Step A Single Frame");
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "P to Pause");
	g_theConsole->AddLine(Rgba8(200, 200, 0, 200), "Esc to Quit the game/Close the application");

	return true;
}

void Game::SetGameTimeScale(float scale)
{
	m_gameClock->SetTimeScale(scale);
}	
