#include "Cloud.hpp"

#include "Engine/Math/RandomNumberGenerator.hpp"
#include "ThirdParty/Engine_Code_ThirdParty_Squirrel/SmoothNoise.hpp"
#include "Engine/Math/CurveUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/StructuredBuffer.hpp"
#include "Game/Perlin3D.hpp"
#include "Game/Weather.hpp"

Cloud::Cloud()
{
	m_gridDimensions = IntVec3(64, 64, 64);
	m_densityField.resize(m_gridDimensions.x * m_gridDimensions.y * m_gridDimensions.z, 0.0f);
}

Cloud::Cloud(Vec3 location, IntVec3 dimensions, Vec3 voxelDimensions, ECloudType type, bool UseTest)
{
	useManagerTest = UseTest;

	if(type != ECloudType::CLOUD_TEST)
	{

		m_center = location;
		m_gridDimensions = dimensions;
		m_voxelDimensions = voxelDimensions;
		m_densityField.resize(dimensions.x * dimensions.y * dimensions.z, 0.0f);
		GenerateCloud();
	}
	else
	{
		m_center = location;
		m_gridDimensions = dimensions;
		m_voxelDimensions = voxelDimensions;
		m_densityField.resize(dimensions.x * dimensions.y * dimensions.z, 0.1f);
		GenerateTestCloud();
	}
}

Cloud::~Cloud()
{

}

//void Cloud::Initialize(const Vec3& position, const Vec3& size)
//{
//	m_center = position;
//	m_size = size;
//
//	//m_voxels.clear();
//
//	//RandomNumberGenerator rng;
//	//rng.SetSeed((unsigned int)GetCurrentTimeSeconds());
//
//	//for (int i = 0; i < 20; i++)
//	//{
//	//	Vec3 randomPosition = Vec3(rng.RollRandomFloatInRange(0.f, size.x), rng.RollRandomFloatInRange(0.f, size.y), rng.RollRandomFloatInRange(0.f, size.z));
//	//	float density = rng.RollRandomFloatInRange(0.5f, 1.f);
//	//
//	//	Voxel voxel = Voxel(randomPosition, density);
//	//	m_voxels.push_back(voxel);
//	//}
//
//	BuildVerts();
//}

void Cloud::GenerateCloud()
{
	int width = m_gridDimensions.x;
	int height = m_gridDimensions.y;
	int depth = m_gridDimensions.z;

	float noiseScale = 0.01f;
	float noiseThreshold = 0.0f;

	GenerateDensityField(width, height, depth, noiseScale, noiseThreshold);

	std::vector<Vertex_PCU> vertices;
	std::vector<unsigned int> indices;

	//for (int z = 0; z < depth; ++z)
	//{
	//	for (int y = 0; y < height; ++y)
	//	{
	//		for (int x = 0; x < width; ++x)
	//		{
	//			int index = x + y * width + z * width * height;
	//			float noiseValue = m_densityField[index];
	//
	//			//if (noiseValue > noiseThreshold)
	//			//{
	//				Voxel voxel;
	//				voxel.m_position = Vec3((float)x, (float)y, (float)z);
	//				voxel.m_density = noiseValue;
	//				//voxel.m_color = Rgba8(220, 220, 220);
	//				m_voxels.push_back(voxel);
	//			//}
	//		}
	//	}
	//}

	//BuildVerts();
}

void Cloud::GenerateTestCloud()
{
	int width = m_gridDimensions.x;
	int height = m_gridDimensions.y;
	int depth = m_gridDimensions.z;

	float noiseScale = 1.f;
	float noiseThreshold = 0.0f;
	//float noiseThreshold2 = 0.3f;

	GenerateDensityField(width, height, depth, noiseScale, noiseThreshold);

	std::vector<Vertex_PCU> vertices;
	std::vector<unsigned int> indices;

	//Voxel voxel;
	//voxel.m_position = Vec3((float)0, (float)0, (float)0) + m_center;
	//voxel.m_density = .02f;
	////voxel.m_color = Rgba8(220, 220, 220);
	//m_voxels.push_back(voxel);

 	//float distanceScale = 1.0f;

	//float offset = 0.f;

	//for (int z = 0; z < m_gridDimensions.z; z++)
	//{
	//	for (int y = 0; y < m_gridDimensions.y; y++)
	//	{
	//		for (int x = 0; x < m_gridDimensions.x; x++)
	//		{
	//			if (useManagerTest == true)
	//			{
	//
	//
	//				int index = (x + y * width + z * width * height);
	//
	//				//if(index > m_densityField.size() - 1) continue;
	//
	//				float noiseValue = m_densityField[index];
	//
	//				//if (noiseValue > noiseThreshold && noiseValue < noiseThreshold2)
	//				//{
	//					Voxel voxel;
	//					voxel.m_position = (Vec3((float)x, (float)y, (float)z) * distanceScale) + m_center; //+ offset;
	//					voxel.m_density = noiseValue;
	//					//voxel.m_density = 10.f;
	//					//voxel.m_color = Rgba8(220, 220, 220);
	//					m_voxels.push_back(voxel);
	//				//}
	//			}
	//			else
	//			{
	//				Voxel voxel;
	//				voxel.m_position = (Vec3((float)x, (float)y, (float)z) * distanceScale) + m_center; //+ offset;
	//				voxel.m_density = 10.f;
	//				//voxel.m_color = Rgba8(220, 220, 220);
	//				m_voxels.push_back(voxel);
	//			}
	//		}
	//	}
	//}

	//2BuildVerts();
}

