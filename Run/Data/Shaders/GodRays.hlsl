struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
    float2 sunScreenPos : TEXCOORD1;
};

cbuffer CameraConstants : register(b2) {
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InverseViewMatrix;
    float4x4 InverseProjMatrix;
    float3 CameraPosition;
    float2 NearScreenSize;
};

cbuffer ShadowConstants : register(b7)
{
    float4x4 LightViewProj;
};

cbuffer GodRaysConstants : register(b8)
{
    float4x4 viewProjection; 

    float3 lightPosition;  // Light position in screen space (normalized)
    float intensity;       // Intensity of the god rays
    float decay;           // Controls how rays fade over distance
};



Texture2D sceneTexture : register(t0);  // Scene texture (optional)
Texture2D ShadowMapTexture : register(t1);  // Shadow map texture (optional)))
SamplerState samplerState : register(s0);

float3 ComputeRayDirection(float2 uv) {

     float2 ndc = uv * 2.0f - 1.0f;
    ndc.y *= -1.0f;

    float4 rayStart = mul(InverseProjMatrix, float4(ndc, 0.0, 1.0));
    rayStart /= rayStart.w;
    float3 worldDir = mul(InverseViewMatrix, float4(rayStart.xyz, 0.0)).xyz;
    return normalize(worldDir);
}

VS_OUTPUT VertexMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = float4(input.position, 1.0);
    output.uv = input.uv;

    float4 sunClipSpace = mul(viewProjection, float4(lightPosition, 1.0));
    sunClipSpace.xyz /= sunClipSpace.w;

        // If the sun is behind the camera (clipSpace.w < 0), mark it as invalid
    if (sunClipSpace.w < 0.0f)
    {
        output.sunScreenPos = float2(-1, -1); // Invalid position
    }
    else
    {
    
        // Compute distance from light position
        output.sunScreenPos.x = (sunClipSpace.x * 0.5f) + 0.5f;
        output.sunScreenPos.y = (sunClipSpace.y * -0.5f) + 0.5f;

        output.sunScreenPos *= 0.5f; // Scale to [0, 1]]
    }

    return output;
}

float3 CalculateGodRays(float2 uv)
{
    float3 rayOrigin = CameraPosition;
    float3 rayDir = ComputeRayDirection(uv * 2.f);
    
    int numSteps = 100;

    float stepSize = 500.f / numSteps;
    float3 stepVec = rayDir * stepSize;

    float3 samplePos = rayOrigin;
    float3 godRayAccum = float3(0, 0, 0);

    for (int i = 0; i < numSteps; i++)
    {
        float4 lightSpace = mul(LightViewProj, float4(samplePos, 1.0));
        float2 shadowUV = lightSpace.xy / lightSpace.w * 0.5f + 0.5f;
        shadowUV.y = 1.0 - shadowUV.y;
        float sampleDepth = lightSpace.z / lightSpace.w;
        
        if (any(shadowUV < 0.0f) || any(shadowUV > 1.0f))
            break;

        float shadowDepth = ShadowMapTexture.Sample(samplerState, shadowUV).r;
        bool inShadow = sampleDepth > shadowDepth + 0.001;
        
        if(!inShadow)
        {
            float density = 0.01;
            float distToCamera = length(samplePos - CameraPosition);
            float attenuation = exp(-distToCamera * decay);
            float3 scatter = float3(1.0, 1.0, 0.0) * density * attenuation * intensity;

            godRayAccum += scatter;
        }
        samplePos += stepVec;
    }
    return godRayAccum;
}

float4 PixelMain(VS_OUTPUT input) : SV_TARGET
{
    float2 uv = input.uv;

    // Check if the sun is in front of the camera
    //float padding = 0.3; // Allow the sun to be slightly offscreen before removing god rays
    //
    //if (input.sunScreenPos.x < -padding || input.sunScreenPos.x > (1.0f + padding) ||
    //    input.sunScreenPos.y < -padding || input.sunScreenPos.y > (1.0f + padding))
    //{
    //    return float4(0, 0, 0, 1); // No god rays if sun is too far offscreen
    //}
    //
    //// Compute vector from light to fragment
    //float2 rayDir = input.sunScreenPos - uv;
    //rayDir.y *= 1.5f;
    //rayDir.x *= 3.f;
    //float dist = length(rayDir);
    //
    //// Normalize ray direction
    //rayDir /= max(dist, 0.001); // Prevent division by zero
    //
    //// Compute radial falloff for god rays
    //float rayStrength = exp(-dist * decay) * .2f;
    //
    //// Sun Glow Effect (Yellow Circle)
    //float innerRadius = 0.02;  // Controls the bright sun core
    //float outerRadius = 0.05    ;  // Controls smooth transition
    //float sunGlow = smoothstep(outerRadius, innerRadius, dist);
    //sunGlow = saturate(sunGlow);  // Ensure it stays within [0,1]
    //
    //// Define the sun's yellow color
    //float3 sunColor = float3(1.0, 1.0, 0.0); // Yellow
    //
    //// Reduce god ray intensity
    //rayStrength *= 0.5f;  // Reduce intensity to avoid overexposure

    // Combine god rays + sun glow
    //float3 finalColor = lerp(float3(0,0,0), sunColor, sunGlow * 0.8f) + (rayStrength * sunColor);

    // Add god rays
    float3 godRays = CalculateGodRays(input.uv);
    float3 finalColor = godRays;

    return float4(finalColor, 1.0);
}