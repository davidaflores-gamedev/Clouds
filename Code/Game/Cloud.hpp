#pragma once

#include "Game/Voxel.hpp"

class Weather;

enum class ECloudType
{
	CLOUD_CUMULUS,
	CLOUD_CIRRUS,
	CLOUD_STRATUS,
	CLOUD_NIMBUS,
	CLOUD_TEST,
	CLOUD_COUNT
};

struct CloudGPU
{
	Vec3 center;
	IntVec3 gridDimensions;
	Vec3 minBounds; // For AABB2 bounding box
	Vec3 maxBounds; // For AABB2 bounding box
	unsigned int voxelOffset;  // Offset into voxel buffer
	unsigned int voxelCount;   // Number of voxels
	unsigned int octreeIndex;
	//unsigned int densityOffset; // Offset into density buffer
	//unsigned int densityCount;  // Number of density values
};

class Cloud
{
public:
	Cloud();
	Cloud(Vec3 location, IntVec3 dimensions, Vec3 voxelDimensions, ECloudType type = ECloudType::CLOUD_CUMULUS, bool useTest = false);
	Cloud(const Cloud& other) = default;
	~Cloud();

	//Cloud(const Vec3& start, float initialSize);

	//void Initialize(const Vec3& position, const Vec3& size);
	void GenerateCloud();
	void GenerateTestCloud();
	void GenerateDensityField(int width, int height, int depth, float noiseScale, float noiseThreshold);

	void Update(float deltaSeconds, const Weather& weather);

	CloudGPU GetCloudGPU(unsigned int& voxelOffset, unsigned int& densityOffset) const;
//IndexedVertexBufferData BuildVerts();
//IndexedVertexBufferData BuildDebugVerts();
	//void Render() const;
	//void DebugRender() const;
	AABB3 GetBounds() const;

	std::vector<Voxel*> GetVoxels() const;

	std::vector<Vec3*> GetVoxelPositions() const;

	std::vector<float> GetDensityField() const;

	bool NeedsRebuild() const;
private:
	//void AddVoxelVertices(IntVec3 location);

	bool IsVoxelActive(IntVec3 location) const;

public:
	//density field?
	Vec3 m_center = Vec3();
	IntVec3 m_gridDimensions = IntVec3(64, 64, 64);
	AABB3 boundingBox = AABB3(Vec3(-10.f, -10.f, -10.f), Vec3(10.f, 10.f, 10.f));

	std::vector<float> m_densityField = {};

	std::vector<Vec3> m_voxelPositions = {};

	std::vector<Voxel> m_voxels = {};


	//std::vector<float> m_densityValues = {};

	bool m_structureChanged = true;

	int m_octreeIndex = -1;
	//IntVec3 m_size = IntVec3(
	//bool m_debug = false;
	//ID3D11Buffer* m_vertexBuffer = nullptr;

	//float m_density = 1.0f;
	//CloudsBuffer* m_voxelBuffer = nullptr;	

	//VertexBuffer* m_vbo = nullptr;
	//VertexBuffer* m_debugVbo = nullptr;
	//IndexBuffer* m_ibo = nullptr;
	//IndexBuffer* m_debugIbo = nullptr;
	bool useManagerTest = false;

	Vec3 m_voxelDimensions = Vec3(1.f, 1.f, 1.f);
};