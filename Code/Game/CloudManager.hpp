#pragma once
#include "Game/Cloud.hpp"
#include "Game/Weather.hpp"
#include <memory>
#include "Game/Octree.hpp"

class Game;

class CloudManager
{
public:
	int m_maxClouds = 200;
	std::vector<Cloud> m_clouds;
	std::vector<CloudGPU> m_cloudsGPU;
public:
	CloudManager(Game* game, int maxClouds = 50);
	~CloudManager();

	void Shutdown();

	//void Initialize();
	//std::unique_ptr<Cloud>& CreateCloud();
	void BeginFrame();
	bool CreateTest();
	bool CreateCloud(Vec3 location, IntVec3 dimesnions, float noiseScale, float threshold);
	void RemoveCloud(Cloud* cloud);
	void UpdateClouds(float deltaSeconds, const Weather&weather);
	void HandleInput(float deltaSeconds);
	void RunCompute() const;
	void RunCloudCompute() const;
	void RunShadowCompute() const;
	void PrepareForRender();
	void RenderClouds() const;
	void RenderShadows() const;
	void DebugRenderClouds() const;

	//void BuildOctrees();
	void SerializeOctreesToGPU(std::vector<OctreeNodeGPU>& gpuNodes, std::vector<Voxel>& gpuVoxels) const;
	void SerializePositionOctreesToGPU(std::vector<OctreeNodeGPU>& gpuNodes, std::vector<Vec3>& gpuPositions) const;
	//void SerializeCloudsToGPU(std::vector<CloudGPU>& gpuClouds, std::vector<Cloud>) const;

	const std::vector<Cloud>& GetClouds() const { return m_clouds; }
	//void SetGlobalRenderState() const;

	void InitializeNoiseTexture(int width, int height, int depth, float frequency, int octaves);
	void InitializeWorleyTexture(int width, int height, int depth, int cellsize, unsigned int seed);
	void BindNoiseTexture() const;
	//const NoiseTexture* GetNoiseTexture() const { return m_noiseTexture; }

	Texture3D* GetNoiseTexture3D() const { return m_noiseTexture; }
	Texture3D* GetWorleyTexture3D() const { return m_worleyTexture; }


public:
	Texture* m_outShadowTexture = nullptr;

private:
	//NoiseTexture* m_noiseTexture = nullptr;
	//Shader* m_cloudShader = nullptr;
	//std::vector<float> m_noiseTexture3D;
	VertexBuffer* m_debugVoxelBuffer = nullptr;
	IndexBuffer* m_debugIndexBuffer = nullptr;

	VertexBuffer* m_debugWireframeVBO = nullptr;
	IndexBuffer* m_debugWireframeIBO = nullptr;

	StructuredBuffer* m_inCloudBuffer = nullptr;
	StructuredBuffer* m_inVoxelBuffer = nullptr;
	StructuredBuffer* m_inVoxelPositionBuffer = nullptr;

	StructuredBuffer* m_cloudOctreeBuffer = nullptr;
	StructuredBuffer* m_voxelOctreeBuffer = nullptr;

	Texture* m_outCloudTexture = nullptr;
	
	Texture3D* m_noiseTexture = nullptr;
	Texture3D* m_worleyTexture = nullptr;
	Shader* m_voxelShader = nullptr;
	Shader* m_cloudShader = nullptr;
	Shader* m_cloudDebugShader = nullptr;
	ComputeShader* m_cloudComputeShader = nullptr;
	ComputeShader* m_cloudShadowShader = nullptr;

	ComputeShader* m_cloudReshapedShader = nullptr;

	Game* m_game = nullptr;

	CloudConstants m_cloudConstants;

	std::vector<Voxel> m_allVoxels;
	std::vector<float> m_allDensities;

	//one octree per cloud
	std::vector<std::unique_ptr<Octree<Voxel>>> m_voxelOctrees;
	std::vector<std::unique_ptr<Octree<Vec3>>> m_voxelPositionOctrees;
	//std::vector<OctreeNodeGPU> m_gpuNodes;

	int demonstration = 0;
	bool useTest = false;
	bool m_needsRebuild = true;

	Vec3 m_voxelDimensions = Vec3(20.f);
};