#include "Game/SkyVolume.hpp"
#include "Game/Game.hpp"
#include "Game/Player.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/StructuredBuffer.hpp"
#include <functional>

#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/imgui_impl_dx11.h"
#include "ThirdParty/ImGui/imgui_impl_win32.h"

extern Renderer* g_theRenderer;

//----------------------------------------------------------------------------------------
SkyVolume::SkyVolume()
{
	// Set default sky bounds - adjust these to match your world
	m_worldBounds = AABB3(Vec3(-500, -500, -500), Vec3(500, 500, 500));
	DebuggerPrintf("SkyVolume created with bounds: (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)\n",
		m_worldBounds.m_mins.x, m_worldBounds.m_mins.y, m_worldBounds.m_mins.z,
		m_worldBounds.m_maxs.x, m_worldBounds.m_maxs.y, m_worldBounds.m_maxs.z);
}

//----------------------------------------------------------------------------------------
SkyVolume::~SkyVolume()
{
	Shutdown();
}

//----------------------------------------------------------------------------------------
void SkyVolume::Initialize()
{
	DebuggerPrintf("SkyVolume::Initialize() - Starting\n");

	InitializeNoiseTexture(256, 256, 256, 1.f, 7);
	InitializeWorleyTexture(256, 256, 256, 32, 4);

	m_sliceExtractShader = g_theRenderer->CreateOrGetComputeShader(
		"Data/Shaders/Extract3DSlice", VertexType::VOXEL_CLOUDS);
	m_debugSliceTexture = g_theRenderer->CreateEmptyTextureWithUAV(
		"DebugSlice", IntVec2(256, 256));

	m_cloudVolumeShader = g_theRenderer->CreateOrGetComputeShader("Data/Shaders/CloudVolumeShader", VertexType::VOXEL_CLOUDS);
	m_shadowShader = g_theRenderer->CreateOrGetComputeShader("Data/Shaders/CloudVolumeShadowShader", VertexType::VOXEL_CLOUDS);

	m_constantBuffer = g_theRenderer->CreateConstantBuffer(sizeof(SkyVolumeConstants));
	m_debugConstantBuffer = g_theRenderer->CreateConstantBuffer(sizeof(DebugConstantsGPU));

	IntVec2 screenDims = g_theWindow->GetClientDimensions();
	m_outputTexture = g_theRenderer->CreateEmptyTextureWithUAV("OutSkyTexture", screenDims);
	m_shadowTexture = g_theRenderer->CreateEmptyTextureWithUAV("ShadowTexture", IntVec2(512, 512));

	// Clear any existing data
	m_octreeNodes.clear();
	m_octreeNodes.reserve(10000);  // Pre-allocate for performance

	// Initialize with root node
	SkyOctreeNode root;
	root.bounds = m_worldBounds;
	root.childrenIndex = -1;
	root.densityDataIndex = 0;
	root.averageDensity = 0.0f;
	root.maxDensity = 0.0f;
	m_octreeNodes.push_back(root);

	// Add some test clouds
	AddDefaultClouds();

	// Build the octree and density field
	RebuildAll();


	DebuggerPrintf("SkyVolume::Initialize() - Complete. Nodes: %d, Regions: %d\n",
		GetNodeCount(), GetRegionCount());
}

//----------------------------------------------------------------------------------------
void SkyVolume::Shutdown()
{
	DebuggerPrintf("SkyVolume::Shutdown()\n");

	// Clean up GPU resources if they exist
	if (m_octreeBuffer) {
		delete m_octreeBuffer;
		m_octreeBuffer = nullptr;
	}

	if (m_shadowTexture) {
		// Texture cleanup handled by renderer
		m_shadowTexture = nullptr;
	}

	if (m_shadowShader) {
		// Shader cleanup handled by renderer
		m_shadowShader = nullptr;
	}

	if (m_densityTexture) {
		m_densityTexture = nullptr;
	}

	m_octreeNodes.clear();
	m_cloudRegions.clear();
}

//----------------------------------------------------------------------------------------
void SkyVolume::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);

	if (deltaSeconds > 0.f)
	{
		m_perfStats.fps = 1.f / deltaSeconds;
	}

	// Day 1: Just animate clouds slightly for visual feedback
	for (CloudRegion& cloud : m_cloudRegions) {
		cloud.windOffset.x += deltaSeconds * 2.0f;  // Slow drift
	}

	g_theRenderer->SetCurrentCamera(PipelineStage::COMPUTE, m_theGame->m_player->m_playerCam);
}

//----------------------------------------------------------------------------------------
void SkyVolume::Render()
{
	UpdatePerformanceStats();

	if (m_debugSettings.showOctreeNodes || m_debugSettings.showDensityField) {
	//	// Draw world bounds
	//
	//	std::vector<Vertex_PCU> verts;
	//
	//	AddVertsForAABB3D(verts, m_worldBounds, Rgba8(100, 100, 255, 128));
	//	
	//	int nodesWithDensity = 0;
	//	float maxDensitySeen = 0.0f;
	//
	//	// Draw octree nodes that contain density
	//	for (int i = 0; i < m_octreeNodes.size(); i++) {
	//		const SkyOctreeNode& node = m_octreeNodes[i];
	//
	//		if (node.maxDensity > 0.01f) {
	//			nodesWithDensity++;
	//			maxDensitySeen = max(maxDensitySeen, node.maxDensity);
	//
	//			// Color based on density (make more visible)
	//			unsigned char alpha = (unsigned char)(min(node.maxDensity * 255.0f, 255.0f));
	//			alpha = max(alpha, (unsigned char)64);  // Minimum visibility
	//
	//			// Use different colors for internal vs leaf nodes
	//			if (node.childrenIndex == -1) {
	//				// Leaf nodes - yellow
	//				AddVertsForAABB3D(verts, node.bounds, Rgba8(255, 255, 0, alpha));
	//			}
	//			else {
	//				// Internal nodes - cyan
	//				AddVertsForAABB3D(verts, node.bounds, Rgba8(0, 255, 255, alpha / 2));
	//			}
	//		}
	//	}
	//	
	//	// Draw cloud regions as spheres
	//	for (const CloudRegion& cloud : m_cloudRegions) {
	//		AddVertsForSphere3D(verts, cloud.center, cloud.radii.x, Rgba8(0, 255, 0, 128));
	//		// Also draw center point
	//		AddVertsForSphere3D(verts, cloud.center, 2.0f, Rgba8(255, 0, 0, 255));
	//	}
	//
	//	g_theRenderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_BACK);
	//
	//	g_theRenderer->DrawVertexArray(verts.size(), verts.data());
	//
	//	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		DrawDebugVisualization();
	}
	if (!m_debugVisualization)
	{
		RenderClouds();
	}

	if (m_debugSettings.showPerformanceStats) {
		DrawPerformanceOverlay();
	}
}