void Cloud::GenerateDensityField(int width, int height, int depth, float noiseScale, float noiseThreshold)
{
	UNUSED(noiseScale);
	m_densityField.clear();
	m_densityField.reserve(width * height * depth);

	m_voxelPositions.clear();
	m_voxelPositions.reserve(width * height * depth);

	m_voxels.clear();
	m_voxels.reserve(width * height * depth);

	//float scale = 1.f;

	//for (int z = 0; z < depth; ++z) {
	//	for (int y = 0; y < height; ++y) {
	//		for (int x = 0; x < width; ++x) {
	//			//float normalizedX = (float)x / (float)width;
	//			//float normalizedY = (float)y / (float)height;
	//			//float normalizedZ = (float)z / (float)depth;
	//
	//			float finalX = x * m_voxelDimensions.x + m_center.x;
	//			float finalY = y * m_voxelDimensions.y + m_center.y;
	//			float finalZ = z * m_voxelDimensions.z + m_center.z;
	//
	//			if (useManagerTest == true)
	//			{
	//				float noiseValue = fabs(Compute3dPerlinNoise(finalX, finalY, finalZ, noiseScale, 5)) * 2.f;
	//				if (noiseValue > noiseThreshold) {
	//					//int index = x + y * width + z * width * height;
	//					m_densityField.push_back(noiseValue); // Store density value at the given point
	//					m_voxelPositions.push_back(Vec3((float)finalX, (float)finalY, (float)finalZ));
	//
	//					m_voxels.push_back(Voxel(Vec3((float)finalX, (float)finalY, (float)finalZ), noiseValue));
	//				}
	//			}
	//			else
	//			{
	//				float testDensity = 2.f;
	//
	//				//int index = x + y * width + z * width * height;
	//				m_densityField.push_back(testDensity); // Store density value at the given point
	//				m_voxelPositions.push_back(Vec3((float)finalX, (float)finalY, (float)finalZ));
	//
	//				m_voxels.push_back(Voxel(Vec3((float)finalX, (float)finalY, (float)finalZ), testDensity));
	//			}
	//		}
	//	}
	//}

	for (int z = 0; z < depth; ++z) {
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				//float normalizedX = (float)x / (float)width;
				//float normalizedY = (float)y / (float)height;
				//float normalizedZ = (float)z / (float)depth;

				float finalX = x * m_voxelDimensions.x + m_center.x;
				float finalY = y * m_voxelDimensions.y + m_center.y;
				float finalZ = z * m_voxelDimensions.z + m_center.z;

				if (useManagerTest == true)
				{
					float baseNoiseValue = fabs(Compute3dWorleyNoise(finalX, finalY, finalZ, 1.5f, 0));
					//float perlinNoiseValue = Compute3dPerlinNoise(finalX, finalY, finalZ, 10.f, 3, 0.5f, 2.f, true, 0);

					//float blend = 0.5f;

					//float blendedNoise = Lerp(baseNoiseValue, perlinNoiseValue, blend);

					float rangeMapNoise = RangeMap(baseNoiseValue, 0.2f, 0.8f, 0.0f, 1.0f);

					float clampedNoise = GetClampedZeroToOne(rangeMapNoise);

					float noiseValue = pow(clampedNoise, 1.2f) * 2.0f;


					if (noiseValue > noiseThreshold) {
						//int index = x + y * width + z * width * height;
						m_densityField.push_back(noiseValue); // Store density value at the given point
						m_voxelPositions.push_back(Vec3((float)finalX, (float)finalY, (float)finalZ));

						m_voxels.push_back(Voxel(Vec3((float)finalX, (float)finalY, (float)finalZ), noiseValue));
					}
					m_densityField.push_back(0.f);
					m_voxels.push_back(Voxel(Vec3((float)finalX, (float)finalY, (float)finalZ), noiseValue));
				}
				else
				{
					float testDensity = 1.f;

					//int index = x + y * width + z * width * height;
					m_densityField.push_back(testDensity); // Store density value at the given point
					m_voxelPositions.push_back(Vec3((float)finalX, (float)finalY, (float)finalZ));

					m_voxels.push_back(Voxel(Vec3((float)finalX, (float)finalY, (float)finalZ), testDensity));
				}
			}
		}
	}
}



