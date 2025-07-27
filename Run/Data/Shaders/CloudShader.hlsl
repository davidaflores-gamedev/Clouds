// Struct definitions
struct Voxel {
    float3 position;
    float density;
    float moisture;
    float temperature;
};

struct Cloud {
    float3 position;
    int3 gridDimensions;
    float3 minBounds;
    float3 maxBounds;
    unsigned int voxelOffset;
    unsigned int voxelCount;
    unsigned int octreeOffset;
};

struct OctreeNode
{
    float3 minBounds; 
    float3 maxBounds;
    float densitySum;             // Aggregate density
    unsigned int firstChildIndex; // Index of the first child (-1 if leaf)
    unsigned int numChildren;     // Number of children (0 if leaf)
    unsigned int firstVoxelIndex; // Index of the first voxel (-1 if none)
    unsigned int numVoxels;       // Number of voxels in this node
    unsigned int depth;           // Depth of this node
};

// Constant buffers
cbuffer LightConstants : register(b1) {
    float3 SunDirection;
    float SunIntensity;
    float AmbientIntensity;
};

cbuffer CameraConstants : register(b2) {
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InverseViewMatrix;
    float4x4 InverseProjMatrix;
    float3 CameraPosition;
    float2 NearScreenSize;
};

cbuffer CloudConstants : register(b6) {
    int     numClouds;
    int     numOctrees;
    float   timeElapsed;
    int     useDensity;

    int     useNoise;
    int     invertNoise;
    float   densityMultiplier;
    int     scrolling;

    int     showBoundingBoxes;
    int     currentDepth;
    float   extinctionCoefficient;
    float   scatteringCoefficient;

    float3  cloadColor;
    int     padding1;

    float3  voxelDimensions;
    int     padding2;

    float   minStepSize;
    float   noiseScale;
    float   noiseLerpVal;
    float   densityNoiseLerpVal;

    float   cloudVoxelDistanceLerpVal;
    float   minWorleyValue;
    float   noisePowVal;
    float   densityThreshold;

    float   scrollFactor;
    float   farDistanceThreshold;
    float   farMultiplier;
    float   shadowFactorMin;

    float   powderBias;
    float   anisotropyG;
    float   densityFalloff;
    float   padding3;
};

cbuffer ShadowConstants : register(b7) {
    float4x4 LightViewProj;
};

// Resources
StructuredBuffer<Cloud>         clouds              : register(t0);
StructuredBuffer<Voxel>         voxels              : register(t1);
Texture3D<float>                perlinNoiseTexture  : register(t2);
Texture3D<float>                worleyNoiseTexture  : register(t3);
StructuredBuffer<OctreeNode>    octreeNodes         : register(t4);
Texture2D<float4>               voxelShadowMap        : register(t5);
SamplerState                    samplerState        : register(s0);
RWTexture2D<float4>             outputTexture       : register(u0);

// Shader constants



// Helper functions
float ApplyBeersLaw(float density, float distance) {
    return exp(-extinctionCoefficient * density * distance);
}

float PowderEffect(float3 viewDir, float3 lightDir, float bias) {
    viewDir = normalize(viewDir);
    lightDir = normalize(lightDir);
    
    float dotVL = dot(viewDir, lightDir);  // Preserve sign
    float adjustedDot = max(dotVL, 0.0f) + bias;  // Clamp to [0,1]])
    float shiftedDot = saturate(adjustedDot);  // Apply bias, clamp to [0,1]
    
    return pow(shiftedDot, scatteringCoefficient);
}

float BoxSDF(float3 p, float3 voxelPos, float3 boxSize) {
    float3 localPos = p - voxelPos;
    float3 d = abs(localPos) - boxSize;
    float outsideDist = length(max(d, 0.0));
    float insideDist = min(max(d.x, max(d.y, d.z)), 0.0);
    return outsideDist + insideDist;

//the value returned will be positive if the point is outside the box, negative if inside, and zero if on the surface
}

float FindAABBExitPoint(float3 rayOrigin, float3 rayDir, float3 minBounds, float3 maxBounds)
{
    float3 invRayDir = 1.0f / rayDir; // Precompute inverse direction
    float3 t1 = (minBounds - rayOrigin) * invRayDir;
    float3 t2 = (maxBounds - rayOrigin) * invRayDir;

    float3 tMax3 = max(t1, t2); // Get the exit times along each axis
    return min(tMax3.x, min(tMax3.y, tMax3.z)); // Smallest exit time is when the ray leaves the AABB
}

float3 ComputeRayDirection(float2 uv) {
    float2 ndc = uv;
    ndc.y = -ndc.y;
    float4 rayStart = mul(InverseProjMatrix, float4(ndc, 0.0, 1.0));
    rayStart /= rayStart.w;
    float3 worldDir = mul(InverseViewMatrix, float4(rayStart.xyz, 0.0)).xyz;
    return normalize(worldDir);
}

