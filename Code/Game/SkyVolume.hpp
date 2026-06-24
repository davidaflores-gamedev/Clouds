#pragma once

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include <vector>

class Game;
class Texture3D;
class StructuredBuffer;

enum class CloudType
{
	CUMULUS,
	STRATUS,
	CIRRUS,
	CUMULONIMBUS
};

struct CloudRegion
{
	int id = 0;
	Vec3 center = Vec3(0, 100, 0);
	Vec3 radii = Vec3(50, 30, 50);
	CloudType type = CloudType::CUMULUS;
	float densityScale = 1.0f;
	float noiseScale = 1.0f;
	Vec3 windOffset = Vec3::ZERO;
	float turbulence = 0.5f;

	float EvaluateDensity(const Vec3& worldPos) const;
};

struct CloudRegionGPU {
	Vec3 center;
	float type;  // CloudType as float
	Vec3 radii;
	float densityScale;
	Vec3 windOffset;
	float noiseScale;
};

struct SkyVolumeConstants {
	Mat44 invViewProj;

	Mat44 shadowViewMatrix;
	Mat44 shadowProjectionMatrix;

	Vec3 cameraPosition;
	float time;

	Vec3 sunDirection;
	float sunIntensity = 1.5f;;

	Vec3 lightColor;
	float ambientIntensity = 0.6f;

	Vec3 skyBoundsMin;
	float stepSize = .59f;

	Vec3 skyBoundsMax;
	int maxSteps = 720;

	float densityScale = 2.0f;
	float densityMultiplier = 2.0f; // From your old tuned value
	float densityThreshold = 0.01f;
	float densityFalloff = 0.11f;

	float noiseScale = 0.01f;
	float noiseLerpVal = 0.5f;
	float noisePowVal = 3.7f; // From your old tuned value
	float densityNoiseLerpVal = 0.92f;

	float minWorleyValue = 0.32f;
	float scrollFactor = 0.5f;
	int scrolling = 1;
	int useNoise = 1;

	float minStepSize = 0.01f;
	float farDistanceThreshold = 50.0f;
	float farMultiplier = 2.8f;
	float cloudVoxelDistanceLerpVal = 0.87f;

	float extinctionCoefficient = 2.5f; // From your old tuned value
	float scatteringCoefficient = 1.22f;
	float lightAbsorption = 0.5f;
	float powderBias = 4.5f; // From your old tuned value

	float anisotropyG = .54f; // From your old tuned value
	float shadowFactorMin = 0.93f; // From your old tuned value
	float minAccepted = 0.5f;
	int useShadowMap = 1; // 1 = on, 0 = off

	int showBoundingBoxes;
	int currentDepth;
	int useDensity;
	int invertNoise;
};

struct DebugConstantsGPU
{
	int debugMode;
	float debugValue;
	IntVec2 debugPixel;

	int currentDepth = 0;
	Vec3 buffer;
};

//enum class SkyDebugMode
//{
//	NORMAL,
//	RAYSTEPS,
//	DENSITY,
//	OCTREE,
//	COUNT
//};

struct DebugSettings
{
	bool showOctreeNodes = false;
	bool showDensityField = false;
	bool showRaySteps = false;
	bool showCloudBounds = true;
	bool showPerformanceStats = true;

	// Octree visualization
	int octreeLevelToShow = -1;  // -1 for all levels
	bool showEmptyNodes = false;
	bool showNodeIndices = false;

	// Density visualization  
	float densitySliceZ = 0.5f;  // Z position for 2D slice
	bool show3DDensity = false;
	float densityThreshold = 0.01f;

	// Ray debugging
	int debugPixelX = -1;  // Specific pixel to debug
	int debugPixelY = -1;
	Vec2 debugScreenPos = Vec2(0.5f, 0.5f); // Or use normalized coords

	// Performance
	bool measureGPUTime = false;
	int frameTimeHistorySize = 60;

	int currentDebugMode = 0;
	float debugValue;

};

struct PerformanceStats {
	// Timing
	float cpuFrameTime = 0.0f;
	float gpuRaymarchTime = 0.0f;
	float densityGenTime = 0.0f;
	float fps = 0.0f;

	// Counts
	int octreeNodeCount = 0;
	int leafNodeCount = 0;
	int nonEmptyNodes = 0;
	int densityTexelCount = 0;
	int nonZeroTexels = 0;

	// Raymarching stats (from GPU readback)
	float avgRaySteps = 0.0f;
	float maxRaySteps = 0.0f;
	int totalRaysPerFrame = 0;

	// Memory usage
	size_t octreeMemoryMB = 0;
	size_t textureMemoryMB = 0;

	std::vector<float> frameTimeHistory;

