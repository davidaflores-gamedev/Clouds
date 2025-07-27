#pragma once
#include "Engine/Math/MathUtils.hpp"

struct Vertex_PCUTBN;
class VoxelGrid
{
public:
	VoxelGrid();
	VoxelGrid(const IntVec3& dimensions, float voxelSize);
	~VoxelGrid();

	float GetDensityAt(IntVec3 localCoords) const;
	void SetDensityAt(IntVec3 localCoords, float density);

	Vec3 GetWorldPositionFromVoxelCoords(IntVec3 localCoords) const;
	Vec3 GetVoxelCoordsFromWorldPosition(const Vec3& worldPosition) const;

	IntVec3 GetDimensions() const { return m_dimensions; }
	float GetVoxelSize() const { return m_voxelSize; }

	void AddVertsForVoxelGrid(std::vector<Vertex_PCUTBN>& verts, std::vector<unsigned int>& indices);

private:
	std::vector<float> m_densityField = {};
	IntVec3 m_dimensions = IntVec3(64, 64, 64);
	float m_voxelSize = 1.0f;
};

