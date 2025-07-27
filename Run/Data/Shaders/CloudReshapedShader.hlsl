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

struct OctreeStackEntry {
    uint nodeIndex;
    float tMin;
};

struct OctreeNode {
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
};

// Resources
StructuredBuffer<Cloud> clouds : register(t0);
StructuredBuffer<Voxel> voxels : register(t1);
Texture3D<float> perlinNoiseTexture : register(t2);
StructuredBuffer<OctreeNode> octreeNodes : register(t3);
SamplerState samplerState : register(s0);
RWTexture2D<float4> outputTexture : register(u0);

bool RayAABBIntersect(float3 rayOrigin, float3 rayDirection, float3 minBounds, float3 maxBounds, out float tEnter, out float tExit)
{
    float3 invDir = 1.0f / rayDirection;
    float3 t0 = (minBounds - rayOrigin) * invDir;
    float3 t1 = (maxBounds - rayOrigin) * invDir;

   float3 tmin = min(t0, t1);
   float3 tmax = max(t0, t1);

    tEnter = max(max(tmin.x, tmin.y), tmin.z);
    tExit  = min(min(tmax.x, tmax.y), tmax.z);

    return (tExit >= tEnter);
}

float BoxSDF(float3 p, float3 voxelPos, float3 boxSize) {
    float3 localPos = p - voxelPos;
    float3 d = abs(localPos) - boxSize;
    float outsideDist = length(max(d, 0.0));
    float insideDist = min(max(d.x, max(d.y, d.z)), 0.0);
    return outsideDist + insideDist;
}

float SphereSDF(float3 p, float3 center, float radius) {
    return length(p - center) - radius;
}

float3 ComputeRayDirection(float2 uv) {
    float2 ndc = uv;
    ndc.y = -ndc.y;
    float4 rayStart = mul(InverseProjMatrix, float4(ndc, 0.0, 1.0));
    rayStart /= rayStart.w;
    float3 worldDir = mul(InverseViewMatrix, float4(rayStart.xyz, 0.0)).xyz;
    return normalize(worldDir);
}

float SampleNoise(float3 rayPos) {
    float3 noiseCoords = rayPos + float3(timeElapsed * 1.0f, -timeElapsed * .5f, -timeElapsed * .1f);
    if (scrolling == 0) {
        noiseCoords = rayPos;
    }
    float noiseValue = perlinNoiseTexture.SampleLevel(samplerState, noiseCoords, 0).r;
    if (useNoise == 0) noiseValue = 0.5f;
    if (invertNoise == 1) noiseValue = 1.0f - noiseValue;
    return saturate(noiseValue);
}

float fbm(float3 p) {
    float f = 0.0;
    float scale = 0.5;
    float factor = 2.0;

    for (int i = 0; i < 5; i++) {
        f += scale * SampleNoise(p);
        p *= factor;
        scale *= 0.5;
    }

    return f;
}

float3 BeerPowderEffect(float density, float3 scatteringColor, float3 extinctionColor, float segmentLength) {
    float extinction = exp(-density * segmentLength);
    float3 scattering = density * scatteringColor * (1.0 - extinction);
    return scattering + extinctionColor * extinction;
}

float ComputeVoxelDensity(Voxel voxel, float sdf, float noiseValue, float threshold) {
    float voxelBlend = saturate(1.0 - sdf / threshold);
    float voxelDensity = voxel.density * noiseValue * densityMultiplier;
    if (useDensity == 0) voxelDensity = 1.0f;
    return voxelDensity * voxelBlend;
}

float3 ComputeScattering(float3 rayDir, float3 voxelPos, float totalDensity) {
    float phaseFunction = 0.5f * (1.0f + pow(dot(rayDir, normalize(SunDirection)), 2));
    float scattering = exp(-totalDensity * 0.2f);
    float intensity = SunIntensity * phaseFunction * scattering;
    return intensity * lerp(float3(0.8f, 0.7f, 0.6f), float3(1.0f, 0.95f, 0.9f), totalDensity);
}

