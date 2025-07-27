#include "Voxel.hpp"

Voxel::Voxel(Vec3 position, float density, float moisture, float temperature)
{
	m_position = position;
	m_density = density;
	m_moisture = moisture;
	m_temperature = temperature;
}

Voxel::~Voxel()
{
}

bool Voxel::isEmpty() const
{
	return m_density == 0.f;
}