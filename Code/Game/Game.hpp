#pragma once
#include "Game/GameCommon.hpp"
#include "Game/Weather.hpp"
#include "Game/CloudManager.hpp"
//#include "Game/PlayerShip.hpp"

//#include "Game/Player.hpp"

class App;
class Entity;
class Clock;
class Player;

static bool Event_SetGameTimeScale(EventArgs& args);
static bool Event_Controls(EventArgs& args);

constexpr int maxEntities = 220;

class Game
{
	public:
		
	Game();
	~Game();

	void Startup();
	void StartGame();
	void Shutdown();
	void Update();

	void RunCompute();
	void PrepareForRender();

	void BeginFrame();
	void EndFrame();

	void RenderShadowMap();

	void Render();
	void RenderAttractScreen();
	void DebugRender();
	void ToggleDebug();
	void Pause();
	void SetGameTimeScale(float scale);
	void SlowMo();
	void NextFrame();
	bool IsAlive(Entity* entity) const;

	bool m_attract = false;
	bool m_victory = false;
	bool shuttingDown = false;
	bool m_isPaused = false;
	bool m_isSlowMo = false;
	bool m_nextFrame = false;
	bool m_restartGame = false;
	bool m_debug = false;
		
	float m_triangleMoveSpeed;
	float m_deathTimer = 3.0f;
	float m_victoryTimer = 5.0f;
	float m_cameraShakeTimer = 0.f;

	//Vertex_PCU m_shipVerts[15];
	//Vertex_PCU m_playButtonVerts[3];
	Clock* m_gameClock = nullptr;

	Entity* m_entities[maxEntities] = {nullptr};

	Player* m_player;

	Weather m_weather;
	//CloudManager* m_cloudManager = nullptr;
	CloudManager* m_singleCloudManager = nullptr;
	//std::vector<Cloud*> m_clouds;
	
	ShadowConstants sc;

	GodRaysConstants grc;

private:
	float m_triOffset1 = 800.f;
	float m_triOffset2 = -800.f;
	float m_rotationoffset = 0;

	//bool direction = true;

	float current = 1.0f;
	// in Game.hpp, inside class Game:
	//PlayerShip* m_playerShip = nullptr; // Just one player ship (for now...)
	//Asteroid* m_asteroids[MAX_ASTEROIDS] = {}; // Fixed number of asteroid “slots”; nullptr if unused.
	//Bullet* m_bullets[MAX_BULLETS] = {}; // The “= {};” syntax initializes the array to zeros.
	float* m_noiseData = nullptr;

	std::vector<Vertex_PCU> m_sunVerts;
	std::vector<Vertex_PCU> m_sunWireframeVerts;
	EulerAngles m_sunOrientation = EulerAngles(180, 30, 0);

	Shader* m_shadowShader = nullptr;


};