float FindClosestCloud(float3 rayPos, out int closestCloudIndex) {
    float minSDF = 100000.0f;
    closestCloudIndex = -1;

    for (int cloudIndex = 0; cloudIndex < numClouds; ++cloudIndex) {
        Cloud cloud = clouds[cloudIndex];
        float distanceToCloud = BoxSDF(rayPos, (cloud.maxBounds + cloud.minBounds) * 0.5f, (cloud.maxBounds - cloud.minBounds) * 0.5f);

        if (distanceToCloud < minSDF) {
            minSDF = distanceToCloud;
            closestCloudIndex = cloudIndex;
        }
    }

    return minSDF;
}

void TraverseOctree(float3 rayPos, float3 rayDir, uint startNodeIndex, inout float totalDensity, inout float4 finalColor, float stepSize) {
    static const uint MAX_STACK_SIZE = 64;
    OctreeStackEntry stack[MAX_STACK_SIZE];
    uint stackSize = 0;

    // Push initial node
    stack[stackSize].nodeIndex = startNodeIndex;
    stack[stackSize].tMin = 0.0f;
    stackSize++;

    while (stackSize > 0) {
        if (stackSize >= MAX_STACK_SIZE) {
            break; // Avoid stack overflow
        }

        stackSize--;
        OctreeStackEntry current = stack[stackSize];
        OctreeNode node = octreeNodes[current.nodeIndex];

               // Parametric intersection with the node's AABB
        float3 minB = node.minBounds;
        float3 maxB = node.maxBounds;

        float tEnter, tExit;
        bool hit = RayAABBIntersect(rayPos, rayDir, minB, maxB, tEnter, tExit);
        if (!hit) {
            // Ray doesn't intersect this bounding box at all
            continue;
        }

        // If intersection is behind the ray start
        if (tExit < 0.0f) {
            continue;
        }

        // Visualize node bounds for debugging
        if (showBoundingBoxes == 1 && node.depth == currentDepth) {
            finalColor.rgb = float3(1.0, 0.0, 0.0); // Red for debugging
            finalColor.a = 1.0;
            return; // Exit early to isolate traversal
        }

        // Process leaf node with voxels
        if (node.numChildren == 0 && node.numVoxels > 0) {
            for (uint i = 0; i < node.numVoxels; i++) {
                Voxel voxel = voxels[node.firstVoxelIndex + i];
                float distanceToVoxel = BoxSDF(rayPos, voxel.position, 0.5f);

                if (distanceToVoxel < 0.05) {
                    float noiseValue    = fbm(rayPos);
                    float voxelDensity  = ComputeVoxelDensity(voxel, distanceToVoxel, noiseValue, 0.05);
                    totalDensity        = saturate(totalDensity + voxelDensity);

                    float3 scatteringColor  = float3(1.0f, 0.95f, 0.9f);
                    float3 extinctionColor  = float3(0.8f, 0.7f, 0.6f);
                    float3 beerPowderEffect = BeerPowderEffect(voxelDensity, scatteringColor, extinctionColor, stepSize);

                    finalColor.rgb += beerPowderEffect;
                }
            }
        } 
        else if (node.numChildren > 0) {
            // Push children to stack
            for (uint i = 0; i < node.numChildren; i++) {
                if (stackSize < MAX_STACK_SIZE) {
                    stack[stackSize].nodeIndex = node.firstChildIndex + i;
                    stack[stackSize].tMin = 0.0f;
                    stackSize++;
                }
            }
        }
    }
}

