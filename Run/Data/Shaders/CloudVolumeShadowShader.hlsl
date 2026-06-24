// CloudVolumeShadowShader.hlsl - Computes shadows from sun's perspective (ORTHOGRAPHIC)
// FIXED VERSION - Proper RGB encoding: R=first distance, G=last distance, B=total density

// Resources
Texture3D<float> g_densityTexture : register(t1);
Texture3D<float> perlinNoiseTexture : register(t2);
Texture3D<float> worleyNoiseTexture : register(t3);
SamplerState g_trilinearSampler : register(s0);

// Output
RWTexture2D<float4> g_shadowOutput : register(u0);

// Constants
cbuffer CameraConstants : register(b2) {
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InverseViewMatrix;
    float4x4 InverseProjMatrix;
    float3 CameraPosition;
    float2 NearScreenSize;
};

cbuffer CloudConstants : register(b6) {
    float4x4 invViewProjMatrix;
    
    float4x4 shadowview;
	float4x4 shadowproj;

    float3 cameraPosition;
    float time;
    
    float3 sunDirection;
    float sunIntensity;
    
    float3 lightColor;
    float ambientIntensity;
    
    float3 skyBoundsMin;
    float stepSize;
    
    float3 skyBoundsMax;
    int maxSteps;
    
    float densityScale;
    float densityMultiplier;
    float densityFalloff;
    float densityThreshold;
    
    float noiseScale;
    float noiseLerpVal;
    float noisePowVal;
    float densityNoiseLerpVal;
    
    float minWorleyValue;
    float scrollFactor;
    int scrolling;
    int useNoise;
    
    float minStepSize;
    float farDistanceThreshold;
    float farMultiplier;
    float cloudVoxelDistanceLerpVal;
    
    float extinctionCoefficient;
    float scatteringCoefficient;
    float lightAbsorption;
    float powderBias;
    
    float anisotropyG;
    float shadowFactorMin;
    float minAccepted;
    int useShadowMap;
}

// Shadow-specific constants
cbuffer ShadowConstants : register(b8) {
    float4x4 shadowViewMatrix;
    float4x4 shadowProjectionMatrix;
    float3 shadowCameraPosition;
    float shadowMapSize;
    float3 shadowVolumeBoundsMin;
    float shadowNearPlane;
    float3 shadowVolumeBoundsMax;
    float shadowFarPlane;
}

// Ray-AABB intersection
bool IntersectAABB(float3 origin, float3 dir, float3 bmin, float3 bmax, 
                   out float tMin, out float tMax) {
    float3 invDir = 1.0 / (dir + 0.0001);
    float3 t0 = (bmin - origin) * invDir;
    float3 t1 = (bmax - origin) * invDir;
    float3 tmin = min(t0, t1);
    float3 tmax = max(t0, t1);
    tMin = max(max(tmin.x, tmin.y), tmin.z);
    tMax = min(min(tmax.x, tmax.y), tmax.z);
    return tMax > tMin && tMax > 0;
}

// Sample density
float SampleDensity(float3 worldPos) {
    float3 texCoord = (worldPos - skyBoundsMin) / (skyBoundsMax - skyBoundsMin);
    if (any(texCoord < 0) || any(texCoord > 1)) {
        return 0.0;
    }
    return g_densityTexture.SampleLevel(g_trilinearSampler, texCoord, 0).r;
}

// Sample noise
float SampleNoise(float3 rayPos) {
    float scale = noiseScale;
    float3 noiseCoords = frac(rayPos * scale + float3(time * 0.1, -time * 0.05, -time * 0.01) * scrollFactor);
    if (scrolling == 0) {
        noiseCoords = rayPos * scale;
    }
    
    float perlinVal = perlinNoiseTexture.SampleLevel(g_trilinearSampler, noiseCoords, 0).r;
    float worleyBase = worleyNoiseTexture.SampleLevel(g_trilinearSampler, noiseCoords, 0).r;
    float noiseValue = saturate(lerp(perlinVal, worleyBase, pow(noiseLerpVal, 1.5f)));
    
    return pow(saturate(noiseValue), 2.0);
}

// Get orthographic ray from pixel (sun's perspective)
void GetOrthographicRay(float2 uv, out float3 rayOrigin, out float3 rayDir)
{
    float2 ndc = uv * 2.0 - 1.0;
    
    ndc.y *= -1.0;

    float4 lightSpacePos = float4(ndc, 0.0, 1.0);

    // Use the inverse matrices from CPU
    float4 worldNear = mul(InverseViewMatrix, mul(InverseProjMatrix, lightSpacePos));

    // Light direction
    rayDir = normalize(float3(InverseViewMatrix._11, InverseViewMatrix._21, InverseViewMatrix._31)); // same as before

    // Start the ray just before the volume
    rayOrigin = worldNear.xyz - rayDir * 1000.0f;
}

