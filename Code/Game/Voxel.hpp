#pragma once

#include "Game/GameCommon.hpp"

class Voxel
{
public:
	Voxel() = default;

	Voxel(Vec3 position, float density, float moisture = 0.f, float temperature = 0.f);

	~Voxel();

	bool isEmpty() const;

public:
	Vec3 m_position;

	float m_density = 0.f;
	float m_moisture = 0.f;
	float m_temperature = 0.f;
};