void SkyVolume::RenderShadowPass()
{
	if (!m_shadowShader || !m_shadowTexture)
	{
		return;
	}

	m_constants.sunDirection = m_constants.sunDirection.GetNormalized();
    g_theRenderer->CopyCPUToGPU(&m_constants, sizeof(m_constants), m_constantBuffer);

	IntVec2 dims = g_theWindow->GetClientDimensions();
	int groupsX = (512 + 15) / 16;
	int groupsY = (512 + 15) / 16;

	// Bind resources (same as main pass)
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_densityTexture, 1);
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_noiseTexture, 2);
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_worleyTexture, 3);
	g_theRenderer->BindConstantBuffer(6, m_constantBuffer, PipelineStage::COMPUTE);
	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);

	// Dispatch shadow compute
	g_theRenderer->BindComputeShader(m_shadowShader);
	g_theRenderer->BindTextureWithUAV(PipelineStage::COMPUTE, m_shadowTexture, 0);
	g_theRenderer->DispatchComputeJob(m_shadowShader, groupsX, groupsY, 1);

	g_theRenderer->UnbindComputeShader();
	g_theRenderer->BindTexture();
}

void SkyVolume::RenderClouds()
{
	if (!m_cloudVolumeShader || !m_densityTexture || !m_octreeBuffer) {
		return;  // Not ready
	}

	Camera& cam = m_theGame->m_player->m_playerCam;

	Mat44 viewProj = cam.GetProjectionMatrix();
	viewProj.Append(cam.GetViewMatrix());
	Mat44 invViewProj = viewProj.GetInverse();

	// Update constants
	m_constants.invViewProj = invViewProj;
	m_constants.cameraPosition = cam.m_position;
	m_constants.time = m_theGame->m_gameClock->GetTotalSeconds();
	
	// Lighting
	m_constants.sunDirection = m_theGame->m_weather.m_lightConstants.SunDirection;
	m_constants.lightColor = Vec3(1.0f, 0.95f, 0.8f);
	
	
	// Bounds
	m_constants.skyBoundsMin = m_worldBounds.m_mins;
	m_constants.skyBoundsMax = m_worldBounds.m_maxs;
	
// Step sizes
//
// m_constants.stepSize = 10.0f;
//m_constants.maxSteps = 100.0f;
//m_constants.minStepSize = 5.0f;

// Density
//m_constants.densityScale = 2.0f;
//m_constants.densityMultiplier = 0.37f; // From your old tuned value
//m_constants.densityThreshold = 0.01f;
//m_constants.densityFalloff = 0.5f;

	// Noise
	m_constants.noiseScale = 0.01f;
	m_constants.noiseLerpVal = 0.5f;
	m_constants.noisePowVal = 3.7f; // From your old tuned value
	m_constants.densityNoiseLerpVal = 0.92f;
	m_constants.minWorleyValue = 0.32f;
	m_constants.scrollFactor = 0.5f;
	m_constants.scrolling = 1;
	m_constants.useNoise = 1;

// Adaptive stepping
	m_constants.farDistanceThreshold = 50.0f;
	m_constants.farMultiplier = 2.8f;
	m_constants.cloudVoxelDistanceLerpVal = 0.87f;

// Lighting effects
//m_constants.extinctionCoefficient = 1.0f; // From your old tuned value
//m_constants.scatteringCoefficient = 1.22f;
//m_constants.lightAbsorption = 0.5f;
//m_constants.powderBias = 0.9f; // From your old tuned value

// Phase function
//m_constants.anisotropyG = 0.01f; // From your old tuned value
//m_constants.shadowFactorMin = 0.93f; // From your old tuned value
//m_constants.minAccepted = 0.1f;

// Debug
	m_constants.showBoundingBoxes = 0;
	m_constants.currentDepth = 0;
	m_constants.useDensity = 1;
	m_constants.invertNoise = 0;

	// Upload constants
	g_theRenderer->CopyCPUToGPU(&m_constants, sizeof(m_constants), m_constantBuffer);
	g_theRenderer->BindConstantBuffer(6, m_constantBuffer, PipelineStage::COMPUTE);

	m_debugConstants.debugMode = m_debugSettings.currentDebugMode; // Add this to DebugSettings
	m_debugConstants.debugPixel = IntVec2(m_debugSettings.debugPixelX, m_debugSettings.debugPixelY);
	m_debugConstants.debugValue = m_debugSettings.debugValue;

	g_theRenderer->CopyCPUToGPU(&m_debugConstants, sizeof(m_debugConstants), m_debugConstantBuffer);
	g_theRenderer->BindConstantBuffer(7, m_debugConstantBuffer, PipelineStage::COMPUTE);

	if (m_constants.useShadowMap == 1)
	{
		//RenderShadowPass();
	}

	// Bind resources
	g_theRenderer->BindStructuredBufferToWrite(0, m_octreeBuffer);
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_densityTexture, 1);
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_noiseTexture, 2);   // Perlin
	g_theRenderer->BindTexture3D(PipelineStage::COMPUTE, m_worleyTexture, 3);  // Worley

	if (m_constants.useShadowMap == 1)
	{
		g_theRenderer->BindTexture(PipelineStage::COMPUTE, m_shadowTexture, 4);
	}

	g_theRenderer->SetSamplerMode(SamplerMode::BILINEAR_WRAP);
	// Get output texture (your cloud texture)
	

	// Dispatch compute
	IntVec2 dims = g_theWindow->GetClientDimensions();
	//int halfWidth = (dims.x + 1) / 2;
	//int halfHeight = (dims.y + 1) / 2;
	//
	int groupsX = (dims.x + 15) / 16;
	int groupsY = (dims.y + 15) / 16;

	g_theRenderer->BindComputeShader(m_cloudVolumeShader);
	g_theRenderer->BindTextureWithUAV(PipelineStage::COMPUTE, m_outputTexture, 0);
	g_theRenderer->DispatchComputeJob(m_cloudVolumeShader, groupsX, groupsY, 1);
	g_theRenderer->UnbindComputeShader();

	g_theRenderer->BindTexture();

	Shader* baseShader = g_theRenderer->CreateOrGetShader("Data/Shaders/BaseShader", VertexType::VERTEX_PCU);

	g_theRenderer->BindShader(baseShader);

	g_theRenderer->SetBlendMode(BlendMode::ALPHAPREMUL);  // or ALPHA
	g_theRenderer->BindShaderResources(m_outputTexture->GetShaderResourceView(), 0);

	g_theRenderer->DrawFullScreenQuad();

	g_theRenderer->UnbindComputeShader();
}