//float LargeScaleWorley(float2 pos) {
//    float worleyVal = worleyNoiseTexture.SampleLevel(samplerState, float3(pos.x, pos.y, .5f) * 0.5f, 0).r; // Larger scale
//    return pow(saturate(worleyVal), 3.0f); // Increase contrast for stronger cloud edges
//}

float SampleNoise(float3 rayPos) {
    float scale = noiseScale / voxelDimensions;

    // Apply scrolling movement to the noise coordinates
    float3 noiseCoords = frac(rayPos * scale + float3(timeElapsed * 0.1, -timeElapsed * 0.05, -timeElapsed * 0.01) * scrollFactor);
    float3 worleyCoords = noiseCoords; // Scale down for Worley noise
    if (scrolling == 0) {
        noiseCoords = rayPos * scale;
    }

    // Sample base Perlin and Worley noise
    float perlinVal = perlinNoiseTexture.SampleLevel(samplerState, noiseCoords, 0).r;
    float worleyBase = worleyNoiseTexture.SampleLevel(samplerState, worleyCoords, 0).r;
    
    //// Threshold the base Worley noise to create initial structure
    if (worleyBase <= minWorleyValue) {
        worleyBase = 0.0f;
    }
    
    float midFrequency = 2.0f; // Increase frequency for medium details
    float worleyMedium = worleyNoiseTexture.SampleLevel(samplerState, worleyCoords * midFrequency, 0.5f).r * 0.4f;
    //if (worleyMedium <= minWorleyValue) {
    //    worleyMedium = 0.0f;
    //}
    
    // Detail octave: higher frequency Worley noise for fine textures
    float detailFrequency = 6.0f; // Increase frequency for finer details
    float worleyDetail = worleyNoiseTexture.SampleLevel(samplerState, worleyCoords * detailFrequency, 1.0f).r * 0.2f;
    //if (worleyDetail <= minWorleyValue) {
    //    worleyDetail = 0.0f;
    //}
    
    
    
    float3 maskCoords = frac(float3(rayPos.y, rayPos.x, rayPos.z) * scale + float3(timeElapsed * 0.1, -timeElapsed * 0.05, -timeElapsed * 0.01) * scrollFactor) * .1f;
    
    // Mask octave: lower frequency Worley noise to control where detail appears
    float maskFrequency = .50f; // Lower frequency for large-scale structure
    float worleyMaskVal = 1 - worleyNoiseTexture.SampleLevel(samplerState, maskCoords * maskFrequency, 0).r;
    float detailMask = pow(smoothstep(0.4f, 0.6f, worleyMaskVal), 1.5f);
    
    // Blend base and detail Worley noise using the mask
    float blendedWorley = saturate(
    worleyBase * 0.6f +  // Strongest influence from base structure
    worleyMedium * 0.3f + // Medium-scale refinement
    worleyDetail * 0.1f   // Fine details
    );
    
    // Combine Perlin and blended Worley noise
    float noiseValue = saturate(
    lerp(perlinVal, worleyBase, pow(noiseLerpVal, 1.5f))
    );

    // Optionally apply a large-scale cloud mask (if you want additional control)
    // float cloudMask = LargeScaleWorley(rayPos.xz);
    // noiseValue *= cloudMask;

    // Apply settings for optional noise inversion and toggling
    if (useNoise == 0) noiseValue = 0.5f;
    if (invertNoise == 1) noiseValue = 1.0f - noiseValue;

    // Enhance contrast and ensure value is within [0,1]
    return pow(saturate(noiseValue), noisePowVal);
}

// Accumulating density for rendering clouds
float AccumulateDensity(Voxel voxel, float noiseValue) {
    float densityWeight = 0.3f; // Lower = less impact from voxel density
    float finalDensity = lerp(voxel.density, noiseValue, densityNoiseLerpVal) * densityMultiplier;
    if (useDensity == 0) finalDensity = 1.0f;
    return finalDensity;
}

float3 ComputeVoxelNormal(float3 localPos) {
    // Assuming localPos is relative to voxel center (ranges from -0.5 to 0.5)
    float3 absLocalPos = abs(localPos);
    
    if (absLocalPos.x >= absLocalPos.y && absLocalPos.x >= absLocalPos.z) {
        // X face is the furthest
        return float3(sign(localPos.x), 0.0, 0.0);
    } else if (absLocalPos.y >= absLocalPos.x && absLocalPos.y >= absLocalPos.z) {
        // Y face is the furthest
        return float3(0.0, sign(localPos.y), 0.0);
    } else {
        // Z face is the furthest
        return float3(0.0, 0.0, sign(localPos.z));
    }
}

// Helper function for the Henyey-Greenstein phase function
float HenyeyGreensteinPhaseFunction(float3 viewDir, float3 lightDir, float g) {
    float cosTheta = dot(viewDir, lightDir);
    float gSquared = g * g;
    float denom = 1.0 + gSquared - 2.0 * g * cosTheta;
    return (1.0 - gSquared) / pow(denom, 1.5);
}

