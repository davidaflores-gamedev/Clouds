#include "Game/CloudManager.hpp"
#include "Game/Perlin3D.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/StructuredBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Game/Game.hpp"
#include "Game/Player.hpp"
#include "Engine/SaveUtils.hpp"
#include "ThirdParty/Engine_Code_ThirdParty_Squirrel/SmoothNoise.hpp"

#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"

CloudManager::CloudManager(Game* game, int maxClouds)
	: m_maxClouds(maxClouds) 
	, m_game(game)
{
	// Reserve space for efficiency 
	InitializeNoiseTexture(256, 256, 256, 1.f, 7);
	InitializeWorleyTexture(256, 256, 256, 32, 4);

	//Save3DTextureToDisk("Data/Textures/WorleyNoise", m_worleyTexture->)

	m_cloudShader = g_theRenderer->CreateOrGetShader("Data/Shaders/Default3D", VertexType::VERTEX_PCU3D);
	m_cloudDebugShader = g_theRenderer->CreateOrGetShader("Data/Shaders/BaseShader", VertexType::VERTEX_PCU);
	//m_voxelShader = g_theRenderer->CreateOrGetShader("Data/Shaders/VoxelShader", VertexType::VOXEL_CLOUDS);
	m_cloudComputeShader = g_theRenderer->CreateOrGetComputeShader("Data/Shaders/CloudShader", VertexType::VOXEL_CLOUDS);
	m_cloudShadowShader = g_theRenderer->CreateOrGetComputeShader("Data/Shaders/CloudShadowShader", VertexType::VOXEL_CLOUDS);
	m_cloudReshapedShader = g_theRenderer->CreateOrGetComputeShader("Data/Shaders/CloudReshapedShader", VertexType::VOXEL_CLOUDS);
	m_outCloudTexture = g_theRenderer->CreateEmptyTextureWithUAV("OutCloudTexture", g_theWindow->GetClientDimensions());
	m_outShadowTexture = g_theRenderer->CreateEmptyTextureWithUAV("OutShadowTexture", g_theWindow->GetClientDimensions());
	m_inCloudBuffer = g_theRenderer->CreateStructuredBuffer(1, sizeof(Cloud), true);

	m_inVoxelBuffer = g_theRenderer->CreateStructuredBuffer(1, sizeof(Voxel), true);
	m_inVoxelPositionBuffer = g_theRenderer->CreateStructuredBuffer(1, sizeof(Vec3), true);


	m_cloudOctreeBuffer = g_theRenderer->CreateStructuredBuffer(1, sizeof(OctreeNodeGPU), true);
	m_voxelOctreeBuffer = g_theRenderer->CreateStructuredBuffer(1, sizeof(OctreeNodeGPU), true);
}	

CloudManager::~CloudManager()
{

	//delete m_cloudShader;
	//m_cloudShader = nullptr;
	m_clouds.clear();

	delete m_inCloudBuffer;
	m_inCloudBuffer = nullptr;

	delete m_inVoxelBuffer;
	m_inVoxelBuffer = nullptr;

	delete m_inVoxelPositionBuffer;
	m_inVoxelPositionBuffer = nullptr;

	delete m_cloudOctreeBuffer;
	m_cloudOctreeBuffer = nullptr;

	delete m_voxelOctreeBuffer;
	m_voxelOctreeBuffer = nullptr;

	delete m_debugVoxelBuffer;
	m_debugVoxelBuffer = nullptr;

	delete m_debugIndexBuffer;
	m_debugIndexBuffer = nullptr;

	delete m_debugWireframeVBO;
	m_debugWireframeVBO = nullptr;

	delete m_debugWireframeIBO;
	m_debugWireframeIBO = nullptr;
}

void CloudManager::Shutdown()
{

	//delete m_cloudShader;
	//m_cloudShader = nullptr;

	//m_clouds.clear();
}

