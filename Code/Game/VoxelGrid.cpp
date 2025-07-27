#include "VoxelGrid.hpp"

VoxelGrid::VoxelGrid()
{
	m_dimensions = IntVec3(64, 64, 64);
	m_voxelSize = 1.0f;
	m_densityField.resize(m_dimensions.x * m_dimensions.y * m_dimensions.z, 0.0f);
}

VoxelGrid::VoxelGrid(const IntVec3& dimensions, float voxelSize)
{
	m_dimensions = dimensions;
	m_voxelSize = voxelSize;
	m_densityField.resize(dimensions.x * dimensions.y * dimensions.z, 0.0f);
}

VoxelGrid::~VoxelGrid()
{

}

float VoxelGrid::GetDensityAt(IntVec3 localCoords) const
{
	if (localCoords.x < 0 || localCoords.x >= m_dimensions.x ||
		localCoords.y < 0 || localCoords.y >= m_dimensions.y ||
		localCoords.z < 0 || localCoords.z >= m_dimensions.z)
	{
		return 0.0f;
	}

	return m_densityField[localCoords.x + localCoords.y * m_dimensions.x + localCoords.z * m_dimensions.x * m_dimensions.y];
}

void VoxelGrid::SetDensityAt(IntVec3 localCoords, float density)
{
	if (localCoords.x < 0 || localCoords.x >= m_dimensions.x ||
		localCoords.y < 0 || localCoords.y >= m_dimensions.y ||
		localCoords.z < 0 || localCoords.z >= m_dimensions.z)
	{
		return;
	}

	m_densityField[localCoords.x + localCoords.y * m_dimensions.x + localCoords.z * m_dimensions.x * m_dimensions.y] = density;
}

Vec3 VoxelGrid::GetWorldPositionFromVoxelCoords(IntVec3 localCoords) const
{
	float worldX = (float)localCoords.x * m_voxelSize;
	float worldY = (float)localCoords.y * m_voxelSize;
	float worldZ = (float)localCoords.z * m_voxelSize;

	return Vec3(worldX, worldY, worldZ);
}

Vec3 VoxelGrid::GetVoxelCoordsFromWorldPosition(const Vec3& worldPosition) const
{
	int x = (int)floor(worldPosition.x / m_voxelSize);
	int y = (int)floor(worldPosition.y / m_voxelSize);
	int z = (int)floor(worldPosition.z / m_voxelSize);

	return IntVec3(x, y, z);
}

void VoxelGrid::AddVertsForVoxelGrid(std::vector<Vertex_PCUTBN>& verts, std::vector<unsigned int>& indices)
{
}