float TraverseOctree(uint rootNodeIndex, float3 rayPos, float3 rayDir, inout float minSDF, inout uint closestNodeIndex) {
    uint stack[1024]; // A simple stack to hold node indices
    int stackPointer = 0; // Stack pointer to manage stack position
    

    // Push the root node onto the stack
    stack[stackPointer++] = rootNodeIndex;

    

    while (stackPointer > 0) {
        // Pop a node from the stack
        uint nodeIndex = stack[--stackPointer];
        OctreeNode node = octreeNodes[nodeIndex];

        // Compute SDF for this node's bounding box
        float3 nodeCenter = (node.minBounds + node.maxBounds) * 0.5f;
        float3 nodeSize = (node.maxBounds - node.minBounds) * 0.5f;
        float distanceToNode = BoxSDF(rayPos, nodeCenter, nodeSize);

        if(distanceToNode < minSDF)
        {
                minSDF = distanceToNode;
        }


        if (distanceToNode <= 0.f) {
            closestNodeIndex = nodeIndex;
        
            // If the node has children, push them onto the stack
            
            float nodeDensity = node.densitySum;                

            if(nodeDensity < densityThreshold) {
                // Skip processing if density is too low
                continue;
            }
            
            if (node.numChildren > 0) {
                for (uint i = 0; i < node.numChildren; ++i) {
                    stack[stackPointer++] = node.firstChildIndex + i;
                }
            }
        }
        //}
    }

    return minSDF;
}       

// Compute diffuse lighting for a voxel, considering shadows and including Henyey-Greenstein
float ComputeLightingWithShadowsOctree(Voxel rayVoxel, float3 rayPosition, float3 sunDirection, float3 normal, float density, float3 viewDir) {
    float NdotL = max(dot(normal, -sunDirection), 0.0f); // Diffuse lighting term

    // Adaptive boost to ensure sufficient lighting
    NdotL = pow(NdotL, 0.5f); // Use an exponent to control brightness more naturally
    float densityFactor = 1.0f - density; // Lower density allows more light

    // March towards the light to compute shadowing using SDF, similar to the ray marching function
    float lightAccumulation = 1.0f;
    float lightDistanceTraveled = 0.0f;
    float lightMaxDistance = 40.0f;
    float lightStepSize = 0.02f; // Increased to avoid overly dense sampling
    float3 lightRayPos = rayPosition;

    while (lightDistanceTraveled < lightMaxDistance) {
        float minSDF = 100000.0f;
        int closestCloudIndex = -1;

        // Check each cloud's bounding box
        for (int cloudIndex = 0; cloudIndex < numClouds; ++cloudIndex) {
            Cloud cloud = clouds[cloudIndex];
            float distanceToCloud = BoxSDF(lightRayPos, (cloud.maxBounds + cloud.minBounds) * 0.5f, (cloud.maxBounds - cloud.minBounds) * 0.5f);

            if (distanceToCloud < minSDF) {
                minSDF = distanceToCloud;
                closestCloudIndex = cloudIndex;
            }
        }

        if (closestCloudIndex != -1) {
            Cloud cloud = clouds[closestCloudIndex];

            if (minSDF <= 0.0f) { // Light ray is inside or near bounding box
                // March through voxels in the cloud
                for (int i = 0; i < cloud.voxelCount; ++i) {
                    Voxel voxel = voxels[cloud.voxelOffset + i];
                    float distanceToVoxel = BoxSDF(lightRayPos, voxel.position, voxelDimensions);

                    if (distanceToVoxel <= 0.05f) { // Light ray is inside voxel
                        if (rayVoxel.position.r != voxel.position.r || rayVoxel.position.b != voxel.position.b || rayVoxel.position.g != voxel.position.g) {
                            lightAccumulation *= 0.97f; // Reduce light for other voxels, but not too aggressively
                            break; // Exit since it's shadowing from another voxel
                        }
                        return 1.f;

                        float noiseValue = SampleNoise(lightRayPos);
                        float shadowDensity = AccumulateDensity(voxel, noiseValue);

                        // New logic to differentiate between self-shadowing and other voxel shadowing
                        float3 voxelOffset = voxel.position - rayVoxel.position;
                        float voxelDistance = length(voxelOffset);
                        float selfShadowThreshold = 0.1f; // Threshold to consider it the "same" voxel for shadowing purposes

                        if (voxelDistance < selfShadowThreshold) {
                            // Shadowing from the same voxel (or very close neighbors)
                            lightAccumulation *= 0.95f; // Reduce slightly for self-shadowing
                        } else {
                            // Shadowing from other voxels
                            if (shadowDensity > 0.01f) {
                                lightAccumulation *= exp(-extinctionCoefficient * shadowDensity * lightStepSize);
                            }
                        }
                    }
                }
            }
        }

        lightStepSize = max(minSDF, 0.02f); // Ensure efficient stepping
        lightRayPos += -sunDirection * lightStepSize;
        lightDistanceTraveled += lightStepSize;
    }

    // Add a constant ambient contribution to avoid too much darkness
    float ambientContribution = 0.2f * AmbientIntensity;

    // Henyey-Greenstein phase function for better scattering
    float phaseFunction = HenyeyGreensteinPhaseFunction(viewDir, -sunDirection, anisotropyG);

    // Combine diffuse, density, ambient lighting, shadowing, and phase function
    return ((NdotL * densityFactor * SunIntensity) + ambientContribution) * lightAccumulation * phaseFunction;
}

