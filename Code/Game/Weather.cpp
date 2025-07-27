#include "Game/Weather.hpp"
#include "Game/CloudManager.hpp"

Weather::Weather()
	:m_temperature(20.0f)
	, m_humidity(50.0f)
	, m_windSpeed(10.0f)
	, m_windDirection(Vec3(0.f, 0.f, 0.f))
{
	m_lightConstants.AmbientIntensity = 0.1f;
	m_lightConstants.SunIntensity = 5.f;
}

Weather::~Weather()
{
}

void Weather::Initialize(float temperature, float humidity, float windSpeed, const Vec3& windDirection)
{
	m_temperature = temperature;
	m_humidity = humidity;
	m_windSpeed = windSpeed;
	m_windDirection = windDirection.GetNormalized();
}



void Weather::Update(float deltaTime, CloudManager& cloudManager)
{
	//cloudManager.BindNoiseTexture();
	cloudManager.UpdateClouds(deltaTime, *this);
}

void Weather::SetWeatherConditions(float temperature, float humidity, float windSpeed, Vec3 windDirection)
{
	m_temperature = temperature;
	m_humidity = humidity;
	m_windSpeed = windSpeed;
	m_windDirection = windDirection.GetNormalized();
}