void Cloud::Update(float deltaSeconds, const Weather& weather)
{
	UNUSED(deltaSeconds);
	UNUSED(weather);

	Vec3 mins = Vec3(m_center - m_voxelDimensions * .5f);
	Vec3 maxs = Vec3(m_center + m_voxelDimensions * .5f);

	//I am going to loop through all the voxels and get the minimum and maximum values for abounding box
	//for (int i = 0; i < m_voxels.size(); i++)
	//{
	//	Vec3 voxelMin = m_voxels[i].m_position - .5f;
	//	Vec3 voxelMax = m_voxels[i].m_position + .5f;
	//
	//	mins = GetMin(mins, voxelMin);
	//	maxs = GetMax(maxs, voxelMax);
	//}

	for (int i = 0; i < m_voxelPositions.size(); i++)
	{
		//if (m_densityField[i] < 0.5f)
		//{
		//	continue;
		//}
		Vec3 voxelMin = m_voxelPositions[i] - m_voxelDimensions * .5f;
		Vec3 voxelMax = m_voxelPositions[i] + m_voxelDimensions * .5f;

		mins = GetMin(mins, voxelMin);
		maxs = GetMax(maxs, voxelMax);
	}

	boundingBox = AABB3(mins, maxs);
	//Vec3 wind = weather.m_windDirection * weather.m_windSpeed * deltaSeconds;
	//m_center += wind;
}

CloudGPU Cloud::GetCloudGPU(unsigned int& voxelOffset, unsigned int& densityOffset) const
{
	UNUSED(densityOffset);
	CloudGPU cloudGPU;

	cloudGPU.center = m_center;
	cloudGPU.gridDimensions = m_gridDimensions;
	cloudGPU.minBounds = boundingBox.m_mins;
	cloudGPU.maxBounds = boundingBox.m_maxs;

	cloudGPU.voxelOffset = voxelOffset;
	cloudGPU.voxelCount = (unsigned int)m_voxels.size();

	voxelOffset += cloudGPU.voxelCount;

	cloudGPU.octreeIndex = m_octreeIndex;
	//cloudGPU.densityOffset = densityOffset;
	//cloudGPU.densityCount = (unsigned int)m_densityField.size();

	//densityOffset += cloudGPU.densityCount;

	return cloudGPU;
}

AABB3 Cloud::GetBounds() const
{
	return boundingBox;
}

std::vector<Voxel*> Cloud::GetVoxels() const
{
	std::vector<Voxel*> voxelPtrs;
	for (const Voxel& voxel : m_voxels) {
		voxelPtrs.push_back(const_cast<Voxel*>(&voxel)); // Return pointers to the cloud's voxels
	}
	return voxelPtrs;
}

std::vector<float> Cloud::GetDensityField() const
{
	return m_densityField;
}

bool Cloud::NeedsRebuild() const
{
	return m_structureChanged;
}