// Compute diffuse lighting for a voxel, considering shadows and including Henyey-Greenstein
float ComputeLightingWithShadows(Voxel rayVoxel, float3 rayPosition, float3 sunDirection, float3 normal, float density, float3 viewDir) {
    float NdotL = max(dot(normal, -sunDirection), 0.0f); // Diffuse lighting term

    // Adaptive boost to ensure sufficient lighting
    NdotL = pow(NdotL, 0.5f); // Use an exponent to control brightness more naturally
    float densityFactor = 1.0f - density; // Lower density allows more light

    // March towards the light to compute shadowing using SDF, similar to the ray marching function
    float lightAccumulation = 1.0f;
    float lightDistanceTraveled = 0.0f;
    float lightMaxDistance = 40.0f;
    float lightStepSize = 0.02f; // Increased to avoid overly dense sampling
    float3 lightRayPos = rayPosition;

    while (lightDistanceTraveled < lightMaxDistance) {
        float minSDF = 100000.0f;
        int closestCloudIndex = -1;

        // Check each cloud's bounding box
        for (int cloudIndex = 0; cloudIndex < numClouds; ++cloudIndex) {
            Cloud cloud = clouds[cloudIndex];
            float distanceToCloud = BoxSDF(lightRayPos, (cloud.maxBounds + cloud.minBounds) * 0.5f, (cloud.maxBounds - cloud.minBounds) * 0.5f);

            if (distanceToCloud < minSDF) {
                minSDF = distanceToCloud;
                closestCloudIndex = cloudIndex;
            }
        }

        if (closestCloudIndex != -1) {
            Cloud cloud = clouds[closestCloudIndex];

            if (minSDF <= 0.0f) { // Light ray is inside or near bounding box
                // March through voxels in the cloud
                for (int i = 0; i < cloud.voxelCount; ++i) {
                    Voxel voxel = voxels[cloud.voxelOffset + i];
                    float distanceToVoxel = BoxSDF(lightRayPos, voxel.position, float3(1.f, 1.f, 1.f) * 0.5f);

                    if (distanceToVoxel <= 0.05f) { // Light ray is inside voxel
                        if (rayVoxel.position.r != voxel.position.r || rayVoxel.position.b != voxel.position.b || rayVoxel.position.g != voxel.position.g) {
                            lightAccumulation *= 0.97f; // Reduce light for other voxels, but not too aggressively
                            break; // Exit since it's shadowing from another voxel
                        }
                        return 1.f;

                        float noiseValue = SampleNoise(lightRayPos);
                        float shadowDensity = AccumulateDensity(voxel, noiseValue);

                        // New logic to differentiate between self-shadowing and other voxel shadowing
                        float3 voxelOffset = voxel.position - rayVoxel.position;
                        float voxelDistance = length(voxelOffset);
                        float selfShadowThreshold = 0.1f; // Threshold to consider it the "same" voxel for shadowing purposes

                        if (voxelDistance < selfShadowThreshold) {
                            // Shadowing from the same voxel (or very close neighbors)
                            lightAccumulation *= 0.95f; // Reduce slightly for self-shadowing
                        } else {
                            // Shadowing from other voxels
                            if (shadowDensity > 0.01f) {
                                lightAccumulation *= exp(-extinctionCoefficient * shadowDensity * lightStepSize);
                            }
                        }
                    }
                }
            }
        }

        lightStepSize = max(minSDF, 0.02f); // Ensure efficient stepping
        lightRayPos += -sunDirection * lightStepSize;
        lightDistanceTraveled += lightStepSize;
    }

    // Add a constant ambient contribution to avoid too much darkness
    float ambientContribution = 0.2f * AmbientIntensity;

    // Henyey-Greenstein phase function for better scattering
    float phaseFunction = HenyeyGreensteinPhaseFunction(viewDir, -sunDirection, anisotropyG);

    // Combine diffuse, density, ambient lighting, shadowing, and phase function
    return ((NdotL * densityFactor * SunIntensity) + ambientContribution) * lightAccumulation * phaseFunction;
}

