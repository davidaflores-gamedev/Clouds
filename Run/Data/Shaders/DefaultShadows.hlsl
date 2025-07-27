//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 localPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 position : SV_Position;
	float4 color	: COLOR;
	float2 uv		: TEXCOORD;
	float3 worldPos	: TEXCOORD1;
};

//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
	float4x4 ViewMatrix;
	float4x4 ProjectionMatrix;
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 ModelMatrix;
	float4 ModelColor;
};

cbuffer ShadowConstants : register(b7)
{
    float4x4 LightViewProj;

    float    minLightValue;
};

//------------------------------------------------------------------------------------------------
Texture2D diffuseTexture : register(t0);

//------------------------------------------------------------------------------------------------
SamplerState diffuseSampler : register(s0);

//------------------------------------------------------------------------------------------------
Texture2D<float> shadowMap : register(t1);
Texture2D<float4> voxelShadowMap : register(t2);
//SamplerState shadowSampler : register(s1);)

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 localPosition = float4(input.localPosition, 1);
	float4 worldPosition = mul(ModelMatrix, localPosition);
	float4 viewPosition = mul(ViewMatrix, worldPosition);
	float4 clipPosition = mul(ProjectionMatrix, viewPosition);

	v2p_t v2p;
	v2p.position = clipPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
    v2p.worldPos = worldPosition.xyz;
	return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
	float4 textureColor = diffuseTexture.Sample(diffuseSampler, input.uv);
	float4 vertexColor = input.color;
	float4 modelColor = ModelColor;
	float4 baseColor = textureColor * vertexColor * modelColor;

	float4 lightClipPos = mul(LightViewProj, float4(input.worldPos, 1));
    float2 shadowUV = 0.5 * lightClipPos.xy / lightClipPos.w + 0.5;
	float depthInLight = lightClipPos.z/lightClipPos.w;
	float bias = 0.001f;

	shadowUV.y = 1.0 - shadowUV.y;
    float4 clampUV = saturate(float4(shadowUV, 0, 0));

// Define texel size for PCF sampling (adjust per your shadow map resolution)
float2 texelSize = 1.0 / float2(1381, 690); // Replace with your ShadowMapSize uniform if available

//----------------------------------------------
// Geometry Shadow PCF (using shadowMap) 
// We loop over a 3x3 kernel and average the shadow contribution.
float geomShadowSum = 0.0;
int numSamples = 0;
for (int x = -1; x <= 1; ++x)
{
    for (int y = -1; y <= 1; ++y)
    {
        float2 offset = float2(x, y) * texelSize;
        float sampleDepth = shadowMap.Sample(diffuseSampler, clampUV + offset).r;
        // If the current depth (with bias) is greater than the stored depth, add shadow.
        geomShadowSum += ((depthInLight - bias) > sampleDepth) ? 0.5f : 0.0f;
        numSamples++;
    }
}
float geomShadow = geomShadowSum / numSamples;

//----------------------------------------------
// Voxel Shadow PCF (using voxelShadowMap)
// The voxel shadow map stores:
//    R = earliest depth where density starts,
//    G = total density accumulated,
//    B = last depth of density.
// Our goal:
//   - If depthInLight is less than voxelData.r, no density has accumulated (fully lit).
//   - If depthInLight is greater than voxelData.b, apply full voxel density shadowing.
//   - Otherwise, leave it as fully lit for now (you can later add interpolation if desired).
float voxelShadowSum = 0.0;
numSamples = 0;
for (int x = -1; x <= 1; ++x)
{
    for (int y = -1; y <= 1; ++y)
    {
        float2 offset = float2(x, y) * texelSize;
        float4 voxelData = voxelShadowMap.Sample(diffuseSampler, clampUV + offset);
        float sampleVoxelShadow = 1.0f;
        
        if (depthInLight < voxelData.r)
        {
            sampleVoxelShadow = 1.0f; // Before density accumulation.
        }
        else if (depthInLight > voxelData.b)
        {
            sampleVoxelShadow = 1.0f - voxelData.g; // Full density applied.
        }
        else
        {
            sampleVoxelShadow = 1.0f; // In between—no change for now.
        }

        // Clamp to a minimum light value (e.g., 0.2) if desired.
        if (sampleVoxelShadow <= minLightValue)
        {
            sampleVoxelShadow = minLightValue;
        }
        
        voxelShadowSum += sampleVoxelShadow;
        numSamples++;
    }
}
float voxelShadow = voxelShadowSum / numSamples;

//----------------------------------------------
// Combine the two shadow contributions
// Using min() ensures that if either shadow darkens the pixel, that darkness is applied.
float finalLitFactor = min(1.0 - geomShadow, voxelShadow);

// Final color computation
float4 color = baseColor * finalLitFactor;
clip(color.a - 0.01);
return color;
}