float ProcessVoxelsInCloud(Cloud cloud, float3 rayPos, float stepsize, inout float totalDensity, inout float4 finalColor) {
    for (int i = 0; i < cloud.voxelCount; ++i) {
        Voxel voxel = voxels[cloud.voxelOffset + i];
        float distanceToVoxel = BoxSDF(rayPos, voxel.position, float3(1.f, 1.f, 1.f) * 0.5f);
        if (distanceToVoxel < 0.05) {
            float noiseValue    = fbm(rayPos);
            float voxelDensity  = ComputeVoxelDensity(voxel, distanceToVoxel, noiseValue, 0.05);
            totalDensity        = saturate(totalDensity + voxelDensity);

            float3 scatteringColor  = float3(1.0f, 0.95f, 0.9f);
            float3 extinctionColor  = float3(0.8f, 0.7f, 0.6f);
            float3 beerPowderEffect = BeerPowderEffect(voxelDensity, scatteringColor, extinctionColor, stepsize);

            finalColor.rgb += beerPowderEffect;

            if (totalDensity > 1.0f) {
                finalColor.a = 1.0f;
                return 0.0f; // Exit early if density exceeds threshold
            }
        }
    }
    return stepsize;
}

float4 RayMarch(float2 uv) {
    float3 rayPos = CameraPosition;
    float3 rayDir = ComputeRayDirection(uv);
    float totalDensity = 0.0f;
    float4 finalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float distanceTraveled = 0.0f;
    float stepsize = 0.01f;
    float maxDepth = 300.0f;
    float minStepSize = 0.01f;

    while (distanceTraveled < maxDepth) {
        int closestCloudIndex;
        float minSDF = FindClosestCloud(rayPos, closestCloudIndex);

        // Bounding box visualization
        if (showBoundingBoxes == 1) {
            float colorOffset = 0;    
            for (uint nodeIndex = 0; nodeIndex < numOctrees; ++nodeIndex) {
                OctreeNode node = octreeNodes[nodeIndex];

                if (node.depth == currentDepth) {
                    float3 nodeCenter = (node.minBounds + node.maxBounds) * 0.5f;
                    float3 nodeSize = (node.maxBounds - node.minBounds) * 0.5f;
                    float distanceToNode = BoxSDF(rayPos, nodeCenter, nodeSize);


                    colorOffset += 1.f / 8.f;
                    if (colorOffset >= 1.f) {
                        colorOffset = 0.f;
                    }

                    if (distanceToNode <= 0.1f) {
                        float depthFactor = (float)node.depth / (float)14;
                        float3 color = lerp(float3(1.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 1.0f), depthFactor);
                        color.b += colorOffset;
                        return float4(color, 1.0f);
                    }
                }
            }
        } 
        else if (closestCloudIndex != -1) {
            Cloud cloud = clouds[closestCloudIndex];
            uint startIndex = cloud.octreeOffset;

            for (uint rootIndex = startIndex; rootIndex < numOctrees; rootIndex++) {
                TraverseOctree(rayPos, rayDir, rootIndex, totalDensity, finalColor, stepsize);
            }
        }
        
        ///////////////////////////////////////////////
        //step forward
        ///////////////////////////////////////////////
        stepsize = max(minSDF, minStepSize);
        rayPos += rayDir * stepsize;
        distanceTraveled += stepsize;

        if (finalColor.a > 0.99f) break;
    }

    ///////////////////////////////////////////////
    //postAdjustments
    ///////////////////////////////////////////////
    finalColor.rgb  = saturate(finalColor.rgb);
    finalColor.a    = saturate(totalDensity - 0.05f);
    finalColor.a    = finalColor.a - 0.1f;
    float alphaThreshold        = .0f;
    float alphaupperThreshold   = 1.1f;

    if (finalColor.a < alphaThreshold || finalColor.a > alphaupperThreshold) finalColor.a = 0.0f;

    return finalColor;
}

// Main compute shader function
[numthreads(16, 16, 1)]
void ComputeMain(uint3 id : SV_DispatchThreadID) {
    uint width, height;
    outputTexture.GetDimensions(width, height);
    float2 uv = (float2(id.xy) / float2(width, height)) * 2.f - 1.f;
    float4 resultColor = RayMarch(uv);
    outputTexture[id.xy] = resultColor;
}
