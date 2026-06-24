// SkyVolumeRaymarch.hlsl - Actual cloud rendering with raymarching

// Match your GPU structure exactly
struct OctreeNodeGPU {
    float3 boundsMin;
    int padding1;
    float3 boundsMax;
    int padding2;
    float averageDensity;
    float maxDensity;
    int childrenIndex;
    int densityIndex;
};

// Resources
StructuredBuffer<OctreeNodeGPU> g_octreeNodes : register(t0);
Texture3D<float> g_densityTexture : register(t1);
Texture3D<float> perlinNoiseTexture : register(t2);
Texture3D<float> worleyNoiseTexture : register(t3);
Texture2D<float4> g_shadowTexture : register(t4);
SamplerState g_trilinearSampler : register(s0);

// Output
RWTexture2D<float4> g_outputTexture : register(u0);

// Constants - expand your existing CloudConstants
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
    
    float4x4 shadowViewMatrix;
    float4x4 shadowProjectionMatrix;

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
    
    int showBoundingBoxes;
    int currentDepth;
    int useDensity;
    int invertNoise;
}

cbuffer DebugConstants : register(b7) {
    int debugMode;  // 0=normal, 1=show steps, 2=show density, 3=show octree level
    int2 debugPixel;
    float debugValue;
}

// Get ray direction from pixel
float3 GetRayDirection(float2 uv) {
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    
    float4 rayStart = mul(InverseProjMatrix, float4(ndc, 0.0, 1.0));
    rayStart /= rayStart.w;
    float3 worldDir = mul(InverseViewMatrix, float4(rayStart.xyz, 0.0)).xyz;
    return normalize(worldDir);;
}

// Ray-AABB intersection
bool IntersectAABB(float3 origin, float3 dir, float3 bmin, float3 bmax, 
                   out float tMin, out float tMax) {
    float3 invDir = 1.0 / (dir + 0.0001); // Avoid div by zero
    float3 t0 = (bmin - origin) * invDir;
    float3 t1 = (bmax - origin) * invDir;
    float3 tmin = min(t0, t1);
    float3 tmax = max(t0, t1);
    tMin = max(max(tmin.x, tmin.y), tmin.z);
    tMax = min(min(tmax.x, tmax.y), tmax.z);
    return tMax > tMin && tMax > 0;
}

int FindOctreeLeaf(float3 worldPos)
{
    int nodeIdx = 0;

    [unroll(10)]
    for (int depth = 0; depth < 10; depth++)
    {
        // earl exit for safety
        if (nodeIdx < 0 || nodeIdx > 10000)
        {
            return -1;
        }

        OctreeNodeGPU node = g_octreeNodes[nodeIdx];
        
        // Check if position is inside this node's bounds
        if (any(worldPos < node.boundsMin) || any(worldPos > node.boundsMax))
        {
            return -1;
        }

        // if this is a leaf node, we are done
        if (node.childrenIndex == -1)
        {
            return nodeIdx;
        }

        float3 center = (node.boundsMin + node.boundsMax) * 0.5f;
        
        int childoffset = 0;
        if (worldPos.x > center.x) childoffset |= 1;
        if (worldPos.y > center.y) childoffset |= 2;
        if (worldPos.z > center.z) childoffset |= 4;
        
        nodeIdx = node.childrenIndex + childoffset;
    }

    return -1;
}

float RayBoxExit(float3 rayOrigin, float3 rayDir, float3 boxMin, float3 boxMax) {
    float tMin, tMax;
    
    // Reuse existing IntersectAABB logic
    float3 invDir = 1.0 / (rayDir + 0.0001);
    float3 t0 = (boxMin - rayOrigin) * invDir;
    float3 t1 = (boxMax - rayOrigin) * invDir;
    float3 tmin = min(t0, t1);
    float3 tmax = max(t0, t1);
    
    tMin = max(max(tmin.x, tmin.y), tmin.z);
    tMax = min(min(tmax.x, tmax.y), tmax.z);
    
    // We want the EXIT distance
    // If we're inside the box (tMin < 0), return tMax
    // If we're outside approaching, return tMax (entry is handled elsewhere)
    return tMax;
}