void CloudManager::BeginFrame()
{
	//ImGui::SetNextWindowSize(ImVec2(410, 520));
	//ImGui::SetNextWindowPos(ImVec2(5, 5));

	static bool showProfiler = true;

	if (showProfiler)
	{

		if (ImGui::Begin("Profiler", &showProfiler, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
			ImGui::Text("Press N to toggle Debug Camera");
			ImGui::Text("Press F2 to toggle Scene");
			ImGui::Text("Press 5 to toggle Hi-Z Buffer View");
			ImGui::Text("Press 6 to toggle Hi-Z Buffer View Mode");
			ImGui::Text("Use ARROW keys to move Debug Camera");

			//static float	scatteringCoefficient = 1.22f;
			static float	extinctionCoefficient = 1.0f;
			static float	densityMultiplier = .37f;

			static float	minStepSize = 0.03f;
			static float	noiseScale = .8f;
			static float	noiseLerpVal = .5f;
			static float	densityNoiseLerpVal = .92f;

			static float	cloudVoxelDistanceLerpVal = .87f;
			static float	minWorleyValue = 0.32f;
			static float	noisePowVal = 3.7f;
			//static float	densitySkipThreshold = 0.0f;

			static float	scrollFactor = 0.5f;
			static float	farDistanceThreshold = 50.f;
			static float	farMultiplier = 2.8f;
			static float	shadowFactorMin = 0.93f;

			static float    powderBias = 0.9f;
			static float    anisotropy = 0.01f;
			static float    falloff = 0.5f;

			static float    rayIntensity = 1.06f;
			static float	rayDecay = .005f;

			static float	shadowCastMin = .95f;

			//ImGui::SliderFloat("Scattering Coefficient", &scatteringCoefficient, .1f, 5.f, "%.2f");
			//m_cloudConstants.scatteringCoefficient = scatteringCoefficient;

			ImGui::SliderFloat("Extinction Coefficient", &extinctionCoefficient, .1f, 5.f, "%.2f");
			m_cloudConstants.extinctionCoefficient = extinctionCoefficient;

			ImGui::SliderFloat("Density Multiplier", &densityMultiplier, 0.f, 3.f, "%.2f");
			m_cloudConstants.densityMultiplier = densityMultiplier;

			ImGui::SliderFloat("Min Step Size", &minStepSize, 0.01f, 1.f, "%.2f");
			m_cloudConstants.minStepSize = minStepSize;

			ImGui::SliderFloat("Noise Scale", &noiseScale, 0.001f, 3.f, "%.3f");
			m_cloudConstants.noiseScale = noiseScale;

			ImGui::SliderFloat("Noise Lerp Value", &noiseLerpVal, 0.f, 1.f, "%.2f");
			m_cloudConstants.noiseLerpVal = noiseLerpVal;

			ImGui::SliderFloat("Density Noise Lerp Value", &densityNoiseLerpVal, 0.f, 1.f, "%.2f");
			m_cloudConstants.densityNoiseLerpVal = densityNoiseLerpVal;

			ImGui::SliderFloat("Cloud Voxel Distance Lerp Value", &cloudVoxelDistanceLerpVal, 0.f, 1.f, "%.2f");
			m_cloudConstants.cloudVoxelDistanceLerpVal = cloudVoxelDistanceLerpVal;

			ImGui::SliderFloat("Min Worley Value", &minWorleyValue, 0.f, 1.f, "%.2f");
			m_cloudConstants.minWorleyValue = minWorleyValue;

			ImGui::SliderFloat("Noise Pow Value", &noisePowVal, 0.f, 5.f, "%.2f");
			m_cloudConstants.noisePowVal = noisePowVal;

			//ImGui::SliderFloat("Density Skip Threshold", &densitySkipThreshold, 0.f, 1.f, "%.2f");
			//m_cloudConstants.densitySkipThreshold = densitySkipThreshold;

			ImGui::SliderFloat("Scroll Factor", &scrollFactor, .0f, 3.f, "%.1f");
			m_cloudConstants.scrollFactor = scrollFactor;

			ImGui::SliderFloat("Far Distance Threshold", &farDistanceThreshold, 0.f, 200.f, "%.1f");
			m_cloudConstants.farDistanceThreshold = farDistanceThreshold;

			ImGui::SliderFloat("Far Multiplier", &farMultiplier, 0.f, 10.f, "%.1f");
			m_cloudConstants.farMultiplier = farMultiplier;

			ImGui::SliderFloat("Shadow Factor Min", &shadowFactorMin, 0.f, 1.f, "%.2f");
			m_cloudConstants.shadowFactorMin = shadowFactorMin;

			ImGui::SliderFloat("Powder Bias", &powderBias, 0.f, 10.f, "%.1f");
			m_cloudConstants.powderBias = powderBias;

			ImGui::SliderFloat("Anisotropy", &anisotropy, 0.f, 1.f, "%.2f");
			m_cloudConstants.anisotropy = anisotropy;

			ImGui::SliderFloat("Falloff", &falloff, 0.f, 5.f, "%.2f");
			m_cloudConstants.falloff = falloff;

			ImGui::SliderFloat("Ray Intensity", &rayIntensity, 0.f, 5.f, "%.2f");
			m_game->grc.intensity = rayIntensity;

			ImGui::SliderFloat("Ray Decay", &rayDecay, 0.f, .1f, "%.3f");
			m_game->grc.decay = rayDecay;

			ImGui::SliderFloat("Minimum Shadow Cast", &shadowCastMin, 0.f, 1.f, "%.2f");
			m_game->sc.minShadow = shadowCastMin;

			ImGui::PopStyleColor();
		}
		ImGui::End();
	}
}

bool CloudManager::CreateTest()
{
	//TODO: why is this function return a bool?
	if (m_clouds.size() >= m_maxClouds)
	{
		ERROR_AND_DIE("Max clouds reached");
	}
	else
	{
		int uniformscale = 1;
		float distance = 45.1f;
		float distanceX = 55.01f;
		float distanceY = 55.01f;
		float distanceZ = 10.f;
		int scaleX = 8;
		int scaleY = 8;
		int scaleZ = 1;


		int uniformScale  = 1;
		int uniformScaleZ = 4;
		int uniformScaleY = 5;
		int uniformScaleX = 7;




		
		m_clouds.reserve(scaleX * scaleY * scaleZ);

		//float offset = 0.001;

		for (int x = 0; x < uniformscale * scaleX; x++)
		{
			for (int y = 0; y < uniformscale * scaleY; y++)
			{
				for (int z = 0; z < uniformscale * scaleZ; z++)
				{
					// Generate a base height for this cloud
					float baseHeight = g_theRandom->RollRandomFloatInRange(200.f, 250.f);

					// Compute a base grid position
					Vec3 gridPos = Vec3((float)x * distanceX, (float)y * distanceY, (float)z * distanceZ);

					// Use noise to compute a jitter offset so clouds don't appear on a strict grid.
					// Adjust the multiplier (0.1f) to change the noise frequency, and jitterScale to change offset magnitude.
					float noiseFreq = 0.1f;
					float jitterScale = distance * 0.5f; // Adjust this to control how far clouds can stray from grid positions

					float noiseX = Compute3dPerlinNoise(x * noiseFreq, y * noiseFreq, z * noiseFreq, 10.f, 3, 0.5f, 2.f, true, 0);
					float noiseY = Compute3dPerlinNoise(x * noiseFreq + 100.f, y * noiseFreq + 100.f, z * noiseFreq + 100.f, 10.f, 3, 0.5f, 2.f, true, 0);
					float noiseZ = Compute3dPerlinNoise(x * noiseFreq + 200.f, y * noiseFreq + 200.f, z * noiseFreq + 200.f, 10.f, 3, 0.5f, 2.f, true, 0);

					// Remap noise from [0,1] to [-1,1] by subtracting 0.5 and scaling by 2
					float offsetX = (noiseX - 0.5f) * 2.f * jitterScale + 20.f;
					float offsetY = (noiseY - 0.5f) * 2.f * jitterScale + 20.f;
					float offsetZ = (noiseZ - 0.5f) * 2.f * jitterScale;

					// Final cloud position gets a noise-based jitter plus a vertical baseHeight offset
					Vec3 cloudPos = gridPos + Vec3(offsetX, offsetY, offsetZ) + Vec3(40.f, 40.f, baseHeight);

					// Optionally, also vary the voxel dimensions per cloud using noise.
					// Here we sample noise for each axis, remap it, and use it as a multiplier.
					float scaleNoiseFreq = 0.2f;
					float scaleNoiseX = 0.75f + Compute3dPerlinNoise(x * scaleNoiseFreq, y * scaleNoiseFreq, z * scaleNoiseFreq, 10.f, 3, 0.5f, 2.f, true, 0) * 0.5f;
					float scaleNoiseY = 0.75f + Compute3dPerlinNoise(x * scaleNoiseFreq + 10.f, y * scaleNoiseFreq + 10.f, z * scaleNoiseFreq + 10.f, 10.f, 3, 0.5f, 2.f, true, 0) * 0.5f;
					float scaleNoiseZ = 0.75f + Compute3dPerlinNoise(x * scaleNoiseFreq + 20.f, y * scaleNoiseFreq + 20.f, z * scaleNoiseFreq + 20.f, 10.f, 3, 0.5f, 2.f, true, 0) * 0.5f;

					int voxelX = int(uniformScale * uniformScaleX * scaleNoiseX);
					int voxelY = int(uniformScale * uniformScaleY * scaleNoiseY);
					int voxelZ = int(uniformScale * uniformScaleZ * scaleNoiseZ);

					// Create and update the cloud
					Cloud cloud = Cloud(cloudPos, IntVec3(voxelX, voxelY, voxelZ), m_voxelDimensions, ECloudType::CLOUD_TEST, useTest);
					cloud.Update(m_game->m_gameClock->GetDeltaSeoconds(), m_game->m_weather);

					m_clouds.push_back(cloud);
					m_voxelOctrees.push_back(std::make_unique<Octree<Voxel>>(cloud.boundingBox, DefaultGetDensity<Voxel>()));
				}
			}
		}
		return true;
	}
}

bool CloudManager::CreateCloud(Vec3 location, IntVec3 dimensions, float noiseScale, float threshold)
{
	UNUSED(noiseScale);
	UNUSED(threshold);

	if (m_clouds.size() >= m_maxClouds)
	{
		ERROR_AND_DIE("Max clouds reached");
	}
	else
	{
		// Create a new cloud object
		Cloud cloud = Cloud(location, dimensions, Vec3(1.f));
		cloud.GenerateCloud();

		m_clouds.push_back(cloud);

		//m_voxelOctrees.push_back(std::make_unique<Octree<Voxel>>(AABB3(location, Vec3(dimensions))));



		//m_voxelPositionOctrees.push_back(std::make_unique<Octree<Vec3>>(AABB3(location, Vec3(dimensions))));
	}
	return new Cloud();
}

void CloudManager::RemoveCloud(Cloud* cloud)
{
	UNUSED(cloud);
	//// Find the cloud in the collection
	//auto foundCloud = std::find_if(m_clouds.begin(), m_clouds.end(), [cloud](const std::unique_ptr<Cloud>& c) { return c.get() == cloud; });
	//
	//// If the cloud was found, remove it
	//if (foundCloud != m_clouds.end())
	//{
	//	m_clouds.erase(foundCloud);
	//}
}

void CloudManager::UpdateClouds(float deltaSeconds, const Weather& weather)
{
	delete m_debugVoxelBuffer;
	m_debugVoxelBuffer = nullptr;

	delete m_debugIndexBuffer;
	m_debugIndexBuffer = nullptr;

	delete m_debugWireframeVBO;
	m_debugWireframeVBO = nullptr;

	delete m_debugWireframeIBO;
	m_debugWireframeIBO = nullptr;
	
	HandleInput(deltaSeconds);

	if (m_needsRebuild)
	{
		m_cloudsGPU.clear();
		m_allVoxels.clear();
		//m_allVoxelPositions.clear();
		
		int octreeIndex = 0;
		

		for (int i = 0; i < m_clouds.size(); i++)
		{
			m_clouds[i].Update(deltaSeconds, weather);


			if (m_clouds[i].NeedsRebuild()) {
				m_clouds[i].m_octreeIndex = octreeIndex;
				m_voxelOctrees[i]->Build(m_clouds[i].GetVoxels(), m_voxelDimensions);



				//m_voxelPositionOctrees[i]->Build(m_clouds[i].GetVoxelPositions(), m_voxelDimensions);

				octreeIndex += (int)m_voxelOctrees[i]->GetAllChildrenSize();
				//octreeIndex += (int)m_voxelPositionOctrees[i]->GetAllChildrenSize();
			}
		}

#pragma region DebugRender

		std::vector<Vertex_PCU> verts;
		std::vector<unsigned int> indices;

		for (const Cloud& cloud : m_clouds)
		{
			for (const Voxel& voxel : cloud.m_voxels)
			{
				AddVertsForAABB3D(verts, indices, AABB3(voxel.m_position, 1.f), Rgba8::PURPLE);
			}

			//for (const Vec3& position : cloud.m_voxelPositions)
			//{
			//	AddVertsForAABB3D(verts, indices, AABB3(position, 1.f), Rgba8::PURPLE);
			//}
		}

		std::vector<Vertex_PCU> wireVerts;
		std::vector<unsigned int> wireIndices;

		for (const Cloud& cloud : m_clouds)
		{
			for (const Voxel& voxel : cloud.m_voxels)
			{
				AddVertsForAABB3D(wireVerts, wireIndices, AABB3(voxel.m_position, 1.f), Rgba8::GREEN);
			}

			//for (const Vec3& position : cloud.m_voxelPositions)
			//{
			//	AddVertsForAABB3D(wireVerts, wireIndices, AABB3(position, 1.f), Rgba8::GREEN);
			//}
		}

		//if (verts.size() > 0)
		//{
		//	m_debugVoxelBuffer = g_theRenderer->CreateVertexBuffer(verts.size(), sizeof(Vertex_PCU));
		//	m_debugIndexBuffer = g_theRenderer->CreateIndexBuffer(indices.size());
		//
		//	g_theRenderer->CopyCPUToGPU(verts.data(), verts.size() * sizeof(Vertex_PCU), m_debugVoxelBuffer);
		//	g_theRenderer->CopyCPUToGPU(indices.data(), indices.size() * sizeof(unsigned int), m_debugIndexBuffer);
		//}
		//
		//if (wireVerts.size() > 0)
		//{
		//	m_debugWireframeVBO = g_theRenderer->CreateVertexBuffer(wireVerts.size(), sizeof(Vertex_PCU));
		//	m_debugWireframeIBO = g_theRenderer->CreateIndexBuffer(wireIndices.size());
		//
		//	g_theRenderer->CopyCPUToGPU(wireVerts.data(), wireVerts.size() * sizeof(Vertex_PCU), m_debugWireframeVBO);
		//	g_theRenderer->CopyCPUToGPU(wireIndices.data(), wireIndices.size() * sizeof(unsigned int), m_debugWireframeIBO);
		//}

#pragma endregion

		delete m_inCloudBuffer;
		m_inCloudBuffer = nullptr;

		delete m_inVoxelBuffer;
		m_inVoxelBuffer = nullptr;

		delete m_inVoxelPositionBuffer;
		m_inVoxelPositionBuffer = nullptr;

		delete m_cloudOctreeBuffer;
		m_cloudOctreeBuffer = nullptr;

		delete m_voxelOctreeBuffer;
		m_voxelOctreeBuffer = nullptr;

		std::vector<OctreeNodeGPU> gpuVoxelNodes;
		std::vector<OctreeNodeGPU> gpuCloudNodes;
		std::vector<Voxel> gpuVoxels;
		std::vector<Vec3> gpuVoxelPositions;

		SerializeOctreesToGPU(gpuVoxelNodes, gpuVoxels);
		SerializePositionOctreesToGPU(gpuCloudNodes, gpuVoxelPositions);
		//SerializeCloudNodesToGPU(gpuCloudNodes, );

		unsigned int voxelOffset = 0;
		unsigned int densityOffset = 0;

		for (Cloud& cloud : m_clouds)
		{
			CloudGPU cloudGPU = cloud.GetCloudGPU(voxelOffset, densityOffset);


			m_allVoxels.insert(m_allVoxels.end(), cloud.m_voxels.begin(), cloud.m_voxels.end());

			//m_allVoxelPositions.insert(m_allVoxelPositions.end(), cloud.m_voxelPositions.begin(), cloud.m_voxelPositions.end());

			m_cloudsGPU.push_back(cloudGPU);
		}



		//m_cloudOctreeBuffer = g_theRenderer->CreateStructuredBuffer(m_voxelOctrees.size(), sizeof(Octree<Voxel>), true);
		//g_theRenderer->CopyCPUToGPU(m_voxelOctrees.data(), m_voxelOctrees.size(), m_cloudOctreeBuffer);

		float mindensity = FLT_MAX;
		float maxdensity = -FLT_MAX;

		for (int i = 0; i < gpuVoxels.size(); i++)
		{
			mindensity = min(mindensity, gpuVoxels[i].m_density);
			maxdensity = max(maxdensity, gpuVoxels[i].m_density);
		}		m_voxelOctreeBuffer = g_theRenderer->CreateStructuredBuffer(gpuVoxelNodes.size(), sizeof(OctreeNodeGPU), true);
		g_theRenderer->CopyCPUToGPU(gpuVoxelNodes.data(), gpuVoxelNodes.size(), m_voxelOctreeBuffer);

		m_inCloudBuffer = g_theRenderer->CreateStructuredBuffer(m_cloudsGPU.size(), sizeof(CloudGPU), true);
		g_theRenderer->CopyCPUToGPU(m_cloudsGPU.data(), m_cloudsGPU.size(), m_inCloudBuffer);

		//m_inVoxelBuffer = g_theRenderer->CreateStructuredBuffer(m_allVoxels.size(), sizeof(Voxel), true);
		//g_theRenderer->CopyCPUToGPU(m_allVoxels.data(), m_allVoxels.size(), m_inVoxelBuffer);

		m_inVoxelBuffer = g_theRenderer->CreateStructuredBuffer(gpuVoxels.size(), sizeof(Voxel), true);
		g_theRenderer->CopyCPUToGPU(gpuVoxels.data(), gpuVoxels.size(), m_inVoxelBuffer);

		//m_inVoxelPositionBuffer = g_theRenderer->CreateStructuredBuffer(gpuVoxelPositions.size(), sizeof(Vec3), true);
		//g_theRenderer->CopyCPUToGPU(gpuVoxelPositions.data(), gpuVoxelPositions.size(), m_inVoxelPositionBuffer);

		m_cloudConstants.numClouds = (int)m_cloudsGPU.size();
		m_cloudConstants.numOctrees = (int)gpuVoxelNodes.size();
		m_cloudConstants.VoxelDimensions = m_voxelDimensions;
		m_needsRebuild = false;
	}

	m_cloudConstants.timeElapsed = m_game->m_gameClock->GetTotalSeconds();

	LightConstants lightConstants;
	lightConstants = m_game->m_weather.m_lightConstants;

	g_theRenderer->SetCurrentCamera(PipelineStage::COMPUTE, m_game->m_player->m_playerCam);
	g_theRenderer->SetLightConstants(PipelineStage::COMPUTE, lightConstants);
	g_theRenderer->SetCloudConstants(PipelineStage::COMPUTE, m_cloudConstants);
	g_theRenderer->SetShadowConstants(PipelineStage::COMPUTE, m_game->sc);
}

void CloudManager::HandleInput(float deltaSeconds)
{
	UNUSED(deltaSeconds);

	if (g_theInputSystem->WasKeyJustPressed('1'))
	{
		m_cloudConstants.useDensity = abs(1 - m_cloudConstants.useDensity);

	}
	if (g_theInputSystem->WasKeyJustPressed('2'))
	{
		m_cloudConstants.useNoise = abs(1 - m_cloudConstants.useNoise);
	}

	if (g_theInputSystem->WasKeyJustPressed('3'))
	{
		m_cloudConstants.invertNoise = abs(1 - m_cloudConstants.invertNoise);
	}

	if (g_theInputSystem->WasKeyJustPressed('9'))
	{
		useTest = !useTest;
		m_clouds.clear();
		m_voxelOctrees.clear();
		CreateTest();
	}

	if (g_theInputSystem->WasKeyJustPressed('0'))
	{
		m_cloudConstants.scrolling = abs(1 - m_cloudConstants.scrolling);
	}

	if (g_theInputSystem->WasKeyJustPressed('N'))
	{
		m_cloudConstants.showBoundingBoxes = abs(1 - m_cloudConstants.showBoundingBoxes);
	}

	if (g_theInputSystem->WasKeyJustPressed(VK_OEM_COMMA))
	{
		if (m_cloudConstants.densityMultiplier > 0.0f)
		{
			m_cloudConstants.densityMultiplier -= 0.1f;
		}
	}

	if (g_theInputSystem->WasKeyJustPressed(VK_OEM_PERIOD))
	{
		if (m_cloudConstants.densityMultiplier < 1.0f)
		{
			m_cloudConstants.densityMultiplier += 0.1f;
		}
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_LEFT_MOUSE))
	{
		if (m_cloudConstants.currentDepth < MAX_OCTREE_DEPTH)
		{
			m_cloudConstants.currentDepth++;
		}
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE))
	{
		if (m_cloudConstants.currentDepth > 0)
		{
			m_cloudConstants.currentDepth--;
		}
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_RIGHT_BRACKET))
	{
		demonstration += 1;
	}

	if (g_theInputSystem->WasKeyJustPressed(KEYCODE_LEFT_BRACKET))
	{
		demonstration -= 1;
	}
}

