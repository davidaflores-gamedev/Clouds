// Basic vertex input
struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 viewDir : TEXCOORD1;
};

// Camera info (just the inverse view-projection for direction)
cbuffer CameraConstants : register(b2)
{
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InverseViewMatrix;
    float4x4 InverseProjMatrix;
};

VSOutput VertexMain(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.uv = input.uv;

        // --- MATCH CLOUD SHADER MATH ---
    float2 ndc = input.uv * 2.0 - 1.0;
    ndc.y = -ndc.y; // <- Important flip to match cloud raymarcher
    float4 rayView = mul(InverseProjMatrix, float4(ndc, 0.0, 1.0));
    rayView /= rayView.w;

    float3 worldDir = mul(InverseViewMatrix, float4(rayView.xyz, 0.0)).xyz;
    output.viewDir = normalize(worldDir);
    return output;
}

float3 RayleighPhase(float cosTheta)
{
    return (3.0 / (16.0 * 3.14159265)) * (1.0 + cosTheta * cosTheta);
}

float3 ComputeSkyColor(float3 viewDir)
{
    // Camera "up" in world space
    float3 cameraUp = normalize((float3)InverseViewMatrix[1].xyz);

    float3 worldUp = float3(0, 0, 1);

    // Angle between view and up = height factor
    float alignment = dot(viewDir, worldUp);
    float t = saturate(alignment * 0.5 + 0.5); // remap [-1,1] to [0,1]

    // Basic sky gradient
    float3 topColor = float3(0.32, 0.52, 1.00);   // Deep blue at zenith
    float3 horizonColor = float3(0.80, 0.88, 1.00); // Pale blue near horizon
    float3 color = lerp(horizonColor, topColor, pow(t, 0.7));

    // Sun direction (world space)
    float3 sunDir = normalize(float3(0.0, 0.5, 0.86)); // 60° above horizon, forward

    // Simple sun highlight
    float sunIntensity = pow(saturate(dot(viewDir, sunDir)), 128.0);
    color += sunIntensity * float3(1.0, 0.95, 0.8);

    return saturate(color * 1.2);
}

float4 PixelMain(VSOutput input) : SV_Target0
{
    float3 viewDir = normalize(input.viewDir);
    float3 skyColor = ComputeSkyColor(viewDir);
    return float4(skyColor, 1.0);
}