float SampleNoise(float3 rayPos) {
    float scale = noiseScale;

    // Scrolling animation
    float3 noiseCoords = frac(rayPos * scale + float3(time * 0.1, -time * 0.05, -time * 0.01) * scrollFactor);
    if (scrolling == 0) {
        noiseCoords = rayPos * scale;
    }

    // Sample base noises
    float perlinVal = perlinNoiseTexture.SampleLevel(g_trilinearSampler, noiseCoords, 0).r;
    float worleyBase = worleyNoiseTexture.SampleLevel(g_trilinearSampler, noiseCoords, 0).r;
    
    // Medium detail
    float worleyMedium = worleyNoiseTexture.SampleLevel(g_trilinearSampler, noiseCoords * 2.0, 0.5f).r * 0.4f;
    
    // Fine detail
    float worleyDetail = worleyNoiseTexture.SampleLevel(g_trilinearSampler, noiseCoords * 6.0, 1.0f).r * 0.2f;
    
    // Combine
    float noiseValue = saturate(
        lerp(perlinVal, worleyBase, pow(noiseLerpVal, 1.5f))
    );

    return pow(saturate(noiseValue), 2.0); // noisePowVal hardcoded to 2 for now
}

// Sample density from 3D texture
float SampleDensity(float3 worldPos) {
    // Convert world to texture coordinates [0,1]
    float3 texCoord = (worldPos - skyBoundsMin) / (skyBoundsMax - skyBoundsMin);
    
    // Check bounds
    if (any(texCoord < 0) || any(texCoord > 1)) {
        return 0.0;
    }
    
    // Sample with trilinear filtering
    return g_densityTexture.SampleLevel(g_trilinearSampler, texCoord, 0).r;
}

// Henyey-Greenstein phase function for anisotropic scattering
float HenyeyGreensteinPhase(float3 viewDir, float3 lightDir, float g) {
    float cosTheta = dot(viewDir, lightDir);
    float gSquared = g * g;
    float denom = 1.0 + gSquared - 2.0 * g * cosTheta;
    return (1.0 - gSquared) / (4.0 * 3.14159265 * pow(abs(denom), 1.5));
}

// Powder effect (Beer's law approximation) - makes clouds look fluffy
float PowderEffect(float density, float cosTheta) {
    float powder = 1.0 - exp(-density * 2.0 * powderBias);
    // Enhance forward scattering
    powder = lerp(powder, 1.0, 0.5 * saturate(cosTheta));
    return powder;
}