// Ray marching function with octree traversal
float4 RayMarchOctree(float2 uv)
{
    float3 rayPos           = CameraPosition;
    float3 rayDir           = ComputeRayDirection(uv);
    float  distanceTraveled = 0.0f;
    float  maxDistance      = 500.0f;
    float3 transmittance    = float3(1.f,1.f,1.f);
    float4 finalColor       = float4(0.f,0.f,0.f,0.f);
    float  totalDensity     = 0.0f;

    float minStep = minStepSize * voxelDimensions.x;
    float minDist = 0.02f;

    const float lowDensityThreshold = 0.1f; // Threshold for low density
    const float densityMultiplier   = 2.0f;  // Multiplier when density is low

    const float leafMultiplier = 5.0f; // Multiplier for leaf nodes


    while (distanceTraveled < maxDistance)
    {
        //--------------------------------------------------
        // 1) Check which cloud bounding box is nearest
        //--------------------------------------------------
        float minDistCloud = 100000;
        int   closestCloudIndex = -1;

        for (int ci = 0; ci < numClouds; ci++) {
            Cloud c = clouds[ci];
            float distToCloud = BoxSDF(rayPos,
                                       0.5f*(c.minBounds + c.maxBounds),
                                       0.5f*(c.maxBounds - c.minBounds));

            if(distToCloud > maxDistance)
            {
                continue;
            }

            if (distToCloud < minDistCloud) {
                minDistCloud      = distToCloud;
                closestCloudIndex = ci;
            }
        }

        //--------------------------------------------------
        // 2) If we found a cloud, see if we are inside it
        //-------------------------------------------------- 
        // If minDistCloud is large, we skip forward a big step
        float stepSize = max(minDistCloud, minStep);

        if (closestCloudIndex >= 0 && minDistCloud < 0.1f) {
            Cloud cloud = clouds[closestCloudIndex];

            //---------------------------------------------
            // 3) Traverse the cloud's octree:
            //    This call finds the nearest node bounding box
            //---------------------------------------------
            float minSDF = 100000.0f;
            uint  closestNodeIndex = 0xFFFFFFFF;

            // Assuming cloud.octreeOffset is the root node for this cloud
            TraverseOctree(cloud.octreeOffset, rayPos, rayDir, minSDF, closestNodeIndex);

            //---------------------------------------------
            // 4) If we are close enough to that node, process it
            //---------------------------------------------
            if (minSDF <= minDist) {
                OctreeNode node = octreeNodes[closestNodeIndex];

                // If node has no children => it's a leaf
                if (node.numChildren == 0) {
                    // We only handle a small # of voxels
                    for (uint v = 0; v < node.numVoxels; v++) {
                        // Get the voxel from the node
                        uint voxelIdx = node.firstVoxelIndex + v;
                        Voxel voxel = voxels[voxelIdx];

                        // Check if inside the voxel
                        float distToVoxel = BoxSDF(rayPos, voxel.position,  voxelDimensions * .5f);
                        if (distToVoxel < minDist) {
                            // If so, accumulate lighting & color, etc.
                            float noiseVal   = SampleNoise(rayPos);
//                             if (noiseVal < 0.1f) {
//                                 noiseVal = 0.f;
//                             }
                            float densityVal = AccumulateDensity(voxel, noiseVal);
                           
                            float voxelDist = length(rayPos - voxel.position);
                            float voxelMaxRadius = length(voxelDimensions * 0.5f);
                            float normalizedVoxelDist = saturate(voxelDist / voxelMaxRadius);
                                
                            // -- Cloud Distance --
                            // Compute cloud center from its bounding box.
                            float3 cloudCenter = (cloud.maxBounds + cloud.minBounds) * 0.5f;
                            float cloudDist = length(rayPos - cloudCenter);
                            float cloudMaxRadius = length((cloud.maxBounds.z - cloud.minBounds.z));
                            float normalizedCloudDist = saturate(cloudDist / cloudMaxRadius);
                            
                            // -- Blend the two distances --
                            float combinedNorm = lerp(normalizedVoxelDist, normalizedCloudDist, cloudVoxelDistanceLerpVal);
                            
                            // Compute a falloff (smooth edge) using smoothstep
                            float falloff = smoothstep(0.0f, 0.95f, combinedNorm);
                            
                            // Alternative: Use an exponential curve for more rounded blending
                            // float falloff = pow(normalizedDist, 3.0f);  // Cubic falloff for softer edges
                            
                            densityVal *= (1.0f - falloff);


                            // // 3) Large-scale Worley mask
                            // float largeScaleFreq = 0.05f;
                            // float2 largeScaleCoords = rayPos.xz * largeScaleFreq;
                            // float largeScaleWorleyVal = worleyNoiseTexture.SampleLevel(samplerState, float3(largeScaleCoords, 0), 0).r;
                            // float largeScaleMask = smoothstep(0.3f, 0.6f, largeScaleWorleyVal);
                            // densityVal *= largeScaleMask;
                            
                            // 4) Radial height function (horizontal fade near edges)
                           // Extract the center in X–Y, ignoring Z
                            float2 centerXY = float2(cloudCenter.x, cloudCenter.y);
                            
                            // Extract the sample position in X–Y, ignoring Z
                            float2 rayXY = float2(rayPos.x, rayPos.y);
                            
                            // Distance in the X–Y plane from cloud center
                            float radialDist = length(rayXY - centerXY);
                            
                            // "bigRadius" could be half the width/length of your cloud in X–Y
                            float2 cloudSizeXY = (cloud.maxBounds - cloud.minBounds).xy; 
                            float bigRadius = length(cloudSizeXY * 0.5f);
                            
                            // Compute a 0..1 factor for how far out we are horizontally
                            float radialNorm = saturate(radialDist / bigRadius);
                            
                            // The simplest fade is: center = 1, edges = 0
                            float radialFalloff = 1.0f - radialNorm;
                            
                            // Optionally, sharpen the fade:
                            radialFalloff = pow(radialFalloff, 2.0f); // or 3.0f, etc.

                            

                            densityVal *= radialFalloff;
                            
                            float densityFactor = 1.0f;

                            

                            if (densityVal < lowDensityThreshold) {
                                
                                densityFactor = densityMultiplier;
                            }

                            // Increase step size for far-away regions
                            float distanceFactor = lerp(1.0f, farMultiplier, saturate(distanceTraveled / farDistanceThreshold));

                            float adaptiveMultiplier = max(densityFactor, distanceFactor);
                            
                            stepSize = max(minSDF, minStep) * adaptiveMultiplier;

                            //Compute alpha via Beer-Lambert: 

                            float transmittanceDecay = exp(-extinctionCoefficient * densityVal * stepSize);
                            float alpha = 1.0f - transmittanceDecay;

                            //float alpha = 1.0f - exp(-extinctionCoefficient * densityVal * stepSize);

                            float3 scatterColor = float3(.85f, .85f, 1.0f);

                            float3 SunPosition = normalize(SunDirection) * - 10000.0f;
                            
                            float3 DirectionToSun = normalize(SunPosition - rayPos);
                            
                           // float anisotropyG = 0.5; // Forward scattering factor
                           // 
                           // // Henyey-Greenstein phase function
                           // float cosTheta = dot(rayDir, DirectionToSun);
                           // float gSquared = anisotropyG * anisotropyG;
                           // float hg = (1.0 - gSquared) / pow(1.0 + gSquared - 2.0 * anisotropyG * cosTheta, 1.5);
                           // 
                           // // Scale by density
                           // float powder = hg * densityVal;
                            //float powder = PowderEffect(rayDir, DirectionToSun, .1f);

                                   // --- Begin Voxel Shadow Map PCF Sampling ---
                           // Transform the current ray position into light space
                           float4 lightPos = mul(LightViewProj, float4(rayPos, 1.0f));
                           float depthInLight = lightPos.z / lightPos.w;
                           float2 lightNDC = lightPos.xy / lightPos.w;
                           float2 voxelShadowUV = 0.5 * lightNDC + 0.5;
                           // Flip Y if needed
                           voxelShadowUV.y = 1.0 - voxelShadowUV.y;
                          
                           // Define texel size for PCF sampling (adjust to your voxel shadow map resolution)
                           float2 texelSize = 1.0 / float2(1381, 690);

                           float voxelShadowSum = 0.0f;
                           int pcfSamples = 0;  // 4x4 PCF
                           for (int x = -1; x <= 1; x++) {
                               for (int y = -1; y <= 1; y++) {
                                   float2 offset = texelSize * float2(x, y); 
                                   float2 sampleUV = voxelShadowUV + offset;
                           
                                   float4 voxelData = voxelShadowMap.SampleLevel(samplerState, sampleUV, 0.0f);
                                   float sampleVoxelShadow = 1.0f;
                           
                                   if (depthInLight < voxelData.r) {
                                       sampleVoxelShadow = 1.0f;  // Fully lit
                                   } else if (depthInLight > voxelData.b) {
                                       sampleVoxelShadow = 1.0f - voxelData.g;  // Fully shadowed
                                   } else {
                                       float t = saturate((depthInLight - voxelData.r) / (voxelData.b - voxelData.r));
                                       sampleVoxelShadow = lerp(1.0f, 1.0f - voxelData.g, t);
                                   }
                                   pcfSamples++;
                                   voxelShadowSum += sampleVoxelShadow;
                               }
                           }
                           
                           // Average the PCF samples
                           float voxelShadowFactor = shadowFactorMin + (voxelShadowSum / (pcfSamples) * 1.f);

                           float powder = PowderEffect(rayDir, -DirectionToSun, powderBias);
                            
                            float hg = HenyeyGreensteinPhaseFunction(rayDir, -DirectionToSun, anisotropyG);
                           
                           //float voxelShadowFactor = voxelShadowSum;
                            // --- End Voxel Shadow Map PCF Sampling ---


                            //front-to-back accumulation
                            finalColor.rgb += transmittance * scatterColor * alpha * powder * hg * voxelShadowFactor; // * voxelShadowFactor; //* powder;

                            // Update transmittance
                            transmittance *= transmittanceDecay;
                            // float3 localPos = rayPos - voxel.position;
                            ////float3 normal = normalize(rayPos - voxel.position);
                            //float3 normal = ComputeVoxelNormal(localPos);

                            // Compute lighting with shadow consideration
                            //float lighting = ComputeLightingWithShadowsOctree(voxel, rayPos, normalize(SunDirection), normal, densityVal, rayDir);

                            //totalDensity += densityVal;

                            //
                            //float attenuation = ApplyBeersLaw(densityVal, stepSize);

                            //float3 colorGradient = lerp(float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0), totalDensity);
                            //finalColor.rgb += colorGradient * attenuation * powder * densityVal; //* lighting;        

                            if (transmittance.r < 0.01f) {
                                finalColor.a = 1.0f;
                                return finalColor;
                            }
                        }
                    }
                }
                else
                {
                    // If node has children
                    if(node.depth != 0)
                    {
                        stepSize *= node.depth;
                    }
                }
                //else {
                //    if (minSDF <= minDist) {
                //        OctreeNode node = octreeNodes[closestNodeIndex];
                //
                //        // Instead of sampling voxels, use the node’s density
                //        float nodeDensity = node.densitySum; // Assuming each node stores a precomputed density
                //
                //        // Skip processing if density is too low
                //        if (nodeDensity > lowDensityThreshold) 
                //        {
                //            float densityFactor = (nodeDensity < lowDensityThreshold) ? densityMultiplier : 1.0f;
                //            float distanceFactor = lerp(1.0f, farMultiplier, saturate(distanceTraveled / farDistanceThreshold));
                //            float adaptiveMultiplier = max(densityFactor, distanceFactor);
                //            
                //            stepSize = max(minSDF, minStep) * adaptiveMultiplier;
                //
                //            // Compute alpha via Beer-Lambert
                //            float value = -extinctionCoefficient * nodeDensity * stepSize;
                //            float alpha = 1.0f - exp(value);
                //
                //            float3 scatterColor = float3(1.0f, 1.0f, 1.0f);
                //            float3 SunPosition = normalize(SunDirection) * -10000.0f;
                //            float3 DirectionToSun = normalize(SunPosition - rayPos);
                //            float powder = PowderEffect(rayDir, -DirectionToSun);
                //
                //            // Front-to-back accumulation
                //            finalColor.rgb += transmittance * scatterColor * alpha * powder;
                //
                //            // Update transmittance
                //            transmittance *= (1.0f - alpha);
                //
                //            if (transmittance.r < 0.01f) {
                //                finalColor.a = 1.0f;
                //                return finalColor;
                //            }
                //        }
                //    }
                //}
            }

            // Potentially set stepSize to minSDF if we want to move forward just enough

            //stepSize = max(minSDF, minStep) * adaptiveStepSize;
        }

        //--------------------------------------------------
        // 5) Advance the ray
        //--------------------------------------------------
        rayPos += rayDir * stepSize;
        distanceTraveled += stepSize;
    }

    // Return final color
    finalColor.a   = 1.f - transmittance.r;

    //if (finalColor.a < 0.2f) {
    //    finalColor.a = 0.0f;
    //}

    finalColor.rgb = saturate(finalColor.rgb);
    return finalColor;
}

