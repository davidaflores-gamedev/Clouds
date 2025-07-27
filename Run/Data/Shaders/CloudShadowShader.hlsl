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
    int numClouds;
    int numOctrees;
    float timeElapsed;
    int useDensity;

    int useNoise;
    int invertNoise;
    float densityMultiplier;
    int scrolling;

    int showBoundingBoxes;
    int currentDepth;
    float   extinctionCoefficient;
    float   scatteringCoefficient;

    float3 cloadColor;
    int     padding1;

    float3 voxelDimensions;
    int     padding2;
};

// Resources
StructuredBuffer<Cloud>         clouds              : register(t0);
StructuredBuffer<Voxel>         voxels              : register(t1);
Texture3D<float>                perlinNoiseTexture  : register(t2);
Texture3D<float>                worleyNoiseTexture  : register(t3);
StructuredBuffer<OctreeNode>    octreeNodes         : register(t4);
SamplerState                    samplerState        : register(s0);
RWTexture2D<float4>             outputTexture       : register(u0);


// Shader constants



// Helper functions
float ApplyBeersLaw(float density, float distance) {
    return exp(-extinctionCoefficient * density * distance);
}

float PowderEffect(float3 viewDir, float3 lightDir) {
    return pow(max(dot(viewDir, lightDir), 0.0), scatteringCoefficient);
}

float BoxSDF(float3 p, float3 voxelPos, float3 boxSize) {
    float3 localPos = p - voxelPos;
    float3 d = abs(localPos) - boxSize;
    float outsideDist = length(max(d, 0.0));
    float insideDist = min(max(d.x, max(d.y, d.z)), 0.0);
    return outsideDist + insideDist;

//the value returned will be positive if the point is outside the box, negative if inside, and zero if on the surface
}

int RaycastVsAABB3D(float3 rayOrigin, float3 rayDir, float3 minBounds, float3 maxBounds, float MaxDist) {
    float3 invDir = 1.0f / rayDir;
    float3 t0s = (minBounds - rayOrigin) * invDir;
    float3 t1s = (maxBounds - rayOrigin) * invDir;
    float3 tsmaller = min(t0s, t1s);
    float3 tbigger = max(t0s, t1s);
    float tmin = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    float tmax = min(min(tbigger.x, tbigger.y), tbigger.z);
    if (tmax < 0.0f || tmin > tmax || tmin > MaxDist) {
        return 0;
    }
    return 1;
}

int RaycastVsClouds(float3 rayOrigin, float3 rayDir, float maxDist)
{
    float minDist = 100000.0f;
    for (int i = 0; i < numClouds; ++i) {
        Cloud cloud = clouds[i];
        float dist = RaycastVsAABB3D(rayOrigin, rayDir, cloud.minBounds, cloud.maxBounds, maxDist);
        if (dist < minDist) {
            minDist = dist;
        }
    }
    return minDist;
}

float3 ComputeRayDirection(float2 uv) {
        // For column-major, extract the basis vectors from the columns:
    // The forward direction can be computed via a direct transform, or extracted from the matrix.
    // Using a transform (which works regardless of layout):
    float3 forward = normalize(float3(InverseViewMatrix._11, InverseViewMatrix._21, InverseViewMatrix._31));

    // Calculate the ray origin on the near plane:
    // uv is in [-1,1], so multiplying by half the near screen size gives the correct offset.
    return forward;
}

float3 ComputeRayPosition(float2 uv)
{
    // For column-major, extract the basis vectors from the columns:
    float3 right   = -normalize(float3(InverseViewMatrix._12, InverseViewMatrix._22, InverseViewMatrix._32));
    float3 up      = -normalize(float3(InverseViewMatrix._13, InverseViewMatrix._23, InverseViewMatrix._33));
    // The forward direction can be computed via a direct transform, or extracted from the matrix.
    // Using a transform (which works regardless of layout):

    // Calculate the ray origin on the near plane:
    // uv is in [-1,1], so multiplying by half the near screen size gives the correct offset.
    float3 rayOrigin = CameraPosition 
                     + right * (uv.x * (500.f))
                     + up    * (uv.y * (500.f));

    return  rayOrigin;
}