// Improved lighting calculation with proper scattering
float3 CalculateLighting(float3 pos, float3 viewDir, float density) {
    
    float3 toSun = normalize(sunDirection);
    float3 toView = normalize(-viewDir);
    
    float transmittance = 1.0;
    float cloudThickness = 0.0;
    float accumulatedDensity = 0.0;
    float depthInCloud = 0.0;
    
    if (useShadowMap == 1) {
      
        float4 shadowPos = float4(pos, 1.0);
        
        float4 shadowViewPos = mul(shadowPos, shadowViewMatrix);
        float4 shadowClipPos = mul(shadowViewPos, shadowProjectionMatrix);
        float3 shadowNDC = shadowClipPos.xyz / shadowClipPos.w;
        
        // Convert NDC (-1,1) to UV (0,1)
        float2 shadowUV;
        shadowUV.x = shadowNDC.x * 0.5 + 0.5;
        shadowUV.y = -shadowNDC.y * 0.5 + 0.5; // Flip Y for DX11
        
        // Check if within shadow map bounds
        if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 && 
            shadowUV.y >= 0.0 && shadowUV.y <= 1.0 &&
            shadowNDC.z >= 0.0 && shadowNDC.z <= 1.0) {
            
            // Sample shadow map
            float4 shadowData = g_shadowTexture.SampleLevel(g_trilinearSampler, shadowUV, 0);
            
            float firstHitDist = shadowData.r;
            float lastHitDist = shadowData.g;
            accumulatedDensity = shadowData.b;
            transmittance = shadowData.a; // Direct transmittance through cloud
            
            // Calculate cloud thickness from sun's perspective
            cloudThickness = max(0, lastHitDist - firstHitDist);
            
            // Calculate how deep we are into the cloud from sun's view
            // This helps with volumetric effects
            float currentDistFromSun = shadowNDC.z * 1000.0; // Scale to your world units
            if (currentDistFromSun > firstHitDist && currentDistFromSun < lastHitDist) {
                depthInCloud = (currentDistFromSun - firstHitDist) / cloudThickness;
                depthInCloud = saturate(depthInCloud);
            }
        }
    }
    else {
             // Fallback: inline shadow march (your existing code)
        float3 shadowPos = pos;
        float shadowStepSize = 15.0;
        int numShadowSteps = 6;
        
        for (int i = 0; i < numShadowSteps; i++) {
            shadowPos += toSun * shadowStepSize;
            float shadowDensity = SampleDensity(shadowPos);
            
            if (useNoise == 1) {
                float shadowNoise = SampleNoise(shadowPos);
                shadowDensity *= shadowNoise;
            }
            
            accumulatedDensity += shadowDensity;
            transmittance *= exp(-shadowDensity * extinctionCoefficient * shadowStepSize);
            
            if (transmittance < 0.01) break;
        }
        
        transmittance = max(transmittance, shadowFactorMin);
    }
    
    float cosTheta = dot(toView, toSun);
    float hg = HenyeyGreensteinPhase(toView, toSun, anisotropyG);
    
    // 2. Powder effect - enhanced with cloud thickness knowledge
    float powder = PowderEffect(density, cosTheta);
    // Thicker clouds = stronger powder effect
    if (cloudThickness > 0) {
        powder *= (1.0 + cloudThickness * 0.001); // Scale based on your units
    }

      // 3. Direct sunlight - using transmittance from shadow map
    float3 directLight = lightColor * sunIntensity * transmittance * hg * powder;
    
    // 4. Multiple scattering approximation
    // Use accumulated density to estimate how much light has been scattered
    float multiScatterStrength = 1.0 - exp(-accumulatedDensity * 0.1);
    multiScatterStrength *= (1.0 - transmittance); // More scattering where less direct light
    
    // Forward scattering is stronger (silver lining effect)
    float forwardScatterBoost = saturate(cosTheta * 0.5 + 0.5);
    multiScatterStrength *= lerp(0.5, 1.5, forwardScatterBoost);
    
    float3 multiScatterLight = lightColor * sunIntensity * 0.3 * multiScatterStrength;
    
    // 5. Ambient light with depth-based occlusion
    // Deeper in cloud = less ambient reaches us
    float ambientOcclusion = lerp(1.0, 0.3, depthInCloud);
    ambientOcclusion *= lerp(0.5, 1.0, transmittance); // Also affected by shadow
    float3 ambientLight = float3(0.4, 0.5, 0.7) * ambientIntensity * ambientOcclusion;
    
    // 6. Sky light contribution
    float upDot = saturate(dot(toView, float3(0, 1, 0)));
    // Less sky light deep in cloud
    float skyOcclusion = lerp(1.0, 0.2, depthInCloud);
    float3 skyLight = float3(0.3, 0.4, 0.6) * ambientIntensity * 0.5 * upDot * skyOcclusion;
    
    // 7. "Silver lining" effect - bright edges when backlit
    float silverLining = 0.0;
    if (cosTheta < -0.2) { // Looking toward sun
        // Strong effect at cloud edges (low accumulated density)
        silverLining = exp(-accumulatedDensity * 2.0) * transmittance;
        silverLining *= saturate(-cosTheta - 0.2) * 2.0;
    }
    float3 silverLight = lightColor * sunIntensity * silverLining * 0.5;
    
    // Combine all lighting
    float3 totalLight = directLight + multiScatterLight + ambientLight + skyLight + silverLight;
    
    return totalLight;
}