// Ray marching function with bounding box checks
//float4 RayMarch(float2 uv) {
//    float3 rayPos = CameraPosition;
//    float3 rayDir = ComputeRayDirection(uv);
//    float totalDensity = 0.0f;
//    float4 finalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
//    float distanceTraveled = 0.0f;
//    float stepsize = 0.02f;
//    float maxDepth = 100.0f;
//
//    while (distanceTraveled < maxDepth) {
//        float minSDF = 100000.0f;
//        int closestCloudIndex = -1;
//
//        // Check each cloud's bounding box
//        for (int cloudIndex = 0; cloudIndex < numClouds; ++cloudIndex) {
//            Cloud cloud = clouds[cloudIndex];
//            float distanceToCloud = BoxSDF(rayPos, (cloud.maxBounds + cloud.minBounds) * 0.5f, (cloud.maxBounds - cloud.minBounds) * 0.5f);
//
//            if (distanceToCloud < minSDF) {
//                minSDF = distanceToCloud;
//                closestCloudIndex = cloudIndex;
//            }
//        }   
//
//        if (closestCloudIndex != -1) {
//            Cloud cloud = clouds[closestCloudIndex];
//
//            if (minSDF <= 0.1f) { // Ray is inside or near bounding box
//                if (showBoundingBoxes == 1) {
//                    // Loop through all octree nodes
//                    float colorOffset = 0;    
//                    for (uint nodeIndex = 0; nodeIndex < numOctrees; ++nodeIndex) {
//                        OctreeNode node = octreeNodes[nodeIndex];
//                        
//                        
//                        // Only process nodes at the specified depth
//                        if (node.depth == currentDepth) {
//                            float3 nodeCenter = (node.minBounds + node.maxBounds) * 0.5f;
//                            float3 nodeSize = (node.maxBounds - node.minBounds) * 0.5f;
//
//                            // Compute distance to the node's bounding box
//                            float distanceToNode = BoxSDF(rayPos, nodeCenter, nodeSize);
//                            colorOffset += 1.f/8.f;
//
//                            if(colorOffset >= 1.f) {colorOffset = 0.f;}
//
//
//                            // If sufficiently close, render the bounding box
//                            if (distanceToNode <= 0.1f) {
//                                // Compute a gradient color based on depth
//                                float depthFactor = (float)node.depth / (float)14;
//                                float3 color = lerp(float3(1.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), depthFactor);
//                                color.b += colorOffset;
//
//                                // Return the color for visualization
//
//                                return float4(color, 1.0f); // Highlight bounding box
//                            }
//                        }
//                    }
//                }
//
//                // Ray march through voxels
//                for (int i = 0; i < cloud.voxelCount; ++i) {
//                    Voxel voxel = voxels[cloud.voxelOffset + i];
//                    float distanceToVoxel = BoxSDF(rayPos, voxel.position, float3(1.f, 1.f, 1.f) * 0.5f);
//
//                    if (distanceToVoxel <= 0.05f) { // Ray is inside voxel
//
//                        float noiseValue = SampleNoise(rayPos);
//                        float voxelDensity = AccumulateDensity(voxel, noiseValue);
//
//                        float3 localPos = rayPos - voxel.position;
//                        //float3 normal = normalize(rayPos - voxel.position);
//                        float3 normal = ComputeVoxelNormal(localPos);
//
//                        // Compute lighting with shadow consideration
//                        float lighting = ComputeLightingWithShadows(voxel, rayPos, normalize(SunDirection), normal, voxelDensity, rayDir);
//
//                        totalDensity += voxelDensity;
//
//                        // Apply scattering and color gradient with lighting
//                        float powder = PowderEffect(rayDir, -SunDirection);
//                        float attenuation = ApplyBeersLaw(voxelDensity, stepsize);
//                        float3 colorGradient = lerp(float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0), totalDensity);
//                        finalColor.rgb += colorGradient * attenuation * powder * voxelDensity * lighting;
//
//                        if (totalDensity > 1.0f) {
//                            finalColor.a = 1.0f;
//                            return finalColor;
//                        }
//                    }
//                }
//            }
//        }
//
//        stepsize = max(minSDF, 0.02f);
//        rayPos += rayDir * stepsize;
//        distanceTraveled += stepsize;
//    }
//
//    // Final color adjustments
//    finalColor.rgb = saturate(finalColor.rgb);
//    finalColor.a = saturate(totalDensity - 0.05f);
//    float alphaThreshold = -1.0f;
//    if (finalColor.a < alphaThreshold) finalColor.a = 0.0f;
//
//    return finalColor;
//}