// Sampling Perlin noise for voxel density
float SampleNoise(float3 rayPos) {
    float scale = 1.0f / voxelDimensions;
    float scrollFactor = 2.0f;

    // Apply scrolling movement to the noise coordinates
    float3 noiseCoords = rayPos * scale + float3(timeElapsed * 0.1, -timeElapsed * 0.05, -timeElapsed * 0.01) * scrollFactor;
    if (scrolling == 0) {
        noiseCoords = rayPos * scale;
    }

    // Sample base Perlin and Worley noise
    float perlinVal = perlinNoiseTexture.SampleLevel(samplerState, noiseCoords, 0).r;
    float worleyBase = worleyNoiseTexture.SampleLevel(samplerState, noiseCoords, 0).r;
    
    // Threshold the base Worley noise to create initial structure
    if (worleyBase <= 0.5f) {
        worleyBase = 0.0f;
    }

    // Detail octave: higher frequency Worley noise for fine textures
    float detailFrequency = 2.0f; // Increase frequency for finer details
    float worleyDetail = worleyNoiseTexture.SampleLevel(samplerState, noiseCoords * detailFrequency, 0).r;
    if (worleyDetail <= 0.5f) {
        worleyDetail = 0.0f;
    }
    
    // Mask octave: lower frequency Worley noise to control where detail appears
    float maskFrequency = 1.0f; // Lower frequency for large-scale structure
    float worleyMaskVal = worleyNoiseTexture.SampleLevel(samplerState, noiseCoords * maskFrequency, 0).r;
    float detailMask = smoothstep(0.4f, 0.6f, worleyMaskVal);
    
    // Blend base and detail Worley noise using the mask
    float blendedWorley = lerp(worleyBase, worleyDetail, detailMask);
    
    // Combine Perlin and blended Worley noise
    float noiseValue = lerp(perlinVal, blendedWorley, 0.25f);

    // Optionally apply a large-scale cloud mask (if you want additional control)
    // float cloudMask = LargeScaleWorley(rayPos.xz);
    // noiseValue *= cloudMask;

    // Apply settings for optional noise inversion and toggling
    if (useNoise == 0) noiseValue = 0.5f;
    if (invertNoise == 1) noiseValue = 1.0f - noiseValue;

    // Enhance contrast and ensure value is within [0,1]
    return pow(saturate(noiseValue), 2.5f);
}

