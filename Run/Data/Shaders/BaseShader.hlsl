// VoxelVertexShader.hlsl
// Vertex shader that processes voxel position, color, and UV coordinates

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

VSOutput VertexMain(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.uv = input.uv;
    return output;
}

// FullScreenQuadPS.hlsl
Texture2D<float4> rayMarchResult : register(t0);
SamplerState samLinear : register(s0);

float4 PixelMain(VSOutput input) : SV_Target {
    return rayMarchResult.Sample(samLinear, input.uv);
}