void CloudManager::RunCloudCompute() const
{
	if (demonstration == 0)
	{
		g_theRenderer->BindComputeShader(m_cloudComputeShader);
	}
	else
	{
		g_theRenderer->BindComputeShader(m_cloudReshapedShader);

	}

	g_theRenderer->BindStructuredBufferToWrite(0, m_inCloudBuffer);
	g_theRenderer->BindStructuredBufferToWrite(1, m_inVoxelBuffer);
	//g_theRenderer->BindStructuredBufferToWrite(1, m_inVoxelPositionBuffer);
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_noiseTexture, 2);
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_worleyTexture, 3);
	g_theRenderer->BindStructuredBufferToWrite(4, m_voxelOctreeBuffer);
	g_theRenderer->BindTexture(PipelineStage::COMPUTE, m_outShadowTexture, 5);
	//g_theRenderer->BindStructuredBufferToWrite(4, m_cloudOctreeBuffer);

	g_theRenderer->BindTextureWithUAV(PipelineStage::COMPUTE, m_outCloudTexture);

	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);

	IntVec2 dimensions = g_theWindow->GetClientDimensions();

	//int threadsx = (dimensions.x + 15) / 16;
	//int threadsy = (dimensions.y + 15) / 16;

	int halfWidth = (dimensions.x + 1) / 2;
	int halfHeight = (dimensions.y + 1) / 2;


	//int threadsx = (halfWidth + 15) / 16;
	//int threadsy = (halfHeight + 15) / 16;b

	int threadsx = (halfWidth + 15) / 16;
	int threadsy = (halfHeight + 7) / 8;

	//int threadsx = (halfWidth + 31) / 32;
	//int threadsy = (halfHeight + 31) / 32;

	if (demonstration == 0)
		g_theRenderer->DispatchComputeJob(m_cloudComputeShader, threadsx, threadsy, 1);
	else
		g_theRenderer->DispatchComputeJob(m_cloudReshapedShader, threadsx, threadsy, 1);

	g_theRenderer->UnbindComputeShader();

	g_theRenderer->BindTexture();

	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);
}