// Accumulating density for rendering clouds
float AccumulateDensity(Voxel voxel, float noiseValue) {
    float densityWeight = 0.3f; // Lower = less impact from voxel density
    float finalDensity = lerp(voxel.density, noiseValue, 0.7f) * densityMultiplier;
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

// Adding anisotropy factor
float anisotropyG = 0.5f; // Positive value means forward scattering; adjust this value as needed

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

    const float densityThreshold = 0.0f;

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
    float3 rayPos           = ComputeRayPosition(uv);
    float3 rayDir           = ComputeRayDirection(uv);
    float  distanceTraveled = 0.0f;
    float  maxDistance      = 1000.0f;
    float3 transmittance    = float3(1.f,1.f,1.f);
    float4 finalColor       = float4(1.f,1.f,1.f,0.f);
    float  totalDensity     = 0.0f;

    float minStep = 0.05f * voxelDimensions.x;
    float minDist = 0.02f;

    float startDist = maxDistance;
    float endDist = 0.0f;

    float avgDensity = 0.0f;

    float aniostropyG = 0.5f;

    

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

            if(distanceTraveled < startDist)
            {            
                startDist = distanceTraveled;
                finalColor.r = distanceTraveled/maxDistance;
            }

            if(distanceTraveled > endDist)
            {
                endDist = distanceTraveled;
                finalColor.b = distanceTraveled/maxDistance;
            }
            

            //

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
                        float distToVoxel = BoxSDF(rayPos, voxel.position, voxelDimensions * .5f);
                        if (distToVoxel < minDist) {
                            // If so, accumulate lighting & color, etc.
                            float noiseVal   = SampleNoise(rayPos);
                            float densityVal = AccumulateDensity(voxel, noiseVal);

                            float voxelDist = length(rayPos - voxel.position);
                            float voxelMaxRadius = length(voxelDimensions * 0.5f);
                            float normalizedVoxelDist = saturate(voxelDist / voxelMaxRadius);
                            
                            // -- Cloud Distance --
                            // Compute cloud center from its bounding box.
                            float3 cloudCenter = (cloud.maxBounds + cloud.minBounds) * 0.5f;
                            float cloudDist = length(rayPos - cloudCenter);
                            float cloudMaxRadius = length((cloud.maxBounds - cloud.minBounds) * 0.5f);
                            float normalizedCloudDist = saturate(cloudDist / cloudMaxRadius);
                            
                            // -- Blend the two distances --
                            float combinedNorm = (.4f * normalizedVoxelDist + .6f * normalizedCloudDist) / (.4f + .6f);
                            
                            // Compute a falloff (smooth edge) using smoothstep
                            float falloff = smoothstep(0.0f, 0.95f, combinedNorm);
                            
                            // Alternative: Use an exponential curve for more rounded blending
                            // float falloff = pow(normalizedDist, 3.0f);  // Cubic falloff for softer edges
                            
                            densityVal *= (1.0f - falloff);

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
                            
                            //Compute alpha via Beer-Lambert: 

                            float value = -extinctionCoefficient * densityVal * stepSize;
                            float expVal = exp(value);
                            float alpha = 1.0f - expVal;

                            float3 scatterColor = float3(1.0f, 1.0f, 1.0f);

                            float3 SunPosition = normalize(SunDirection) * - 10000.0f;
                            
                            float3 DirectionToSun = normalize(SunPosition - rayPos);

                            float anisotropyG = 0.5; // Forward scattering factor
                            
                            // Henyey-Greenstein phase function
                            float cosTheta = dot(rayDir, DirectionToSun);
                            float gSquared = anisotropyG * anisotropyG;
                            float hg = (1.0 - gSquared) / pow(1.0 + gSquared - 2.0 * anisotropyG * cosTheta, 1.5);
                            
                            // Scale by density
                            float powder = hg * densityVal;

                            //front-to-back accumulation
                            finalColor.g += transmittance * scatterColor * alpha * powder;

                            // Update transmittance
                            transmittance *= (1.0f - alpha);
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
                                finalColor.g = 1.0f;
                                //return finalColor;
                            }
                        }
                    }
                }
                else    
                {
                    if(node.depth != 0)
                    {
                        stepSize *= node.depth;
                    }
                }                //else {
                //    // If it’s not a leaf, you might do nothing this pass
                //    // or you can keep stepping or subdivide further.
                //    // Typically, you’d move the ray forward by some fraction
                //    // of minSDF to get closer to the leaf node.
                //}
            }

            // Potentially set stepSize to minSDF if we want to move forward just enough
        }

        //--------------------------------------------------
        // 5) Advance the ray
        //--------------------------------------------------
        rayPos += rayDir * stepSize;
        distanceTraveled += stepSize;
    }

    //float shadowDistance = endDist - startDist;

    // Return final color
    finalColor.g   = 1.f - transmittance.r;
    //finalColor.rgb = saturate(finalColor.rgb);
    return finalColor;
}

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
        
    //if (RaycastVsClouds(  CameraPosition, ComputeRayDirection(uv), 300.0f) = 0) {
    //    outputTexture[baseCoord] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    //    outputTexture[baseCoord + uint2(1, 0)] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    //    outputTexture[baseCoord + uint2(0, 1)] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    //    outputTexture[baseCoord + uint2(1, 1)] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    //    return;
    //}

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