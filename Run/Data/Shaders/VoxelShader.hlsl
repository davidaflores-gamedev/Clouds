cbuffer ConstantBuffer : register(b0)
{
    float4x4 gWorldViewProjection;  // Matrix for transforming vertices
    float3 gLightDirection;          // Sunlight direction
    float3 gCameraPosition;          // Camera position
    float gStepSize;                 // Step size for ray marching
};

// Sampler and texture for voxel density field
Texture3D<float> gVoxelDensityTexture : register(t0);
SamplerState gSampler : register(s0);   

struct PSInput
{
    float4 position : SV_POSITION;
    float3 rayDir : TEXCOORD0; // Ray direction for ray marching
};

float4 PixelMain(PSInput input) : SV_TARGET
{
    // Initialize variables
    float3 rayOrigin = gCameraPosition;
    float3 rayDir = normalize(input.rayDir);

    float3 currentPos = rayOrigin;
    float totalDensity = 0.0;
    float3 lightAcc = float3(0, 0, 0); // Accumulated light color

    // Ray marching loop
    for (float t = 0; t < 1.0; t += gStepSize)
    {
        // Sample the voxel density at the current position
        float3 samplePos = currentPos * float3(0.5) + float3(0.5); // Normalize to [0,1] range
        float density = gVoxelDensityTexture.SampleLevel(gSampler, samplePos, 0).r;

        if (density > 0.01) // If there's some density
        {
            // Simple single scattering based on light direction
            float3 lightDir = normalize(gLightDirection);
            float lightFactor = max(dot(lightDir, rayDir), 0.0);
            lightAcc += density * lightFactor;
        }

        // Accumulate density for final color
        totalDensity += density;

        // Step forward along the ray
        currentPos += gStepSize * rayDir;
    }

    // Final color based on accumulated density and light
    float alpha = saturate(totalDensity);
    float3 color = float3(0.7, 0.7, 0.9) * lightAcc;  // Light blue clouds
    return float4(color, alpha);
}