[numthreads(16, 16, 1)]
void ComputeMain(uint3 id : SV_DispatchThreadID) {
    uint width, height;
    g_shadowOutput.GetDimensions(width, height);
    
    if (id.x >= width || id.y >= height)
        return;
    
    // Get UV coordinates for this pixel
    float2 uv = (id.xy + 0.5) / float2(width, height);
    
    // Get orthographic ray from sun's perspective
    float3 rayOrigin, rayDir;
    GetOrthographicRay(uv, rayOrigin, rayDir);
    
    // Intersect with sky volume
    float tMin, tMax;
    if (!IntersectAABB(rayOrigin, rayDir, skyBoundsMin, skyBoundsMax, tMin, tMax)) {
        // No intersection - output max distance and no density
        // R = max distance, G = max distance, B = 0 (no density), A = 1 (full light)
        float maxDist = length(skyBoundsMax - skyBoundsMin);
        g_shadowOutput[id.xy] = float4(maxDist, maxDist, 0.0, 1.0);
        return;
    }
    
    // Initialize tracking variables
    tMin = max(tMin, 0.0);
    float t = tMin;
    float transmittance = 1.0;
    float shadowStepSize = stepSize * 2.0;
    int steps = 0;
    int maxShadowSteps = maxSteps / 2;
    
    // RGB encoding:
    float firstHitDistance = -1.0;   // R: First distance we entered a cloud
    float lastHitDistance = -1.0;    // G: Last distance we were in a cloud
    float totalDensity = 0.0;        // B: Accumulated density (0-1)
    bool wasInCloud = false;         // Track if we're currently in cloud
    
    // For normalizing distances to a reasonable range
    float maxRayDistance = tMax - tMin;
    
    while (t < tMax && steps < maxShadowSteps) {
        float3 pos = rayOrigin + rayDir * t;
        
        // Sample density at this position
        float density = SampleDensity(pos);
        
        if (useNoise == 1) {
            density *= SampleNoise(pos);
        }
        
        float finalDensity = density * densityScale * densityMultiplier;
        
        // Determine if we're in cloud (above threshold)
        bool inCloud = (finalDensity >= densityThreshold);
        
        if (inCloud) {
            // Record first hit if this is the first time entering cloud
            if (firstHitDistance < 0.0) {
                firstHitDistance = t;
            }
            
            // Always update last hit while in cloud
            lastHitDistance = t;
            wasInCloud = true;
            
            // Accumulate density - weight by step size
            // Normalize by a reasonable max value (adjust this based on your scene)
            totalDensity += finalDensity * shadowStepSize * 0.1; // 0.1 is a tuning factor
            
            // Accumulate extinction (proper Beer's law)
            float opticalDepth = finalDensity * extinctionCoefficient * shadowStepSize;
            transmittance *= exp(-opticalDepth);
            
            // Early exit if fully shadowed
            if (transmittance < 0.001) {
                break;
            }
            
            // Use smaller steps in dense areas for accuracy
            t += shadowStepSize;
        } else {
            // Empty space - use larger steps
            t += shadowStepSize * 2.0;
        }
        
        steps++;
    }
    
    // If we completed the march and were in cloud, set last distance to end
    if (wasInCloud && t >= tMax) {
        lastHitDistance = tMax;
    }
    
    // If we never hit a cloud, use sentinel values
    if (firstHitDistance < 0.0) {
        firstHitDistance = tMax;  // Max distance = no hit
        lastHitDistance = tMax;
    }
    
    // Normalize distances to [0,1] range based on ray length
    // This makes the visualization more useful
    float normalizedFirstHit = (firstHitDistance - tMin) / maxRayDistance;
    float normalizedLastHit = (lastHitDistance - tMin) / maxRayDistance;
    
    // Clamp total density to [0,1]
    totalDensity = saturate(totalDensity);
    
    // Clamp transmittance to minimum shadow factor
    float shadow = max(transmittance, shadowFactorMin);
    
    // OUTPUT FORMAT:
    // R = First hit distance (normalized 0-1)
    // G = Last hit distance (normalized 0-1)
    // B = Total accumulated density (0-1)
    // A = Shadow/transmittance value (for actual shadow sampling)
    g_shadowOutput[id.xy] = float4(
        normalizedFirstHit,
        normalizedLastHit,
        totalDensity,
        shadow
    );
}