[numthreads(16, 16, 1)]
void ComputeMain(uint3 id : SV_DispatchThreadID) {
    // Get screen dimensions
    uint width, height;
    g_outputTexture.GetDimensions(width, height);
    
    if (id.x >= width || id.y >= height)
        return;
    
    // Get ray for this pixel
    float2 uv = (id.xy + 0.5) / float2(width, height);
    float3 rayOrigin = CameraPosition;
    float3 rayDir = GetRayDirection(uv);
    
// DEBUG: Check octree root node data - ONLY CENTER PIXEL
//if (id.x == width/2 && id.y == height/2) {
//       OctreeNodeGPU rootNode = g_octreeNodes[0];
//    
//    // Calculate differences
//    float3 minDiff = abs(rootNode.boundsMin - skyBoundsMin);
//    float3 maxDiff = abs(rootNode.boundsMax - skyBoundsMax);
//    
//    // Show the differences as colors
//    // R = X difference, G = Y difference, B = Z difference
//    
//    // Check which component has the biggest mismatch
//    float maxMinDiff = max(minDiff.x, max(minDiff.y, minDiff.z));
//    float maxMaxDiff = max(maxDiff.x, max(maxDiff.y, maxDiff.z));
//    
//    if (maxMinDiff > 1.0 || maxMaxDiff > 1.0) {
//        // Show which axis is wrong
//        // Encode the difference magnitude into color
//        float r = saturate(maxMinDiff / 1000.0);  // X difference
//        float g = saturate(maxMaxDiff / 1000.0);  // Y difference
//        g_outputTexture[id.xy] = float4(r, g, 0, 1);
//        return;
//    }
//    
//    g_outputTexture[id.xy] = float4(0, 1, 0, 1); // GREEN if close enough
//    return;
//}
//
// Intersect with sky volume bounds
float tMin, tMax;
    if (!IntersectAABB(rayOrigin, rayDir, skyBoundsMin, skyBoundsMax, tMin, tMax)) {
        g_outputTexture[id.xy] = float4(0, 0, 0, 0);
        return;
    }
    
    // DEBUG MODE 1: Visualize ray steps
    if (debugMode == 1) {
       float stepCount = (tMax - tMin) / stepSize;
        // Normalize to a useful range (e.g., 0-50 steps = black to red)
        float stepVis = saturate(stepCount / 50.0); // Instead of maxSteps
        
        // Add distance banding for better visualization
        if (stepCount < 10) {
            g_outputTexture[id.xy] = float4(0, 0, stepVis, 1); // Blue for close
        } else if (stepCount < 30) {
            g_outputTexture[id.xy] = float4(0, stepVis, 0, 1); // Green for medium
        } else {
            g_outputTexture[id.xy] = float4(stepVis, 0, 0, 1); // Red for far
        }

        return;
    }
    
    // DEBUG MODE 2: Visualize density directly
    if (debugMode == 2) {
        float3 samplePos = rayOrigin + rayDir * (tMin + (tMax - tMin) * 0.5);
        float density = SampleDensity(samplePos);
        float densVis = saturate(density * 5.0); // Scale for visibility
        g_outputTexture[id.xy] = float4(densVis, densVis * 0.5, 0, densVis);
        return;
    }
    
    // DEBUG MODE 3: Visualize octree traversal
    if (debugMode == 3) {
        // Traverse octree and count nodes hit
        int nodeCount = 0;
        float t = tMin;
        while (t < tMax && nodeCount < 100) {
            float3 pos = rayOrigin + rayDir * t;
            
            // Find octree node at this position
            int nodeIdx = 0;
            int depth = 0;
            bool foundLeaf = false;
            
            // Simple traversal (you'll need to expand this)
            [loop]
            for (int iter = 0; iter < 10; iter++) {
                if (nodeIdx >= 0 && nodeIdx < 10000) { // Safety check
                    OctreeNodeGPU node = g_octreeNodes[nodeIdx];
                    
                    // Check if we're in bounds
                    if (pos.x < node.boundsMin.x || pos.x > node.boundsMax.x ||
                        pos.y < node.boundsMin.y || pos.y > node.boundsMax.y ||
                        pos.z < node.boundsMin.z || pos.z > node.boundsMax.z) {
                        break;
                    }
                    
                    if (node.childrenIndex == -1) {
                        foundLeaf = true;
                        nodeCount++;
                        break;
                    }
                    
                    // Find which child
                    float3 center = (node.boundsMin + node.boundsMax) * 0.5;
                    int childIdx = 0;
                    if (pos.x > center.x) childIdx |= 1;
                    if (pos.y > center.y) childIdx |= 2;
                    if (pos.z > center.z) childIdx |= 4;
                    
                    nodeIdx = node.childrenIndex + childIdx;
                    depth++;
                }
            }
            
            t += stepSize * 2.0; // Larger steps for debug
        }
        
        // Color based on octree nodes traversed
        float nodeVis = saturate(nodeCount / 20.0);
        g_outputTexture[id.xy] = float4(0, nodeVis, 1.0 - nodeVis, 1);
        return;
    }
    
    // DEBUG MODE 4: Show specific pixel ray (if matches debug pixel)
    if (debugMode == 4 && all(id.xy == debugPixel)) {
        // Highlight the debug pixel
        g_outputTexture[id.xy] = float4(1, 0, 1, 1); // Magenta
        return;
    }
    
    // NORMAL RENDERING MODE (debugMode == 0)
    float4 result = float4(0, 0, 0, 0);
    float transmittance = 1.0;
    
    tMin = max(tMin, 0.0);
    float t = tMin;
    
    int steps = 0;
    int octreeSkips = 0;
    float distanceTraveled = 0.0;
    float totalDensity = 0.0; // Track for debug
    
    while (t < tMax && steps < maxSteps && transmittance > 0.01) {
        float3 pos = rayOrigin + rayDir * t;
        
        int nodeIdx = FindOctreeLeaf(pos);
        
        if (nodeIdx >= 0)
        {
            OctreeNodeGPU node = g_octreeNodes[nodeIdx];

            float scaledMaxDensity = node.maxDensity * densityScale * densityMultiplier;

            if (scaledMaxDensity < densityThreshold)
            {
                float exitDist = RayBoxExit(pos, rayDir, node.boundsMin, node.boundsMax);
                
                float skipDist = max(exitDist, minStepSize);
                t += skipDist;
                steps++;
                octreeSkips++;
                continue;
            }
        }

        float baseDensity = SampleDensity(pos);

        // Early skip if no density
        if (baseDensity < 0.001) {
            t += stepSize * 2.0; // Larger steps in empty space
            steps++;
            continue;
        }
        
        float noiseValue = 1.0;
        if (useNoise == 1) {
            noiseValue = SampleNoise(pos);
        }
        
        // Combine density with noise
        float finalDensity = baseDensity * noiseValue * densityScale * densityMultiplier;
        
        // Apply density threshold
        if (finalDensity < densityThreshold) {
            t += stepSize;
            steps++;
            continue;
        }
        
        // Calculate lighting at this point
        float3 lighting = CalculateLighting(pos, rayDir, finalDensity);
        
        // PHYSICALLY-BASED ABSORPTION AND SCATTERING
        
        // Total extinction (absorption + out-scattering)
        float extinction = finalDensity * extinctionCoefficient * stepSize;
        
        // Split extinction into absorption and out-scattering
        // For realistic clouds: ~10% absorption, ~90% scattering
        float absorptionRatio = lightAbsorption; // Use your lightAbsorption parameter (should be ~0.1)
        float absorptionCoeff = extinction * absorptionRatio;
        float outScatterCoeff = extinction * (1.0 - absorptionRatio);
        
        // Beer-Lambert law for transmission through this step
        float stepTransmittance = exp(-extinction);
        
        // How much energy is removed from the beam at this step
        float energyExtincted = 1.0 - stepTransmittance;
        
        // In-scattering: light scattered INTO our view direction
        // This is what makes clouds visible
        float inScatterAmount = finalDensity * scatteringCoefficient * stepSize;
        
        // Cloud color (slightly cool white for realism)
        float3 cloudColor = float3(0.95, 0.96, 1.0);
        
        // Calculate radiance contribution from this step
        // Lighting * in-scattering * cloud color * remaining transmittance
        float3 stepRadiance = lighting * cloudColor * inScatterAmount * transmittance;
        
        // Add this step's contribution to the result
        result.rgb += stepRadiance;
        
        // Opacity contribution (how much this step blocks the background)
        float stepOpacity = energyExtincted * transmittance;
        result.a += stepOpacity;
        
        // Update transmittance for next step
        transmittance *= stepTransmittance;
        
        // ADAPTIVE STEPPING (keep your existing logic)
        float densityFactor = 1.0;
        if (finalDensity < densityThreshold * 2.0) {
            densityFactor = farMultiplier; // Larger steps in low density
        }
    
        float distanceFactor = lerp(1.0, farMultiplier, 
        saturate(distanceTraveled / farDistanceThreshold));
    
        float adaptiveStep = max(minStepSize, stepSize) * max(densityFactor, distanceFactor);
    
        t += adaptiveStep;
        distanceTraveled += adaptiveStep;
        steps++;
    }
    
    // DEBUG MODE 5: Overlay stats on normal render
    if (debugMode == 5) {
        // Add a colored border showing performance
        if (id.x < 2 || id.y < 2 || id.x > width - 3 || id.y > height - 3) {
            float perfColor = saturate(steps / maxSteps);
            result = float4(perfColor, 1.0 - perfColor, 0, 1);
        }
    }
    
    result.a = saturate(result.a);
    float alphaFade = smoothstep(0.0, minAccepted, result.a);
    result.a *= alphaFade;
    
    result.rgb *= saturate(result.a * 1.5);

    if (result.a < 0.01) {
        result = float4(0, 0, 0, 0);
    }

    g_outputTexture[id.xy] = result;
}