//void  Cloud::BuildVerts()
//{
//	IndexedVertexBufferData data;
//
//	data.m_WOVertices.reserve(1000);
//	data.m_indices.reserve(1000);
//	//vertices.reserve
//	//indices.reserve(m_voxels.size());
//
//
//
//	for (int x = 0; x < m_gridDimensions.x; ++x) {
//		for (int y = 0; y < m_gridDimensions.y; ++y) {
//			for (int z = 0; z < m_gridDimensions.z; ++z) {
//				// If voxel is part of the cloud (density check)
//				if (IsVoxelActive(IntVec3(x, y, z))) {
//
//					Vec3 voxelPosition = Vec3((float)x, (float)y, (float)z);
//					Vec3 adjustedPosition = voxelPosition - Vec3(m_gridDimensions.x * 0.5f, m_gridDimensions.y * 0.5f, m_gridDimensions.z * 0.5f) + m_center;
//					// Create vertices for this voxel, calculate positions
//					AddVertsForAABB3D(data.m_WOVertices, data.m_indices, AABB3(adjustedPosition, 1.f), Rgba8Interpolate(Rgba8(255, 255, 255, 0), Rgba8::WHITE, m_densityField[x + y * m_gridDimensions.x + z * m_gridDimensions.x * m_gridDimensions.y]));
//
//				}
//			}
//		}
//	}
//
//	return data;
//	//int numberofIndices = (int)indices.size();
//
//	//if (vertices.size() > 0)
//	//{
//	//	m_vbo = g_theRenderer->CreateVertexBuffer(vertices.size(), VertexType::VERTEX_PCU);
//	//	m_ibo = g_theRenderer->CreateIndexBuffer(indices.size());
//	//
//	//	g_theRenderer->CopyCPUToGPU(vertices.data(), vertices.size() * sizeof(Vertex_PCU), m_vbo);
//	//	g_theRenderer->CopyCPUToGPU(indices.data(), indices.size() * sizeof(unsigned int), m_ibo);
//	//}
//	//
//	//if (m_debug)
//	//{
//	//	BuildDebugVerts();
//	//}
//}
//
//IndexedVertexBufferData Cloud::BuildDebugVerts()
//{
//	IndexedVertexBufferData debugData;
//
//	debugData.m_WOVertices.reserve(1000);
//	debugData.m_indices.reserve(1000);
//
//	AddVertsForAABB3D(debugData.m_WOVertices, debugData.m_indices, AABB3(m_center, (float)m_gridDimensions.x, (float)m_gridDimensions.y, (float)m_gridDimensions.z), g_theRandom->RollRandomColorInRangeFixedAlpha(255, 255, 255, 100));
//
//	for (int x = 0; x < m_gridDimensions.x; ++x) {
//		for (int y = 0; y < m_gridDimensions.y; ++y) {
//			for (int z = 0; z < m_gridDimensions.z; ++z) {
//				//add every box regardless of it being active or not
//				Vec3 voxelPosition = Vec3((float)x, (float)y, (float)z);
//				Vec3 adjustedPosition = voxelPosition - Vec3(m_gridDimensions.x * 0.5f, m_gridDimensions.y * 0.5f, m_gridDimensions.z * 0.5f) + m_center;
//				// Create vertices for this voxel, calculate positions
//
//				AddVertsForAABB3D(debugData.m_WOVertices, debugData.m_indices, AABB3(adjustedPosition, 1.f), Rgba8::WHITE);
//
//			}
//		}
//	}
//
//	return debugData;
//
//	//for (int x = 0; x < m_gridDimensions.x; ++x) {
//	//	for (int y = 0; y < m_gridDimensions.y; ++y) {
//	//		for (int z = 0; z < m_gridDimensions.z; ++z) {
//	//			// If voxel is part of the cloud (density check)
//	//			if (IsVoxelActive(IntVec3(x, y, z))) {
//	//
//	//				Vec3 voxelPosition = Vec3(x, y, z);
//	//				Vec3 adjustedPosition = voxelPosition - Vec3(m_gridDimensions.x * 0.5f, m_gridDimensions.y * 0.5f, m_gridDimensions.z * 0.5f) + m_center;
//	//				// Create vertices for this voxel, calculate positions
//	//				AddVertsForAABB3D(m_debugVerts, AABB3(adjustedPosition, 1.f), Rgba8::WHITE);
//	//
//	//			}
//	//		}
//	//	}
//	//}
//}

//void Cloud::DebugRender() const 
//{
//	if (m_debugVbo)
//	{
//		g_theRenderer->BindTexture(nullptr);
//		g_theRenderer->BindShader(nullptr);
//		g_theRenderer->DrawIndexedBuffer(m_debugVbo, m_debugIbo);
//	}
//}

//void Cloud::AddVoxelVertices(IntVec3 location)
//{
//	Vec3 voxelPosition = location;
//	float density = m_densityField[location.x + location.y * m_gridDimensions.x + location.z * m_gridDimensions.x * m_gridDimensions.y];
//}

bool Cloud::IsVoxelActive(IntVec3 location) const
{
	int index = location.x + location.y * m_gridDimensions.x + location.z * m_gridDimensions.x * m_gridDimensions.y;
	
	return m_densityField[index] > 0.5f;
}