	void UpdateFrameTime(float deltaSeconds) {
		cpuFrameTime = deltaSeconds * 1000.0f; // to ms
		frameTimeHistory.push_back(cpuFrameTime);
		if (frameTimeHistory.size() > 60) {
			frameTimeHistory.erase(frameTimeHistory.begin());
		}
	}

	float GetAverageFrameTime() const {
		if (frameTimeHistory.empty()) return 0.0f;
		float sum = 0.0f;
		for (float t : frameTimeHistory) sum += t;
		return sum / frameTimeHistory.size();
	}
};

class SkyVolume {
public:
	SkyVolume();
	~SkyVolume();

	// Core functions for Day 1
	void Initialize();
	void Shutdown();
	void Update(float deltaSeconds);
	void Render();
	void RenderShadowPass();
	void RenderClouds();
	void RenderImGuiPanel();

	// Cloud management
	void AddCloudRegion(const CloudRegion& region);
	void RebuildAll();

	// Debug
	int GetNodeCount() const { return (int)m_octreeNodes.size(); }
	int GetRegionCount() const { return (int)m_cloudRegions.size(); }
	void ToggleDebugVisualization() { m_debugVisualization = !m_debugVisualization; }

	void DrawDebugVisualization();
	void DrawOctreeDebug();
	void DrawDensitySlice();
	void DrawRayDebug();
	void DrawPerformanceOverlay();
	void UpdatePerformanceStats();
	void DrawFrameTimeGraph(Vec2 pos, Vec2 size);

	// Debug state
	DebugSettings m_debugSettings;
	PerformanceStats m_perfStats;

	SkyVolumeConstants m_constants;
	DebugConstantsGPU m_debugConstants;

	Game* m_theGame;
	Texture* m_shadowTexture = nullptr;
private:
	// Simplified octree node for Day 1
	struct SkyOctreeNode {
		AABB3 bounds;
		float averageDensity = 0.0f;
		float maxDensity = 0.0f;
		int childrenIndex = -1;      // -1 if leaf
		int densityDataIndex = -1;   // Index into density array if leaf
	};

	struct SkyOctreeNodeGPU
	{
		Vec3 boundsMin;
		int padding1;  // Ensure 16-byte alignment

		Vec3 boundsMax;
		int padding2;  // Ensure 16-byte alignment

		float averageDensity;
		float maxDensity;

		int childrenIndex;
		int densityIndex;
	};

	// Debug GPU resources
	StructuredBuffer* m_debugRayBuffer = nullptr;  // Store ray steps
	Texture* m_debugOutputTexture = nullptr;       // Visualize specific data

	// For single-ray debugging
	struct RayDebugInfo {
		Vec3 origin;
		Vec3 direction;
		std::vector<Vec3> samplePoints;
		std::vector<float> densityValues;
		std::vector<int> octreeNodes;
		int totalSteps = 0;
		float totalDensity = 0.0f;
	};

	RayDebugInfo m_lastDebugRay;

	// Core data
	std::vector<SkyOctreeNode> m_octreeNodes;
	std::vector<float> m_densityData;
	std::vector<CloudRegion> m_cloudRegions;

	// World configuration
	AABB3 m_worldBounds;
	int m_octreeDepth = 5;  // Start simple
	IntVec3 m_textureResolution = IntVec3(256, 256, 128);  // Smaller for testing

	ComputeShader* m_cloudVolumeShader = nullptr;
	ComputeShader* m_shadowShader = nullptr;
	ComputeShader* m_sliceExtractShader = nullptr;

	Texture* m_outputTexture = nullptr;
	Texture* m_debugSliceTexture = nullptr;


	// GPU resources (Day 1: just allocate, don't use yet)
	StructuredBuffer* m_octreeBuffer = nullptr;
	ConstantBuffer* m_constantBuffer = nullptr;
	ConstantBuffer* m_debugConstantBuffer = nullptr;

	Texture3D* m_densityTexture = nullptr;
	Texture3D* m_noiseTexture = nullptr;
	Texture3D* m_worleyTexture = nullptr;


	// Settings
	bool m_debugVisualization = false;
	int m_nextCloudId = 0;

	// Internal methods for Day 1
	void BuildOctree();
	bool ShouldSubdivide(const SkyOctreeNode& node);
	void GenerateDensityField();
	void AddDefaultClouds();
	float SampleOctreeDensity(const Vec3& worldPos) const;

	void CreateGPUResources();
	void UpdateGPUBuffers();
	void GenerateDensityTexture();
	void UploadDensityTexture(const std::vector<float>& textureData);
	Vec3 TextureCoordToWorld(const Vec3& texCoord) const;
	Vec3 WorldToTextureCoord(const Vec3& worldPos) const;

	void InitializeNoiseTexture(int width, int height, int depth, float frequency, int octaves);
	void InitializeWorleyTexture(int width, int height, int depth, int cellsize, unsigned int seed);
};
