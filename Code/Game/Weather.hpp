#pragma once

#include "Game/Cloud.hpp"

class CloudManager;

class Weather
{
public:
	Weather();
	~Weather();

	// Initialize weather parameters
	void Initialize(float temperature, float humidity, float windSpeed, const Vec3& windDirection);

	// Update weather conditions over time
	void Update(float deltaTime, CloudManager& cloudManager);

	// Setters for weather parameters
	void SetWeatherConditions(float temperature, float humidity, float windSpeed, Vec3 windDirection);

	// Getters for weather parameters
	float GetTemperature() const { return m_temperature; }
	float GetHumidity() const { return m_humidity; }
	float GetWindSpeed() const { return m_windSpeed; }
	Vec3 GetWindDirection() const { return m_windDirection; }

public:
	Vec3 m_windDirection = Vec3(0.f, 0.f, 0.f);
	float m_windSpeed = 0.f;
	float m_temperature = 0.f;
	float m_humidity = 0.f;
	LightConstants m_lightConstants;
};