void CloudManager::RunShadowCompute() const
{
	//Bind the shadow compute shader for drawing the shadow map
	g_theRenderer->BindComputeShader(m_cloudShadowShader);



	g_theRenderer->BindStructuredBufferToWrite(0, m_inCloudBuffer);
	g_theRenderer->BindStructuredBufferToWrite(1, m_inVoxelBuffer);
	//g_theRenderer->BindStructuredBufferToWrite(1, m_inVoxelPositionBuffer);
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_noiseTexture, 2);
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_worleyTexture, 3);
	g_theRenderer->BindStructuredBufferToWrite(4, m_voxelOctreeBuffer);
	//g_theRenderer->BindStructuredBufferToWrite(4, m_cloudOctreeBuffer);

	g_theRenderer->BindTextureWithUAV(PipelineStage::COMPUTE, m_outShadowTexture);

	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);

	IntVec2 dimensions = g_theWindow->GetClientDimensions();

	//int threadsx = (dimensions.x + 15) / 16;
	//int threadsy = (dimensions.y + 15) / 16;

	int halfWidth = (dimensions.x + 1) / 2;
	int halfHeight = (dimensions.y + 1) / 2;


	//int threadsx = (halfWidth + 15) / 16;
	//int threadsy = (halfHeight + 15) / 16;b

	int threadsx = (halfWidth + 15) / 16;
	int threadsy = (halfHeight + 7) / 8;

	//int threadsx = (halfWidth + 31) / 32;
	//int threadsy = (halfHeight + 31) / 32;

	if (demonstration == 0)
		g_theRenderer->DispatchComputeJob(m_cloudShadowShader, threadsx, threadsy, 1);
	else
		g_theRenderer->DispatchComputeJob(m_cloudShadowShader, threadsx, threadsy, 1);

	g_theRenderer->UnbindComputeShader();

	g_theRenderer->BindTexture();

	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);
}