void SkyVolume::RenderImGuiPanel()
{
	ImGui::SetNextWindowSize(ImVec2(420, 700), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("Sky Volume Controls", nullptr, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return;
	}

	// === RENDERING ===
	if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Step Size", &m_constants.stepSize, .01f, 50.0f);
		ImGui::SliderInt("Max Steps", &m_constants.maxSteps, 50, 1000);
		ImGui::SliderFloat("Min Step", &m_constants.minStepSize, .01f, 10.0f);
	}

	// === DENSITY ===
	if (ImGui::CollapsingHeader("Density", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Density Scale", &m_constants.densityScale, 0.1f, 10.0f);
		ImGui::SliderFloat("Density Multiplier", &m_constants.densityMultiplier, 0.0f, 2.0f);
		ImGui::SliderFloat("Density Threshold", &m_constants.densityThreshold, 0.0f, 0.1f, "%.4f");
		ImGui::SliderFloat("Density Falloff", &m_constants.densityFalloff, 0.0f, 1.f, "%.2f");
		ImGui::SliderFloat("Min Accepted Alpha", &m_constants.minAccepted, 0.0f, 0.5f);
	}

	// === NOISE ===
	if (ImGui::CollapsingHeader("Noise", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Use Noise", (bool*)&m_constants.useNoise);
		ImGui::SliderFloat("Noise Scale", &m_constants.noiseScale, 0.001f, 0.1f, "%.4f");
		ImGui::SliderFloat("Noise Lerp", &m_constants.noiseLerpVal, 0.0f, 1.0f);
		ImGui::SliderFloat("Noise Pow", &m_constants.noisePowVal, 0.5f, 5.0f);
		ImGui::SliderFloat("Density-Noise Lerp", &m_constants.densityNoiseLerpVal, 0.0f, 1.0f);
		ImGui::SliderFloat("Min Worley", &m_constants.minWorleyValue, 0.0f, 1.0f);
		ImGui::Checkbox("Scrolling", (bool*)&m_constants.scrolling);
		ImGui::SliderFloat("Scroll Factor", &m_constants.scrollFactor, 0.0f, 3.0f);
	}

	// === LIGHTING ===
	if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Use Shadow Pass", (bool*)&m_constants.useShadowMap);
		ImGui::Separator();
		ImGui::SliderFloat("Sun Intensity", &m_constants.sunIntensity, 0.0f, 5.0f);
		ImGui::SliderFloat("Ambient", &m_constants.ambientIntensity, 0.0f, 2.0f);
		ImGui::ColorEdit3("Light Color", &m_constants.lightColor.x);

		ImGui::Separator();
		ImGui::Text("Scattering");
		ImGui::SliderFloat("Extinction", &m_constants.extinctionCoefficient, 0.1f, 3.0f);
		ImGui::SliderFloat("Scattering", &m_constants.scatteringCoefficient, 0.1f, 3.0f);

		ImGui::SliderFloat("Light Absorption", &m_constants.lightAbsorption, 0.1f, 2.0f);
		ImGui::SliderFloat("Powder Bias", &m_constants.powderBias, 0.0f, 5.0f);
		ImGui::SliderFloat("Anisotropy G", &m_constants.anisotropyG, -2.f, 2.f);
		ImGui::SliderFloat("Shadow Factor Min", &m_constants.shadowFactorMin, 0.0f, 1.0f);
	}

	if (ImGui::CollapsingHeader("GodRays", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("God Rays");
		ImGui::Separator();

		ImGui::SliderFloat("Ray Intensity", &m_theGame->grc.intensity, 0.0f, 5.0f, "%.2f");
		ImGui::SliderFloat("Ray Decay", &m_theGame->grc.decay, 0.0f, 0.1f, "%.3f");

	}

	// === ADAPTIVE STEPPING ===
	if (ImGui::CollapsingHeader("Adaptive Stepping")) {
		ImGui::SliderFloat("Far Distance Threshold", &m_constants.farDistanceThreshold, 10.0f, 500.0f);
		ImGui::SliderFloat("Far Multiplier", &m_constants.farMultiplier, 1.0f, 10.0f);
	}

	// === DEBUG ===
	if (ImGui::CollapsingHeader("Debug")) {
		const char* debugModes[] = { "Normal", "Ray Steps", "Density", "Octree", "Specific Pixel", "Stats Overlay", "Noise" };
		ImGui::Combo("Debug Mode", &m_debugSettings.currentDebugMode, debugModes, 7);

		if (m_debugSettings.currentDebugMode == 4) {
			ImGui::InputInt("Pixel X", &m_debugSettings.debugPixelX);
			ImGui::InputInt("Pixel Y", &m_debugSettings.debugPixelY);
		}

		ImGui::Checkbox("Show Octree", &m_debugSettings.showOctreeNodes);
		ImGui::Checkbox("Show Density Slice", &m_debugSettings.showDensityField);

		if (m_debugSettings.showOctreeNodes) {
			ImGui::SliderInt("Octree Level", &m_debugSettings.octreeLevelToShow, -1, m_octreeDepth);
		}
	}

	// === PERFORMANCE ===
	if (ImGui::CollapsingHeader("Performance")) {
		ImGui::Text("FPS: %1.d", 1.0f/(Clock::GetSystemClock()).GetDeltaSeoconds());
		ImGui::Separator();
		ImGui::Text("Octree Nodes: %d", m_perfStats.octreeNodeCount);
		ImGui::Text("Leaf Nodes: %d", m_perfStats.leafNodeCount);
		ImGui::Text("Non-Empty Nodes: %d", m_perfStats.nonEmptyNodes);
		ImGui::Text("Octree Memory: %.2f MB", m_perfStats.octreeMemoryMB);
		ImGui::Text("Texture Memory: %.2f MB", m_perfStats.textureMemoryMB);
		ImGui::Text("Texture Resolution: %dx%dx%d",
			m_textureResolution.x, m_textureResolution.y, m_textureResolution.z);
	}

	// Add Debug Textures Window
	static bool showDebugTextures = false;

	if (ImGui::Checkbox("Show Debug Textures", &showDebugTextures)) {
		// Toggle debug texture display
	}

	ImGui::End();

	if (showDebugTextures) {
		if (ImGui::Begin("Debug Textures", &showDebugTextures)) {
			// Cloud output texture
			if (m_outputTexture) {
				ImGui::Text("Cloud Output:");
				ImGui::Image(ImTextureID(m_outputTexture->GetShaderResourceView()),
					ImVec2(256, 256));
			}

			// Shadow texture
			if (m_shadowTexture) {
				ImGui::Text("Shadow Map:");
				ImGui::Image(ImTextureID(m_shadowTexture->GetShaderResourceView()),
					ImVec2(256, 256));
			}

			// 3D Texture Slice Viewer
			ImGui::Separator();
			ImGui::Text("3D Noise Texture Slice:");

			static int sliceDepth = 128;
			if (ImGui::SliderInt("Slice Depth", &sliceDepth, 0, 255)) {
				m_debugConstants.currentDepth = sliceDepth;
				// You'll need to extract and display this slice
				// Could use a compute shader like the old Extract3DSlice
			}

			// If you have a debug slice texture, show it here
			// ImGui::Image((void*)m_debugSliceTexture->GetSRV(), ImVec2(256, 256));

			ImGui::End();
		}
	}
}

//----------------------------------------------------------------------------------------
void SkyVolume::AddCloudRegion(const CloudRegion& region)
{
	CloudRegion newRegion = region;
	newRegion.id = m_nextCloudId++;
	m_cloudRegions.push_back(newRegion);
	DebuggerPrintf("Added cloud region %d at (%.1f, %.1f, %.1f)\n",
		newRegion.id, newRegion.center.x, newRegion.center.y, newRegion.center.z);
}

//----------------------------------------------------------------------------------------
void SkyVolume::RebuildAll()
{
	DebuggerPrintf("SkyVolume::RebuildAll() - Starting\n");

	BuildOctree();
	GenerateDensityField();

	// ADD THESE NEW LINES
	CreateGPUResources();
	GenerateDensityTexture();

	DebuggerPrintf("SkyVolume::RebuildAll() - Complete. Final node count: %d\n",
		GetNodeCount());
}

void SkyVolume::DrawDebugVisualization()
{
	if (m_debugSettings.showOctreeNodes) {
		DrawOctreeDebug();
	}

	if (m_debugSettings.showDensityField) {
		DrawDensitySlice();
	}

	if (m_debugSettings.showRaySteps && m_debugSettings.debugPixelX >= 0) {
		DrawRayDebug();
	}

	if (m_debugSettings.showPerformanceStats) {
		DrawPerformanceOverlay();
	}
}

void SkyVolume::DrawOctreeDebug()
{
	std::vector<Vertex_PCU> verts;

	// Lambda to recursivelyt draw nodes
	std::function<void(int, int)> drawNode = [&](int nodeIdx, int depth)
	{
		if (nodeIdx >= m_octreeNodes.size()) return;
		const SkyOctreeNode& node = m_octreeNodes[nodeIdx];

		// Check if we should draw this level
		if (m_debugSettings.octreeLevelToShow >= 0 && depth != m_debugSettings.octreeLevelToShow)
		{
			//Still recurtse to children
			if (node.childrenIndex >= 0)
			{
				for (int i = 0; i < 8; i++)
				{
					drawNode(node.childrenIndex + i, depth + 1);
				}
			}
			return;
		}

		if (!m_debugSettings.showEmptyNodes && node.maxDensity < 0.001f) {
			return;
		}

		// Color based on depth and density
		float depthColor = depth / (float)m_octreeDepth;
		unsigned char r = (unsigned char)(depthColor * 255);
		unsigned char g = (unsigned char)((1.0f - depthColor) * 255);
		unsigned char b = (unsigned char)(node.maxDensity * 255);
		unsigned char a = node.childrenIndex >= 0 ? 64 : 128; // Transparent for internal

		AddVertsForAABB3D(verts, node.bounds, Rgba8(r, g, b, a));

		// Draw node index at center if requested
		if (m_debugSettings.showNodeIndices) {
			Vec3 center = node.bounds.GetCenter();
			// You'd need a text rendering system here
			// DebugAddTextWorldPos(std::to_string(nodeIdx), center, 0.5f);
		}

		// Recurse to children
		if (node.childrenIndex >= 0) {
			for (int i = 0; i < 8; i++) {
				drawNode(node.childrenIndex + i, depth + 1);
			}
		}
	};

	drawNode(0, 0);

	g_theRenderer->SetModelConstants();
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->DrawVertexArray(verts.size(), verts.data());
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
}

void SkyVolume::DrawDensitySlice()
{
	std::vector<Vertex_PCU> verts;

	// Place slice at camera position instead of arbitrary Z
	Vec3 cameraPos = m_theGame->m_player->m_playerCam.m_position;

	// Draw slice perpendicular to camera forward
	Vec3 forward, left, up;
	m_theGame->m_player->m_playerCam.m_orientation.GetVectors_XFwd_YLeft_ZUp(forward, left, up);

	// Create a grid in front of the camera
	float sliceDistance = 10.0f; // 10 units in front
	Vec3 sliceCenter = cameraPos + forward * sliceDistance;

	const int sampleCount = 30;
	float sliceSize = 50.0f; // 50x50 unit slice

	for (int y = 0; y < sampleCount; y++) {
		for (int x = 0; x < sampleCount; x++) {
			float u = (x / (float)(sampleCount - 1)) - 0.5f;
			float v = (y / (float)(sampleCount - 1)) - 0.5f;

			Vec3 worldPos = sliceCenter + left * (u * sliceSize) + up * (v * sliceSize);

			// Sample density
			float density = 0.0f;
			for (const CloudRegion& cloud : m_cloudRegions) {
				density += cloud.EvaluateDensity(worldPos);
			}

			if (density > m_debugSettings.densityThreshold) {
				// Color code by density level
				Rgba8 color;
				if (density < 0.3f) {
					color = Rgba8(0, 0, 255, 128);    // Blue = low
				}
				else if (density < 0.6f) {
					color = Rgba8(0, 255, 0, 128);    // Green = medium  
				}
				else {
					color = Rgba8(255, 0, 0, 128);    // Red = high
				}

				// Draw a small quad
				float quadSize = sliceSize / sampleCount;
				Vec3 corners[4] = {
					worldPos + left * quadSize / 2 - up * quadSize / 2,
					worldPos - left * quadSize / 2 - up * quadSize / 2,
					worldPos - left * quadSize / 2 + up * quadSize / 2,
					worldPos + left * quadSize / 2 + up * quadSize / 2,
				};

				AddVertsForQuad3D(verts, corners[0], corners[1], corners[2], corners[3], color);
			}
		}
	}

	// Draw slice border for reference
	Vec3 corners[4] = {
		sliceCenter + left * sliceSize / 2 - up * sliceSize / 2, // bottom left
		sliceCenter - left * sliceSize / 2 - up * sliceSize / 2, // bottom right
		sliceCenter - left * sliceSize / 2 + up * sliceSize / 2, // top right
		sliceCenter + left * sliceSize / 2 + up * sliceSize / 2 // top left
	};

	AddVertsForQuad3D(verts, corners[0], corners[1], corners[2], corners[3], Rgba8::YELLOW);

	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->DrawVertexArray(verts.size(), verts.data());
}

void SkyVolume::DrawRayDebug()
{
	if (m_lastDebugRay.samplePoints.empty()) return;

	std::vector<Vertex_PCU> verts;

	// Draw ray as a line
	for (size_t i = 0; i < m_lastDebugRay.samplePoints.size() - 1; i++) {
		Vec3 p1 = m_lastDebugRay.samplePoints[i];
		Vec3 p2 = m_lastDebugRay.samplePoints[i + 1];

		// Color based on density
		float density = m_lastDebugRay.densityValues[i];
		unsigned char r = (unsigned char)(density * 255.0f);
		Rgba8 color(r, 255 - r, 0, 255);

		AddVertsForCylinder3D(verts, p1, p2, 0.5f, color);

		// Draw sample point as small sphere
		AddVertsForSphere3D(verts, p1, 1.0f, color);
	}

	// Draw ray origin and direction
	AddVertsForArrow3D(verts, m_lastDebugRay.origin,
		m_lastDebugRay.origin + m_lastDebugRay.direction * 50.0f,.7f, 1.5f,
		2.0f, Rgba8(0, 255, 0, 255));

	g_theRenderer->DrawVertexArray(verts.size(), verts.data());

	// Draw text overlay with ray info
	//Vec2 textPos(10, 100);
	//std::string rayInfo = StringF("Debug Ray: Steps=%d, TotalDensity=%.3f, Nodes=%d",
	//	m_lastDebugRay.totalSteps,
	//	m_lastDebugRay.totalDensity,
	//	(int)m_lastDebugRay.octreeNodes.size());
	//// DebugAddTextScreen(rayInfo, textPos, 0.5f);
}

void SkyVolume::DrawPerformanceOverlay()
{

}

void SkyVolume::UpdatePerformanceStats()
{
	// Update counts
	m_perfStats.octreeNodeCount = (int)m_octreeNodes.size();
	m_perfStats.leafNodeCount = 0;
	m_perfStats.nonEmptyNodes = 0;

	for (const auto& node : m_octreeNodes) {
		if (node.childrenIndex == -1) {
			m_perfStats.leafNodeCount++;
		}
		if (node.maxDensity > 0.001f) {
			m_perfStats.nonEmptyNodes++;
		}
	}

	// Calculate memory usage
	m_perfStats.octreeMemoryMB = (m_octreeNodes.size() * sizeof(SkyOctreeNode)) / (1024.0f * 1024.0f);
	m_perfStats.textureMemoryMB = (m_textureResolution.x * m_textureResolution.y *
		m_textureResolution.z * sizeof(float)) / (1024.0f * 1024.0f);

	// GPU timing would require GPU timer queries
	// This is pseudocode - you'd need actual GPU timer implementation
	// m_perfStats.gpuRaymarchTime = g_theRenderer->GetLastComputeTime();
}

// Helper for drawing frame time graph
void SkyVolume::DrawFrameTimeGraph(Vec2 pos, Vec2 size) {
	std::vector<Vertex_PCU> verts;

	// Draw background
	AABB2 graphBounds(pos, pos + size);
	AddVertsForAABB2D(verts, graphBounds, Rgba8(0, 0, 0, 128));

	// Draw frame times as line graph
	if (m_perfStats.frameTimeHistory.size() > 1) {
		float maxTime = 33.33f; // Cap at 30 FPS for scale
		for (float t : m_perfStats.frameTimeHistory) {
			maxTime = max(maxTime, t);
		}

		for (size_t i = 0; i < m_perfStats.frameTimeHistory.size() - 1; i++) {
			float x1 = pos.x + (i / (float)m_perfStats.frameTimeHistory.size()) * size.x;
			float x2 = pos.x + ((i + 1) / (float)m_perfStats.frameTimeHistory.size()) * size.x;

			float y1 = pos.y + size.y - (m_perfStats.frameTimeHistory[i] / maxTime) * size.y;
			float y2 = pos.y + size.y - (m_perfStats.frameTimeHistory[i + 1] / maxTime) * size.y;

			// Color based on performance (green = good, red = bad)
			float t = m_perfStats.frameTimeHistory[i] / 33.33f;
			unsigned char r = (unsigned char)(t * 255);
			unsigned char g = (unsigned char)((1.0f - t) * 255);

			AddVertsForLineSegment2D(verts, Vec2(x1, y1), Vec2(x2, y2), 1.0f, Rgba8(r, g, 0, 255));
		}

		// Draw 16.66ms line (60 FPS target)
		float targetY = pos.y + size.y - (16.66f / maxTime) * size.y;
		AddVertsForLineSegment2D(verts, Vec2(pos.x, targetY), Vec2(pos.x + size.x, targetY),
			1.0f, Rgba8(0, 255, 0, 64));
	}

	g_theRenderer->DrawVertexArray(verts.size(), verts.data());
}

//----------------------------------------------------------------------------------------
void SkyVolume::AddDefaultClouds()
{
	// Add 3 test clouds
	for (int i = 0; i < 3; i++) {
		CloudRegion cloud;
		cloud.center = Vec3(-100.0f + i * 100.0f, -200.0f + i * 200.0f, 350.0f);
		cloud.radii = Vec3(100, 60, 80);
		cloud.type = CloudType::CUMULUS;
		cloud.densityScale = 1.0f;
		cloud.noiseScale = 1.0f;

		AddCloudRegion(cloud);
	}

	CloudRegion cloud2;
	cloud2.center = Vec3(70, 70, 350.f);
	cloud2.radii = Vec3(200, 70, 50);
	cloud2.type = CloudType::CUMULUS;
	cloud2.densityScale = 1.0f;
	cloud2.noiseScale = 1.0f;

	AddCloudRegion(cloud2);
}

//----------------------------------------------------------------------------------------
void SkyVolume::BuildOctree()
{
	DebuggerPrintf("BuildOctree: Starting with %d cloud regions\n", GetRegionCount());

	// For Day 1: Simple uniform subdivision to depth 3
	// We'll make this smarter tomorrow

	// Clear all but root
	m_octreeNodes.resize(1);
	m_octreeNodes[0].childrenIndex = -1;

	// Queue for subdivision: pair<nodeIndex, currentDepth>
	std::vector<std::pair<int, int>> nodesToProcess;
	nodesToProcess.push_back({ 0, 0 });

	int leafCount = 0;

	while (!nodesToProcess.empty()) {
		int nodeIdx = nodesToProcess.back().first;
		int depth = nodesToProcess.back().second;
		nodesToProcess.pop_back();

		SkyOctreeNode& node = m_octreeNodes[nodeIdx];

		// Subdivide if we should
		if (depth < m_octreeDepth && ShouldSubdivide(node)) {
			// Create 8 children
			node.childrenIndex = (int)m_octreeNodes.size();

			Vec3 center = node.bounds.GetCenter();

			for (int i = 0; i < 8; i++) {
				SkyOctreeNode child;

				// Calculate child bounds
				Vec3 childMin = node.bounds.m_mins;
				Vec3 childMax = center;

				if (i & 1) { childMin.x = center.x; childMax.x = node.bounds.m_maxs.x; }
				if (i & 2) { childMin.y = center.y; childMax.y = node.bounds.m_maxs.y; }
				if (i & 4) { childMin.z = center.z; childMax.z = node.bounds.m_maxs.z; }

				child.bounds = AABB3(childMin, childMax);
				child.childrenIndex = -1;
				child.densityDataIndex = -1;

				m_octreeNodes.push_back(child);

				// Add child to process list
				if (depth + 1 < m_octreeDepth) {
					nodesToProcess.push_back({ node.childrenIndex + i, depth + 1 });
				}
			}
		}
		else {
			// This is a leaf
			node.densityDataIndex = leafCount++;
		}
	}

	// Allocate density data
	m_densityData.resize(leafCount, 0.0f);

	DebuggerPrintf("BuildOctree: Created %d nodes, %d leaves\n",
		GetNodeCount(), leafCount);
}

//----------------------------------------------------------------------------------------

bool SkyVolume::ShouldSubdivide(const SkyOctreeNode& node) {
	Vec3 dims = node.bounds.m_maxs - node.bounds.m_mins;
	if (dims.x < 20.0f && dims.y < 20.0f && dims.z < 20.0f)
		return false;  // Stop at ~20 unit cells

	for (const CloudRegion& cloud : m_cloudRegions) {
		// Tighter bounds check
		float maxRadius = max(cloud.radii.x, max(cloud.radii.y, cloud.radii.z));
		Vec3 closestPoint = node.bounds.GetNearestPoint(cloud.center);
		float dist = GetDistance3D(closestPoint, cloud.center);

		if (dist < maxRadius * 1.2f) {  // Only 20% padding instead of 50%
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------
void SkyVolume::GenerateDensityField()
{
	DebuggerPrintf("GenerateDensityField: Processing %d nodes\n", GetNodeCount());
	DebuggerPrintf("  Number of clouds: %d\n", GetRegionCount());

	// First, print cloud info
	for (const CloudRegion& cloud : m_cloudRegions) {
		DebuggerPrintf("  Cloud %d: center(%.1f,%.1f,%.1f) radii(%.1f,%.1f,%.1f)\n",
			cloud.id, cloud.center.x, cloud.center.y, cloud.center.z,
			cloud.radii.x, cloud.radii.y, cloud.radii.z);
	}

	int nonZeroCount = 0;
	int leafCount = 0;
	float maxFound = 0.0f;

	// Calculate density at each leaf
	for (int i = 0; i < m_octreeNodes.size(); i++) {
		SkyOctreeNode& node = m_octreeNodes[i];

		if (node.childrenIndex == -1) {  // Is leaf
			leafCount++;
			Vec3 center = node.bounds.GetCenter();

			// Debug first few leaves
			if (leafCount <= 5) {
				DebuggerPrintf("  Leaf %d: center(%.1f,%.1f,%.1f) bounds min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f)\n",
					leafCount, center.x, center.y, center.z,
					node.bounds.m_mins.x, node.bounds.m_mins.y, node.bounds.m_mins.z,
					node.bounds.m_maxs.x, node.bounds.m_maxs.y, node.bounds.m_maxs.z);
			}

			float totalDensity = 0.0f;

			// Check each cloud
			for (const CloudRegion& cloud : m_cloudRegions) {
				float d = cloud.EvaluateDensity(center);
				totalDensity += d;

				// Debug if we found density
				if (d > 0.001f && nonZeroCount < 5) {
					DebuggerPrintf("    Found density %.3f at (%.1f,%.1f,%.1f) from cloud %d\n",
						d, center.x, center.y, center.z, cloud.id);
				}
			}

			if (node.densityDataIndex >= 0 && node.densityDataIndex < m_densityData.size()) {
				m_densityData[node.densityDataIndex] = totalDensity;
			}

			node.averageDensity = totalDensity;
			node.maxDensity = totalDensity;

			if (totalDensity > 0.001f) {
				nonZeroCount++;
				maxFound = max(maxFound, totalDensity);
			}
		}
	}

	DebuggerPrintf("  Processed %d leaves, found %d with density > 0.001, max: %.3f\n",
		leafCount, nonZeroCount, maxFound);

	// Propagate up tree
	for (int i = (int)m_octreeNodes.size() - 1; i >= 0; i--) {
		SkyOctreeNode& node = m_octreeNodes[i];

		if (node.childrenIndex != -1) {
			float maxChild = 0.0f;
			float avgChild = 0.0f;
			int validChildren = 0;

			for (int c = 0; c < 8; c++) {
				int childIdx = node.childrenIndex + c;
				if (childIdx < m_octreeNodes.size()) {
					const SkyOctreeNode& child = m_octreeNodes[childIdx];
					maxChild = max(maxChild, child.maxDensity);
					avgChild += child.averageDensity;
					validChildren++;
				}
			}

			if (validChildren > 0) {
				node.maxDensity = maxChild;
				node.averageDensity = avgChild / float(validChildren);
			}
		}
	}

	DebuggerPrintf("GenerateDensityField: Complete. Root max density: %.3f\n",
		m_octreeNodes.empty() ? 0.0f : m_octreeNodes[0].maxDensity);
}

//----------------------------------------------------------------------------------------
float SkyVolume::SampleOctreeDensity(const Vec3& worldPos) const
{
	// Traverse octree to find density at this point
	int nodeIdx = 0;

	while (nodeIdx < m_octreeNodes.size()) {
		const SkyOctreeNode& node = m_octreeNodes[nodeIdx];

		if (!node.bounds.IsPointInside(worldPos)) {
			return 0.0f;  // Outside bounds
		}

		if (node.childrenIndex == -1) {
			// Leaf node
			if (node.densityDataIndex >= 0) {
				return m_densityData[node.densityDataIndex];
			}
			return 0.0f;
		}

		// Find which child
		Vec3 center = node.bounds.GetCenter();
		int childIdx = 0;
		if (worldPos.x > center.x) childIdx |= 1;
		if (worldPos.y > center.y) childIdx |= 2;
		if (worldPos.z > center.z) childIdx |= 4;

		nodeIdx = node.childrenIndex + childIdx;
	}

	return 0.0f;
}

void SkyVolume::CreateGPUResources()
{
	DebuggerPrintf("CreateGPUResources: Starting\n");

	// Clean up old buffers
	if (m_octreeBuffer) {
		delete m_octreeBuffer;
		m_octreeBuffer = nullptr;
	}

	// Create octree buffer
	if (!m_octreeNodes.empty()) {
		// Convert nodes to GPU format
		std::vector<SkyOctreeNodeGPU> gpuNodes;
		gpuNodes.reserve(m_octreeNodes.size());

		for (const SkyOctreeNode& node : m_octreeNodes) {
			SkyOctreeNodeGPU gpuNode;
			gpuNode.boundsMin = node.bounds.m_mins;
			gpuNode.averageDensity = node.averageDensity;
			gpuNode.boundsMax = node.bounds.m_maxs;
			gpuNode.maxDensity = node.maxDensity;
			gpuNode.childrenIndex = node.childrenIndex;
			gpuNode.densityIndex = node.densityDataIndex;
			gpuNode.padding1 = 0;
			gpuNode.padding2 = 0;

			gpuNodes.push_back(gpuNode);
		}

		// Create structured buffer
		size_t bufferSize = sizeof(SkyOctreeNodeGPU);
		m_octreeBuffer = g_theRenderer->CreateStructuredBuffer(
			gpuNodes.size(),
			bufferSize,
			false  // allowUAV = false (we only read from this buffer)
		);

		// Upload the data
		size_t totalBytes = gpuNodes.size() * bufferSize;
		g_theRenderer->CopyCPUToGPU(gpuNodes.data(), totalBytes, m_octreeBuffer);

		DebuggerPrintf("  Created octree buffer with %d nodes\n", (int)gpuNodes.size());
	}

	// Create density data buffer
	if (!m_densityData.empty()) {
		// We'll add this next if needed
	}

	DebuggerPrintf("CreateGPUResources: Complete\n");
}

void SkyVolume::UpdateGPUBuffers()
{
}

void SkyVolume::GenerateDensityTexture()
{
	DebuggerPrintf("GenerateDensityTexture: Creating %dx%dx%d texture\n",
		m_textureResolution.x, m_textureResolution.y, m_textureResolution.z);

	// Allocate CPU-side texture data
	int totalVoxels = m_textureResolution.x * m_textureResolution.y * m_textureResolution.z;
	std::vector<float> textureData(totalVoxels, 0.0f);

	// Sample density field into texture
	int nonZeroVoxels = 0;
	float maxDensityInTexture = 0.0f;

	for (int z = 0; z < m_textureResolution.z; z++) {
		for (int y = 0; y < m_textureResolution.y; y++) {
			for (int x = 0; x < m_textureResolution.x; x++) {
				// Convert voxel index to world position
				Vec3 texCoord(
					(float)x / (float)(m_textureResolution.x - 1),
					(float)y / (float)(m_textureResolution.y - 1),
					(float)z / (float)(m_textureResolution.z - 1)
				);

				Vec3 worldPos = TextureCoordToWorld(texCoord);

				// Sample density from cloud regions directly (for now)
				float density = 0.0f;
				for (const CloudRegion& cloud : m_cloudRegions) {
					density += cloud.EvaluateDensity(worldPos);
				}

				// Store in texture
				int index = x + y * m_textureResolution.x +
					z * m_textureResolution.x * m_textureResolution.y;
				textureData[index] = density;

				if (density > 0.001f) {
					nonZeroVoxels++;
					maxDensityInTexture = max(maxDensityInTexture, density);
				}
			}
		}

		// Progress indicator for large textures
		if (z % 8 == 0) {
			DebuggerPrintf("  Progress: %d/%d slices\n", z, m_textureResolution.z);
		}
	}

	DebuggerPrintf("  Texture generated: %d non-zero voxels, max density: %.3f\n",
		nonZeroVoxels, maxDensityInTexture);

	// Upload to GPU
	UploadDensityTexture(textureData);
}

void SkyVolume::UploadDensityTexture(const std::vector<float>& textureData)
{
	DebuggerPrintf("UploadDensityTexture: Uploading %zu floats to GPU\n", textureData.size());

	const int bytesPerTexel = sizeof(float); // one float per voxel

	m_densityTexture = g_theRenderer->CreateTexture3DFromData(
		"DensityVolume",
		m_textureResolution,
		bytesPerTexel,
		textureData.data()
	);

	if (m_densityTexture) {
		DebuggerPrintf("  3D density texture created successfully (%dx%dx%d)\n",
			m_textureResolution.x, m_textureResolution.y, m_textureResolution.z);
	}
	else {
		DebuggerPrintf("  ERROR: Failed to create 3D density texture\n");
	}
}

Vec3 SkyVolume::TextureCoordToWorld(const Vec3& texCoord) const
{
	// Convert from [0,1] texture space to world space
	Vec3 worldPos;
	worldPos.x = Interpolate(m_worldBounds.m_mins.x, m_worldBounds.m_maxs.x, texCoord.x);
	worldPos.y = Interpolate(m_worldBounds.m_mins.y, m_worldBounds.m_maxs.y, texCoord.y);
	worldPos.z = Interpolate(m_worldBounds.m_mins.z, m_worldBounds.m_maxs.z, texCoord.z);

	return worldPos;
}

Vec3 SkyVolume::WorldToTextureCoord(const Vec3& worldPos) const
{
	// Convert from world space to [0,1] texture space
	Vec3 texCoord;
	Vec3 worldSize = m_worldBounds.m_maxs - m_worldBounds.m_mins;
	texCoord.x = (worldPos.x - m_worldBounds.m_mins.x) / worldSize.x;
	texCoord.y = (worldPos.y - m_worldBounds.m_mins.y) / worldSize.y;
	texCoord.z = (worldPos.z - m_worldBounds.m_mins.z) / worldSize.z;

	// Clamp to valid range
	texCoord.x = GetClamped(texCoord.x, 0.0f, 1.0f);
	texCoord.y = GetClamped(texCoord.y, 0.0f, 1.0f);
	texCoord.z = GetClamped(texCoord.z, 0.0f, 1.0f);

	return texCoord;
}

//----------------------------------------------------------------------------------------
// CloudRegion implementation
float CloudRegion::EvaluateDensity(const Vec3& worldPos) const
{
	Vec3 localPos = worldPos - center;

	// Check each axis separately for ellipsoid
	float dx = localPos.x / radii.x;
	float dy = localPos.y / radii.y;
	float dz = localPos.z / radii.z;

	float distSq = dx * dx + dy * dy + dz * dz;

	// Debug first few evaluations
	static int debugCount = 0;
	if (debugCount < 10) {
		DebuggerPrintf("EvaluateDensity: worldPos(%.1f,%.1f,%.1f), center(%.1f,%.1f,%.1f), distSq=%.3f\n",
			worldPos.x, worldPos.y, worldPos.z,
			center.x, center.y, center.z, distSq);
		debugCount++;
	}

	if (distSq > 1.0f) return 0.0f;

	// Simple density falloff
	float density = 1.0f - sqrtf(distSq);  // Use sqrt for smoother falloff
	density = density * density;  // Quadratic falloff

	return density * densityScale;
}

void SkyVolume::InitializeNoiseTexture(int width, int height, int depth, float frequency, int octaves)
{
	UNUSED(frequency);
	UNUSED(octaves);
	m_noiseTexture = g_theRenderer->CreateTexture3DFromNoise(IntVec3(width, height, depth), 100.f, octaves, 0.5f, 2.f, true);

	//m_noiseTexture3D = GeneratePerlin3D(width, height, depth, 1.f, frequency, 1.f, 1.f, octaves, 0);
	//m_noiseTexture3D = GeneratePerlin3D(width, height, depth, frequency, octaves, 1.0f, 0.5f, 4, 0);
}

void SkyVolume::InitializeWorleyTexture(int width, int height, int depth, int cellSize, unsigned int seed)
{
	m_worleyTexture = g_theRenderer->CreateTexture3DFromWorley(IntVec3(width, height, depth), cellSize, seed);
}