// Main compute shader function
//[numthreads(16, 16, 1)]
//void ComputeMain(uint3 id : SV_DispatchThreadID) {
//    uint width, height;
//    outputTexture.GetDimensions(width, height);
//    float2 uv = (float2(id.xy) / float2(width, height)) * 2.f - 1.f;
//    //float4 resultColor = RayMarch(uv);
//    float4 resultColor = RayMarchOctree(uv);
//    outputTexture[id.xy] = resultColor;
//}

//[numthreads(32, 32, 1)]
[numthreads(16, 8, 1)]
//[numthreads(16, 16, 1)]
void ComputeMain(uint3 dtid : SV_DispatchThreadID)
{
    // 1) Get full-resolution dimensions
    uint fullWidth, fullHeight;
    outputTexture.GetDimensions(fullWidth, fullHeight);

    // 2) The dtid.xy now goes only up to (fullWidth/2, fullHeight/2) if 
    //    you dispatch the job at half resolution from the CPU side.
    //    So (dtid.x, dtid.y) is your "half res" coordinate.
    uint halfX = dtid.x;
    uint halfY = dtid.y;

    // 3) Convert halfX, halfY to the top-left of a 2x2 block in the full res image.
    //    Each halfX => a 2 pixel range in X.  Similarly for halfY in Y.
    uint2 baseCoord = uint2(halfX * 2, halfY * 2);

    // 4) Compute a UV from the "center" of that 2x2 block
    //    so that the ray is representative of that block.
    //    We'll pick the pixel center = baseCoord + (1,1).
    uint2 blockCenter = baseCoord + uint2(1, 1);

    // Make sure we don't exceed boundaries if fullWidth or fullHeight is odd
    if (blockCenter.x >= fullWidth)  blockCenter.x = fullWidth  - 1;
    if (blockCenter.y >= fullHeight) blockCenter.y = fullHeight - 1;

    // Convert from [0..width-1] range to [0..1], then map to [-1..1]
    float2 uv = (float2(blockCenter) / float2(fullWidth, fullHeight)) * 2.0f - 1.0f;

    // 5) Call your existing RayMarch(uv) to get a color
    //float4 color = RayMarch(uv);
        
    float4 color = RayMarchOctree(uv);

    // 6) Write that color to the 2x2 block: (baseCoord.x .. baseCoord.x+1, baseCoord.y .. baseCoord.y+1)
    [unroll]
    for (uint dy = 0; dy < 2; dy++)
    {
        [unroll]
        for (uint dx = 0; dx < 2; dx++)
        {
            uint2 outPixel = baseCoord + uint2(dx, dy);

            // Safety check in case the full res is odd
            if (outPixel.x < fullWidth && outPixel.y < fullHeight)
            {
                outputTexture[outPixel] = color;
            }
        }
    }
}