void CloudManager::RenderClouds() const
{
	RunCloudCompute();

	g_theRenderer->SetBlendMode(BlendMode::ALPHA);

	g_theRenderer->BindShader(m_cloudDebugShader);

	g_theRenderer->BindShaderResources(m_outCloudTexture->GetShaderResourceView(), 0);

	g_theRenderer->DrawFullScreenQuad();

	g_theRenderer->UnbindComputeShader();
}

void CloudManager::RenderShadows() const
{
	RunShadowCompute();

	g_theRenderer->UnbindComputeShader();
}

void CloudManager::DebugRenderClouds() const
{
	//g_theRenderer->DrawIndexedBuffer(m_debugVoxelBuffer, m_debugIndexBuffer);
	//
	//g_theRenderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_NONE);
	//
	//g_theRenderer->DrawIndexedBuffer(m_debugWireframeVBO, m_debugWireframeIBO);
	//
	//g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
}

//void CloudManager::BuildOctrees()
//{
//	m_voxelOctrees.clear();
//
//	for (Cloud& cloud : m_clouds)
//	{
//		AABB3 bounds = cloud.GetBounds();
//		std::vector<Voxel*> cloudVoxels = cloud.GetVoxels();
//
//		std::unique_ptr<Octree<Voxel>> octree = std::make_unique<Octree<Voxel>>(bounds);
//		octree->Build(cloudVoxels, );
//
//		m_voxelOctrees.push_back(std::move(octree));
//	}
//}

void CloudManager::SerializeOctreesToGPU(std::vector<OctreeNodeGPU>& gpuNodes, std::vector<Voxel>& gpuVoxels) const
{
	gpuNodes.clear();
	gpuVoxels.clear();

	// Map to track unique voxels by position
	std::unordered_map<Vec3, int, Vec3Hasher> voxelMap;

	for (const auto& octree : m_voxelOctrees) {
		// Serialize octree nodes and voxels
		octree->SerializeToGPU(gpuNodes, gpuVoxels, voxelMap);
	}

	// Log duplicate voxels (optional, now redundant with voxelMap)
	for (int i = 0; i < gpuVoxels.size(); i++) {
		for (int j = i + 1; j < gpuVoxels.size(); j++) {
			if (gpuVoxels[i].m_position == gpuVoxels[j].m_position) {
				printf("Duplicate voxel found at %i and %i\n", i, j);
			}
		}
	}
}

void CloudManager::SerializePositionOctreesToGPU(std::vector<OctreeNodeGPU>& gpuNodes, std::vector<Vec3>& gpuPositions) const
{
	gpuNodes.clear();
	gpuPositions.clear();
	//
	//// Map to track unique voxels by position
	//std::unordered_map<Vec3, int, Vec3Hasher> voxelMap;
	//
	//for (const auto& octree : m_voxelPositionOctrees) {
	//	// Serialize octree nodes and voxels
	//	octree->SerializeToGPU(gpuNodes, gpuPositions, voxelMap);
	//}
	//
	//// Log duplicate voxels (optional, now redundant with voxelMap)
	//for (int i = 0; i < gpuPositions.size(); i++) {
	//	for (int j = i + 1; j < gpuPositions.size(); j++) {
	//		if (gpuPositions[i] == gpuPositions[j]) {
	//			printf("Duplicate voxel found at %i and %i\n", i, j);
	//		}
	//	}
	//}
}

void CloudManager::InitializeNoiseTexture(int width, int height, int depth, float frequency, int octaves)
{
	UNUSED(frequency);
	UNUSED(octaves);
	m_noiseTexture = g_theRenderer->CreateTexture3DFromNoise(IntVec3(width, height, depth), 100.f, octaves, 0.5f, 2.f, true);

	//m_noiseTexture3D = GeneratePerlin3D(width, height, depth, 1.f, frequency, 1.f, 1.f, octaves, 0);
	//m_noiseTexture3D = GeneratePerlin3D(width, height, depth, frequency, octaves, 1.0f, 0.5f, 4, 0);
}

void CloudManager::InitializeWorleyTexture(int width, int height, int depth, int cellSize, unsigned int seed)
{
	m_worleyTexture = g_theRenderer->CreateTexture3DFromWorley(IntVec3(width, height, depth), cellSize, seed);
}

void CloudManager::BindNoiseTexture() const
{
	//g_theRenderer->SetLightConstants(m_sunDirection.GetNormalized(), m_sunIntensity, m_ambientIntensity);
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(m_cloudShader);
	//g_theRenderer->BindTexture3D(m_noiseTexture);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
}